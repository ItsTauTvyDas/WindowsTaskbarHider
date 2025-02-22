#ifndef GLOBALS_H
#define GLOBALS_H
#include "taskbar.h"

class globals {
public:
    static LPCSTR exe;
    static HWND hWnd;
    static bool noConfigFile;
    static bool taskbarLoopRunState;
};

#endif //GLOBALS_H
