#include <i3ipc++/ipc.hpp>
#include <map>
#include <string>
#include <regex>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>
#include <X11/Xutil.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <cstring>
#include <jsoncpp/json/json.h>

typedef std::vector<std::string> StrVect;

int devId = XkbUseCoreKbd;
std::map<uint64_t, std::string> window_layouts;
uint64_t activeWindow = -1;
std::mutex wl_mutex;

//taken from https://github.com/grwlf/xkb-switch
// XkbRF_VarDefsRec contains heap-allocated C strings, but doesn't provide a
// direct cleanup method. This wrapper privides a workaround.
// See also https://gitlab.freedesktop.org/xorg/lib/libxkbfile/issues/6
struct XkbRF_VarDefsRec_wrapper {

    XkbRF_VarDefsRec _it;

    XkbRF_VarDefsRec_wrapper() {
        std::memset(&_it, 0, sizeof(_it));
    }

    ~XkbRF_VarDefsRec_wrapper() {
        if (_it.model) std::free(_it.model);
        if (_it.layout) std::free(_it.layout);
        if (_it.variant) std::free(_it.variant);
        if (_it.options) std::free(_it.options);
    }
};

std::vector<std::string> split(const std::string &in, const std::string &stRegex) {
    const std::regex regex(stRegex);
    std::sregex_token_iterator wit(in.begin(), in.end(), regex, -1, std::regex_constants::match_default);
    std::vector<std::string> tokens(
            std::sregex_token_iterator(in.begin(), in.end(), regex, -1),
            std::sregex_token_iterator()
    );
    return tokens;
}

//debug reporting
void printLayoutsTable() {
    for(auto &p : window_layouts) {
        fprintf(stdout, "[%lu, %s]", p.first, p.second.c_str());
    }
    fprintf(stdout, "\n");
}

void updateLayoutIfDifferent(uint64_t winId, const std::string &newLayout) {
    bool updateRequired = false;
    auto findLayout = window_layouts.find(winId);
    if (findLayout == window_layouts.end()) {
        updateRequired = true;
    } else {
        if (findLayout->second != newLayout) {
            updateRequired = true;
        }
    }
    if (updateRequired) {
        fprintf(stdout, "DEBUG: updating layout for window %lu, %s\n", winId, newLayout.c_str());
        std::lock_guard<std::mutex> guard(wl_mutex);
        window_layouts[winId] = newLayout;
        wl_mutex.unlock();
    }
    //printLayoutsTable();
}

void switchLayout(const std::string &name) {
    Display *display = XOpenDisplay(nullptr);
    if (display == NULL) {
        fprintf(stderr, "ERROR: Cannot open display\n");
        exit(1);
    }

    XkbRF_VarDefsRec_wrapper vdw;
    char *tmp = NULL;
    Bool res = XkbRF_GetNamesProp(display, &tmp, &vdw._it);
    free(tmp);
    if (res != True) {
        fprintf(stderr, "ERROR: Failed to fetch layout properties");
        XCloseDisplay(display);
        return;
    }
    StrVect vl = split(vdw._it.layout, ",");
    auto it = std::find(vl.begin(), vl.end(), name);
    if (it == vl.end()) {
        fprintf(stderr, "ERROR: Layout name is not found: %s\n", name.c_str());
        XCloseDisplay(display);
        return;
    }

    Bool result = XkbLockGroup(display, devId, it - vl.begin());
    if (result == True) {
        XFlush(display);
    } else {
        fprintf(stderr, "ERROR: failed to lock the keysym group");
    }
    XCloseDisplay(display);
}

std::string getCurrentLayout() {
    Display *display = XOpenDisplay(nullptr);
    if (display == NULL) {
        fprintf(stderr, "ERROR: Cannot open display\n");
        exit(1);
    }
    XkbStateRec state;
    XkbGetState(display, XkbUseCoreKbd, &state);

    XkbRF_VarDefsRec_wrapper vdw;
    XkbRF_GetNamesProp(display, nullptr, &vdw._it);
    char *tok = strtok(vdw._it.layout, ",");

    for (int i = 0; i < state.group; i++) {
        tok = strtok(nullptr, ",");
        if (tok == nullptr) {
            fprintf(stderr, "ERROR: failed to parse current keyboard layout group\n");
        }
    }
    XCloseDisplay(display);
    return std::string(tok);
}

void applyLayout(uint64_t winId) {
    auto findLayout = window_layouts.find(winId);
    std::string layout = "us";
    if (findLayout != window_layouts.end()) {
        layout = findLayout->second;
    }
    if (layout != getCurrentLayout()) {
        std::lock_guard<std::mutex> guard(wl_mutex);
        switchLayout(layout);
        wl_mutex.unlock();
    }
}

void detectLayoutChange() {
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        fprintf(stderr, "ERROR: Cannot open display\n");
        exit(1);
    }

    XEvent e;
    XKeysymToKeycode(display, XK_F1);
    int xkbEventType;
    XkbQueryExtension(display, 0, &xkbEventType, 0, 0, 0);
    XkbSelectEvents(display, XkbUseCoreKbd, XkbAllEventsMask, XkbAllEventsMask);

    XSync(display, False);

    while (true) {
        XNextEvent(display, &e);
        if (e.type == xkbEventType) {
            XkbEvent *xkbEvent = (XkbEvent *) &e;
            if (xkbEvent->any.xkb_type == KeymapNotify) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string layout = getCurrentLayout();
                if (activeWindow != -1) {
                    updateLayoutIfDifferent(activeWindow, layout);
                }
            }
        }
    }
    fprintf(stderr, "ERROR: exiting detection thread\n");
}

int main() {
    std::thread lswith_detect_thread(detectLayoutChange);

    i3ipc::connection conn;
    conn.subscribe(i3ipc::ET_WINDOW);

    conn.signal_window_event.connect([](const i3ipc::window_event_t &ev) {
        if (ev.type == i3ipc::WindowEventType::FOCUS) {
            if (ev.container) {
                activeWindow = ev.container->xwindow_id;
                std::string wName = ev.container->window_properties.xclass;
                fprintf(stdout, "DEBUG: active window: %lu, name: %s\n", activeWindow, wName.c_str());
                applyLayout(activeWindow);
            } else {
                activeWindow = -1;
            }
        }
    });

    while (true) {
        try {
            conn.handle_event();
        } catch (const Json::Exception &e) {
            fprintf(stderr, "ERROR: %s\n", e.what());
        }
    }
    return 0;
}
