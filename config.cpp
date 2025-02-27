#include "config.h"

#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>
#include "globals.h"
#include "language.h"
#include "taskbar.h"
#include "utils.h"

#define CONFIG_FILENAME L"config.ini"

bool config::alwaysIgnoreWhenNotMaximized = true;
bool config::darkMode;
bool config::livePreview = true;
bool config::openOnStart = false;
bool config::closeToTray = false;
bool config::closeConfirmMessage = false;

int config::taskbarUpdateInterval = 10;
int config::opacity = 0;

std::vector<std::wstring> config::ignoredWindows = {L"title:", L"process:ApplicationFrameHost.exe"};
std::vector<std::wstring> config::exceptionalWindows = {};

void config::save() {
    std::wofstream file(CONFIG_FILENAME, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        utils::messageBox(MSG_CONFIG_LOAD_FAILED, MB_ICONERROR | MB_OK);
        return;
    }
    file << "[Window]" << std::endl;
    file << "; Default values for checkboxes in the window display" << std::endl;
    file << "DarkMode = " << darkMode << std::endl;
    file << "LivePreview = " << livePreview << std::endl;
    file << "[Window Behaviour]" << std::endl;
    file << "OpenOnStart = " << openOnStart << std::endl;
    file << "CloseToTray = " << openOnStart << std::endl;
    file << "; Only works if CloseToTray is disabled" << std::endl;
    file << "CloseConfirmMessage = " << openOnStart << std::endl;
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
    file << "IgnoredWindows = " << utils::joinString(ignoredWindows, L"|") << std::endl;
    file << "ExceptionalWindows = " << std::endl;
    file.flush();
    file.close();
}

void config::ensureConfigurationExists() {
    if (!utils::fileExists(CONFIG_FILENAME))
        save();
}

void config::open() {
    ShellExecute(nullptr, L"open", CONFIG_FILENAME, nullptr, nullptr, SW_SHOWNORMAL);
}

void invalidIntegerValue(const std::wstring &key, int &value, const int min, const int max) {
    if (value > max) {
        value = max;
        utils::messageBox(MSG_CONFIG_ABOVE_MAX, MB_ICONWARNING | MB_OK, {key, std::to_wstring(max)});
    } else if (value < min) {
        value = min;
        utils::messageBox(MSG_CONFIG_BELOW_MIN, MB_ICONWARNING | MB_OK, {key, std::to_wstring(min)});
    }
}

bool config::processSingle(const std::wstring &key, const std::wstring &value) {
    std::wstring formattedKey;
    try {
        if (key.empty()) {

        }

        const size_t pos = key.find('.');
        std::wstring category = key.substr(0, pos);
        std::ranges::replace(category, '_', ' ');
        const std::wstring readableKey = key.substr(pos + 1);
        formattedKey = category + L" > " + readableKey;

        if (key == L"Taskbar.UpdateInterval") {
            taskbarUpdateInterval = std::stoi(value);
            invalidIntegerValue(formattedKey, taskbarUpdateInterval, 1, 1000);
        } else if (key == L"Window.DarkMode") {
            darkMode = value != L"0";
        } else if (key == L"Window.LivePreview") {
            livePreview = value != L"0";
        } else if (key == L"Window_Behaviour.OpenOnStart") {
            openOnStart = value != L"0";
        } else if (key == L"Window_Behaviour.CloseToTray") {
            closeToTray = value != L"0";
        } else if (key == L"Window_Behaviour.CloseConfirmMessage") {
            closeConfirmMessage = value != L"0";
        } else if (key == L"Taskbar.Opacity") {
            opacity = std::stoi(value);
            invalidIntegerValue(formattedKey, opacity, 0, 255);
        } else if (key == L"Ignored_Windows.IgnoredWindows") {
            ignoredWindows = utils::splitString(value, '|');
        } else if (key == L"Ignored_Windows.ExceptionalWindows") {
            exceptionalWindows = utils::splitString(value, '|');
        } else if (key == L"Ignored_Windows.AlwaysIgnoreWhenNotMaximized") {
            alwaysIgnoreWhenNotMaximized = value != L"0";
        } else {
            utils::messageBox(MSG_CONFIG_INVALID_KEY, MB_ICONWARNING | MB_OK, {key});
            return false;
        }
    } catch (const std::exception& e) {
        // Rethrow with formatted text
        std::wstring msg;
        if (e.what() == "stoi")
            msg = utils::message(MSG_CONFIG_NOT_NUMBER);
        else {
            wchar_t cMsg[256];
            utils::toUnicode(e.what(), cMsg);
            msg = cMsg;
        }
        utils::messageBox(formattedKey + L": " + msg, MB_ICONWARNING | MB_OK);
    }
    return true;
}

void config::load() {
    ensureConfigurationExists();

    std::wifstream file(CONFIG_FILENAME);
    std::wstring line;
    std::wstring prefix;

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

        std::wstring key = line.substr(0, pos);
        utils::trim(key);
        std::wstring value = line.substr(pos + 1);
        utils::trim(value);

        processSingle(prefix + key, value);
    }
    file.close();
}