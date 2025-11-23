#include "globals.h"

DWORD globals::sysBuildNumber;
std::wstring globals::exe;
std::wstring globals::args;
HWND globals::hWnd = nullptr;
HINSTANCE globals::hIns = nullptr;
std::atomic<bool> globals::noConfigFile;
std::atomic<bool> globals::taskbarLoopRunState = true;
std::atomic<bool> globals::sessionLocked;
std::atomic<bool> globals::isShuttingDown;