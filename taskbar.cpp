#include "taskbar.h"

#include <iostream>
#include <windows.h>
#include <cmath>
#include "config.h"
#include "utils.h"
#include <iomanip>
#include <numeric>

constexpr std::string debug_column_headers[] = { "i", "e", "ig. tag", "ex. tag", "f", "m", "left", "top", "righ", "bott", "process", "class", "title" };
constexpr int debug_column_sizes[] = { 1, 1, 10, 10, 1, 1, 4, 4, 4, 4, 20, 20, 20 };
const int taskbar::debug_columns_total_width = std::accumulate(
        std::begin(debug_column_sizes),
        std::end(debug_column_sizes),
        0,
        std::plus<int>()
    ) + 3 * std::size(debug_column_sizes) + 1;

bool taskbar::wasDebugFlushed = false;
bool taskbar::canUpdateDebugMessages = false;

bool canCreateTable = false;

HWND taskbar::getTaskbarHandle() {
    return FindWindowW(L"Shell_TrayWnd", nullptr);
}

bool taskbar::isCursorOverTaskbar() {
    HWND taskbar = getTaskbarHandle();
    if (!taskbar) return false;

    RECT taskbarRect;
    GetWindowRect(taskbar, &taskbarRect);

    POINT cursorPos;
    GetCursorPos(&cursorPos);

    return PtInRect(&taskbarRect, cursorPos);
}

bool loopThroughWindowTags(std::vector<std::string> vector, const HWND hwnd, std::string &processName, std::string &title,
    std::string &wndclass, DWORD &focusStatus, WINDOWINFO &wi, const WINDOWPLACEMENT &wp, RECT wRect, std::string &succeededTagGroup) {
    for (const auto& tagGroup: vector) {
        if (tagGroup.length() == 0)
            continue;
        std::vector<std::string> tags = utils::splitString(tagGroup, '&');
        if (tags.size() == 0)
            continue;
        int succeededTags = 0;
        for (const auto& texpr : tags) {
            const auto pos = texpr.find(":", 0);
            if (pos == std::string::npos)
                continue;
            char _[256];
            std::string key = texpr.substr(0, pos);
            std::string value = texpr.substr(pos + 1);
            if (key == "process" || key == "p") {
                if (processName.length() == 0)
                    utils::getProcessInfo(hwnd, processName);
                if (processName == value)
                    succeededTags++;
            } else if (key == "title" || key == "t") {
                if (title.length() == 0) {
                    GetWindowTextA(hwnd, _, sizeof(_));
                    title = _;
                }
                if (title == value)
                    succeededTags++;
            } else if (key == "focus" || key == "f") {
                if (focusStatus == -1) {
                    wi.cbSize = sizeof(WINDOWINFO);
                    GetWindowInfo(hwnd, &wi);
                    focusStatus = wi.dwWindowStatus;
                }
                if (focusStatus == stoi(value))
                    succeededTags++;
            } else if (key == "class" || key == "c") {
                if (wndclass.length() == 0) {
                    GetClassNameA(hwnd, _, sizeof(_));
                    wndclass = _;
                }
                if (std::string(wndclass) == value)
                    succeededTags++;
            } else if (key == "maximized" || key == "m") {
                if (wp.showCmd == SW_MAXIMIZE == stoi(value))
                    succeededTags++;
            } else if (key == "left" || key == "right" || key == "top" || key == "bottom") {
                if (wRect.left == -1 && wRect.top == -1 && wRect.right == -1 && wRect.bottom == -1)
                    GetWindowRect(hwnd, &wRect);
                const int iValue = std::stoi(value);
                int rect;
                if (key == "left")
                    rect = wRect.left;
                else if (key == "right")
                    rect = wRect.right;
                else if (key == "top")
                    rect = wRect.top;
                else if (key == "bottom")
                    rect = wRect.bottom;
                else
                    continue;
                if (rect == iValue)
                    succeededTags++;
            }
        }
        if (succeededTags == tags.size()) {
            succeededTagGroup = tagGroup;
            return true;
        }
    }
    return false;
}

void coloredLine(const int length, const bool endLAfter) {
    const HANDLE hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsoleOutput, BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
    for (auto i = 0; i < length; i++)
        std::cout << ' ';
    SetConsoleTextAttribute(hConsoleOutput, 15);
    if (endLAfter)
        std::cout << std::endl;
}

void stdCOutRepeat(const char ch, const int len, const bool end) {
    if (!len) return;
    for (auto i = 0; i < len; i++)
        std::cout << ch;
    if (end)
        std::cout << std::endl;
}

bool taskbar::isAnyWindowMaximized() {
    bool maximized = false;
    wasDebugFlushed = false;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (config::debug && canUpdateDebugMessages) {
        int consoleMaxLength = 0;
        HANDLE hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        constexpr COORD startCoord = { 0, 2 };
        if(GetConsoleScreenBufferInfo(hConsoleOutput, &csbi)) {
            consoleMaxLength = csbi.srWindow.Right - csbi.srWindow.Left;
            bool canCreateTable0 = consoleMaxLength >= debug_columns_total_width;
            if (canCreateTable != canCreateTable0) {
                utils::clearConsole(startCoord, true);
                canCreateTable = canCreateTable0;
            }
        }
        if (!canCreateTable)
            utils::clearConsole(startCoord, true);
        else
            SetConsoleCursorPosition(hConsoleOutput, startCoord);
        std::cout << "[DEBUG] Loop start" << std::endl;
        if (!canCreateTable)
            std::cout << "[DEBUG] Table couldn't be formatted, required min width " << debug_columns_total_width << ", got " << consoleMaxLength << std::endl;
        else {
            coloredLine(debug_columns_total_width, true);
            coloredLine(1, false);
            constexpr int dSize = std::size(debug_column_sizes);
            for (auto i = 0; i < dSize; i++) {
                const int size = debug_column_sizes[i];
                std::cout << ' ';
                std::cout << std::setw(size) << std::left << debug_column_headers[i];
                std::cout << ' ';
                coloredLine(1, i + 1 == dSize);
            }
            coloredLine(debug_columns_total_width, true);
        }
    }

    EnumWindows([](HWND hwnd, const LPARAM lParam) -> BOOL {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);

        if (!GetWindowPlacement(hwnd, &wp) || !IsWindowVisible(hwnd) || IsIconic(hwnd))
            return TRUE;

        if (config::alwaysIgnoreWhenNotMaximized && wp.showCmd != SW_MAXIMIZE)
            return TRUE;

        RECT wRect { -1, -1, -1, -1 };
        std::string processName = "",
                    title = "",
                    wndclass = "",
                    succeededIgnoreTagGroup = "",
                    succeededExceptionTagGroup = "";
        WINDOWINFO wi;
        DWORD focusStatus = -1;

        if (config::debug && canUpdateDebugMessages) {
            // Focus status (0 or 1)
            wi.cbSize = sizeof(WINDOWINFO);
            GetWindowInfo(hwnd, &wi);
            focusStatus = wi.dwWindowStatus;
            // Process filename
            utils::getProcessInfo(hwnd, processName);
            // Title
            char _[256];
            GetWindowTextA(hwnd, _, sizeof(_));
            title = _;
            // Class
            GetClassNameA(hwnd, _, sizeof(_));
            wndclass = _;
            // Rect
            GetWindowRect(hwnd, &wRect);
        }

        bool ignored = true, exceptional = false;

        if (!config::ignoredWindows.empty()) {
            ignored = loopThroughWindowTags(config::ignoredWindows, hwnd, processName, title, wndclass, focusStatus, wi, wp, wRect, succeededIgnoreTagGroup);
        }

        if (!config::exceptionalWindows.empty()) {
            exceptional = loopThroughWindowTags(config::exceptionalWindows, hwnd, processName, title, wndclass, focusStatus, wi, wp, wRect, succeededExceptionTagGroup);
        }

        if (config::debug && canUpdateDebugMessages) {
            if (!canCreateTable) {
                std::cout << " ig=" << ignored
                          << " ex=" << exceptional
                          << " igT=" << (succeededIgnoreTagGroup.length() == 0 ? "NUL" : succeededIgnoreTagGroup)
                          << " exT=" << (succeededExceptionTagGroup.length() == 0 ? "NUL" : succeededExceptionTagGroup)
                          << " f=" << (focusStatus == -1 ? "N" : std::to_string(focusStatus))
                          << " m=" << (wp.showCmd == SW_MAXIMIZE)
                          << " sl=" << wRect.left
                          << " st=" << wRect.top
                          << " sr=" << wRect.right
                          << " sb=" << wRect.bottom
                          << " p=" << processName
                          << " c=" << wndclass
                          << " t=" << title << std::endl;
            } else {
                const std::vector values = {
                    std::to_string(ignored),
                    std::to_string(exceptional),
                    succeededIgnoreTagGroup.length() == 0 ? "NUL" : succeededIgnoreTagGroup,
                    succeededExceptionTagGroup.length() == 0 ? "NUL" : succeededExceptionTagGroup,
                    focusStatus == -1 ? "N" : std::to_string(focusStatus),
                    std::to_string(wp.showCmd == SW_MAXIMIZE),
                    std::to_string(wRect.left),
                    std::to_string(wRect.top),
                    std::to_string(wRect.right),
                    std::to_string(wRect.bottom),
                    processName,
                    wndclass,
                    title
                };
                coloredLine(1, false);
                std::vector<std::vector<std::string>> splits = {};
                int maxSplits = 0;
                constexpr int cSize = std::size(debug_column_sizes);
                for (auto i = 0; i < cSize; i++) {
                    const int size = debug_column_sizes[i];
                    std::cout << ' ';
                    if (const std::string value = values[i]; size < value.length()) {
                        std::vector<std::string> currentSplits = utils::splitToGroups(value, size);
                        splits.push_back(currentSplits);
                        std::cout << std::setw(size) << std::left << currentSplits[0];
                        if (currentSplits.size() > maxSplits)
                            maxSplits = currentSplits.size();
                    } else {
                        splits.push_back({});
                        std::cout << std::setw(size) << std::left << value;
                    }
                    std::cout << ' ';
                    coloredLine(1, i + 1 == cSize);
                }
                if (maxSplits > 1) {
                    for (auto i = 1; i < maxSplits; i++) {
                        for (auto j = 0; j < cSize; j++) {
                            const int size = debug_column_sizes[j];
                            const std::vector<std::string> currentSplits = splits[j];
                            if (j == 0)
                                coloredLine(1, false);
                            std::cout << ' ';
                            if (currentSplits.size() == 0) {
                                stdCOutRepeat(' ', size, false);
                            } else {
                                std::cout << std::setw(size) << std::left << splits[j][i];
                            }
                            std::cout << ' ';
                            coloredLine(1, j + 1 == cSize);
                        }
                    }
                }
            }
        }

        if (exceptional || !ignored) {
            *reinterpret_cast<bool*>(lParam) = true;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&maximized));
    if (config::debug && canUpdateDebugMessages) {
        if (!maximized)
            std::cout << "[DEBUG] No maximized windows found." << std::endl;
        if (canCreateTable) {
            coloredLine(debug_columns_total_width, true);
            COORD pos = { 0, csbi.dwCursorPosition.Y};
            pos.Y++;
            if (!maximized)
                pos.Y++;
            utils::clearConsole(pos, false);
        }
    }
    wasDebugFlushed = true;
    canUpdateDebugMessages = false;
    return maximized;
}

void taskbar::setTaskbarVisibility(const bool visible) {
    HWND taskbar = getTaskbarHandle();
    if (!taskbar) return;
    const LONG_PTR style = GetWindowLongPtr(taskbar, GWL_EXSTYLE);
    if (visible) {
        SetWindowLongPtr(taskbar, GWL_EXSTYLE, style & ~WS_EX_LAYERED);
        ShowWindow(taskbar, SW_SHOW);
    } else {
        SetWindowLongPtr(taskbar, GWL_EXSTYLE, style | WS_EX_LAYERED);
        SetLayeredWindowAttributes(taskbar, 0, 0, LWA_ALPHA);
        ShowWindow(taskbar, SW_HIDE);
    }
}

void taskbar::resetTaskbar() {
    HWND taskbar = getTaskbarHandle();
    if (!taskbar) return;
    SetWindowLongPtr(taskbar, GWL_EXSTYLE, GetWindowLongPtr(taskbar, GWL_EXSTYLE) & ~WS_EX_LAYERED);
    SetLayeredWindowAttributes(taskbar, 0, 255, LWA_ALPHA);
    ShowWindow(taskbar, SW_SHOW);
}

void taskbar::updateTaskbarState() {
    setTaskbarVisibility(isCursorOverTaskbar() || isAnyWindowMaximized());
}