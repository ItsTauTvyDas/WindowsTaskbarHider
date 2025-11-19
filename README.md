# Windows Taskbar Hider - Make your wallpaper look cleaner! (Not finished)

## What is this?
This is a light-weight Windows-only application that hides the taskbar when no window is maximized!
The initial idea was to show a cleaner desktop with desktop icons disabled.
This can work perfectly with **TranslucentTB** to make the desktop look even more cleaner.
**Keep in mind that the taskbar still pertains its area, it doesn't extend your desktop.**

***The application has not been yet tested on Windows 11!***

## How it works?
The application loops over all visible, non-iconic windows and checks specified tags in the configuration.
All of this is happening every 10ms (can be changed) + indeterminate loop processing time.

Everything was made with "pure" and simple C++, no extra packages, only using WIN32 API.
Not to mention this is my first real C++ project, so it was quite a fun suffering.

## Doesn't Windows already have this?
Yes, it does, but it's really buggy, plus this application has extra features. It has two main problems:

1. When you want to click something that is near the taskbar, and you accidentally move your mouse too close to the taskbar line,
it triggers the taskbar to open and that is really annoying!
2. Sometimes taskbar goes behind maximized window.

This application doesn't hide the taskbar completely like Windows does, it only hides itself when there are no maximized windows.

## Seems like an over-kill for such simple thing as hiding a taskbar
You are indeed right, but to be honest, I did all this just for fun and for better understanding how C++ and Windows (internally) works.

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
* Taskbar opacity when it's hidden, being shown (triggered by maximized window) and when hovered over with mouse
* Hover/disappearing taskbar animations
* Portable and installable versions

## Difference between portable and installed versions
| Feature               | Portable                                                       | Installed |
|-----------------------|----------------------------------------------------------------|-----------|
| Configuration         | Loads if exists or saves if reloaded through *actions* button  | ✔️        |
| Custom languages      | ❌ (not even loadable)                                          | ✔️        |
| Run on startup option | ❌                                                              | ✔️        |

## Arguments
| Argument          | Alias      |  Parameters  | Description                                     |
|-------------------|------------|:------------:|-------------------------------------------------|
| `--reset-taskbar` | `-rtb`     |     None     | Reset taskbar and exit                          |
| `--no-config`     | `-nc`      |     None     | Don't load/save config, unless loaded from menu |
| `--config:<key>`  | `-c:<key>` |  \<value\>   | Override config's value (internally)            |

## Configuration
A file called config.ini is going to be created next to exe file (unless `--no-config` argument specified).
Extra configuration can be enabled by triggering "Expose internal configuration keys" button in GUI's actions select
(if you want to restore them to default, just delete that section from the file and reload the config).
Application has custom built-in INI parser, minimal *INI standards* are implemented but with one exception, where boolean is required,
besides `0` and `1` it can also be `false` or `true` keywords.

```ini
; Github: https://github.com/ItsTauTvyDas/WindowsTaskbarHider

;  _    _ _           _                 _____         _    _                _   _ _     _
; | |  | (_)         | |               |_   _|       | |  | |              | | | (_)   | |
; | |  | |_ _ __   __| | _____      _____| | __ _ ___| | _| |__   __ _ _ __| |_| |_  __| | ___ _ __
; | |/\| | | '_ \ / _` |/ _ \ \ /\ / / __| |/ _` / __| |/ / '_ \ / _` | '__|  _  | |/ _` |/ _ \ '__|
; \  /\  / | | | | (_| | (_) \ V  V /\__ \ | (_| \__ \   <| |_) | (_| | |  | | | | | (_| |  __/ |
;  \/  \/|_|_| |_|\__,_|\___/ \_/\_/ |___|_/\__,_|___/_|\_\_.__/ \__,_|_|  \_| |_/_|\__,_|\___|_|

[General]
Language = en

[Window]
; Default values for checkboxes in the window display
DarkMode = 1
AutoUpdate = 0
ShowAllWindows = 0

[Window Behaviour]
OpenOnStart = 0
CloseToTray = 0
MinimizeToTray = 1
; Only works if CloseToTray is disabled
CloseConfirmMessage = 1
; Disable automatic updates to the GUI when program gets unfocused
DisableAutoUpdateWhenUnfocused = 1

[Taskbar]
; Taskbar update loop interval in milliseconds
UpdateInterval = 10
; If enabled, opacity levels can be defined up to 255
UseRealOpacityValues = 0
; Opacity level from 0 to 100 (or 255 if above is enabled)
OpacityWhenHidden = 0
; Bellow minimum opacity limits change to 1, because 0 causes the taskbar to lose interactivity
OpacityWhenShown = 90
OpacityWhenHoveredOver = 100

[Taskbar Hover Animation]
; Animation between OpacityWhenShown/OpacityWhenHidden and OpacityWhenHoveredOver
; If changed while application is running, restart is required!
Enabled = 1
AnimationStepDelay = 3
AnimationOpacityStep = 10
[Ignored Windows]
; Setting this to false (0) could slow down the application with debug mode on
AlwaysIgnoreWhenNotMaximized = 1
; Available tags: 
;    process/p (text)
;    title/t (text)
;    class/c (text)
;    focus/f (0 or 1)
;    maximized/m (0 or 1)
;    left (number)
;    top (number)
;    right (number)
;    bottom (number)
;    monitor/mon (number >= 0)
;
; Separators: | (acts as 'or'), & (acts as 'and')
;
; Ignore UWP container window and windows with empty titles
; ApplicationFrameHost.exe (UWP containers) is used by mostly by Windows applications
; Some of the processes seems to have maximized windows, even though they are not visible
; We don't have a way to distinguish between that invisible window,
; so the taskbar is going to be still invisible when opening something like Settings
; Tag 'process' (or 'p') is case-insensitive
IgnoredWindows = title:|process:ApplicationFrameHost.exe
ExceptionalWindows =
```

## Issues
- Settings and other similar apps that uses ApplicationFrameHost.exe don't get detected (it's buggy)
- Not every context menu popup from taskbar is supported (taskbar can still disappear)

## TODO
- [ ] Find a way to fix issue with ApplicationFrameHost.exe
- [ ] Show taskbar when any context menu from taskbar is opened (kinda works already but not for all popups)
- [ ] Maybe use window events listener instead of a loop
- [ ] Recheck how main thread interacts with other threads (variable safety-wise)

## Configuration examples
Tags short versions can be used but just for simplicity I will write them fully.
### Making taskbar visible when explorer.exe is opened even when it's not maximized
```ini
AlwaysIgnoreWhenNotMaximized = 0
IgnoredWindows = maximized:0|process:ApplicationFrameHost.exe
ExceptionalWindows = process:explorer.exe&class=
```

## Building
This project was built using CMake (^3.10), MinGW (^11.0 w64), Ninja and CLion IDE.
You can build the project without any studios with the following commands:
### Configure the release build (Ninja)
```bash
cmake.exe -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=ninja.exe -G Ninja -S <source> -B <source>/cmake-build-release
```
### Build the executable
```bash
cmake.exe --build <source>\cmake-build-release --target WindowsTaskbarHider -j 6
```
`<source>` is a path (absolute or relative) to the source directory. If you don't have added cmake.exe and/or ninja.exe,
you can provide an absolute path to the mentioned executables, but don't forget to quote them like this
```bash
"C:\path\to\cmake.exe" ... -DCMAKE_MAKE_PROGRAM="C:\path\to\ninja.exe" -G ...
```
### Updating language keys (language.h.in and resources/language/language.\*.ini files)
Make sure to clean CMake project before building, otherwise CMake won't notice those changes
```bash
cmake.exe --build <source>\cmake-build-debug --target clean -j 6
```

### CMake used flags (from CMakeLists.txt)
| Build Type | Category            | Flags                                                                            |
|------------|---------------------|----------------------------------------------------------------------------------|
| Release    | `C++ compiler`      | -static -static-libgcc -static-libstdc++ -Os -fdata-sections -ffunction-sections |
| Release    | `Executable linker` | -Wl,--gc-sections -s -static -lpthread                                           |
| Debug      | `C++ compiler`      | -gdwarf-3                                                                        |

## Credits and appreciation!
* Thanks to [SuperNeon4ik](https://github.com/SuperNeon4ik) for Ukrainian translations!
* Program icon credits goes to [justicon (freepik.com)](https://www.freepik.com/icon/programming_1567754)