#include "taskbar.h"

#include <iostream>
#include <windows.h>
#include "config.h"
#include "globals.h"
#include "resources.h"
#include "utils.h"

std::vector<taskbar::WindowInfo> taskbar::windows;
taskbar::WindowInfo lastDetectedWindow;
bool taskbar::collectWindowsInfo = false;
bool canCollect = false;

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

bool loopThroughWindowTags(const std::vector<std::wstring>& vector, HWND hwnd, taskbar::WindowInfo &wInfo, WINDOWPLACEMENT wp, std::wstring &succeededTagGroup) {
    for (const auto& tagGroup: vector) {
        if (tagGroup.empty())
            continue;
        std::vector<std::wstring> tags = utils::splitString(tagGroup, '&');
        if (tags.empty())
            continue;
        int succeededTags = 0;
        for (const auto& tExpr : tags) {
            const auto pos = tExpr.find(L":", 0);
            if (pos == std::wstring::npos)
                continue;
            std::wstring key = tExpr.substr(0, pos);
            std::wstring value = tExpr.substr(pos + 1);
            if (key == L"process" || key == L"p") {
                if (wInfo.procFilename.empty())
                    utils::getProcessInfo(hwnd, wInfo.procFilename);
                if (wInfo.procFilename == value)
                    succeededTags++;
            } else if (key == L"title" || key == L"t") {
                if (wInfo.title[0] == L'\0')
                    GetWindowText(hwnd, wInfo.title, sizeof(wInfo.title));
                if (wInfo.title == value)
                    succeededTags++;
            } else if (key == L"focus" || key == L"f") {
                if (wInfo.focus == -1) {
                    WINDOWINFO wi;
                    wi.cbSize = sizeof(WINDOWINFO);
                    GetWindowInfo(hwnd, &wi);
                    wInfo.focus = wi.dwWindowStatus;
                }
                if (wInfo.focus == stoi(value))
                    succeededTags++;
            } else if (key == L"class" || key == L"c") {
                if (wInfo.wndClass[0] == L'\0')
                    GetClassName(hwnd, wInfo.wndClass, sizeof(wInfo.wndClass));
                if (std::wstring(wInfo.wndClass) == value)
                    succeededTags++;
            } else if (key == L"maximized" || key == L"m") {
                if (wp.showCmd == SW_MAXIMIZE == stoi(value))
                    succeededTags++;
            } else if (key == L"left" || key == L"right" || key == L"top" || key == L"bottom") {
                if (!wInfo.wasRectModified) {
                    GetWindowRect(hwnd, &wInfo.rect);
                    wInfo.wasRectModified = true;
                }
                const int iValue = std::stoi(value);
                int rect;
                if (key == L"left")
                    rect = wInfo.rect.left;
                else if (key == L"right")
                    rect = wInfo.rect.right;
                else if (key == L"top")
                    rect = wInfo.rect.top;
                else if (key == L"bottom")
                    rect = wInfo.rect.bottom;
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

bool taskbar::isAnyWindowMaximized() {
    if (collectWindowsInfo) {
        // Since collectWindowsInfo can get updated inside EnumWindows (by main thread), let's ensure whenever it can start collecting
        canCollect = true;
        windows.clear();
    } else if (lastDetectedWindow.hwnd) {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(lastDetectedWindow.hwnd, &wp) && IsWindowVisible(lastDetectedWindow.hwnd) & !IsIconic(lastDetectedWindow.hwnd)) {
            bool ignored = true, exceptional = false;
            std::wstring s;
            if (!config::exceptionalWindows.empty())
                exceptional = loopThroughWindowTags(config::exceptionalWindows, lastDetectedWindow.hwnd, lastDetectedWindow, wp, s);
            if (!config::ignoredWindows.empty())
                ignored = loopThroughWindowTags(config::ignoredWindows, lastDetectedWindow.hwnd, lastDetectedWindow, wp, s);
            if (exceptional || !ignored)
                return true;
        }
        lastDetectedWindow = WindowInfo();
    }
    bool maximized = false;
    EnumWindows([](HWND hwnd, const LPARAM lParam) -> BOOL {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);

        if (!GetWindowPlacement(hwnd, &wp) || !IsWindowVisible(hwnd) || IsIconic(hwnd))
            return TRUE;

        if (config::alwaysIgnoreWhenNotMaximized && wp.showCmd != SW_MAXIMIZE)
            return TRUE;

        WindowInfo wInfo;
        std::wstring succeededIgnoreTagGroup,
                     succeededExceptionTagGroup;

        if (collectWindowsInfo && canCollect) {
            // Focus status (0 or 1)
            WINDOWINFO wi;
            wi.cbSize = sizeof(WINDOWINFO);
            GetWindowInfo(hwnd, &wi);
            wInfo.focus = wi.dwWindowStatus;
            // Process filename
            utils::getProcessInfo(hwnd, wInfo.procFilename);
            // Title
            GetWindowText(hwnd, wInfo.title, sizeof(wInfo.title));
            // Class
            GetClassName(hwnd, wInfo.wndClass, sizeof(wInfo.wndClass));
            // Rect
            GetWindowRect(hwnd, &wInfo.rect);
        }

        bool ignored = true, exceptional = false;

        if (!config::ignoredWindows.empty()) {
            ignored = loopThroughWindowTags(config::ignoredWindows, hwnd, wInfo, wp, succeededIgnoreTagGroup);
        }

        if (!config::exceptionalWindows.empty()) {
            exceptional = loopThroughWindowTags(config::exceptionalWindows, hwnd, wInfo, wp, succeededExceptionTagGroup);
        }

        if (collectWindowsInfo && canCollect) {
            wInfo.fault = exceptional ? succeededExceptionTagGroup : succeededIgnoreTagGroup;
            wInfo.wasExceptional = exceptional;
            wInfo.detected = exceptional || !ignored;
            windows.push_back(wInfo);
        }

        if (exceptional || !ignored) {
            *reinterpret_cast<bool*>(lParam) = true;
            wInfo.hwnd = hwnd;
            lastDetectedWindow = wInfo;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&maximized));
    // ReSharper disable once CppDFAConstantConditions
    if (collectWindowsInfo && canCollect) {
        SendMessage(globals::hWnd, WM_UPDATE_GRID_REQUEST, 0, 0);
        collectWindowsInfo = false;
        canCollect = false;
    }
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