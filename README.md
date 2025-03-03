# Windows Taskbar Hider - Make your wallpaper look cleaner!

## What is this?
This is an application that hides the taskbar when no window is maximized!
The initial idea was to show a cleaner desktop with desktop icons disabled.
This can work perfectly with **TranslucentTB** to make the desktop look even more cleaner.
**Keep in mind that the taskbar still pertains its area, it doesn't extend your desktop.**

***The application was not yet tested on Windows 11***

## How it works?
The application loops over all visible, non-iconic windows and checks specified tags in the configuration.
All of this is happening every 10ms (can be changed) + indeterminate loop processing time.

Everything was made with "pure" C++, no libraries, only using WIN32 API. Not to mention this is my first real C++ project, so it was quite a fun suffering.

## Doesn't Windows already have this?
Yes, it does, but it's really buggy, plus this application has extra features. It has two main problems:

1. When you want to click something that is near the taskbar, and you accidentally move your mouse too close to the taskbar line,
it triggers the taskbar to open and that is really annoying!
2. Sometimes taskbar goes behind maximized window.

This application doesn't hide the taskbar completely like Windows does, it only hides itself when there are no maximized windows.

## Features
* WIN32 API based UI window for debugging (overworked idea)
  * Dark mode (the most important thing here)
  * Auto table updater
  * Ability to show all windows even though one was marked as detected.
* Multi-language support
  * Supported default languages: **English**, **Ukrainian**
* Multi-monitor support (only tested with two)
* Hovering over taskbar area will cause it to appear
* Configuration
* Open and reload the config file
* Ignore specific windows with tags or make exceptions
* Add/Remove from startup folder
* Taskbar pausing
* Taskbar opacity when it's hidden, being shown and when hovered over with mouse
* Automatically notices display changes (e.g. another monitor got connected/disconnected)

## Program's arguments
| Argument          | Alias      |  Parameters  | Description                                     |
|-------------------|------------|:------------:|-------------------------------------------------|
| `--reset-taskbar` | `-rtb`     |     None     | Reset taskbar and exit                          |
| `--no-config`     | `-nc`      |     None     | Don't load/save config, unless loaded from menu |
| `--config:<key>`  | `-c:<key>` |  \<value\>   | Override config's value (internally)            |

## Configuration
A file called config.ini is going to be created next to exe file (unless `--no-config` argument specified).



## TODO
- [ ] Settings and other similar apps that uses ApplicationFrameHost.exe don't get detected
- [ ] Show taskbar when any context menu from taskbar is opened.
- [ ] Maybe add animations when taskbar is appearing/disappearing?

## Building
This project was built using MinGW and CLion IDE.

You can build the project without any studios with the following commands:

### Configure the release build.

```bash
cmake.exe -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=ninja.exe -G Ninja -S <source> -B <source>/cmake-build-release
```

### Build the executable.

```bash
cmake.exe --build <source>/cmake-build-release --target WindowsTaskbarHider -j 6
```
`<source>` is a path (absolute or relative) to the source directory. If you don't have added cmake.exe and/or ninja.exe,
you can provide an absolute path to the mentioned executables, but don't forget to quote them like:

```bash
"C:\path\to\cmake.exe" ... -DCMAKE_MAKE_PROGRAM="C:\path\to\ninja.exe" -G ...
```

### Updating language keys (language.h.in and assets/language/language.\*.ini files)
If you are using IDE like CLion, make sure to clean CMake project before building, otherwise CMake won't notice those changed.

## Credits and appreciation!
* Thanks to [SuperNeon4ik](https://github.com/SuperNeon4ik) for Ukrainian translations!
* The icon was designed by [justicon (freepik.com)](https://www.freepik.com/icon/programming_1567754)