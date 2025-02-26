#include "taskbar.h"
#include "utils.h"
#include "config.h"
#include "globals.h"
#include "resources.h"
#include <dwmapi.h>
#include <sstream>
#include <thread>
#include <numeric>

#include "language.h"

#define WM_TRAY_ICON           (WM_USER + 1)

#define WCP_BASE               config::darkMode ? darkColorPalette[0] : lightColorPalette[0]
#define WCP_FOREGROUND         config::darkMode ? darkColorPalette[1] : lightColorPalette[1]
#define WCP_BACKGROUND         config::darkMode ? darkColorPalette[2] : lightColorPalette[2]
#define WCP_BACKGROUND2        config::darkMode ? darkColorPalette[3] : lightColorPalette[3]
#define WCP_BUTTON_BG          config::darkMode ? darkColorPalette[4] : lightColorPalette[4]
#define WCP_BUTTON_BORDER      config::darkMode ? darkColorPalette[5] : lightColorPalette[5]
#define WCP_BUTTON_CLICKED_BG  config::darkMode ? darkColorPalette[6] : lightColorPalette[6]
#define WCP_SCROLLBAR_COLOR    config::darkMode ? darkColorPalette[7] : lightColorPalette[7]
#define WCP_SCROLLBAR_BG       config::darkMode ? darkColorPalette[8] : lightColorPalette[8]

#define WSC_HEADER             70
#define WSC_SCROLLBAR_WIDTH    20
#define WSC_GRID_Y             100
#define WSC_GRID_TOP_OFFSET    40
#define WSC_SCROLL_ROWS        1
#define WSC_MAX_SCROLL         (contentHeight - (windowClientHeight - WSC_HEADER) - WSC_GRID_Y)
#define WSC_VISIBLE_AREA       (windowClientHeight - WSC_HEADER)

constexpr COLORREF darkColorPalette[] = {
    RGB(  0,   0,   0), // Base color
    RGB(255, 255, 255), // Text color
    RGB( 30,  30,  30), // Background color
    RGB( 20,  20,  20), // Second background color
    RGB( 50,  50,  50), // Button background color
    RGB( 55,  55,  55), // Button border color
    RGB( 80,  80,  80), // Button clicked background color
    RGB( 80,  80,  80), // Scrollbar color
    RGB( 80,  80,  80), // Scrollbar background color
};

constexpr COLORREF lightColorPalette[] = {
    RGB(255, 255, 255), // Base color
    RGB(  0,   0,   0), // Text color
    RGB(255, 255, 255), // Background color
    RGB(240, 240, 240), // Second background color
    RGB(220, 220, 220), // Button background color
    RGB(180, 180, 180), // Button border color
    RGB(200, 200, 200), // Button clicked background color
    RGB(150, 150, 150), // Scrollbar color
    RGB(150, 150, 150), // Scrollbar background color
};

bool quitting = false;
HWND hScrollBar = nullptr;
std::thread taskbarLoopThread;
HFONT hFont = nullptr;
int windowWidth = 0, windowHeight = 0, windowClientHeight = 0, windowScrollPos = 0;
HBRUSH lastCreatedBrush = nullptr;

// for testing
int colWidths[] = { 100, 150, 200 };
int columns = std::size(colWidths);
int rowHeight = 30;
int totalRows = 30;

void taskbarLoop() {
    bool called = false;
    while (true) {
        if (quitting)
            return;
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

void drawText(HDC hdc, const std::wstring &text, const int x, const int y) {
    const auto oldFont = static_cast<HFONT>(SelectObject(hdc, hFont));
    SetBkMode(hdc, TRANSPARENT);
    TextOut(hdc, x, y, text.c_str(), static_cast<int>(text.length()));
    SelectObject(hdc, oldFont);
}

void redrawLowerArea(HWND hwnd) {
    const RECT rect { 0, WSC_HEADER, windowWidth - 20, windowClientHeight };
    InvalidateRect(hwnd, &rect, TRUE);
}

void updateScrollbarColors() {
    constexpr int sysColors[] = { COLOR_SCROLLBAR, COLOR_BACKGROUND };
    const COLORREF newColors[] = { WCP_SCROLLBAR_COLOR, WCP_SCROLLBAR_BG };
    SetSysColors(2, sysColors, newColors);
}

void redrawHeader(HWND hwnd) {
    updateScrollbarColors();

    const int oldScrollPos = windowScrollPos;
    windowScrollPos = 0;
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    windowScrollPos = oldScrollPos;

    redrawLowerArea(hwnd);
}

void paintGrid(auto hdc, const int sx, const int sy) {
    const auto hPen = CreatePen(PS_SOLID, 1, WCP_FOREGROUND);
    const auto hOldPen = static_cast<HPEN>(SelectObject(hdc, hPen));

    const int gridWidth = std::accumulate(std::begin(colWidths), std::end(colWidths), 0, std::plus());
    const int gridHeight = totalRows * rowHeight;

    for (int row = 0; row <= totalRows; row++) {
        const int y = row * rowHeight + sy;
        MoveToEx(hdc, sx, y, nullptr);
        LineTo(hdc, gridWidth + sx, y);
    }

    int x = sx;
    for (int col = 0; col <= columns; col++) {
        MoveToEx(hdc, x, sy, nullptr);
        LineTo(hdc, x, gridHeight + sy);
        if (col < columns)
            x += colWidths[col];
    }

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

void updateScrollbarInfo() {
    const int contentHeight = totalRows * rowHeight + WSC_GRID_Y + WSC_GRID_TOP_OFFSET;

    if (windowScrollPos > WSC_MAX_SCROLL)
        windowScrollPos = WSC_MAX_SCROLL;
    if (windowScrollPos < 0)
        windowScrollPos = 0;

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = contentHeight - WSC_GRID_Y;
    si.nPage  = WSC_VISIBLE_AREA;
    si.nPos   = windowScrollPos;
    SetScrollInfo(hScrollBar, SB_CTL, &si, TRUE);
}

HBRUSH createBrush(const COLORREF color) {
    lastCreatedBrush = CreateSolidBrush(color);
    return lastCreatedBrush;
}

void deleteLastBrush() {
    DeleteObject(lastCreatedBrush);
    lastCreatedBrush = nullptr;
}

LRESULT CALLBACK WndProc(HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) {
    static NOTIFYICONDATA nid = {};

    switch (uMsg) {
        case WM_CREATE:
        {
            // Get icon for system tray
            const auto pcs = reinterpret_cast<CREATESTRUCT *>(lParam);
            const auto hTrayIcon   = static_cast<HICON>(pcs->lpCreateParams);
            // Create system tray icon
            nid.cbSize = sizeof(nid);
            nid.hWnd = hwnd;
            nid.uID = 1;
            nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            nid.uCallbackMessage = WM_TRAY_ICON;
            nid.hIcon = hTrayIcon;
            // Set icon tip
            lstrcpy(nid.szTip, VER_FILEDESCRIPTION_STR);

            hFont = CreateFont(
                16,
                0,
                0,
                0,
                FW_NORMAL,
                FALSE,
                FALSE,
                FALSE,
                ANSI_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Consolas"
            );

            const auto hButtonUpdate = CreateWindow(
                L"BUTTON", L"Update",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                10, 10, 120, 30,
                hwnd, reinterpret_cast<HMENU>(ID_BUTTON_UPDATE), nullptr, nullptr);
            SendMessage(hButtonUpdate, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            SendMessage(hButtonUpdate, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
            SetClassLongPtr(hButtonUpdate, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(LoadCursor(nullptr, IDC_HAND)));

            const auto hCheckBoxAutoUpdate = CreateWindowW(
                L"BUTTON", L"Live Preview",
                WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_OWNERDRAW,
                140, 10, 120, 30,
                hwnd, reinterpret_cast<HMENU>(ID_CHECKBOX_AUTO_UPDATE), nullptr, nullptr);
            SendMessage(hCheckBoxAutoUpdate, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            SendMessage(hCheckBoxAutoUpdate, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
            SetClassLongPtr(hCheckBoxAutoUpdate, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(LoadCursor(nullptr, IDC_HAND)));
            SetWindowLongPtr(hCheckBoxAutoUpdate, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&config::livePreview));

            const auto hCheckboxDarkMode = CreateWindow(
                L"BUTTON", utils::message(MSG_APP_WIN_DARK_MODE).c_str(),
                WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_OWNERDRAW,
                270, 10, 100, 30,
                hwnd, reinterpret_cast<HMENU>(ID_CHECKBOX_DARK_MODE), nullptr, nullptr);
            SendMessage(hCheckboxDarkMode, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            SendMessage(hCheckboxDarkMode, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
            SetClassLongPtr(hCheckboxDarkMode, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(LoadCursor(nullptr, IDC_HAND)));
            SetWindowLongPtr(hCheckboxDarkMode, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&config::darkMode));

            hScrollBar = CreateWindowEx(
                WS_VSCROLL, L"SCROLLBAR", nullptr,
                WS_CHILD | WS_VISIBLE | SBS_VERT,
                -WSC_SCROLLBAR_WIDTH, // Hide scrollbar by making it go beyond visible area
                WSC_HEADER,
                WSC_SCROLLBAR_WIDTH, 0,
                hwnd,
                reinterpret_cast<HMENU>(ID_SCROLLBAR),
                reinterpret_cast<LPCREATESTRUCT>(lParam)->hInstance,
                nullptr
            );

            // Add icon
            Shell_NotifyIcon(NIM_ADD, &nid);

            updateScrollbarInfo();
            break;
        }
        case WM_SIZE:
        {
            const int newWindowWidth = LOWORD(lParam);
            const int newWindowHeight = HIWORD(lParam);
            if (newWindowHeight != windowHeight) {
                redrawHeader(hwnd);
                redrawLowerArea(hwnd);
            }
            windowHeight = newWindowHeight;
            windowWidth = newWindowWidth;
            // Reposition scrollbar
            MoveWindow(hScrollBar, windowWidth - WSC_SCROLLBAR_WIDTH, WSC_HEADER, 20, windowHeight - WSC_HEADER, TRUE);
            RECT rect;
            GetClientRect(hwnd, &rect);
            windowClientHeight = rect.bottom - rect.top;

            updateScrollbarInfo();
            break;
        }
        case WM_DRAWITEM:
        {
            const auto draw = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
            wchar_t text[256];
            GetWindowText(draw->hwndItem, text, 256);
            switch (draw->CtlID) {
                case ID_BUTTON_UPDATE:
                {
                    COLORREF buttonBackgroundColor = WCP_BUTTON_BG;
                    if (draw->itemState & ODS_HOTLIGHT)
                        buttonBackgroundColor = RGB(0, 0, 0);
                    if (draw->itemState & ODS_SELECTED)
                        buttonBackgroundColor = WCP_BUTTON_CLICKED_BG;
                    const auto buttonBackgroundBrush = CreateSolidBrush(buttonBackgroundColor);
                    const auto buttonBaseBackgroundBrush = CreateSolidBrush(WCP_BACKGROUND2);
                    const auto buttonBorderBrush = CreateSolidBrush(WCP_BUTTON_BORDER);

                    RECT rect = draw->rcItem;
                    FillRect(draw->hDC, &rect, buttonBaseBackgroundBrush);
                    FrameRect(draw->hDC, &rect, draw->itemState & ODS_SELECTED ? buttonBackgroundBrush : buttonBorderBrush);
                    rect.left += 3;
                    rect.top += 3;
                    rect.right -= 3;
                    rect.bottom -= 3;
                    FillRect(draw->hDC, &rect, buttonBackgroundBrush);

                    SetBkColor(draw->hDC, buttonBackgroundColor);
                    SetTextColor(draw->hDC, WCP_FOREGROUND);
                    DrawTextW(draw->hDC, text, -1, &draw->rcItem, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

                    DeleteObject(buttonBaseBackgroundBrush);
                    DeleteObject(buttonBackgroundBrush);
                    break;
                }
                case ID_CHECKBOX_DARK_MODE:
                case ID_CHECKBOX_AUTO_UPDATE:
                {
                    const auto background = CreateSolidBrush(WCP_BACKGROUND2);
                    const auto foreground = CreateSolidBrush(WCP_FOREGROUND);

                    // Background color
                    FillRect(draw->hDC, &draw->rcItem, background);

                    // Create checkbox rect
                    RECT boxRect = draw->rcItem;
                    boxRect.right = boxRect.left + 16;
                    boxRect.top += (boxRect.bottom - boxRect.top - 16 ) / 2;
                    boxRect.bottom = boxRect.top + 16;

                    // Draw checkbox rect
                    FillRect(draw->hDC, &boxRect, background);
                    FrameRect(draw->hDC, &boxRect, foreground);
                    bool* pState = reinterpret_cast<bool*>(GetWindowLongPtr(draw->hwndItem, GWLP_USERDATA));
                    if (pState ? *pState : false)
                    {
                        // Create a little rect inside checkbox rect
                        boxRect.left += 3;
                        boxRect.top += 3;
                        boxRect.right -= 3;
                        boxRect.bottom -= 3;
                        FillRect(draw->hDC, &boxRect, foreground);
                    }

                    RECT textRect = draw->rcItem;
                    textRect.left += 20;
                    SetBkColor(draw->hDC, WCP_BACKGROUND2);
                    SetTextColor(draw->hDC, WCP_FOREGROUND);
                    DrawTextW(draw->hDC, text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

                    DeleteObject(background);
                    DeleteObject(foreground);
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Update colors for window
            SetBkColor(hdc, WCP_BACKGROUND);
            SetTextColor(hdc, WCP_FOREGROUND);
            FillRect(hdc, &ps.rcPaint, createBrush(WCP_BACKGROUND));
            deleteLastBrush();

            RECT wRect;
            GetWindowRect(hwnd, &wRect);
            wRect.left = 0;
            wRect.top = 0;
            wRect.bottom = WSC_HEADER;
            FillRect(hdc, &wRect, createBrush(WCP_BACKGROUND2));
            deleteLastBrush();

            drawText(hdc, L"Here you can see which window was ignored and which one was exceptional!", 10, 45);

            drawText(hdc, L"Debug table:", 10, 80 - windowScrollPos);
            paintGrid(hdc, 10, WSC_GRID_Y - windowScrollPos);

            EndPaint(hwnd, &ps);
            break;
        }
        case WM_TRAY_ICON:
        {
            if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP || lParam == WM_CONTEXTMENU) {
                HMENU hMenu = CreatePopupMenu();
                AppendMenu(hMenu, MF_STRING | MF_DISABLED, ID_TRAY_HEADER, TRAY_TITLE);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_OPEN_CONFIG, utils::message(MSG_TRAY_CONFIG_OPEN).c_str());
                AppendMenu(hMenu, MF_STRING, ID_TRAY_RELOAD_CONFIG, utils::message(MSG_TRAY_CONFIG_RELOAD).c_str());
                AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_PAUSE_HIDER, (globals::taskbarLoopRunState ? utils::message(MSG_TRAY_TB_PAUSE) : utils::message(MSG_TRAY_TB_RESUME)).c_str());
                AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_ADD_REMOVE_STARTUP, (utils::doesAutoStart() ? utils::message(MSG_TRAY_REM_STARTUP) : utils::message(MSG_TRAY_ADD_STARTUP)).c_str());
                //AppendMenu(hMenu, MF_STRING, ID_TRAY_ATTACH_DEBUG_CONSOLE, config::debug ? "Detach console (debug)" : "Attach console (debug)");
                AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_GITHUB, utils::message(MSG_TRAY_OPEN_GITHUB).c_str());
                AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, utils::message(MSG_TRAY_EXIT).c_str());
                POINT p;
                GetCursorPos(&p);
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, p.x, p.y, 0, hwnd, nullptr);
                DestroyMenu(hMenu);
            }
            break;
        }
        case WM_COMMAND:
        {
            switch (const auto lwParam = LOWORD(wParam)) {
                case ID_BUTTON_UPDATE:
                {
                    InvalidateRect(hwnd, nullptr, TRUE);
                    break;
                }
                case ID_CHECKBOX_AUTO_UPDATE:
                case ID_CHECKBOX_DARK_MODE:
                {
                    HWND hCheckbox = GetDlgItem(hwnd, lwParam);
                    if (const auto pState = reinterpret_cast<bool*>(GetWindowLongPtr(hCheckbox, GWLP_USERDATA))) {
                        *pState = !*pState;
                        redrawHeader(hwnd);
                    }
                    break;
                }
                // Next ones are for system tray
                case ID_TRAY_EXIT:
                {
                    DestroyWindow(hwnd);
                    break;
                }
                case ID_TRAY_OPEN_CONFIG:
                {
                    config::open();
                    break;
                }
                case ID_TRAY_RELOAD_CONFIG:
                {
                    config::load();
                    break;
                }
                case ID_TRAY_PAUSE_HIDER:
                {
                    globals::taskbarLoopRunState = !globals::taskbarLoopRunState;
                    break;
                }
                case ID_TRAY_ADD_REMOVE_STARTUP:
                {
                    utils::toggleStartup();
                    break;
                }
                case ID_TRAY_ATTACH_DEBUG_CONSOLE:
                {
                    break;
                }
                case ID_TRAY_GITHUB:
                {
                    ShellExecute(nullptr, L"open", PRODUCT_URL, nullptr, nullptr, SW_SHOWNORMAL);
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case WM_VSCROLL:
        {
            switch (LOWORD(wParam))
            {
                case SB_LINEUP:     windowScrollPos -= 10;  break; // Arrow up
                case SB_LINEDOWN:   windowScrollPos += 10;  break; // Arrow down
                case SB_PAGEUP:     windowScrollPos -= 100; break; // Click upper thumb
                case SB_PAGEDOWN:   windowScrollPos += 100; break; // Click lower thumb
                case SB_THUMBTRACK: {
                    SCROLLINFO si;
                    si.cbSize = sizeof(si);
                    si.fMask = SIF_TRACKPOS;
                    GetScrollInfo(hScrollBar, SB_CTL, &si);
                    windowScrollPos = si.nTrackPos;
                    break;
                }
                default: break;
            }

            updateScrollbarInfo();
            redrawLowerArea(hwnd);
            break;
        }
        case WM_MOUSEWHEEL:
        {
            windowScrollPos += -(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA) * (WSC_SCROLL_ROWS * rowHeight);
            updateScrollbarInfo();
            redrawLowerArea(hwnd);
            break;
        }
        case WM_GETMINMAXINFO: {
            const auto lpMinMaxInfo = reinterpret_cast<LPMINMAXINFO>(lParam);
            lpMinMaxInfo->ptMinTrackSize.x = 620;
            lpMinMaxInfo->ptMinTrackSize.y = 400;
            break;
        }
        case WM_DESTROY: {
            if (hFont)
                DeleteObject(hFont);
            if (hScrollBar)
                DeleteObject(hScrollBar);
            PostQuitMessage(0);
            Shell_NotifyIcon(NIM_DELETE, &nid);
            break;
        }
        case WM_ERASEBKGND: {
            return TRUE;
        }
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LONG WINAPI CrashHandler(const EXCEPTION_POINTERS* pException) {
    utils::showExceptionMessageBox([pException](std::wstringstream& crashInfo) {
        const EXCEPTION_RECORD* record = pException->ExceptionRecord;
        LPWSTR lpwstr = utils::NTStatusMessageToText(record->ExceptionCode);
        // A workaround, EXCEPTION_ACCESS_VIOLATION returns this message:
        // "The instruction at 0xp referenced memory at 0xp. The memory could not be s."
        // There are missing %, but p and s letters are not being used in any words, so we can just replace them
        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
            std::wstring operation;
            switch (record->ExceptionInformation[0]) {
                case 0: operation = L"read"; break;
                case 1: operation = L"written"; break;
                case 8: operation = L"execute (DEP)"; break;
                default:
                    operation = L"unknown(" + std::to_wstring(record->ExceptionInformation[0]) + L")";
                    break;
            }
            lpwstr = utils::replaceCharacterWithText(lpwstr, 's', operation, 1);
            lpwstr = utils::replaceCharacterWithText(lpwstr, 'p', std::to_wstring(record->ExceptionInformation[1]));
            lpwstr = utils::replaceCharacterWithText(lpwstr, 'p', std::to_wstring(record->ExceptionInformation[2]));
        } else if (record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {
            // I hope these are correct
            // Message: "The instruction at 0xp referenced memory at 0xp. The required data was not placed into memory because of an I/O error status of 0xx."
            lpwstr = utils::replaceCharacterWithText(lpwstr, 'p', std::to_wstring(record->ExceptionInformation[0]));
            lpwstr = utils::replaceCharacterWithText(lpwstr, 'p', std::to_wstring(record->ExceptionInformation[1]));
            lpwstr = utils::replaceCharacterWithText(lpwstr, 'x', std::to_wstring(record->ExceptionInformation[2]), 3);
        }
        crashInfo << utils::message(MSG_UNCAUGHT_EXCEPTION_WILL_TERMINATE) << std::endl;
        crashInfo << utils::message(MSG_UNCAUGHT_EXCEPTION_TRANSLATED_MSG, {utils::exceptionName(record->ExceptionCode)}) << std::endl;
        crashInfo << std::endl;
        crashInfo << lpwstr << std::endl;
        crashInfo << utils::message(MSG_UNCAUGHT_EXCEPTION_INFO) << std::endl;
        crashInfo << L"  " << utils::message(MSG_UNCAUGHT_EXCEPTION_INFO_CODE) << L" 0x" << std::hex << record->ExceptionCode << std::endl;
        crashInfo << L"  " << utils::message(MSG_UNCAUGHT_EXCEPTION_INFO_ADDR) << L" " << record->ExceptionAddress << std::endl;
        LocalFree(lpwstr);
    }, true);
    taskbar::resetTaskbar();
    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, const int nCmdShow) {
    globals::hIns = hInstance;
    // for (auto i = 1; i < argc; i++)
    //     globals::args += argv[i];
    SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(CrashHandler));
    // const auto exe = std::string(argv[0]);
    // globals::exe = exe.substr(exe.find_last_of("/\\") + 1);
    //
    // if (!utils::processArguments(argc, argv))
    //     return 0;

    config::darkMode = utils::isUserUsingDarkTheme();

    if (!globals::noConfigFile)
        config::load();

    HANDLE hMutex = CreateMutex(nullptr, TRUE, PROJECT_NAME);
    if (!hMutex)
        utils::messageBox(MSG_MUTEX_FAILED, MB_ICONWARNING | MB_OK, {utils::NTStatusMessageToText(GetLastError())});

    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (utils::messageBox(MSG_APP_ALREADY_RUNNING, MB_ICONQUESTION | MB_YESNO) == 6) {
            utils::killProcessByName(globals::exe.c_str(), GetCurrentProcessId());
            taskbar::resetTaskbar();
            return 0;
        }
        return 1;
    }

    // In case when mutex fails
    if (utils::killProcessByName(globals::exe.c_str(), GetCurrentProcessId())) {
        if (utils::messageBox(MSG_APP_ALREADY_RUNNING_BUT_KILLED, MB_ICONQUESTION | MB_YESNO) == 7) {
            exit(0);
        }
    }

    // Load icon
    HICON hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_APP_ICON));
    if (!hIcon) {
        utils::showExceptionMessageBox([](std::wstringstream& crashInfo) {
            crashInfo << utils::message(MSG_WINDOW_IRON_FAILED);
        }, true);
        return 1;
    }

    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon         = hIcon;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = PROJECT_NAME;

    if (!RegisterClassEx(&wc)) {
        utils::showExceptionMessageBox([](std::wstringstream& crashInfo) {
            crashInfo << utils::message(MSG_WINDOW_REGISTER_FAILED);
        }, true);
        return 1;
    }

    // Set initial window size
    windowWidth = 620, windowHeight = 500;

    // Get monitor size
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Create window
    globals::hWnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        PROJECT_NAME,
        utils::message(MSG_APPLICATION_NAME).c_str(),
        WS_OVERLAPPEDWINDOW,
        static_cast<short>((screenWidth - windowWidth) / 2),
        static_cast<short>((screenHeight - windowHeight) / 2),
        windowWidth, windowHeight,
        nullptr, nullptr,
        hInstance, hIcon);

    if (globals::hWnd == nullptr) {
        utils::showExceptionMessageBox([](std::wstringstream& crashInfo) {
            crashInfo << utils::message(MSG_WINDOW_CREATION_FAILED);
        }, true);
        return 1;
    }

    // atexit([] {
    //     MessageBoxA(globals::hWnd, "Application was closed.", PROJECT_NAME, MB_ICONINFORMATION | MB_OK);
    // });

    taskbarLoopThread = std::thread(taskbarLoop);

    ShowWindow(globals::hWnd, nCmdShow);
    UpdateWindow(globals::hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    quitting = true;
    if (taskbarLoopThread.joinable())
        taskbarLoopThread.join();

    taskbar::resetTaskbar();
    DestroyIcon(hIcon);
    CloseHandle(hMutex);
    DestroyWindow(globals::hWnd);
    return static_cast<int>(msg.wParam);
}