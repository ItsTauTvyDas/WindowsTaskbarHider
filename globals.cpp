#include "globals.h"

#include "taskbar.h"

std::wstring globals::exe;
std::wstring globals::args;
HWND globals::hWnd = nullptr;
HINSTANCE globals::hIns = nullptr;
bool globals::noConfigFile;
bool globals::taskbarLoopRunState = true;
bool globals::sessionLocked;
bool globals::isShuttingDown;