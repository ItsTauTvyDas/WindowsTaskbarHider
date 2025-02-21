#include "config.h"

#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>
#include "globals.h"
#include "resources.h"
#include "taskbar.h"
#include "utils.h"

#define CONFIG_FILENAME "config.ini"

bool config::debug = false;
int config::taskbarUpdateInterval = 10;
int config::opacity = 0;
std::vector<std::string> config::ignoredWindows = {"title:", "process:ApplicationFrameHost.exe"};

void config::save() {
    std::ofstream file(CONFIG_FILENAME, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        MessageBoxA(globals::hWnd, "Failed to load configuration", PROJECT_NAME, MB_ICONERROR | MB_OK);
        return;
    }
    file << "[General]" << std::endl;
    file << "Debug=" << debug << std::endl;
    file << "[Taskbar]" << std::endl;
    file << "; Taskbar update loop interval in milliseconds" << std::endl;
    file << "UpdateInterval=" << taskbarUpdateInterval << std::endl;
    file << "Opacity=" << opacity << std::endl;
    file << "; Available tags: process, title, class" << std::endl;
    file << "; Ignore UWP container window and windows with empty titles" << std::endl;
    file << "; ApplicationFrameHost.exe (UWP containers) is used by mostly by Windows applications" << std::endl;
    file << "; Some of the processes seems to have maximized windows, even though they are not visible" << std::endl;
    file << "; We don't have a way to distinguish between that invisible window," << std::endl;
    file << "; so the taskbar is going to be still invisible when opening something like Settings" << std::endl;
    file << "IgnoreMaximizedWindows=" << utils::joinString(ignoredWindows, "|") << std::endl;
    file.flush();
    file.close();
}

void config::ensureConfigurationExists() {
    if (!utils::fileExists(CONFIG_FILENAME))
        save();
}

void config::open() {
    system(("explorer " + std::string(CONFIG_FILENAME)).c_str());
}

bool config::processSingle(const std::string &key, const std::string &value) {
    if (key.empty())
        throw std::invalid_argument("Empty key specified");

    const size_t pos = key.find('.');
    std::string category = key.substr(0, pos);
    std::ranges::replace(category, '_', ' ');
    const std::string readableKey = key.substr(pos + 1);
    const std::string formattedKey = category + " > " + readableKey;

    try {
        if (key == "Taskbar.UpdateInterval") {
            taskbarUpdateInterval = std::stoi(value);
        } else if (key == "General.Debug") {
            debug = value != "0";
        } else if (key == "Taskbar.IgnoreMaximizedWindows") {
            ignoredWindows = utils::splitString(value, '|');
        } else {
            return false;
        }
    } catch (const std::exception& e) {
        // Rethrow with formatted text
        auto msg = e.what();
        if (std::string(e.what()) == "stoi")
            msg = "Value is not a number";
        throw std::invalid_argument(formattedKey + ": " + msg);
    }
    return true;
}

void config::load() {
    ensureConfigurationExists();

    std::ifstream file(CONFIG_FILENAME);
    std::string line;
    std::string prefix;

    while (getline(file, line)) {
        if (line.rfind('[', 0) == 0) {
            prefix = line.substr(1, line.size() - 2);
            std::ranges::replace(prefix, ' ', '_');
            prefix += '.';
            continue;
        }

        if (line.rfind(';', 0) == 0)
            continue;

        const size_t pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        const std::string key = prefix + line.substr(0, pos);
        const std::string value = line.substr(pos + 1);

        processSingle(key, value);
    }
    file.close();
}