#ifndef TASKBAR_H
#define TASKBAR_H

#include <dwmapi.h>
#include <string>
#include <vector>

class taskbar {
public:
    struct WindowInfo {
        HWND hwnd = nullptr;
        bool maximized = false;
        bool detected = false;
        bool wasExceptional = false;
        bool wasRectModified = false;
        bool finalDetection = false;
        bool initiallyIgnored = false;
        DWORD focused = -1;
        wchar_t wndClass[256] = {};
        RECT rect;
        wchar_t title[256] = {};
        std::wstring fault;
        std::wstring procFilename;

        bool operator==(const WindowInfo& o) const {
            return maximized                         == o.maximized &&
                   detected                          == o.detected &&
                   wasExceptional                    == o.wasExceptional &&
                   wasRectModified                   == o.wasRectModified &&
                   finalDetection                    == o.finalDetection &&
                   focused                           == o.focused &&
                   initiallyIgnored                  == o.initiallyIgnored &&
                   std::wcscmp(wndClass, o.wndClass) == 0 &&
                   rect.left                         == o.rect.left &&
                   rect.top                          == o.rect.top &&
                   rect.right                        == o.rect.right &&
                   rect.bottom                       == o.rect.bottom &&
                   std::wcscmp(title, o.title)       == 0 &&
                   fault                             == o.fault &&
                   procFilename                      == o.procFilename;
        }
    };

    static std::vector<WindowInfo> windows;
    static bool collectWindowsInfo;

    static HWND getTaskbarHandle();
    static void setTaskbarVisibility(bool visible, bool hoveredOver);
    static void resetTaskbar();
    static void updateTaskbarState();
    static void checkForAutoCollect();
private:
    static bool isAnyWindowMaximized();
    static bool isCursorOverTaskbar();
};

#endif //TASKBAR_H
