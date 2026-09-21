#ifndef CONFIG_H
#define CONFIG_H

#include <atomic>
#include <string>
#include <vector>

#define CONFIG_KEY(cat, key)                          cat L"." key
#define CONFIG_SECTION(cat)                           L"[" cat L"]"

#define CONFIG_CAT_GENERAL                            L"General"
#define CONFIG_KEY_LANGUAGE                           L"Language"

#define CONFIG_CAT_WINDOW                             L"Window"
#define CONFIG_KEY_DARK_MODE                          L"DarkMode"
#define CONFIG_KEY_AUTO_UPDATE                        L"AutoUpdate"
#define CONFIG_KEY_SHOW_ALL_WINDOWS                   L"ShowAllWindows"

#define CONFIG_CAT_WINDOW_BEHAVIOUR                   L"Window Behaviour"
#define CONFIG_KEY_DISABLE_AUTO_UPDATE_WHEN_UNFOCUSED L"DisableAutoUpdateWhenUnfocused"
#define CONFIG_KEY_AUTO_UPDATE_ON_OPEN                L"AutoUpdateOnOpen"
#define CONFIG_KEY_OPEN_ON_START                      L"OpenOnStart"
#define CONFIG_KEY_CLOSE_TO_TRAY                      L"CloseToTray"
#define CONFIG_KEY_CLOSE_CONFIRM_MESSAGE              L"CloseConfirmMessage"
#define CONFIG_KEY_MINIMIZE_TO_TRAY                   L"MinimizeToTray"

#define CONFIG_CAT_TASKBAR                            L"Taskbar"
#define CONFIG_KEY_UPDATE_INTERVAL                    L"UpdateInterval"
#define CONFIG_KEY_USE_REAL_OPACITY_VALUES            L"UseRealOpacityValues"
#define CONFIG_KEY_OPACITY_WHEN_HIDDEN                L"OpacityWhenHidden"
#define CONFIG_KEY_OPACITY_WHEN_SHOWN                 L"OpacityWhenShown"
#define CONFIG_KEY_OPACITY_WHEN_HOVERED_OVER          L"OpacityWhenHoveredOver"

#define CONFIG_CAT_IGNORED_WINDOWS                    L"Ignored Windows"
#define CONFIG_KEY_IGNORED_WINDOWS                    L"IgnoredWindows"
#define CONFIG_KEY_EXCEPTIONAL_WINDOWS                L"ExceptionalWindows"
#define CONFIG_KEY_ALWAYS_IGNORE_WHEN_NOT_MAXIMIZED   L"AlwaysIgnoreWhenNotMaximized"

#define CONFIG_CAT_TASKBAR_HOVER_ANIMATION            L"Taskbar Hover Animation"
#define CONFIG_KEY_ANIMATION_ENABLED                  L"Enabled"
#define CONFIG_KEY_ANIMATION_STEP_DELAY               L"AnimationStepDelay"
#define CONFIG_KEY_ANIMATION_OPACITY_STEP             L"AnimationOpacityStep"

#define CONFIG_CAT_INTERNAL                           L"Internal"
#define CONFIG_KEY_TASKBAR_WINDOW_CLASS_NAME_STARTS   L"___TaskbarWindowClassNameStarts"
#define CONFIG_KEY_TASKBAR_WINDOW_CLASS_NAME_ENDS     L"___TaskbarWindowClassNameEnds"
#define CONFIG_KEY_TASKBAR_EXCEPTIONAL_WINDOWS        L"___TaskbarExceptionalWindows"

#define CONFIG_CAT_WINDOWS_10_FIXES                   L"Windows 10 Fixes"
#define CONFIG_KEY_FIX_TASKBAR_HOVER_GLITCH           L"FixTaskbarHoverGlitch"
#define CONFIG_KEY_EXCEPT_TASKBAR_POPUPS              L"ExceptTaskbarPopups"

class config {
public:
    static std::atomic<bool> darkMode;
    static std::atomic<bool> autoUpdate;
    static std::atomic<bool> showAllWindows;
    static std::atomic<bool> openOnStart;
    static std::atomic<bool> closeToTray;
    static std::atomic<bool> minimizeToTray;
    static std::atomic<bool> closeConfirmMessage;
    static std::atomic<bool> alwaysIgnoreWhenNotMaximized;
    static std::atomic<bool> disableAutoUpdateWhenUnfocused;
    static std::atomic<bool> useRealOpacityValues;
    static std::atomic<bool> animationsEnabled;
    static std::atomic<bool> autoUpdateOnOpen;
    static std::atomic<bool> fixTaskbarHoverGlitch;
    static std::atomic<bool> exceptTaskbarPopups;

    static std::atomic<int> taskbarUpdateInterval;
    static std::atomic<int> opacityWhenHidden;
    static std::atomic<int> opacityWhenShown;
    static std::atomic<int> opacityWhenHovered;
    static std::atomic<int> opacityWhenHiddenInternal;
    static std::atomic<int> opacityWhenShownInternal;
    static std::atomic<int> opacityWhenHoveredInternal;
    static std::atomic<int> languageCode;
    static std::atomic<int> animationStepDelay;
    static std::atomic<int> animationOpacityStep;

    static std::vector<std::wstring> ignoredWindows;
    static std::vector<std::wstring> exceptionalWindows;
    static std::wstring languageShortName;

    static void init();
    static bool load();
    static bool save(bool exposeInternalKeys);
    static bool exists();
    static bool directoryExists();
    static void open();
    static void openDirectory();
    static bool processSingle(const std::wstring &key, const std::wstring &value);

    // Internal
    static std::wstring I_TaskbarWindowClassNameStarts;
    static std::wstring I_TaskbarWindowClassNameEnds;
    static std::vector<std::wstring> I_ExceptionalWindows;
private:
    static bool ensureConfigurationExists();
};

#endif //CONFIG_H
