#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>

class config {
public:
    static bool darkMode;
    static bool livePreview;
    static bool showAllWindows;
    static bool openOnStart;
    static bool closeToTray;
    static bool closeConfirmMessage;
    static bool alwaysIgnoreWhenNotMaximized;
    static int taskbarUpdateInterval;
    static int opacity;
    static std::vector<std::wstring> ignoredWindows;
    static std::vector<std::wstring> exceptionalWindows;

    static void load();
    static void save();
    static void open();
    static bool processSingle(const std::wstring &key, const std::wstring &value);

private:
    static void ensureConfigurationExists();
};

#endif //CONFIG_H
