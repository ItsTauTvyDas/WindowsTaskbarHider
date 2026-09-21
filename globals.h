#ifndef GLOBALS_H
#define GLOBALS_H

#include <atomic>
#include <string>

#include "taskbar.h"
#include "utils.h"

#define EVENT_OBJECT_CLOAKED   0x8017
#define EVENT_OBJECT_UNCLOAKED 0x8018
#define WMC_SCROLLBAR          L"SCROLLBAR"
#define WMC_BUTTON             L"BUTTON"

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
    static HFONT* hDefaultFontP;
    static HFONT* hDefaultFontBoldP;
    static const utils::pair<const std::wstring_view, const DWORD> cloakedBitmasks[3];
};

#endif //GLOBALS_H
