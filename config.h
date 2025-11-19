#ifndef CONFIG_H
#define CONFIG_H

#include <atomic>
#include <string>
#include <vector>

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
    static void open();
    static bool processSingle(const std::wstring &key, const std::wstring &value);

    // Internal
    static std::wstring I_TaskbarWindowClassNameStarts;
    static std::wstring I_TaskbarWindowClassNameEnds;
    static std::vector<std::wstring> I_ExceptionalWindows;
private:
    static bool ensureConfigurationExists();
};

#endif //CONFIG_H
