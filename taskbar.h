#ifndef TASKBAR_H
#define TASKBAR_H

#include <dwmapi.h>

class taskbar {
public:
    static bool debug;
    static HWND getTaskbarHandle();
    static void setTaskbarVisibility(bool visible);
    static void resetTaskbar();
    static void updateTaskbarState();
private:
    static bool isAnyWindowMaximized();
    static bool isCursorOverTaskbar();
};

#endif //TASKBAR_H
