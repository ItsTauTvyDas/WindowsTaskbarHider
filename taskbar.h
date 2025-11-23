#ifndef TASKBAR_H
#define TASKBAR_H

#include <cstring>
#include <dwmapi.h>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

class taskbar {
public:
    struct WindowInfo {
        HWND hWnd = nullptr;
        HMONITOR hMonitor = nullptr;
        bool maximized = false;
        bool detected = false;
        bool wasExceptional = false;
        bool initiallyIgnored = false;
        std::optional<bool> focused = -1;
        std::optional<DWORD> cloaked = -1;
        wchar_t wndClass[256] = {};
        std::optional<RECT> rect;
        wchar_t title[256] = {};
        std::wstring fault;
        std::wstring procFilename;

        [[nodiscard]] WindowInfo reset() const;
        void updateMonitor();
        void updateValues();

        bool operator==(const WindowInfo& o) const {
            return maximized                         == o.maximized &&
                   detected                          == o.detected &&
                   hWnd                              == o.hWnd &&
                   hMonitor                          == o.hMonitor &&
                   wasExceptional                    == o.wasExceptional &&
                   focused                           == o.focused &&
                   initiallyIgnored                  == o.initiallyIgnored &&
                   std::wcscmp(wndClass, o.wndClass) == 0 &&
                   rect.has_value()                  == o.rect.has_value() &&
                   (
                       !rect.has_value() || std::memcmp(&rect.value(), &o.rect.value(), sizeof(RECT)) == 0
                   ) &&
                   std::wcscmp(title, o.title)       == 0 &&
                   fault                             == o.fault &&
                   procFilename                      == o.procFilename;
        }
    };

    static std::mutex taskbarMutex;
    static std::unordered_map<HMONITOR, bool> taskbarForcedVisibilityStates;
    static std::unordered_map<HMONITOR, HWND> taskbarHandles;
    static std::thread updateThread;

    static void initThread();
    static void findTaskbarHandles();
    static void setTaskbarVisibility(HWND hWnd, bool visible, bool hoveredOver, bool causedByMaximizedWindow);
    static void resetTaskbar();
    static void resumeTaskbar();
    static void updateTaskbarState();
    static void clearErrorState();
    static void clearForcedVisibilityStates();
    static void collectWindowData(bool _ignorePreviousWindowsCheck, bool _ignoreGUIUpdateChecks);

private:
    struct EnumWindowParam {
        std::unordered_map<HMONITOR, WindowInfo> *maximizedWindows;
        bool collectWindowsInfo;
        bool ignorePreviousWindowsCheck;
        bool collectAllWindows;
    };

    static std::unordered_map<HMONITOR, WindowInfo> findAllMaximizedWindows();
    static bool isCursorOverTaskbar(HWND &taskbarWindow, const POINT &cursorPos);
    static void checkForAutoCollect();
};

#endif //TASKBAR_H
