#ifndef GLOBALS_H
#define GLOBALS_H

#include "taskbar.h"
#include <string>

class globals {
public:
    static std::string exe;
    static std::string args;
    static HWND hWnd;
    static bool noConfigFile;
    static bool taskbarLoopRunState;
};

#endif //GLOBALS_H
