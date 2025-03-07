#include "taskbar.h"

#include <iostream>
#include <mutex>
#include <windows.h>
#include <ranges>
#include "config.h"
#include "globals.h"
#include "monitors.h"
#include "resources.h"
#include "utils.h"

std::mutex taskbarMutex;
bool errorState = false;

std::vector<taskbar::WindowInfo> taskbar::windows;
std::unordered_map<HMONITOR, bool> taskbar::taskbarForcedVisibilityStates;
std::unordered_map<HMONITOR, HWND> taskbar::taskbarHandles;
bool taskbar::collectWindowsInfo = false;
bool taskbar::forceToCollect = false;

void taskbar::findTaskbarHandles() {
    std::lock_guard lock(taskbarMutex);
    taskbarHandles.clear();
    EnumWindows([](HWND hwnd, const LPARAM) -> BOOL {
        std::wstring className(256, L'\0');
        if (const int len = GetClassNameW(hwnd, className.data(), static_cast<int>(className.size())); len > 0) {
            className.resize(len);
            const std::wstring& prefix = config::I_TaskbarWindowClassNameStarts;
            const std::wstring& suffix = config::I_TaskbarWindowClassNameEnds;
            if (className.find(prefix) != 0)
                return TRUE;
            if (className.size() >= suffix.size() && className.compare(className.size() - suffix.size(), suffix.size(), suffix) == 0) {
                WindowInfo wInfo { hwnd };
                wInfo.updateMonitor();
                taskbarHandles[wInfo.hMonitor] = hwnd;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(nullptr));
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
                std::wstring process = wInfo.procFilename;
                std::transform(process.begin(), process.end(), process.begin(), tolower);
                std::transform(value.begin(), value.end(), value.begin(), tolower);
                if (process == value)
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
            } else if (key == L"monitor" || key == L"mon") {
                if (const int index = std::stoi(value); index >= 0 && index < sizeof(monitors::indexedMonitors) && monitors::indexedMonitors[index] == wInfo.hMonitor)
                    succeededTags++;
            } else if (key == L"maximized" || key == L"m") {
                if ((wp.showCmd == SW_MAXIMIZE) == stoi(value))
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

taskbar::WindowInfo taskbar::WindowInfo::reset() const {
    return { hwnd, hMonitor };
}

void taskbar::WindowInfo::updateMonitor() {
    hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
}

void taskbar::WindowInfo::updateValues(HWND hwnd) {
    WINDOWINFO wi; wi.cbSize = sizeof(WINDOWINFO); GetWindowInfo(hwnd, &wi);
    focused = wi.dwWindowStatus; // Focus status (0 or 1)
    utils::getProcessInfo(hwnd, procFilename); // Process filename
    GetWindowText(hwnd, title, sizeof(title)); // Title
    GetClassName(hwnd, wndClass, sizeof(wndClass)); // Class
    GetWindowRect(hwnd, &rect); // Rect
}

std::unordered_map<HMONITOR, taskbar::WindowInfo> taskbar::findAllMaximizedWindows() {
    checkForAutoCollect();
    static std::vector<WindowInfo> previousWindows;
    static bool canCollect = false;

    if (collectWindowsInfo) {
        // Since collectWindowsInfo can get updated inside EnumWindows (by main thread), let's ensure whenever it can start collecting
        canCollect = true;
        windows.clear();
    }
    // TODO implement caching
    std::unordered_map<HMONITOR, WindowInfo> maximizedWindows = {};
    try {
        EnumWindows([](HWND hwnd, const LPARAM lParam) -> BOOL {
            WINDOWPLACEMENT wp;
            wp.length = sizeof(WINDOWPLACEMENT);

            if (!GetWindowPlacement(hwnd, &wp) || !IsWindowVisible(hwnd) || IsIconic(hwnd))
                return TRUE;

            WindowInfo wInfo = {};
            if (config::alwaysIgnoreWhenNotMaximized && wp.showCmd != SW_MAXIMIZE) {
                if (collectWindowsInfo && canCollect)
                    wInfo.initiallyIgnored = true;
                else
                    return TRUE;
            }
            wInfo.hwnd = hwnd;
            wInfo.updateMonitor();
            if (const auto taskbar = taskbarHandles.find(wInfo.hMonitor); taskbar != taskbarHandles.end() && taskbarHandles[wInfo.hMonitor] == hwnd)
                return TRUE;
            wInfo.maximized = wp.showCmd == SW_MAXIMIZE;

            std::wstring succeededIgnoreTagGroup, succeededExceptionTagGroup;
            if (collectWindowsInfo && canCollect)
                wInfo.updateValues(hwnd);

            if (!wInfo.initiallyIgnored) {
                if (!config::ignoredWindows.empty())
                    wInfo.detected = loopThroughWindowTags(config::ignoredWindows, wInfo, wp, &succeededIgnoreTagGroup);
                if (!config::exceptionalWindows.empty())
                    wInfo.wasExceptional = loopThroughWindowTags(config::exceptionalWindows, wInfo, wp, &succeededExceptionTagGroup);
                wInfo.detected = (wInfo.wasExceptional || !wInfo.detected) && !wInfo.initiallyIgnored;
            }

            if (collectWindowsInfo && canCollect) {
                wInfo.fault = wInfo.wasExceptional ? succeededExceptionTagGroup : succeededIgnoreTagGroup;
                wInfo.hwnd = nullptr;
                windows.push_back(wInfo);
            }

            if (wInfo.detected) {
                auto &map = *reinterpret_cast<std::unordered_map<HMONITOR, WindowInfo>*>(lParam);
                map[wInfo.hMonitor] = wInfo;
                return config::showAllWindows || map.size() != monitors::monitorCount;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&maximizedWindows));
    } catch (std::exception &ex) {
        if (!errorState)
            PostMessage(globals::hWnd, WM_TASKBAR_THREAD_ERROR, 0, reinterpret_cast<LPARAM>(new std::string(ex.what())));
        errorState = true;
    }
    // ReSharper disable once CppDFAConstantConditions
    if (collectWindowsInfo && canCollect) {
        if (previousWindows != windows || forceToCollect) {
            PostMessage(globals::hWnd, WM_UPDATE_GRID_REQUEST, 0, 0);
            previousWindows = windows;
            forceToCollect = false;
        }
        collectWindowsInfo = false;
        canCollect = false;
    }
    return maximizedWindows;
}

void taskbar::clearErrorState() {
    errorState = false;
}

bool taskbar::isCursorOverTaskbar(HWND &taskbarWindow, POINT &cursorPos) {
    GetCursorPos(&cursorPos);
    std::lock_guard lock(taskbarMutex);
    for (const auto &taskbar : taskbarHandles | std::views::values) {
        RECT taskbarRect;
        GetWindowRect(taskbar, &taskbarRect);
        if (PtInRect(&taskbarRect, cursorPos)) {
            taskbarWindow = taskbar;
            return true;
        }
    }
    return false;
}

void taskbar::setTaskbarVisibility(HWND taskbar, bool visible, bool hoveredOver) {
    const LONG_PTR style = GetWindowLongPtr(taskbar, GWL_EXSTYLE);
    // Force taskbar to be visible when it's paused.
    // Since findAllMaximizedWindows() can take some time to process, while it's being processed, user could pause it or switch sessions,
    // so in any case, make it visible
    if (!globals::taskbarLoopRunState) {
        visible = true;
        hoveredOver = false;
    }

    if (visible) {
        int opacity = config::opacityWhenShownInternal;
        if (hoveredOver)
            opacity = config::opacityWhenHoveredInternal;
        SetWindowLongPtr(taskbar, GWL_EXSTYLE, opacity == 0 ? style & ~WS_EX_LAYERED : style | WS_EX_LAYERED);
        SetLayeredWindowAttributes(taskbar, 0, opacity, LWA_ALPHA);
        ShowWindow(taskbar, SW_SHOW);
    } else {
        SetWindowLongPtr(taskbar, GWL_EXSTYLE, style | WS_EX_LAYERED);
        SetLayeredWindowAttributes(taskbar, 0, config::opacityWhenHiddenInternal, LWA_ALPHA);
        if (config::opacityWhenHiddenInternal == 0)
            ShowWindow(taskbar, SW_HIDE);
    }
}

void taskbar::resetTaskbar() {
    std::lock_guard lock(taskbarMutex);
    for (HWND hwnd : taskbarHandles | std::views::values) {
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        ShowWindow(hwnd, SW_SHOW);
    }
}

void taskbar::updateTaskbarState() {
    HWND hoveredTaskbar;
    for (auto i = 0; i < monitors::monitorCount; i++) {
        const auto hMonitor = monitors::indexedMonitors[i];
        if (const auto state = taskbarForcedVisibilityStates.find(hMonitor); state != taskbarForcedVisibilityStates.end()) {
            if (state->second) {
                setTaskbarVisibility(taskbarHandles[hMonitor], true, false);
                return;
            }
        }
    }
    if (POINT cursorPos; isCursorOverTaskbar(hoveredTaskbar, cursorPos)) {
        setTaskbarVisibility(hoveredTaskbar, true, true);
        return;
    }
    const auto windows = findAllMaximizedWindows();
    std::lock_guard lock(taskbarMutex);
    for (auto &[hMonitor, taskbar] : taskbarHandles)
        setTaskbarVisibility(taskbar, windows.contains(hMonitor), false);
}

void taskbar::checkForAutoCollect() {
    if (collectWindowsInfo || !config::autoUpdate)
        return;
    static DWORD lastTick = GetTickCount();
    // Make it only enable if passed time was 1 second, otherwise the window could lag a lot
    if (const DWORD currentTick = GetTickCount(); currentTick - lastTick >= 1000) {
        collectWindowsInfo = true;
        lastTick = currentTick;
    }
}