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
bool config::autoUpdate;
bool config::showAllWindows;
bool config::openOnStart;
bool config::closeToTray;
bool config::minimizeToTray = true;
bool config::closeConfirmMessage = true;
bool config::disableAutoUpdateWhenUnfocused = true;

int config::taskbarUpdateInterval = 10;
int config::opacityWhenHidden;
int config::opacityWhenShown = 90;
int config::opacityWhenHovered = 100;
int config::opacityWhenHiddenInternal;
int config::opacityWhenShownInternal;
int config::opacityWhenHoveredInternal;
int config::languageCode = IDR_INI_LANG_EN;
std::wstring config::languageShortName = L"en";

bool config::animationsEnabled = true;
int config::animationStepDelay = 3;
int config::animationOpacityStep = 10;

std::wstring config::I_TaskbarWindowClassNameStarts = L"Shell_";
std::wstring config::I_TaskbarWindowClassNameEnds = L"TrayWnd";
std::vector<std::wstring> config::I_ExceptionalWindows = {
    L"explorer.exe=TaskListThumbnailWnd",                  // Preview of windows when hovered over a taskbar app icon
    L"explorer.exe=TaskListOverlayWnd",                    // Same as above
    L"explorer.exe=NotifyIconOverflowWindow",              // More tray icons arrow window
    L"explorer.exe=CiceroUIWndFrame",                      // Language chooser window
    L"shellexperiencehost.exe=Windows.UI.Core.CoreWindow", // Wireless/Ethernet, sound, time windows
};

std::vector<std::wstring> config::ignoredWindows = {L"title:", L"process:ApplicationFrameHost.exe"};
std::vector<std::wstring> config::exceptionalWindows = {};

bool config::save(const bool exposeInternalKeys) {
    std::wofstream file(CONFIG_FILENAME, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        utils::messageBox(MSG_CONFIG_LOAD_FAILED, MB_ICONERROR | MB_OK);
        return false;
    }
    file << "; Github: " << PRODUCT_URL << std::endl;
    file << std::endl;
    file << ";  _    _ _           _                 _____         _    _                _   _ _     _" << std::endl;
    file << "; | |  | (_)         | |               |_   _|       | |  | |              | | | (_)   | |" << std::endl;
    file << "; | |  | |_ _ __   __| | _____      _____| | __ _ ___| | _| |__   __ _ _ __| |_| |_  __| | ___ _ __" << std::endl;
    file << "; | |/\\| | | '_ \\ / _` |/ _ \\ \\ /\\ / / __| |/ _` / __| |/ / '_ \\ / _` | '__|  _  | |/ _` |/ _ \\ '__|" << std::endl;
    file << "; \\  /\\  / | | | | (_| | (_) \\ V  V /\\__ \\ | (_| \\__ \\   <| |_) | (_| | |  | | | | | (_| |  __/ |" << std::endl;
    file << ";  \\/  \\/|_|_| |_|\\__,_|\\___/ \\_/\\_/ |___|_/\\__,_|___/_|\\_\\_.__/ \\__,_|_|  \\_| |_/_|\\__,_|\\___|_|" << std::endl;
    file << std::endl;
    file << "[General]" << std::endl;
    file << "Language = " << languageShortName << std::endl;
    file << std::endl;
    file << "[Window]" << std::endl;
    file << "; Default values for checkboxes in the window display" << std::endl;
    file << "DarkMode = " << darkMode << std::endl;
    file << "AutoUpdate = " << autoUpdate << std::endl;
    file << "ShowAllWindows = " << showAllWindows << std::endl;
    file << std::endl;
    file << "[Window Behaviour]" << std::endl;
    file << "OpenOnStart = " << openOnStart << std::endl;
    file << "CloseToTray = " << closeToTray << std::endl;
    file << "MinimizeToTray = " << minimizeToTray << std::endl;
    file << "; Only works if CloseToTray is disabled" << std::endl;
    file << "CloseConfirmMessage = " << closeConfirmMessage << std::endl;
    file << std::endl;
    file << "[Taskbar]" << std::endl;
    file << "; Taskbar update loop interval in milliseconds" << std::endl;
    file << "UpdateInterval = " << taskbarUpdateInterval << std::endl;
    file << "; Disable automatic updates to the GUI when program gets unfocused" << std::endl;
    file << "DisableAutoUpdateWhenUnfocused = " << disableAutoUpdateWhenUnfocused << std::endl;
    file << "; Opacity level from 0 to 100" << std::endl;
    file << "OpacityWhenHidden = " << opacityWhenHidden << std::endl;
    file << "; Bellow limit changes from 1 to 100, 0 causes the taskbar to lose interactivity" << std::endl;
    file << "OpacityWhenShown = " << opacityWhenShown << std::endl;
    file << "OpacityWhenHoveredOver = " << opacityWhenHovered << std::endl;
    file << std::endl;
    file << "[Taskbar Hover Animation]" << std::endl;
    file << "; Animation between OpacityWhenShown/OpacityWhenHidden and OpacityWhenHoveredOver" << std::endl;
    file << "; If changed while application is running, restart is required!" << std::endl;
    file << "Enabled = " << animationsEnabled << std::endl;
    file << "AnimationStepDelay = " << animationStepDelay << std::endl;
    file << "AnimationOpacityStep = " << animationOpacityStep << std::endl;
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
    file << "; Tag 'process' (or 'p') is case-insensitive" << std::endl;
    file << "IgnoredWindows = " << utils::joinString(ignoredWindows, L"|") << std::endl;
    file << "ExceptionalWindows = " << utils::joinString(exceptionalWindows, L"|") << std::endl;
    if (exposeInternalKeys) {
        file << std::endl;
        file << "[Internal]" << std::endl;
        file << "; ONLY CHANGE VALUES BELOW IF YOU KNOW WHAT YOU'RE DOING" << std::endl;
        file << "___TaskbarWindowClassNameStarts = " << I_TaskbarWindowClassNameStarts << std::endl;
        file << "___TaskbarWindowClassNameEnds = " << I_TaskbarWindowClassNameEnds << std::endl;
        file << "; Executable names must be lowercase" << std::endl;
        file << "___TaskbarExceptionalWindows = " << utils::joinString(I_ExceptionalWindows, L",") << std::endl;
    }
    return true;
}

bool config::exists() {
    return utils::fileExists(CONFIG_FILENAME);
}

bool config::ensureConfigurationExists() {
#if IS_PORTABLE
    return exists();
#else
    if (!exists())
        return save(false);
    return true;
#endif
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

bool checkForEmptyValueS(const std::wstring &key, const std::wstring &value, std::wstring &obj, bool &noErrors) {
    if (value.empty()) {
        utils::messageBox(MSG_CONFIG_NO_VALUE, MB_ICONWARNING | MB_OK, {key, obj});
        noErrors = false;
        return false;
    }
    obj = value;
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
        formattedKey = key.substr(0, pos) + L" > " + key.substr(pos + 1);

        if (key == L"Taskbar.UpdateInterval") {
            if (checkForEmptyValueI(formattedKey, value, taskbarUpdateInterval, taskbarUpdateInterval, noErrors))
                checkForInvalidIntegerValue(formattedKey, taskbarUpdateInterval, 1, 1000, noErrors);
        } else if (key == L"General.Language") {
            auto languages = std::unordered_map<std::wstring, int>(APP_DEFAULT_LANGUAGES);
#if IS_PORTABLE == 0
            if (utils::fileExists(std::wstring(L"languages/language." + value + L".ini").c_str())) {
                languageCode = IDR_INI_LANG_CUSTOM;
                languageShortName = value;
                if (languages.contains(value))
                    utils::updateLanguageFile();
            } else {
#endif
                if (!languages.contains(value)) {
                    auto keysView = std::views::keys(languages);
                    const std::vector languagesVector(keysView.begin(), keysView.end());
                    utils::messageBox(MSG_CONFIG_INVALID_LANGUAGE, MB_ICONWARNING | MB_OK, { utils::joinString(languagesVector, L", ") });
                    return false;
                }
                languageShortName = value;
                languageCode = languages[value];
#if IS_PORTABLE == 0
            }
#endif
            utils::loadIfNeededAndGetCachedLanguageString(0, nullptr); // Trigger language cache to update
        } else if (key == L"Window.DarkMode") {
            checkBoolValidation(formattedKey, value, darkMode, darkMode, noErrors);
        } else if (key == L"Window.AutoUpdate") {
            checkBoolValidation(formattedKey, value, autoUpdate, autoUpdate, noErrors);
        } else if (key == L"Window.ShowAllWindows") {
            checkBoolValidation(formattedKey, value, showAllWindows, showAllWindows, noErrors);
        } else if (key == L"Window Behaviour.OpenOnStart") {
            checkBoolValidation(formattedKey, value, openOnStart, openOnStart, noErrors);
        } else if (key == L"Window Behaviour.CloseToTray") {
            checkBoolValidation(formattedKey, value, closeToTray, closeToTray, noErrors);
        } else if (key == L"Window Behaviour.CloseConfirmMessage") {
            checkBoolValidation(formattedKey, value, closeConfirmMessage, closeConfirmMessage, noErrors);
        } else if (key == L"Window Behaviour.MinimizeToTray") {
            checkBoolValidation(formattedKey, value, minimizeToTray, minimizeToTray, noErrors);
        } else if (key == L"Taskbar.OpacityWhenHidden") {
            if (checkForEmptyValueI(formattedKey, value, opacityWhenHidden, opacityWhenHidden, noErrors))
                checkForInvalidIntegerValue(formattedKey, opacityWhenHidden, 0, 100, noErrors);
            opacityWhenHiddenInternal = opacityWhenHidden > 0 ? 255 * opacityWhenHidden / 100 : 0;
        } else if (key == L"Taskbar.OpacityWhenShown") {
            if (checkForEmptyValueI(formattedKey, value, opacityWhenShown, opacityWhenShown, noErrors))
                checkForInvalidIntegerValue(formattedKey, opacityWhenShown, 1, 100, noErrors);
            opacityWhenShownInternal = 255 * opacityWhenShown / 100;
        } else if (key == L"Taskbar.DisableAutoUpdateWhenUnfocused") {
            checkBoolValidation(formattedKey, value, disableAutoUpdateWhenUnfocused, disableAutoUpdateWhenUnfocused, noErrors);
        } else if (key == L"Taskbar.OpacityWhenHoveredOver") {
            if (checkForEmptyValueI(formattedKey, value, opacityWhenHovered, opacityWhenHovered, noErrors))
                checkForInvalidIntegerValue(formattedKey, opacityWhenHovered, 1, 100, noErrors);
            opacityWhenHoveredInternal = 255 * opacityWhenHovered / 100;
        } else if (key == L"Ignored Windows.IgnoredWindows") {
            ignoredWindows = utils::splitString(value, '|');
        } else if (key == L"Ignored Windows.ExceptionalWindows") {
            exceptionalWindows = utils::splitString(value, '|');
        } else if (key == L"Ignored Windows.AlwaysIgnoreWhenNotMaximized") {
            checkBoolValidation(formattedKey, value, alwaysIgnoreWhenNotMaximized, alwaysIgnoreWhenNotMaximized, noErrors);
        } else if (key == L"Taskbar Hover Animation.Enabled") {
            checkBoolValidation(formattedKey, value, animationsEnabled, animationsEnabled, noErrors);
        } else if (key == L"Taskbar Hover Animation.AnimationStepDelay") {
            if (checkForEmptyValueI(formattedKey, value, animationStepDelay, animationStepDelay, noErrors))
                checkForInvalidIntegerValue(formattedKey, animationStepDelay, 1, 100, noErrors);
        } else if (key == L"Taskbar Hover Animation.AnimationOpacityStep") {
            if (checkForEmptyValueI(formattedKey, value, animationOpacityStep, animationOpacityStep, noErrors))
                checkForInvalidIntegerValue(formattedKey, animationOpacityStep, 1, 255, noErrors);
        } else if (key == L"Taskbar Hover Animation.Enabled") {
            checkBoolValidation(formattedKey, value, animationsEnabled, animationsEnabled, noErrors);
        } else if (key == L"Taskbar Hover Animation.AnimationStepDelay") {
            if (checkForEmptyValueI(formattedKey, value, animationStepDelay, animationStepDelay, noErrors))
                checkForInvalidIntegerValue(formattedKey, animationStepDelay, 1, 500, noErrors);
        } else if (key == L"Taskbar Hover Animation.AnimationOpacityStep") {
            if (checkForEmptyValueI(formattedKey, value, animationOpacityStep, animationOpacityStep, noErrors))
                checkForInvalidIntegerValue(formattedKey, animationOpacityStep, 1, 255, noErrors);
        } else if (key == L"Internal.___TaskbarWindowClassNameStarts") {
            checkForEmptyValueS(formattedKey, value, I_TaskbarWindowClassNameStarts, noErrors);
        } else if (key == L"Internal.___TaskbarWindowClassNameEnds") {
            checkForEmptyValueS(formattedKey, value, I_TaskbarWindowClassNameEnds, noErrors);
        } else if (key == L"Internal.___TaskbarExceptionalWindows") {
            I_ExceptionalWindows = utils::splitString(value, L',');
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
    if (!ensureConfigurationExists())
        return false;

    std::ifstream file(CONFIG_FILENAME, std::ios::binary);
    if (!file) {
        utils::messageBox(MSG_CONFIG_LOAD_FAILED, MB_ICONERROR | MB_OK);
        return false;
    }

    std::string line;
    std::wstring utf8line;
    std::wstring prefix;

    bool noErrors = true;
    int encodingErrorCount = 0;

    int lineNum = 0;
    while (getline(file, line)) {
        ++lineNum;

        try {
            utf8line = utils::utf8ToWide(line);
        } catch (const std::exception&) {
            // my IDE is kinda stupid lol
            // ReSharper disable once CppDFAUnusedValue
            noErrors = false;
            ++encodingErrorCount;
            if (encodingErrorCount >= 3)
                continue;
            utils::messageBox(MSG_CONFIG_INVALID_SYNTAX, MB_ICONERROR | MB_OK, { std::to_wstring(lineNum), utils::message(MSG_CONFIG_ENCODING_ERROR) });
            continue;
        }

        std::wstring key, value;
        if (!utils::processIniFileLine(utf8line, &prefix, key, value))
            continue;

        if (key.empty()) {
            utils::messageBox(MSG_CONFIG_INVALID_SYNTAX, MB_ICONERROR | MB_OK, { std::to_wstring(lineNum), utf8line });
            noErrors = false;
            continue;
        }

        if (!processSingle(prefix + key, value))
            noErrors = false;
    }
    file.close();
    return noErrors;
}