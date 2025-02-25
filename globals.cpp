#include "globals.h"

#include "taskbar.h"

std::wstring globals::exe;
std::wstring globals::args = L"";
HWND globals::hWnd = nullptr;
HINSTANCE globals::hIns = nullptr;
bool globals::noConfigFile = false;
bool globals::taskbarLoopRunState = true;