#include "taskbar.h"

#include <iostream>
#include <windows.h>
#include "config.h"
#include "globals.h"
#include "resources.h"
#include "utils.h"

std::vector<taskbar::WindowInfo> taskbar::windows;
bool taskbar::collectWindowsInfo = false;

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

bool loopThroughWindowTags(const std::vector<std::wstring>& vector, taskbar::WindowInfo &wInfo, const WINDOWPLACEMENT &wp, std::wstring *succeededTagGroup) {
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
                    utils::getProcessInfo(wInfo.hwnd, wInfo.procFilename);
                if (wInfo.procFilename == value)
                    succeededTags++;
            } else if (key == L"title" || key == L"t") {
                if (wInfo.title[0] == L'\0')
                    GetWindowText(wInfo.hwnd, wInfo.title, sizeof(wInfo.title));
                if (wInfo.title == value)
                    succeededTags++;
            } else if (key == L"focus" || key == L"f") {
                if (wInfo.focused == -1) {
                    WINDOWINFO wi;
                    wi.cbSize = sizeof(WINDOWINFO);
                    GetWindowInfo(wInfo.hwnd, &wi);
                    wInfo.focused = wi.dwWindowStatus;
                }
                if (static_cast<int>(wInfo.focused) == stoi(value))
                    succeededTags++;
            } else if (key == L"class" || key == L"c") {
                if (wInfo.wndClass[0] == L'\0')
                    GetClassName(wInfo.hwnd, wInfo.wndClass, sizeof(wInfo.wndClass));
                if (std::wstring(wInfo.wndClass) == value)
                    succeededTags++;
            } else if (key == L"maximized" || key == L"m") {
                if (wp.showCmd == SW_MAXIMIZE == stoi(value))
                    succeededTags++;
            } else if (key == L"left" || key == L"right" || key == L"top" || key == L"bottom") {
                if (!wInfo.wasRectModified) {
                    GetWindowRect(wInfo.hwnd, &wInfo.rect);
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
            if (succeededTagGroup)
                *succeededTagGroup = tagGroup;
            return true;
        }
    }
    return false;
}

bool taskbar::isAnyWindowMaximized() {
    checkForAutoCollect();
    static WindowInfo lastDetectedWindow;
    static std::vector<WindowInfo> previousWindows;
    static bool wasFound = false, canCollect = false;

    if (collectWindowsInfo) {
        // Since collectWindowsInfo can get updated inside EnumWindows (by main thread), let's ensure whenever it can start collecting
        canCollect = true;
        windows.clear();
        lastDetectedWindow = {};
    } else if (lastDetectedWindow.hwnd) {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(lastDetectedWindow.hwnd, &wp) && IsWindowVisible(lastDetectedWindow.hwnd) & !IsIconic(lastDetectedWindow.hwnd)) {
            lastDetectedWindow = { lastDetectedWindow.hwnd };
            if (!config::ignoredWindows.empty())
                lastDetectedWindow.detected = loopThroughWindowTags(config::ignoredWindows, lastDetectedWindow, wp, nullptr);
            if (!config::exceptionalWindows.empty())
                lastDetectedWindow.wasExceptional = loopThroughWindowTags(config::exceptionalWindows, lastDetectedWindow, wp, nullptr);
            if (lastDetectedWindow.wasExceptional || !lastDetectedWindow.detected || (config::alwaysIgnoreWhenNotMaximized && wp.showCmd == SW_MAXIMIZE))
                return true;
        }
        lastDetectedWindow = {};
    }

    bool maximized = false;
    EnumWindows([](HWND hwnd, const LPARAM lParam) -> BOOL {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);

        if (!GetWindowPlacement(hwnd, &wp) || !IsWindowVisible(hwnd) || IsIconic(hwnd))
            return TRUE;

        WindowInfo wInfo = { hwnd };
        if (config::alwaysIgnoreWhenNotMaximized && wp.showCmd != SW_MAXIMIZE) {
            if (collectWindowsInfo && canCollect)
                wInfo.initiallyIgnored = true;
            else
                return TRUE;
        }
        wInfo.maximized = wp.showCmd == SW_MAXIMIZE;

        std::wstring succeededIgnoreTagGroup, succeededExceptionTagGroup;

        if (collectWindowsInfo && canCollect) {
            WINDOWINFO wi;
            wi.cbSize = sizeof(WINDOWINFO);
            GetWindowInfo(hwnd, &wi);
            wInfo.focused = wi.dwWindowStatus;                                    // Focus status (0 or 1)
            utils::getProcessInfo(hwnd, wInfo.procFilename);                   // Process filename
            GetWindowText(hwnd, wInfo.title, sizeof(wInfo.title));      // Title
            GetClassName(hwnd, wInfo.wndClass, sizeof(wInfo.wndClass)); // Class
            GetWindowRect(hwnd, &wInfo.rect);                                     // Rect
        }

        if (!wInfo.initiallyIgnored) {
            if (!config::ignoredWindows.empty())
                wInfo.detected = loopThroughWindowTags(config::ignoredWindows, wInfo, wp, &succeededIgnoreTagGroup);
            if (!config::exceptionalWindows.empty())
                wInfo.wasExceptional = loopThroughWindowTags(config::exceptionalWindows, wInfo, wp, &succeededExceptionTagGroup);
            wInfo.detected = (wInfo.wasExceptional || !wInfo.detected) && !wInfo.initiallyIgnored;
        }

        if (collectWindowsInfo && canCollect) {
            wInfo.fault = wInfo.wasExceptional ? succeededExceptionTagGroup : succeededIgnoreTagGroup;
            if (wInfo.detected && !wasFound) {
                wasFound = true;
                wInfo.finalDetection = true;
            }
            wInfo.hwnd = nullptr;
            windows.push_back(wInfo);
        }

        if (wInfo.detected) {
            *reinterpret_cast<bool*>(lParam) = true;
            if (!(collectWindowsInfo && canCollect))
                lastDetectedWindow = { hwnd };
            return config::showAllWindows;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&maximized));
    // ReSharper disable once CppDFAConstantConditions
    if (collectWindowsInfo && canCollect) {
        if (previousWindows != windows) {
            SendMessage(globals::hWnd, WM_UPDATE_GRID_REQUEST, 0, 0);
            previousWindows = windows;
        }
        collectWindowsInfo = false;
        canCollect = false;
    }
    wasFound = false;
    return maximized;
}


void taskbar::setTaskbarVisibility(const bool visible, const bool hoveredOver) {
    HWND taskbar = getTaskbarHandle();
    if (!taskbar) return;
    const LONG_PTR style = GetWindowLongPtr(taskbar, GWL_EXSTYLE);
    if (visible) {
        int opacity = config::opacityWhenShown;
        if (hoveredOver)
            opacity = config::opacityWhenHovered;
        SetWindowLongPtr(taskbar, GWL_EXSTYLE, opacity == 0 ? style & ~WS_EX_LAYERED : style | WS_EX_LAYERED);
        SetLayeredWindowAttributes(taskbar, 0, opacity, LWA_ALPHA);
        ShowWindow(taskbar, SW_SHOW);
    } else {
        SetWindowLongPtr(taskbar, GWL_EXSTYLE, style | WS_EX_LAYERED);
        SetLayeredWindowAttributes(taskbar, 0, config::opacityWhenHidden, LWA_ALPHA);
        if (config::opacityWhenHidden == 0)
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
    const bool hoveredOver = isCursorOverTaskbar();
    setTaskbarVisibility(hoveredOver || isAnyWindowMaximized(), hoveredOver);
}

void taskbar::checkForAutoCollect() {
    if (collectWindowsInfo || !config::livePreview)
        return;
    static DWORD lastTick = GetTickCount();
    DWORD currentTick = GetTickCount();
    if (currentTick - lastTick >= 1000) {
        collectWindowsInfo = true;
        lastTick = currentTick;
    }
}