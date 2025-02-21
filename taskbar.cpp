#include "taskbar.h"

#include <iostream>
#include "config.h"
#include "utils.h"

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

bool taskbar::isAnyWindowMaximized() {
    bool maximized = false;
    if (config::debug)
        std::cout << "[DEBUG] Loop start" << std::endl;
    EnumWindows([](HWND hwnd, const LPARAM lParam) -> BOOL {
        if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;
        if (!IsWindowVisible(hwnd)) return TRUE;

        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);
        if (!GetWindowPlacement(hwnd, &wp) || wp.showCmd != SW_MAXIMIZE)
            return TRUE;
        const std::string processName = utils::getProcessName(hwnd);

        char title[256];
        GetWindowTextA(hwnd, title, sizeof(title));
        char wndclass[256];
        GetClassNameA(hwnd, wndclass, sizeof(wndclass));

        if (config::debug)
            std::cout << "[DEBUG] Found maximized window: " << title << " | Process: " << processName << " | Class: " << wndclass << std::endl;
        if (!config::ignoredWindows.empty())
            for (const auto& window: config::ignoredWindows) {
                bool skipped = false;
                if ("process:" + processName == window)
                    skipped = true;
                if ("title:" + std::string(title) == window)
                    skipped = true;
                if ("class:" + std::string(wndclass) == window)
                    skipped = true;
                if (skipped) {
                    if (config::debug)
                        std::cout << "[DEBUG] Skipping..." << std::endl;
                    return TRUE;
                }
             }
        *reinterpret_cast<bool*>(lParam) = true;
        return FALSE;
    }, reinterpret_cast<LPARAM>(&maximized));
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