#include "globals.h"
#include "taskbar.h"

LPCSTR globals::exe;
HWND globals::hWnd = nullptr;
bool globals::noConfigFile = false;
bool globals::taskbarLoopRunState = true;