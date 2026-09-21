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
HFONT *globals::hDefaultFontP;
HFONT *globals::hDefaultFontBoldP;
const utils::pair<const std::wstring_view, const DWORD> globals::cloakedBitmasks[3] = {
    {L"app", DWM_CLOAKED_APP},
    {L"shell", DWM_CLOAKED_SHELL},
    {L"inherited", DWM_CLOAKED_INHERITED}
};