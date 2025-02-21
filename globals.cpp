#include "globals.h"
#include "taskbar.h"

LPCSTR globals::exe;
HWND globals::hWnd = nullptr;
bool globals::noMessageBoxes = false;
bool globals::noConfigFile = false;