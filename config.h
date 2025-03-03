#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>

class config {
public:
    static bool darkMode;
    static bool autoUpdate;
    static bool showAllWindows;
    static bool openOnStart;
    static bool closeToTray;
    static bool minimizeToTray;
    static bool closeConfirmMessage;
    static bool alwaysIgnoreWhenNotMaximized;
    static bool disableAutoUpdateWhenUnfocused;
    static bool languageLoaded;
    static int taskbarUpdateInterval;
    static int opacityWhenHidden;
    static int opacityWhenShown;
    static int opacityWhenHovered;
    static int opacityWhenHiddenInternal;
    static int opacityWhenShownInternal;
    static int opacityWhenHoveredInternal;
    static int languageCode;
    static std::wstring I_TaskbarWindowClassNameStarts;
    static std::wstring I_TaskbarWindowClassNameEnds;
    static std::vector<std::wstring> ignoredWindows;
    static std::vector<std::wstring> exceptionalWindows;

    static bool load();
    static void save(bool exposeInternalKeys);
    static void open();
    static bool processSingle(const std::wstring &key, const std::wstring &value);

private:
    static void ensureConfigurationExists();
};

#endif //CONFIG_H
