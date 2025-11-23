#ifndef GLOBALS_H
#define GLOBALS_H

#include <atomic>
#include <string>

#include "taskbar.h"
#include "utils.h"

class globals {
public:
    static DWORD sysBuildNumber;
    static std::wstring exe;
    static std::wstring args;
    static HWND hWnd;
    static HINSTANCE hIns;
    static std::atomic<bool> noConfigFile;
    static std::atomic<bool> taskbarLoopRunState;
    static std::atomic<bool> sessionLocked;
    static std::atomic<bool> isShuttingDown;
    static const utils::pair<const std::wstring_view, const DWORD> cloakedBitmasks[3];
};

#endif //GLOBALS_H
