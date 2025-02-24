#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>

class config {
public:
    static bool debug;
    static bool keepConsoleWindowOpen;
    static bool alwaysIgnoreWhenNotMaximized;
    static int taskbarUpdateInterval;
    static int opacity;
    static std::vector<std::string> ignoredWindows;
    static std::vector<std::string> exceptionalWindows;

    static void load();
    static void save();
    static void open();
    static bool processSingle(const std::string &key, const std::string &value);

private:
    static void ensureConfigurationExists();
};

#endif //CONFIG_H
