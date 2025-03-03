#include "config.h"

#include <fstream>
#include <string>
#include <algorithm>
#include <ranges>
#include "globals.h"
#include "language.h"
#include "resources.h"
#include "taskbar.h"
#include "utils.h"

#define CONFIG_FILENAME L"config.ini"

bool config::alwaysIgnoreWhenNotMaximized = true;
bool config::darkMode;
bool config::livePreview;
bool config::showAllWindows;
bool config::openOnStart;
bool config::closeToTray;
bool config::closeConfirmMessage;
bool config::languageLoaded;

int config::taskbarUpdateInterval = 10;
int config::opacityWhenHidden;
int config::opacityWhenShown = 90;
int config::opacityWhenHovered = 100;
int config::languageCode = IDR_INI_LANG_EN;

std::vector<std::wstring> config::ignoredWindows = {L"title:", L"process:ApplicationFrameHost.exe"};
std::vector<std::wstring> config::exceptionalWindows = {};

void config::save() {
    std::wofstream file(CONFIG_FILENAME, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        utils::messageBox(MSG_CONFIG_LOAD_FAILED, MB_ICONERROR | MB_OK);
        return;
    }
    file << "[General]" << std::endl;
    file << "Language = en" << std::endl;
    file << "[Window]" << std::endl;
    file << "; Default values for checkboxes in the window display" << std::endl;
    file << "DarkMode = " << darkMode << std::endl;
    file << "LivePreview = " << livePreview << std::endl;
    file << "ShowAllWindows = " << showAllWindows << std::endl;
    file << "[Window Behaviour]" << std::endl;
    file << "OpenOnStart = " << openOnStart << std::endl;
    file << "CloseToTray = " << openOnStart << std::endl;
    file << "; Only works if CloseToTray is disabled" << std::endl;
    file << "CloseConfirmMessage = " << openOnStart << std::endl;
    file << "[Taskbar]" << std::endl;
    file << "; Taskbar update loop interval in milliseconds" << std::endl;
    file << "UpdateInterval = " << taskbarUpdateInterval << std::endl;
    file << "; Opacity level from 0 to 100" << std::endl;
    file << "OpacityWhenHidden = " << opacityWhenHidden << std::endl;
    file << "; Bellow limit changes from 1 to 100, 0 causes the taskbar to lose interactivity" << std::endl;
    file << "OpacityWhenShown = " << opacityWhenShown << std::endl;
    file << "OpacityWhenHoveredOver = " << opacityWhenHovered << std::endl;
    file << "[Ignored Windows]" << std::endl;
    file << "; Setting this to false (0) could slow down the application with debug mode on" << std::endl;
    file << "AlwaysIgnoreWhenNotMaximized = " << alwaysIgnoreWhenNotMaximized << std::endl;
    file << "; Available tags: process/p (text), title/t (text), class/c (text), focus/f (0 or 1), maximized/m (0 or 1), left (number), top (int), right (number), bottom (number), monitor/mon (number >= 0)" << std::endl;
    file << "; Separator: |" << std::endl;
    file << ";" << std::endl;
    file << "; Ignore UWP container window and windows with empty titles" << std::endl;
    file << "; ApplicationFrameHost.exe (UWP containers) is used by mostly by Windows applications" << std::endl;
    file << "; Some of the processes seems to have maximized windows, even though they are not visible" << std::endl;
    file << "; We don't have a way to distinguish between that invisible window," << std::endl;
    file << "; so the taskbar is going to be still invisible when opening something like Settings" << std::endl;
    file << "IgnoredWindows = " << utils::joinString(ignoredWindows, L"|") << std::endl;
    file << "ExceptionalWindows = " << utils::joinString(exceptionalWindows, L"|") << std::endl;
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

void checkForInvalidIntegerValue(const std::wstring &key, auto &value, const int min, const int max, bool &noErrors) {
    if (value > max) {
        value = max;
        utils::messageBox(MSG_CONFIG_ABOVE_MAX, MB_ICONWARNING | MB_OK, {key, std::to_wstring(max)});
        noErrors = false;
    } else if (value < min) {
        value = min;
        utils::messageBox(MSG_CONFIG_BELOW_MIN, MB_ICONWARNING | MB_OK, {key, std::to_wstring(min)});
        noErrors = false;
    }
}

bool checkForEmptyValueI(const std::wstring &key, const std::wstring &value, int &obj, const int defaultValue, bool &noErrors) {
    if (value.empty()) {
        obj = defaultValue;
        utils::messageBox(MSG_CONFIG_NO_VALUE, MB_ICONWARNING | MB_OK, {key, std::to_wstring(defaultValue)});
        noErrors = false;
        return false;
    }
    obj = std::stoi(value);
    return true;
}

void checkBoolValidation(const std::wstring &key, const std::wstring &value, bool &obj, const bool defaultValue, bool &noErrors) {
    if (value.empty()) {
        obj = defaultValue;
        utils::messageBox(MSG_CONFIG_NO_VALUE, MB_ICONWARNING | MB_OK, {key, std::to_wstring(defaultValue)});
        noErrors = false;
        return;
    }
    int bValue = std::stoi(value);
    checkForInvalidIntegerValue(key, bValue, 0, 1, noErrors);
    obj = bValue;
}

bool config::processSingle(const std::wstring &key, const std::wstring &value) {
    bool noErrors = true;
    std::wstring formattedKey;
    try {
        const size_t pos = key.find('.');
        std::wstring category = key.substr(0, pos);
        std::ranges::replace(category, '_', ' ');
        const std::wstring readableKey = key.substr(pos + 1);
        formattedKey = category + L" > " + readableKey;

        if (key == L"Taskbar.UpdateInterval") {
            if (checkForEmptyValueI(formattedKey, value, taskbarUpdateInterval, taskbarUpdateInterval, noErrors))
                checkForInvalidIntegerValue(formattedKey, taskbarUpdateInterval, 1, 1000, noErrors);
        } else if (key == L"General.Language") {
            languageLoaded = false;
            auto languages = std::unordered_map<std::wstring, int>(APP_DEFAULT_LANGUAGES);
            if (!languages.contains(value)) {
                auto keysView = std::views::keys(languages);
                const std::vector languagesVector(keysView.begin(), keysView.end());
                utils::messageBox(MSG_CONFIG_INVALID_LANGUAGE, MB_ICONWARNING | MB_OK, {utils::joinString(languagesVector, L", ")});
                languageLoaded = true;
                return false;
            }
            languageCode = languages[value];
            utils::logcLangString(0, nullptr); // Trigger language cache to update
            languageLoaded = true;
        } else if (key == L"Window.DarkMode") {
            checkBoolValidation(formattedKey, value, darkMode, darkMode, noErrors);
        } else if (key == L"Window.LivePreview") {
            checkBoolValidation(formattedKey, value, livePreview, livePreview, noErrors);
        } else if (key == L"Window.ShowAllWindows") {
            checkBoolValidation(formattedKey, value, showAllWindows, showAllWindows, noErrors);
        } else if (key == L"Window_Behaviour.OpenOnStart") {
            checkBoolValidation(formattedKey, value, openOnStart, openOnStart, noErrors);
        } else if (key == L"Window_Behaviour.CloseToTray") {
            checkBoolValidation(formattedKey, value, closeToTray, closeToTray, noErrors);
        } else if (key == L"Window_Behaviour.CloseConfirmMessage") {
            checkBoolValidation(formattedKey, value, closeConfirmMessage, closeConfirmMessage, noErrors);
        } else if (key == L"Taskbar.OpacityWhenHidden") {
            if (checkForEmptyValueI(formattedKey, value, opacityWhenHidden, opacityWhenHidden, noErrors))
                checkForInvalidIntegerValue(formattedKey, opacityWhenHidden, 0, 100, noErrors);
            opacityWhenHidden = opacityWhenHidden > 0 ? 255 * opacityWhenHidden / 100 : 0;
        } else if (key == L"Taskbar.OpacityWhenShown") {
            if (checkForEmptyValueI(formattedKey, value, opacityWhenShown, opacityWhenShown, noErrors))
                checkForInvalidIntegerValue(formattedKey, opacityWhenShown, 1, 100, noErrors);
            opacityWhenShown = 255 * opacityWhenShown / 100;
        } else if (key == L"Taskbar.OpacityWhenHoveredOver") {
            if (checkForEmptyValueI(formattedKey, value, opacityWhenHovered, opacityWhenHovered, noErrors))
                checkForInvalidIntegerValue(formattedKey, opacityWhenHovered, 1, 100, noErrors);
            opacityWhenHovered = 255 * opacityWhenHovered / 100;
        } else if (key == L"Ignored_Windows.IgnoredWindows") {
            ignoredWindows = utils::splitString(value, '|');
        } else if (key == L"Ignored_Windows.ExceptionalWindows") {
            exceptionalWindows = utils::splitString(value, '|');
        } else if (key == L"Ignored_Windows.AlwaysIgnoreWhenNotMaximized") {
            checkBoolValidation(formattedKey, value, alwaysIgnoreWhenNotMaximized, alwaysIgnoreWhenNotMaximized, noErrors);
        } else {
            utils::messageBox(MSG_CONFIG_INVALID_KEY, MB_ICONWARNING | MB_OK, {key});
            return false;
        }
    } catch (const std::exception& e) {
        wchar_t cMsg[256];
        utils::toUnicode(e.what(), cMsg);
        std::wstring msg = cMsg;
        if (msg == L"stoi")
            msg = utils::message(MSG_CONFIG_NOT_NUMBER);
        utils::messageBox(formattedKey + L": " + msg, MB_ICONWARNING | MB_OK);
        return false;
    }
    return noErrors;
}

bool config::load() {
    ensureConfigurationExists();

    std::wifstream file(CONFIG_FILENAME);
    std::wstring line;
    std::wstring prefix;
    bool noErrors = true;

    int lineNum = 0;
    while (getline(file, line)) {
        lineNum++;
        std::wstring key, value;
        if (!utils::processIniFileLine(line, &prefix, key, value))
            continue;

        if (key.empty()) {
            utils::messageBox(MSG_CONFIG_INVALID_SYNTAX, MB_ICONERROR | MB_OK, { std::to_wstring(lineNum), line });
            noErrors = false;
            continue;
        }

        if (!processSingle(prefix + key, value))
            noErrors = false;
    }
    file.close();
    return noErrors;
}