#include "globals.h"

#include "taskbar.h"

std::string globals::exe;
HWND globals::hWnd = nullptr;
bool globals::noConfigFile = false;
bool globals::taskbarLoopRunState = true;