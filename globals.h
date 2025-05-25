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
    static std::atomic<bool> noConfigFile;
    static std::atomic<bool> taskbarLoopRunState;
    static std::atomic<bool> sessionLocked;
    static std::atomic<bool> isShuttingDown;
};

#endif //GLOBALS_H
