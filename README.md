# rwm
A terminal-based window manager with additional desktop environment.

## Build
`scripts/build.sh` generates `./rwm`, the executable. `scripts/build.sh DEBUG` will compile unoptimised version with debug symbols (not to be confused with the `DEBUG` macro in the code which is currently broken).
`scripts/build.sh GENLIB` will generate a separate .so for the desktop that `rwm` would then load at runtime, if one should desire this (`scripts/runlocal.sh` to test on the library locally installed in this folder).

You can write your own desktop environment on top of RWM. For this purpose we provide `source/desktop_template.cpp`.
Currently, the `.cpp` files are not very well documented and neither is the internal structure of RWM, though the headers should have good-enough comments to where this should not be too painful to do.

## Config
`etc/widgets.cfg` is a TSV with the format `display-command` `command-on-launch` `window-height` `window-width`.

`etc/env.cfg` is for setting environment variables.

`etc/theme.cfg` defines the window theme. Under the section `[Color]`, the color scheme works as follows:
- a hex digit (0-9, a-f) is treated as a four bit colour code
- a number in square brackets `([123])` is treated as a VGA colour code
- an asterisk `*` is the default background/foreground colour
- currently unsupported: full RGB

`etc/settings.cfg` is a general config file with the following settings:
- `desktop_directory`: Directory whose files will be rendered on the desktop
- `cwd`: Directory in which all new programs start
- `background`: process running as background. We recommend our `./ascii2utf8` for rendering ANSI art
- `draw_icons`: whether to draw desktop icons
- `taskbar_pos`: where to draw the taskbar (currently unused)
- `task_tab_size`: the size of a taskbar window/process tab
- `default_window_size`: default dimensions of a newly spawned window
- `force_convert`: whether to forcefully convert UTF-8 to ASCII/whatever encoding the system may support
- `bold_mode`: how the bold text escape sequence is to be rendered (currently unused)
- `default_shell`: default shell that spawns when a new shell is opened; also used with Alt-D menu to spawn new windows

## Keybinds
Currently, RWM uses keybinds similar to i3:
- `Alt + Enter`: new shell window
- `Alt + D`: open program in new window
- `Alt + J`: focus left
- `Alt + K`: focus down
- `Alt + L`: focus up
- `Alt + ;`: focus right
- `Alt + Shift + J`: move window to the left
- `Alt + Shift + K`: move window to the down
- `Alt + Shift + L`: move window to the up
- `Alt + Shift + ;`: move window to the right
- `Alt + E`: toggle tiled/windowed mode
- `Alt + W`: tabbed mode
- `Alt + H`: horizontal
- `Alt + V`: vertical
- `Alt + F`: fullscreen
- `Alt + M`: maximise window
- `Alt + Shift + Q`: close window 
- `Alt + Shift + E` or `Ctrl + C`: quit RWM
- `Alt + Shift + R`: reinitialise RWM
- `Alt + Shift + C`: reload config files

Some of these keybinds may change in the future. Also, because of how `ncurses` treats key presses, some of these may be broken for you.
The current fix is to either manually edit the source code yourself or change your keyboard layout to one where `Alt + Key` does not produce accented characters.
Soonish, I plan to make a configurable keymap file and perhaps a fix for the `Alt` key combo issue.

## How RWM works
The basic principle is that we spawn a process for which we spawn a new virtual tty. 
Its `stdin`, `stdout` and `stderr` are replaced by the new tty `slave` file descriptor, which allows RWM to act as a terminal for it.
RWM then periodically reads the output the process produces from its own corresponding `master` file descriptor.
Any escape sequences are parsed and the corresponding `ncurses` library calls are called.\*
Each RWM window has a parser state, which keeps track of all the things that have been set/unset by escape sequences. 
This works even if the output parser routine receives only half an escape sequence, with the rest being given later.

In the main loop, RWM first queries output from all active windows (so not in the `FROZEN` or `ZOMBIE` state) and renders it to the screen.
It then queries keyboard input, which it first attempts to send to the desktop manager `desktop.cpp` via `key_priority`.
If this fails, it sends it to the active window, or, if not present, to `desktop.cpp` via `key_pressed`.
Mouse presses are handled similarly, where they are either sent to a window or to `desktop.cpp` via `mouse_pressed` or `frame_click`.

\**Note: we cannot just forward the escape sequences to the main terminal, since most of them either mustn't be forwarded; e.g. `\033[J` clear screen, which should only clear the inner RWM window, not the entire screen; or require keeping track of anyway, e.g. colour codes, which we need to switch away from when we render a new window and switch back to when rendering it again.*