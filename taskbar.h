#ifndef TASKBAR_H
#define TASKBAR_H

#include <dwmapi.h>

class taskbar {
public:
    static bool wasDebugFlushed;
    static bool canUpdateDebugMessages;
    static const int debug_columns_total_width;

    static HWND getTaskbarHandle();
    static void setTaskbarVisibility(bool visible);
    static void resetTaskbar();
    static void updateTaskbarState();
private:
    static bool isAnyWindowMaximized();
    static bool isCursorOverTaskbar();
};

#endif //TASKBAR_H
