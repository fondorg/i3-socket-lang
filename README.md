# i3-socket-lang

Utility allowing the use of different keyboard layouts for every window.  
Remembers the last used keyboard layout and switches automatically when window is focused.  
  
The application is using [i3ipc++](https://github.com/drmgc/i3ipcpp) library to connect to the i3 [IPC Interface](https://i3wm.org/docs/ipc.html).
It is subscribing to the window focus events.   
Utility manages keyboard layout switching using X keyboard extension.

### Requirements

Follow the requirements section of [i3ipc++](https://github.com/drmgc/i3ipcpp) library.  
You also  will need `libxkbfile-dev` package

### Installation

Select appropriate location for the utility, e.g. `~/bin/i3_socket_lang`  
add to your i3 config:  
```bash
exec_always ~/bin/i3_socket_lang
```
restart i3

### Usage

**TBD**