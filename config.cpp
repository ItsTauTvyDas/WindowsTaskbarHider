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
bool config::keepConsoleWindowOpen = false;
bool config::alwaysIgnoreWhenNotMaximized = true;
int config::taskbarUpdateInterval = 10;
int config::opacity = 0;
std::vector<std::string> config::ignoredWindows = {"title:", "process:ApplicationFrameHost.exe"};
std::vector<std::string> config::exceptionalWindows = {};

void config::save() {
    std::ofstream file(CONFIG_FILENAME, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        MessageBoxA(globals::hWnd, "Failed to load configuration.", PROJECT_NAME, MB_ICONERROR | MB_OK);
        return;
    }
    file << "[General]" << std::endl;
    file << "; If enabled (1), application will open a console window" << std::endl;
    file << "DebugEnabled = " << debug << std::endl;
    file << "; Keep console window open if user sends CTRL+C action to console window" << std::endl;
    file << "KeepConsoleWindowOpen = " << keepConsoleWindowOpen << std::endl;
    file << "[Taskbar]" << std::endl;
    file << "; Taskbar update loop interval in milliseconds" << std::endl;
    file << "UpdateInterval = " << taskbarUpdateInterval << std::endl;
    file << "; Opacity from 0 to 255" << std::endl;
    file << "Opacity = " << opacity << std::endl;
    file << "[Ignored Windows]" << std::endl;
    file << "; Setting this to false (0) could slow down the application with debug mode on" << std::endl;
    file << "AlwaysIgnoreWhenNotMaximized = " << alwaysIgnoreWhenNotMaximized << std::endl;
    file << "; Available tags: process/p (text), title/t (text), class/c (text), focus/f (0 or 1), maximized/m (0 or 1), left (int), top (int), right (int), bottom (int)" << std::endl;
    file << "; Separator: |" << std::endl;
    file << ";" << std::endl;
    file << "; Ignore UWP container window and windows with empty titles" << std::endl;
    file << "; ApplicationFrameHost.exe (UWP containers) is used by mostly by Windows applications" << std::endl;
    file << "; Some of the processes seems to have maximized windows, even though they are not visible" << std::endl;
    file << "; We don't have a way to distinguish between that invisible window," << std::endl;
    file << "; so the taskbar is going to be still invisible when opening something like Settings" << std::endl;
    file << "IgnoredWindows = " << utils::joinString(ignoredWindows, "|") << std::endl;
    file << "ExceptionalWindows = " << std::endl;
    file.flush();
    file.close();
}

void config::ensureConfigurationExists() {
    if (!utils::fileExists(CONFIG_FILENAME))
        save();
}

void config::open() {
    ShellExecuteA(nullptr, "open", CONFIG_FILENAME, nullptr, nullptr, SW_SHOWNORMAL);
}

void invalidIntegerValue(const std::string &key, int &value, const int min, const int max) {
    if (value > max) {
        value = max;
        MessageBoxA(globals::hWnd, ("Config value (" + key + ") number was above the limit, the value was reset to " + std::to_string(max) + ".").c_str(), PROJECT_NAME, MB_ICONWARNING | MB_OK);
    } else if (value < min) {
        value = min;
        MessageBoxA(globals::hWnd, ("Config value (" + key + ") number was below the limit, the value was reset to " + std::to_string(min) + ".").c_str(), PROJECT_NAME, MB_ICONWARNING | MB_OK);
    }
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
            invalidIntegerValue(formattedKey, taskbarUpdateInterval, 1, 1000);
        } else if (key == "General.DebugEnabled") {
            debug = value != "0";
        } else if (key == "General.KeepConsoleWindowOpen") {
            keepConsoleWindowOpen = value != "0";
        } else if (key == "Taskbar.Opacity") {
            opacity = std::stoi(value);
            invalidIntegerValue(formattedKey, opacity, 0, 255);
        } else if (key == "Ignored_Windows.IgnoredWindows") {
            ignoredWindows = utils::splitString(value, '|');
        } else if (key == "Ignored_Windows.ExceptionalWindows") {
            exceptionalWindows = utils::splitString(value, '|');
        } else if (key == "Ignored_Windows.AlwaysIgnoreWhenNotMaximized") {
            alwaysIgnoreWhenNotMaximized = value != "0";
        } else {
            MessageBoxA(globals::hWnd, ("Invalid configuration key: " + key).c_str(), PROJECT_NAME, MB_ICONWARNING | MB_OK);
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

        std::string key = line.substr(0, pos);
        utils::trim(key);
        std::string value = line.substr(pos + 1);
        utils::trim(value);

        processSingle(prefix + key, value);
    }
    file.close();
}