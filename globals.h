#ifndef GLOBALS_H
#define GLOBALS_H

#include "taskbar.h"
#include <string>

class globals {
public:
    static std::wstring exe;
    static std::wstring args;
    static HWND hWnd;
    static HINSTANCE hIns;
    static bool noConfigFile;
    static bool taskbarLoopRunState;
    static bool sessionLocked;
    static bool isShuttingDown;
};

#endif //GLOBALS_H
