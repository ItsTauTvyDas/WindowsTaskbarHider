# Windows Taskbar Hider - Make your wallpaper look cleaner!

## What is this?
This is an application that hides the taskbar when no window is maximized!
The initial idea was to show a cleaner desktop with desktop icons disabled.
This can work perfectly with TranslucentTB to make the desktop look even more cleaner.
**Keep in mind that the taskbar still pertains its area, it doesn't extend your desktop.**

## Doesn't Windows already have this?
Yes, it does have it, but it's really buggy. It has two main problems:

1. When you want to click something that is near the taskbar, and you accidentally move your mouse too close to the taskbar line, it triggers the taskbar to open and that is really annoying!
2. Sometimes taskbar goes behind maximized window.

This application doesn't hide the taskbar completely like Windows does, it only hides itself when there are no maximized windows.

## Features
* Hovering over taskbar area will cause it to appear.
* Configuration.
* Open and reload the config file.
* Attach/Detach console window (auto enables debug mode when attached). Console shows the windows that are in maximized state.
* Ignore specific windows (either by title, class or process path filename).
* Add/Remove from startup folder.
* Pausing.
* Taskbar opacity.
* Process duplication prevention. If already running, the application will ask user if they want to forcefully shutdown that running process.

## Program's arguments
| Argument           | Alias        |  Parameters  | Description                                                            |
|--------------------|--------------|:------------:|------------------------------------------------------------------------|
| `--reset-taskbar`  | `-rtb`       |     None     | Reset taskbar and exit.                                                |
| `--debug`          | `-d`         |     None     | Enable debug mode.                                                     |
| `--no-config`      | `-nc`        |     None     | Don't load/save config, unless loaded from system tray menu.           |
| `--config:\<key\>` | `-c:\<key\>` |  \<value\>   | Override config's value (internally) before launching the application. |

## Configuration
A file called config.ini is going to be created next to exe file (unless `--no-config` argument specified).

## TODO
- [ ] Show taskbar when start menu is open.
- [ ] Show taskbar when hovering over window's preview.
- [ ] Show taskbar when any context menu from taskbar is opened.
- [ ] Add more window tags for ignoring specific windows (currently supported: class, title and process path filename).
- [ ] Make debug console work on another thread.
- [ ] Update debug console on key press.