#include "taskbar.h"

#include <iostream>
#include <windows.h>
#include <ranges>

#include "config.h"
#include "globals.h"
#include "monitors.h"
#include "resources.h"
#include "taskbar_animation.h"
#include "utils.h"

std::atomic<bool> errorState;
std::atomic<bool> collectWindowsInfo;
std::atomic<bool> ignorePreviousWindowsCheck;
std::atomic<bool> ignoreGUIUpdateChecks;
std::atomic<bool> clearForcedVisibilityStatesBool;

std::unordered_map<HMONITOR, bool> taskbar::taskbarForcedVisibilityStates;
std::unordered_map<HMONITOR, HWND> taskbar::taskbarHandles;

std::thread taskbar::updateThread;
std::mutex taskbar::taskbarMutex;

void taskbarLoop() {
    bool called = false;
    while (true) {
        if (globals::isShuttingDown)
            break;
        if (!globals::taskbarLoopRunState) {
            if (!called) {
                taskbar::resetTaskbar();
                called = true;
            }
            continue;
        }
        if (clearForcedVisibilityStatesBool) {
            {
                std::lock_guard lock(taskbar::taskbarMutex);
                for (int i = 0; i < monitors::monitorCount; i++)
                    taskbar::taskbarForcedVisibilityStates[monitors::monitor(i)] = false;
            }
            clearForcedVisibilityStatesBool = false;
        }
        called = false;
        taskbar::updateTaskbarState();
        std::this_thread::sleep_for(std::chrono::milliseconds(config::taskbarUpdateInterval));
    }
}

void taskbar::initThread() {
    if (updateThread.joinable())
        return;
    updateThread = std::thread(taskbarLoop);
}

void taskbar::collectWindowData(const bool _ignorePreviousWindowsCheck, const bool _ignoreGUIUpdateChecks) {
    collectWindowsInfo = true;
    ignorePreviousWindowsCheck = _ignorePreviousWindowsCheck;
    ignoreGUIUpdateChecks = _ignoreGUIUpdateChecks;
}

void taskbar::findTaskbarHandles() {
    {
        std::lock_guard lock(taskbarMutex);
        taskbarHandles.clear();
    }
    EnumWindows([](HWND hWnd, const LPARAM) -> BOOL {
        std::wstring className(256, L'\0');
        if (const int len = GetClassNameW(hWnd, className.data(), static_cast<int>(className.size())); len > 0) {
            className.resize(len);
            const std::wstring& prefix = config::I_TaskbarWindowClassNameStarts;
            const std::wstring& suffix = config::I_TaskbarWindowClassNameEnds;
            if (className.find(prefix) != 0)
                return TRUE;
            if (className.size() >= suffix.size() && className.compare(className.size() - suffix.size(), suffix.size(), suffix) == 0) {
                WindowInfo wInfo { hWnd };
                wInfo.updateMonitor();
                const LONG_PTR style = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
                std::lock_guard lock(taskbarMutex);
                taskbarHandles[wInfo.hMonitor] = hWnd;
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
            bool temp;
            if (key == L"process" || key == L"p") {
                if (wInfo.procFilename.empty())
                    utils::getProcessInfo(wInfo.hWnd, wInfo.procFilename);
                std::wstring process = wInfo.procFilename;
                std::transform(process.begin(), process.end(), process.begin(), tolower);
                std::transform(value.begin(), value.end(), value.begin(), tolower);
                if (process == value)
                    succeededTags++;
            } else if (key == L"title" || key == L"t") {
                if (wInfo.title[0] == L'\0')
                    GetWindowTextW(wInfo.hWnd, wInfo.title, sizeof(wInfo.title));
                if (wInfo.title == value)
                    succeededTags++;
            } else if (key == L"focus" || key == L"f") {
                if (!wInfo.focused) {
                    WINDOWINFO wi;
                    wi.cbSize = sizeof(WINDOWINFO);
                    GetWindowInfo(wInfo.hWnd, &wi);
                    wInfo.focused = wi.dwWindowStatus == WS_ACTIVECAPTION;
                }
                if (static_cast<int>(wInfo.focused.value()) == utils::stoi(value.c_str(), &temp) && temp)
                    succeededTags++;
            } else if (key == L"class" || key == L"c") {
                if (wInfo.wndClass[0] == L'\0')
                    GetClassNameW(wInfo.hWnd, wInfo.wndClass, sizeof(wInfo.wndClass));
                if (std::wstring(wInfo.wndClass) == value)
                    succeededTags++;
            } else if (key == L"cloaked" || key == L"clk") {
                if (!wInfo.cloaked) {
                    DWORD cloaked;
                    if (const HRESULT hr = DwmGetWindowAttribute(wInfo.hWnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)); FAILED(hr)) {
                        // Fail silently?
                        goto end;
                    }
                    wInfo.cloaked = cloaked;
                }
                if (std::to_wstring(*wInfo.cloaked) == value || utils::hasBitmaskStrW(value, *wInfo.cloaked, globals::cloakedBitmasks, 3))
                    succeededTags++;
            } else if (key == L"monitor" || key == L"mon") {
                if (const int index = utils::stoi(value.c_str()); index >= 0 && index < monitors::monitorCount && monitors::monitor(index) == wInfo.hMonitor)
                    succeededTags++;
            } else if (key == L"maximized" || key == L"m") {
                if ((wp.showCmd == SW_MAXIMIZE) == (value == L"1"))
                    succeededTags++;
            } else if (key == L"left" || key == L"right" || key == L"top" || key == L"bottom") {
                if (!wInfo.rect) {
                    wInfo.rect.emplace();
                    GetWindowRect(wInfo.hWnd, &*wInfo.rect);
                }
                std::wstring rect;
                if (key == L"left")
                    rect = wInfo.rect->left;
                else if (key == L"right")
                    rect = wInfo.rect->right;
                else if (key == L"top")
                    rect = wInfo.rect->top;
                else if (key == L"bottom")
                    rect = wInfo.rect->bottom;
                else
                    continue;
                if (rect == value)
                    succeededTags++;
            }
        }
        end:
        if (succeededTags == tags.size()) {
            if (succeededTagGroup)
                *succeededTagGroup = tagGroup;
            return true;
        }
    }
    return false;
}

taskbar::WindowInfo taskbar::WindowInfo::reset() const {
    return { hWnd, hMonitor };
}

void taskbar::WindowInfo::updateMonitor() {
    hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
}

void taskbar::WindowInfo::updateValues() {
    WINDOWINFO wi; wi.cbSize = sizeof(WINDOWINFO); GetWindowInfo(hWnd, &wi);
    focused = wi.dwWindowStatus; // Focus status (0 or 1)
    utils::getProcessInfo(hWnd, procFilename); // Process filename
    GetWindowTextW(hWnd, title, sizeof(title)); // Title
    GetClassNameW(hWnd, wndClass, sizeof(wndClass)); // Class
    rect = wi.rcWindow; // Rect
    if (const HRESULT hr = DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)); FAILED(hr)) {
        cloaked = -2; // Fail silently?
    }
}

std::unordered_map<HMONITOR, taskbar::WindowInfo> taskbar::findAllMaximizedWindows() {
    checkForAutoCollect();

    static std::vector<WindowInfo> previousWindows;
    static std::vector<WindowInfo> windows;

    if (collectWindowsInfo)
        windows.clear();

    std::unordered_map<HMONITOR, WindowInfo> maximizedWindows;
    EnumWindowParam ewp = {};
    ewp.maximizedWindows = &maximizedWindows;
    ewp.collectWindowsInfo = collectWindowsInfo;
    ewp.ignorePreviousWindowsCheck = ignorePreviousWindowsCheck;
    ewp.collectAllWindows = config::showAllWindows;
    try {
        EnumWindows([](HWND hWnd, const LPARAM lParam) -> BOOL {
            EnumWindowParam ewp = *reinterpret_cast<EnumWindowParam*>(lParam);

            WINDOWPLACEMENT wp;
            wp.length = sizeof(WINDOWPLACEMENT);

            if (!GetWindowPlacement(hWnd, &wp) || !IsWindowVisible(hWnd) || IsIconic(hWnd))
                return TRUE;

            WindowInfo wInfo = {};
            if (config::alwaysIgnoreWhenNotMaximized && wp.showCmd != SW_MAXIMIZE) {
                if (ewp.collectWindowsInfo)
                    wInfo.initiallyIgnored = true;
                else
                    return TRUE;
            }
            wInfo.hWnd = hWnd;
            wInfo.updateMonitor();
            if (const auto taskbar = taskbarHandles.find(wInfo.hMonitor); taskbar != taskbarHandles.end() && taskbarHandles[wInfo.hMonitor] == hWnd)
                return TRUE;
            wInfo.maximized = wp.showCmd == SW_MAXIMIZE;

            std::wstring succeededIgnoreTagGroup, succeededExceptionTagGroup;
            if (ewp.collectWindowsInfo)
                wInfo.updateValues();

            if (!wInfo.initiallyIgnored) {
                if (!config::ignoredWindows.empty())
                    wInfo.detected = loopThroughWindowTags(config::ignoredWindows, wInfo, wp, &succeededIgnoreTagGroup);
                if (!config::exceptionalWindows.empty())
                    wInfo.wasExceptional = loopThroughWindowTags(config::exceptionalWindows, wInfo, wp, &succeededExceptionTagGroup);
                wInfo.detected = (wInfo.wasExceptional || !wInfo.detected) && !wInfo.initiallyIgnored;
            }

            if (ewp.collectWindowsInfo) {
                wInfo.fault = wInfo.wasExceptional ? succeededExceptionTagGroup : succeededIgnoreTagGroup;
                wInfo.hWnd = nullptr;
                windows.push_back(wInfo);
            }

            if (wInfo.detected) {
                (*ewp.maximizedWindows)[wInfo.hMonitor] = wInfo;
                // only continue cycling through windows if we are collecting them or if we didn't have collected enough windows (at least one per monitor)
                return ewp.collectAllWindows || ewp.maximizedWindows->size() != monitors::monitorCount;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ewp));
    } catch (std::exception &ex) {
        if (!errorState)
            PostMessage(globals::hWnd, WM_TASKBAR_THREAD_ERROR, 0, reinterpret_cast<LPARAM>(new std::string(ex.what())));
        errorState = true;
    }

    if (ewp.collectWindowsInfo) {
        const int size = static_cast<int>(ewp.maximizedWindows->size());
        // some monitor can have none of the windows maximized, and it causes a bug where all windows are displayed in debug table
        if (!windows.empty() && ewp.collectWindowsInfo && !ewp.collectAllWindows && size > 0 && size!= monitors::monitorCount)
            while (!windows.empty() && !windows.back().detected)
                windows.pop_back();
        collectWindowsInfo = false;
    }
    if (previousWindows != windows || ewp.ignorePreviousWindowsCheck) {
        auto windowsCopy = new std::vector(windows);
        const auto lParam = windows.size() << 1 | static_cast<UINT_PTR>(ignoreGUIUpdateChecks);
        previousWindows = windows;
        ignorePreviousWindowsCheck = false;
        ignoreGUIUpdateChecks = false;
        PostMessage(globals::hWnd, WM_UPDATE_GRID_REQUEST, reinterpret_cast<WPARAM>(windowsCopy), static_cast<LPARAM>(lParam));
    }
    return maximizedWindows;
}

void taskbar::clearErrorState() {
    errorState = false;
}

bool taskbar::isCursorOverTaskbar(HWND &taskbarWindow, const POINT &cursorPos) {
    std::unordered_map<HMONITOR, HWND> _taskbarHandles;
    {
        std::lock_guard lock(taskbarMutex);
        _taskbarHandles = taskbarHandles;
    }
    if (taskbarWindow != nullptr) {
        RECT taskbarRect;
        GetWindowRect(taskbarWindow, &taskbarRect);
        return PtInRect(&taskbarRect, cursorPos);
    }
    // Unused
    for (const auto &taskbar : _taskbarHandles | std::views::values) {
        RECT taskbarRect;
        GetWindowRect(taskbar, &taskbarRect);
        if (PtInRect(&taskbarRect, cursorPos)) {
            taskbarWindow = taskbar;
            return true;
        }
    }
    return false;
}

void taskbar::setTaskbarVisibility(HWND taskbar, bool visible, bool hoveredOver, const bool causedByMaximizedWindow) {
    // Force taskbar to be visible when it's paused.
    // Since findAllMaximizedWindows() can take some time to process, while it's being processed, user could pause it or switch sessions,
    // so in any case, make it visible
    if (!globals::taskbarLoopRunState) {
        visible = true;
        hoveredOver = false;
    }

    if (visible) {
        const int opacity = hoveredOver ? config::opacityWhenHoveredInternal : config::opacityWhenShownInternal;
        ShowWindow(taskbar, SW_SHOW);

        if (/*hoveredOver && */config::animationsEnabled) {
            taskbar_animation::animate(taskbar, causedByMaximizedWindow);
            return;
        }
        SetLayeredWindowAttributes(taskbar, 0, opacity, LWA_ALPHA);
        ShowWindow(taskbar, SW_SHOW);
    } else {
        if (config::animationsEnabled) {
            taskbar_animation::animate(taskbar, causedByMaximizedWindow);
            return;
        }
        SetLayeredWindowAttributes(taskbar, 0, config::opacityWhenHiddenInternal, LWA_ALPHA);
        if (config::opacityWhenHiddenInternal == 0)
            ShowWindow(taskbar, SW_HIDE);
    }
}

void taskbar::resetTaskbar() {
    std::lock_guard lock(taskbarMutex);
    for (HWND hWnd : taskbarHandles | std::views::values) {
        // SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, GetWindowLongPtr(hWnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
        ShowWindowAsync(hWnd, SW_SHOW);
    }
}

void taskbar::resumeTaskbar() {
    std::lock_guard lock(taskbarMutex);
    for (HWND hWnd : taskbarHandles | std::views::values) {
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, GetWindowLongPtr(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);
        ShowWindowAsync(hWnd, SW_SHOW);
    }
}

void taskbar::updateTaskbarState() {
    std::unordered_map<HMONITOR, HWND> _taskbarHandles;
    {
        std::lock_guard taskbarLock(taskbarMutex);
        _taskbarHandles = taskbarHandles;
    }
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    const auto windows = findAllMaximizedWindows();
    for (auto &[hMonitor, taskbar] : _taskbarHandles) {
        const bool contains = windows.contains(hMonitor);
        if (const auto state = taskbarForcedVisibilityStates.find(hMonitor); state != taskbarForcedVisibilityStates.end() && state->second) {
            setTaskbarVisibility(taskbar, true, false, true);
            continue;
        }
        setTaskbarVisibility(taskbar, contains, isCursorOverTaskbar(taskbar, cursorPos), contains);
    }
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

void taskbar::clearForcedVisibilityStates() {
    clearForcedVisibilityStatesBool = true;
}
