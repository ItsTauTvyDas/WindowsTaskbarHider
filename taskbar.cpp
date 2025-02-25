#include "taskbar.h"

#include <iostream>
#include <windows.h>
#include <cmath>
#include "config.h"
#include "utils.h"
#include <iomanip>
#include <numeric>

constexpr std::wstring debug_column_headers[] = { L"i", L"e", L"ig. tag", L"ex. tag", L"f", L"m", L"left", L"top", L"righ", L"bott", L"process", L"class", L"title" };
constexpr int debug_column_sizes[] = { 1, 1, 10, 10, 1, 1, 4, 4, 4, 4, 20, 20, 20 };
const int taskbar::debug_columns_total_width = std::accumulate(
        std::begin(debug_column_sizes),
        std::end(debug_column_sizes),
        0,
        std::plus()
    ) + 3 * std::size(debug_column_sizes) + 1;

bool taskbar::wasDebugFlushed = false;
bool taskbar::canUpdateDebugMessages = false;

bool canCreateTable = false;

HWND taskbar::getTaskbarHandle() {
    return FindWindow(L"Shell_TrayWnd", nullptr);
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

bool loopThroughWindowTags(const std::vector<std::wstring>& vector, HWND hwnd, std::wstring &processName, std::wstring &title,
    std::wstring &wndclass, DWORD &focusStatus, WINDOWINFO &wi, const WINDOWPLACEMENT &wp, RECT wRect, std::wstring &succeededTagGroup) {
    for (const auto& tagGroup: vector) {
        if (tagGroup.empty())
            continue;
        std::vector<std::wstring> tags = utils::splitString(tagGroup, '&');
        if (tags.empty())
            continue;
        int succeededTags = 0;
        for (const auto& texpr : tags) {
            const auto pos = texpr.find(L":", 0);
            if (pos == std::wstring::npos)
                continue;
            wchar_t _[256];
            std::wstring key = texpr.substr(0, pos);
            std::wstring value = texpr.substr(pos + 1);
            if (key == L"process" || key == L"p") {
                if (processName.empty())
                    utils::getProcessInfo(hwnd, processName);
                if (processName == value)
                    succeededTags++;
            } else if (key == L"title" || key == L"t") {
                if (title.empty()) {
                    GetWindowText(hwnd, _, sizeof(_));
                    title = _;
                }
                if (title == value)
                    succeededTags++;
            } else if (key == L"focus" || key == L"f") {
                if (focusStatus == -1) {
                    wi.cbSize = sizeof(WINDOWINFO);
                    GetWindowInfo(hwnd, &wi);
                    focusStatus = wi.dwWindowStatus;
                }
                if (focusStatus == stoi(value))
                    succeededTags++;
            } else if (key == L"class" || key == L"c") {
                if (wndclass.empty()) {
                    GetClassName(hwnd, _, sizeof(_));
                    wndclass = _;
                }
                if (std::wstring(wndclass) == value)
                    succeededTags++;
            } else if (key == L"maximized" || key == L"m") {
                if (wp.showCmd == SW_MAXIMIZE == stoi(value))
                    succeededTags++;
            } else if (key == L"left" || key == L"right" || key == L"top" || key == L"bottom") {
                if (wRect.left == -1 && wRect.top == -1 && wRect.right == -1 && wRect.bottom == -1)
                    GetWindowRect(hwnd, &wRect);
                const int iValue = std::stoi(value);
                int rect;
                if (key == L"left")
                    rect = wRect.left;
                else if (key == L"right")
                    rect = wRect.right;
                else if (key == L"top")
                    rect = wRect.top;
                else if (key == L"bottom")
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
    HANDLE hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
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

    EnumWindows([](HWND hwnd, const LPARAM lParam) -> BOOL {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);

        if (!GetWindowPlacement(hwnd, &wp) || !IsWindowVisible(hwnd) || IsIconic(hwnd))
            return TRUE;

        if (config::alwaysIgnoreWhenNotMaximized && wp.showCmd != SW_MAXIMIZE)
            return TRUE;

        RECT wRect { -1, -1, -1, -1 };
        std::wstring processName,
                    title,
                    wndclass,
                    succeededIgnoreTagGroup,
                    succeededExceptionTagGroup;
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
            wchar_t _[256];
            GetWindowText(hwnd, _, sizeof(_));
            title = _;
            // Class
            GetClassName(hwnd, _, sizeof(_));
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

        if (exceptional || !ignored) {
            *reinterpret_cast<bool*>(lParam) = true;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&maximized));
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