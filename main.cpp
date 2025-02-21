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

bool running = true;
bool quitting = false;

void taskbarLoop() {
    bool called = false;
    while (true) {
        if (quitting || globals::hWnd == nullptr)
            break;
        if (!running) {
            if (!called) {
                taskbar::resetTaskbar();
                called = true;
            }
            continue;
        }
        taskbar::updateTaskbarState();
        Sleep(config::taskbarUpdateInterval);
        called  = false;
    }
}

void quit() {
    quitting = true;
    running = false;
    Shell_NotifyIconA(NIM_DELETE, &nid);
    PostQuitMessage(0);
    if (globals::hWnd)
        DestroyWindow(globals::hWnd);
    globals::hWnd = nullptr;
    taskbar::resetTaskbar();
}

BOOL WINAPI ConsoleHandler(const DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_LOGOFF_EVENT || signal == CTRL_SHUTDOWN_EVENT)
        quit();
    return TRUE;
}

LRESULT CALLBACK WindowProc(HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) {
    switch (uMsg) {
        case WM_TRAY_ICON:
            if (lParam == WM_RBUTTONUP) {
                HMENU hMenu = CreatePopupMenu();
                AppendMenuA(hMenu, MF_STRING, ID_TRAY_OPEN_CONFIG, "Open config file");
                AppendMenuA(hMenu, MF_STRING, ID_TRAY_RELOAD_CONFIG, "Reload config");
                AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuA(hMenu, MF_STRING, ID_TRAY_PAUSE_HIDER, running ? "Pause" : "Resume");
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
                    quit();
                break;
                case ID_TRAY_OPEN_CONFIG:
                    config::open();
                break;
                case ID_TRAY_RELOAD_CONFIG:
                    config::load();
                break;
                case ID_TRAY_PAUSE_HIDER:
                    running = !running;
                break;
                default:
                    break;
            }
            break;
        case WM_CLOSE:
            quit();
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
    globals::exe = argv[0];
    SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(CrashHandler));
    try {
        if (!utils::processArguments(argc, argv))
            return 0;
        HANDLE hMutex = CreateMutex(nullptr, TRUE, PROJECT_NAME);
        if (!hMutex) {
            std::cerr << "Internal error: Failed to create mutex" << std::endl;
            utils::showExceptionMessageBox([](std::stringstream& crashInfo) {
                crashInfo << "Failed to create mutex, but the application can still be running." << std::endl;
                crashInfo << "Be aware that this will not prevent application process duplicates!";
            });
        }

        if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
            std::cerr << "Fatal error: Application is already running" << std::endl;
            if (!globals::noMessageBoxes) {
                const int reply = MessageBoxA(globals::hWnd, "Application is already running! Forcefully shutdown it?", PROJECT_NAME, MB_ICONQUESTION | MB_YESNO);
                if (reply == 6) {
                    DWORD currentPid = GetCurrentProcessId();
                    utils::killProcessByName((std::string(PROJECT_NAME) + ".exe").c_str(), currentPid);
                    taskbar::resetTaskbar();
                    return 0;
                }
            }
            return 1;
        }
        if (!globals::noConfigFile)
            config::load();
        SetConsoleCtrlHandler(ConsoleHandler, TRUE);

        const auto className = "WindowsTaskbarHider_TrayIcon";
        WNDCLASSA wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = className;
        RegisterClassA(&wc);

        globals::hWnd = CreateWindowA(className, PROJECT_NAME, 0, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

        HICON hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_APP_ICON));
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
        strcpy_s(nid.szTip, PROJECT_NAME);

        if (!Shell_NotifyIconA(NIM_ADD, &nid)) {
            std::cerr << "Fatal error: Failed to create system tray icon" << std::endl;
            utils::showExceptionMessageBox([](std::stringstream& crashInfo) {
                crashInfo << "Failed to create system tray icon";
            });
            return 1;
        }

        std::thread taskbarLoopThread(taskbarLoop);

        MSG msg;
        while (GetMessageA(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        taskbarLoopThread.join();
        DestroyIcon(hIcon);
        CloseHandle(hMutex);
        taskbar::resetTaskbar();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        utils::showExceptionMessageBox([&e](std::stringstream& crashInfo) {
            crashInfo << e.what() << std::endl;
        });
        taskbar::resetTaskbar();
    }
}