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

void taskbarLoop() {
    bool called = false;
    while (true) {
        if (globals::hWnd == nullptr)
            break;
        if (!globals::taskbarLoopRunState) {
            if (!called) {
                taskbar::resetTaskbar();
                called = true;
            }
            continue;
        }
        taskbar::updateTaskbarState();
        Sleep(config::taskbarUpdateInterval);
        called = false;
    }
}

std::thread taskbarLoopThread;

void cleanup(bool clearIconAndMutex) {
    globals::taskbarLoopRunState = false;
    Shell_NotifyIconA(NIM_DELETE, &nid);
    if (globals::hWnd != nullptr)
        DestroyWindow(globals::hWnd);
    globals::hWnd = nullptr;
    if (taskbarLoopThread.joinable())
        taskbarLoopThread.join();
    if (clearIconAndMutex) {
        DestroyIcon(hIcon);
        CloseHandle(hMutex);
    }
    taskbar::resetTaskbar();
}

BOOL WINAPI ConsoleHandler(const DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_LOGOFF_EVENT || signal == CTRL_SHUTDOWN_EVENT) {
        if (config::debug && !config::keepConsoleWindowOpen)
            utils::toggleConsoleWindow(nullptr, false);
        bool wasNull = globals::hWnd == nullptr;
        cleanup(false);
        if (config::keepConsoleWindowOpen)
            std::cerr << std::endl << "> Do CTRL + C again to exit." << std::endl;
        if (signal != CTRL_C_EVENT || wasNull) {
            utils::toggleConsoleWindow(nullptr, false);
            DestroyIcon(hIcon);
            CloseHandle(hMutex);
            exit(0); // Just making sure the application exists completely
        }
    }
    return TRUE;
}

LRESULT CALLBACK WindowProc(HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) {
    switch (uMsg) {
        case WM_TRAY_ICON:
            if (lParam == WM_RBUTTONUP) {
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
                    if (config::debug)
                        utils::toggleConsoleWindow(ConsoleHandler, false);
                    exit(0);
                case ID_TRAY_OPEN_CONFIG:
                    config::open();
                    break;
                case ID_TRAY_RELOAD_CONFIG:
                    config::load();
                    utils::toggleConsoleWindow(ConsoleHandler, config::debug);
                    break;
                case ID_TRAY_PAUSE_HIDER:
                    globals::taskbarLoopRunState = !globals::taskbarLoopRunState;
                    break;
                case ID_TRAY_ADD_REMOVE_STARTUP:
                    utils::toggleStartup();
                    break;
                case ID_TRAY_ATTACH_DEBUG_CONSOLE:
                    config::debug = !config::debug;
                    utils::toggleConsoleWindow(ConsoleHandler, config::debug);
                    break;
                case ID_TRAY_GITHUB:
                    ShellExecuteA(nullptr, nullptr, PRODUCT_URL, nullptr, nullptr, SW_SHOWDEFAULT);
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
        crashInfo << utils::NTStatusMessageToText(pException->ExceptionRecord->ExceptionCode) << std::endl;
        crashInfo << "Exception Code: 0x" << std::hex << pException->ExceptionRecord->ExceptionCode << std::endl;
        crashInfo << "Fault Address: " << pException->ExceptionRecord->ExceptionAddress << std::endl;
    });
    taskbar::resetTaskbar();
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(const int argc, char* argv[]) {
    atexit([] {
        cleanup(true);
        MessageBoxA(globals::hWnd, "Application was closed.", PROJECT_NAME, MB_ICONINFORMATION | MB_OK);
    });
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    std::string exe = std::string(argv[0]);
    globals::exe = exe.substr(exe.find_last_of("/\\") + 1);
    SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(CrashHandler));
    try {
        if (!utils::processArguments(argc, argv))
            return 0;

        hMutex = CreateMutex(nullptr, TRUE, PROJECT_NAME);
        if (!hMutex) {
            std::cerr << "Internal error: Failed to create mutex" << std::endl;
            utils::showExceptionMessageBox([](std::stringstream& crashInfo) {
                crashInfo << "Failed to create mutex, but the application can still be running." << std::endl;
                crashInfo << "Be aware that this will not prevent application process duplicates!";
            });
        }

        if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
            std::cerr << "Fatal error: Application is already running" << std::endl;
            const int reply = MessageBoxA(globals::hWnd, "Application is already running! Forcefully shutdown it?", PROJECT_NAME, MB_ICONQUESTION | MB_YESNO);
            if (reply == 6) {
                DWORD currentPid = GetCurrentProcessId();
                utils::killProcessByName((std::string(PROJECT_NAME) + ".exe").c_str(), currentPid);
                taskbar::resetTaskbar();
                return 0;
            }
            return 1;
        }
        if (!globals::noConfigFile)
            config::load();

        if (config::debug)
            utils::toggleConsoleWindow(ConsoleHandler, true);

        const auto className = (std::string(PROJECT_NAME) + "_TrayIcon").c_str();
        WNDCLASSA wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = className;
        RegisterClassA(&wc);

        globals::hWnd = CreateWindowA(className, PROJECT_NAME, 0, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

        hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_APP_ICON));
        if (!hIcon) {
            std::cerr << "Fatal error: Failed to load icon" << std::endl;
            utils::showExceptionMessageBox([](std::stringstream& crashInfo) {
                crashInfo << "Failed to load icon";
            });
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
            std::cerr << "Fatal error: Failed to create system tray icon" << std::endl;
            utils::showExceptionMessageBox([](std::stringstream& crashInfo) {
                crashInfo << "Failed to create system tray icon";
            });
            return 1;
        }

        taskbarLoopThread = std::thread(taskbarLoop);

        MSG msg;
        while (GetMessageA(&msg, globals::hWnd, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        cleanup(true);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        utils::showExceptionMessageBox([&e](std::stringstream& crashInfo) {
            crashInfo << e.what() << std::endl;
        });
        taskbar::resetTaskbar();
    }
}