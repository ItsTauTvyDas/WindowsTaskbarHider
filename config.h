#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>

class config {
public:
    static bool debug;
    static int taskbarUpdateInterval;
    static int opacity;
    static int systemTrayIconIndex;
    static std::string systemTrayIconSource;
    static std::vector<std::string> ignoredWindows;

    static void load();
    static void save();
    static void open();
    static bool processSingle(const std::string &key, const std::string &value);

private:
    static void ensureConfigurationExists();
};

#endif //CONFIG_H
