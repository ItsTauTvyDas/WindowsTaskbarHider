#include "taskbar.h"
#include "utils.h"
#include "config.h"
#include "globals.h"
#include "resources.h"
#include <dwmapi.h>
#include <iostream>
#include <sstream>
#include <thread>

NOTIFYICONDATAA nid;
HANDLE hMutex;
HICON hIcon;

#define DEBUG_MESSAGES_UPDATE_INTERVAL 10

bool quitting = false;

void taskbarLoop() {
    bool called = false;
    int debugCounter = 0;
    while (true) {
        if (quitting)
            return;
        if (!globals::taskbarLoopRunState) {
            if (quitting)
                return;
            if (!called) {
                taskbar::resetTaskbar();
                called = true;
            }
            continue;
        }
        taskbar::updateTaskbarState();
        for (auto i = 1; i <= config::taskbarUpdateInterval; i++) {
            if (quitting)
                return;
            debugCounter++;
            Sleep(1);
        }
        called = false;
        if (debugCounter >= DEBUG_MESSAGES_UPDATE_INTERVAL) {
            taskbar::canUpdateDebugMessages = true;
            debugCounter = 0;
        }
    }
}

std::thread taskbarLoopThread;

void cleanup(const bool clearIconAndMutex) {
    if (quitting) {
        MessageBoxA(globals::hWnd, "Cleanup called again, ignoring.", PROJECT_NAME, MB_ICONWARNING | MB_OK);
        return;
    }
    quitting = true;
    globals::taskbarLoopRunState = false;
    if (taskbarLoopThread.joinable())
        taskbarLoopThread.join();
    Shell_NotifyIconA(NIM_DELETE, &nid);
    if (globals::hWnd != nullptr)
        DestroyWindow(globals::hWnd);
    globals::hWnd = nullptr;
    if (clearIconAndMutex) {
        DestroyIcon(hIcon);
        CloseHandle(hMutex);
    }
    taskbar::resetTaskbar();
}

BOOL WINAPI ConsoleHandler(const DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_LOGOFF_EVENT || signal == CTRL_SHUTDOWN_EVENT) {
        if (!config::keepConsoleWindowOpen) {
            utils::toggleConsoleWindow(nullptr, false);
            cleanup(true);
            exit(0);
        }
        const bool wasNull = globals::hWnd == nullptr;
        if (!wasNull) {
            cleanup(false);
            while (config::debug) {
                // Prevent console from being cleared
                if (taskbar::wasDebugFlushed)
                    break;
            }
        }
        std::cerr << std::endl << "> Do CTRL + C again to exit." << std::endl;
        if (signal != CTRL_C_EVENT || wasNull) {
            utils::toggleConsoleWindow(nullptr, false);
            DestroyIcon(hIcon);
            CloseHandle(hMutex);
            exit(0);
        }
    }
    return TRUE;
}

LRESULT CALLBACK WindowProc(HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) {
    bool debugStatus;
    switch (uMsg) {
        case WM_TRAY_ICON:
            if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
                HMENU hMenu = CreatePopupMenu();
                AppendMenuA(hMenu, MF_STRING | MF_DISABLED, ID_TRAY_HEADER, (std::string(VER_FILEDESCRIPTION_STR) + " " + std::string(VER_FILEVERSION_STR)).c_str());
                AppendMenuA(hMenu, MF_STRING, ID_TRAY_OPEN_CONFIG, "Open config file");
                AppendMenuA(hMenu, MF_STRING, ID_TRAY_RELOAD_CONFIG, "Reload config");
                AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuA(hMenu, MF_STRING, ID_TRAY_PAUSE_HIDER, globals::taskbarLoopRunState ? "Pause" : "Resume");
                AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuA(hMenu, MF_STRING, ID_TRAY_ADD_REMOVE_STARTUP, utils::doesAutoStart() ? "Remove from startup" : "Add to startup");
                AppendMenuA(hMenu, MF_STRING, ID_TRAY_ATTACH_DEBUG_CONSOLE, config::debug ? "Detach console (debug)" : "Attach console (debug)");
                AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuA(hMenu, MF_STRING, ID_TRAY_GITHUB, "Open GitHub page");
                AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuA(hMenu, MF_STRING, ID_TRAY_EXIT, "Exit");
                POINT p;
                GetCursorPos(&p);
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, p.x, p.y, 0, hwnd, nullptr);
                DestroyMenu(hMenu);
            }
            break;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_TRAY_EXIT:
                    if (quitting) {
                        MessageBoxA(globals::hWnd, "Seemed like the application was frozen, terminating.", PROJECT_NAME, MB_ICONWARNING | MB_OK);
                        exit(0);
                    }
                    PostQuitMessage(0);
                    break;
                case ID_TRAY_OPEN_CONFIG:
                    config::open();
                    break;
                case ID_TRAY_RELOAD_CONFIG:
                    debugStatus = config::debug;
                    config::load();
                    if (debugStatus == config::debug)
                        break;
                    debugStatus = config::debug;
                    if (!utils::toggleConsoleWindow(ConsoleHandler, debugStatus))
                        config::debug = false;
                    break;
                case ID_TRAY_PAUSE_HIDER:
                    globals::taskbarLoopRunState = !globals::taskbarLoopRunState;
                    break;
                case ID_TRAY_ADD_REMOVE_STARTUP:
                    utils::toggleStartup();
                    break;
                case ID_TRAY_ATTACH_DEBUG_CONSOLE:
                    debugStatus = !config::debug;
                    if (!utils::toggleConsoleWindow(ConsoleHandler, debugStatus))
                        config::debug = false;
                    else
                        config::debug = debugStatus;
                    break;
                case ID_TRAY_GITHUB:
                    ShellExecuteA(nullptr, "open", PRODUCT_URL, nullptr, nullptr, SW_SHOWNORMAL);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LONG WINAPI CrashHandler(const EXCEPTION_POINTERS* pException) {
    utils::showExceptionMessageBox([pException](std::stringstream& crashInfo) {
        const EXCEPTION_RECORD* record = pException->ExceptionRecord;
        auto lpstr = utils::NTStatusMessageToText(record->ExceptionCode);
        // A workaround, EXCEPTION_ACCESS_VIOLATION returns this message:
        // "The instruction at 0xp referenced memory at 0xp. The memory could not be s."
        // There are missing %, but p and s letters are not being used in any words, so we can just replace them
        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
            std::string operation;
            switch (record->ExceptionInformation[0]) {
                case 0: operation = "read"; break;
                case 1: operation = "written"; break;
                case 8: operation = "execute (DEP)"; break;
                default:
                    operation = "unknown(" + std::to_string(record->ExceptionInformation[0]) + ")";
                    break;
            }
            lpstr = utils::replaceCharacterWithText(lpstr, 's', operation, 1);
            lpstr = utils::replaceCharacterWithText(lpstr, 'p', std::to_string(record->ExceptionInformation[1]));
            lpstr = utils::replaceCharacterWithText(lpstr, 'p', std::to_string(record->ExceptionInformation[2]));
        } else if (record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {
            // I hope these are correct
            // Message: "The instruction at 0xp referenced memory at 0xp. The required data was not placed into memory because of an I/O error status of 0xx."
            lpstr = utils::replaceCharacterWithText(lpstr, 'p', std::to_string(record->ExceptionInformation[0]));
            lpstr = utils::replaceCharacterWithText(lpstr, 'p', std::to_string(record->ExceptionInformation[1]));
            lpstr = utils::replaceCharacterWithText(lpstr, 'x', std::to_string(record->ExceptionInformation[2]), 3);
        }
        crashInfo << "Application will be terminated." << std::endl;
        crashInfo << "Translated Exception: " << utils::exceptionName(record->ExceptionCode) << std::endl;
        crashInfo << std::endl;
        crashInfo << lpstr << std::endl;
        crashInfo << "Exception Information:" << std::endl;
        crashInfo << "  Code: 0x" << std::hex << record->ExceptionCode << std::endl;
        crashInfo << "  Address: " << record->ExceptionAddress << std::endl;
    }, true);
    taskbar::resetTaskbar();
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(const int argc, char* argv[]) {
    for (auto i = 1; i < argc; i++)
        globals::args += argv[i];
    SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(CrashHandler));
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    const auto exe = std::string(argv[0]);
    globals::exe = exe.substr(exe.find_last_of("/\\") + 1);

    if (!utils::processArguments(argc, argv))
        return 0;

    hMutex = CreateMutex(nullptr, TRUE, PROJECT_NAME);
    if (!hMutex) {
        utils::showExceptionMessageBox([](std::stringstream& crashInfo) {
            crashInfo << "Failed to create mutex, but the application can still continue.";
        }, false);
    }

    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (MessageBoxA(globals::hWnd,
            "Application is already running! Forcefully shutdown it?",
            PROJECT_NAME, MB_ICONQUESTION | MB_YESNO) == 6) {
            utils::killProcessByName(globals::exe.c_str(), GetCurrentProcessId());
            taskbar::resetTaskbar();
            return 0;
        }
        return 1;
    }

    // In case when mutex fails
    if (utils::killProcessByName(globals::exe.c_str(), GetCurrentProcessId())) {
        if (MessageBoxA(globals::hWnd,
            "Application seemed to be already running, but we forcefully shutdown it. "
            "Do you want to continue and run this application again?",
            PROJECT_NAME, MB_ICONQUESTION | MB_YESNO) == 7) {
            exit(0);
        }
    }

    const auto className = (std::string(PROJECT_NAME) + "_TrayIcon").c_str();
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = className;
    RegisterClassA(&wc);

    globals::hWnd = CreateWindowA(className, PROJECT_NAME, 0, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

    hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_APP_ICON));
    if (!hIcon) {
        utils::showExceptionMessageBox([](std::stringstream& crashInfo) {
            crashInfo << "Failed to load icon: {}";
        }, true);
        return 1;
    }

    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = globals::hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY_ICON;
    nid.hIcon = hIcon;
    strcpy_s(nid.szTip, VER_FILEDESCRIPTION_STR);

    if (!Shell_NotifyIconA(NIM_ADD, &nid)) {
        utils::showExceptionMessageBox([](std::stringstream& crashInfo) {
            crashInfo << "Failed to create system tray icon: {}";
        }, true);
        return 1;
    }

    atexit([] {
        if (config::debug)
            utils::toggleConsoleWindow(ConsoleHandler, false);
        MessageBoxA(globals::hWnd, "Application was closed.", PROJECT_NAME, MB_ICONINFORMATION | MB_OK);
    });

    if (!globals::noConfigFile)
        config::load();

    if (config::debug) {
        if (!utils::toggleConsoleWindow(ConsoleHandler, true))
            config::debug = false;
    }

    taskbarLoopThread = std::thread(taskbarLoop);

    MSG msg;
    while (GetMessageA(&msg, globals::hWnd, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    cleanup(true);
    return 0;
}