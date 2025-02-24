#include "taskbar.h"

#include <iostream>
#include <cmath>
#include "config.h"
#include "utils.h"
#include <iomanip>

int debug_column_sizes[] = {1, 1, 1, 10, 10, 2, 1, 4, 4, 4, 4, 20, 20, 20};
constexpr int debug_columns_total_width = 102 + 2 * sizeof(debug_column_sizes);

bool taskbar::wasDebugFlushed = false;
bool taskbar::canUpdateDebugMessages = false;
int consoleMaxLength = 0;

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
    std::string &wndclass, DWORD &focusStatus, WINDOWINFO &wi, const WINDOWPLACEMENT &wp, RECT wRect, bool &priority,
    std::string &succeededTagGroup) {
    for (const auto& tagGroup: vector) {
        if (tagGroup.length() == 0)
            continue;
        bool _priority = false;
        std::vector<std::string> tags = utils::splitString(tagGroup, '&');
        if (tags.size() == 0)
            continue;
        if (tags[0].rfind("^", 0) == 0) {
            tags[0] = tags[0].substr(1);
            _priority = true;
        }
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
            priority = _priority;
            return true;
        }
    }
    return false;
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

    if (config::debug && canUpdateDebugMessages) {
        utils::clearConsole({ 0, 2 });
        std::cout << "[DEBUG] Loop start" << std::endl;
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
            consoleMaxLength = csbi.srWindow.Right-csbi.srWindow.Left;
            if (consoleMaxLength < debug_columns_total_width) {
                std::cout << "[DEBUG] Table couldn't be formatted, required min width " << debug_columns_total_width << ", got " << consoleMaxLength << std::endl;
                consoleMaxLength = 0;
            }
            stdCOutRepeat('#', consoleMaxLength, true);
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

        bool ignored = true, exceptional = false, priority = false;

        if (!config::ignoredWindows.empty()) {
            bool _;
            ignored = loopThroughWindowTags(config::ignoredWindows, hwnd, processName, title, wndclass, focusStatus, wi, wp, wRect, _, succeededIgnoreTagGroup);
        }

        if (!config::exceptionalWindows.empty()) {
            exceptional = loopThroughWindowTags(config::exceptionalWindows, hwnd, processName, title, wndclass, focusStatus, wi, wp, wRect, priority, succeededExceptionTagGroup);
        }

        if (config::debug && canUpdateDebugMessages) {
            if (!consoleMaxLength) {
                std::cout << " ig=" << ignored
                          << " ex=" << exceptional
                          << " pr=" << priority
                          << " igT=" << (succeededIgnoreTagGroup.length() == 0 ? "NUL" : succeededIgnoreTagGroup)
                          << " exT=" << (succeededExceptionTagGroup.length() == 0 ? "NUL" : succeededExceptionTagGroup)
                          << " f=" << focusStatus
                          << " m=" << (wp.showCmd == SW_MAXIMIZE)
                          << " sl=" << wRect.left
                          << " st=" << wRect.top
                          << " sr=" << wRect.right
                          << " sb=" << wRect.bottom
                          << " p=" << processName
                          << " c=" << wndclass
                          << " t=" << title << std::endl;
            } else {
                try {
                    std::vector<std::string> values = {};
                    values.push_back(std::to_string(ignored));
                    values.push_back(std::to_string(exceptional));
                    values.push_back(std::to_string(priority));
                    values.push_back(succeededIgnoreTagGroup.length() == 0 ? "NUL" : succeededIgnoreTagGroup);
                    values.push_back(succeededExceptionTagGroup.length() == 0 ? "NUL" : succeededExceptionTagGroup);
                    values.push_back(std::to_string(focusStatus));
                    values.push_back(std::to_string(wp.showCmd == SW_MAXIMIZE));
                    values.push_back(std::to_string(wRect.left));
                    values.push_back(std::to_string(wRect.top));
                    values.push_back(std::to_string(wRect.right));
                    values.push_back(std::to_string(wRect.bottom));
                    values.push_back(processName);
                    values.push_back(wndclass);
                    values.push_back(title);
                    std::cout << '#';
                    for (auto i = 0; i < values.size(); i++) {
                        std::cout << ' ';
                        const int size = debug_column_sizes[i];
                        if (size < values[i].length())
                            debug_column_sizes[i] = values[i].length();
                        std::cout << std::setw(size) << std::left << values[i];
                        std::cout << ' ';
                        if (i < values.size() - 1)
                            std::cout << '#';
                    }
                    std::cout << '#' << std::endl;
                } catch (const std::exception e) {
                    std::cout << "ERROR: " << e.what() << std::endl;
                }
            }
        }

        if (exceptional || !ignored || priority) {
            *reinterpret_cast<bool*>(lParam) = true;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&maximized));
    if (config::debug && canUpdateDebugMessages) {
        stdCOutRepeat('#', consoleMaxLength, true);
        if (!maximized)
            std::cout << "[DEBUG] No maximized windows found." << std::endl;
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