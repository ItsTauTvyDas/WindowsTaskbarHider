#ifndef GLOBALS_H
#define GLOBALS_H
#include "taskbar.h"

class globals {
public:
    static LPCSTR exe;
    static HWND hWnd;
    static bool noMessageBoxes;
    static bool noConfigFile;
};

#endif //GLOBALS_H
