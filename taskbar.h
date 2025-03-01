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
    };

    static std::vector<WindowInfo> windows;
    static bool collectWindowsInfo;

    static HWND getTaskbarHandle();
    static void setTaskbarVisibility(bool visible, bool hoveredOver);
    static void resetTaskbar();
    static void updateTaskbarState();
private:
    static bool isAnyWindowMaximized();
    static bool isCursorOverTaskbar();
};

#endif //TASKBAR_H
