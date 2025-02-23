#include "globals.h"

#include "taskbar.h"

std::string globals::exe;
std::string globals::args = "";
HWND globals::hWnd = nullptr;
bool globals::noConfigFile = false;
bool globals::taskbarLoopRunState = true;