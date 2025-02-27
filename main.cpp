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

#define WSC_BUTTON_DEFAULT_W   120
#define WSC_BUTTON_DEFAULT_H   30
#define WSC_HEADER             70
#define WSC_SCROLLBAR_WIDTH    20
#define WSC_GRID_Y             100
#define WSC_GRID_TOP_OFFSET    40
#define WSC_SCROLL_ROWS        1
#define WSC_MAX_SCROLL         (contentHeight - (windowClientHeight - WSC_HEADER) - WSC_GRID_Y)
#define WSC_VISIBLE_AREA       (windowClientHeight - WSC_HEADER)

#define WMC_BUTTON             L"BUTTON"
#define WMC_SCROLLBAR          L"SCROLLBAR"
#define WMC_STATIC             L"STATIC"

#define W_GRID_MAX_COLUMNS     6

constexpr COLORREF darkColorPalette[] = {
    RGB(  0,   0,   0), // Base color
    RGB(255, 255, 255), // Text color
    RGB( 30,  30,  30), // Background color
    RGB( 25,  25,  25), // Second background color
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

constexpr std::wstring g_tableHeaders[] = {
    L"Status",
    L"Process",
    L"Class",
    L"Rect",
    L"State",
    L"Title"
};

std::thread taskbarLoopThread;

bool quitting                = false,
     focused                 = false,
     g_tableSizesInitialized = false;

HWND   g_hScrollBar        = nullptr,
       g_hScrollBarOverlay = nullptr,
       g_hSettingsButton     = nullptr;
HFONT  g_hDefaultFont      = nullptr,
       g_hTableFont        = nullptr;
HBRUSH g_lastCreatedBrush  = nullptr;

int windowWidth          = 0,
    windowHeight         = 0,
    windowClientHeight   = 0,
    g_windowScrollYPos   = 0,
    g_tableDefaultColumnWidths[W_GRID_MAX_COLUMNS],
    g_tableColumnWidths[W_GRID_MAX_COLUMNS],
    g_tableColumnXMargin = 10,
    g_tableRowHeight     = 30;

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

HBRUSH g_createBrush(const COLORREF color) {
    g_lastCreatedBrush = CreateSolidBrush(color);
    return g_lastCreatedBrush;
}

void g_deleteLastBrush() {
    DeleteObject(g_lastCreatedBrush);
    g_lastCreatedBrush = nullptr;
}

HDC g_doubleBuffering(HWND hwnd, PAINTSTRUCT &ps, const HDC oHdc, const bool start) {
    static HBITMAP memBitmap;
    static HGDIOBJ oldBitmap;
    static HDC mHdc, hdc;
    static int width, height;
    if (start) {
        if (!oHdc)
            hdc = BeginPaint(hwnd, &ps);
        else
            hdc = oHdc;
        mHdc = CreateCompatibleDC(hdc);

        RECT rc;
        GetClientRect(hwnd, &rc);
        width = rc.right - rc.left;
        height = rc.bottom - rc.top;

        memBitmap = CreateCompatibleBitmap(hdc, width, height);
        oldBitmap = SelectObject(mHdc, memBitmap);
        return mHdc;
    }

    BitBlt(hdc, 0, 0, width, height, mHdc, 0, 0, SRCCOPY);

    SelectObject(mHdc, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(mHdc);
    if (!oHdc) {
        EndPaint(hwnd, &ps);
        hdc = nullptr;
    }
    memBitmap = nullptr;
    oldBitmap = nullptr;
    mHdc = nullptr;
    width = 0;
    height = 0;
    return nullptr;
}

inline void g_drawText(HDC hdc, const std::wstring &text, const int x, const int y) {
    TextOut(hdc, x, y, text.c_str(), static_cast<int>(text.length()));
}

void g_redrawLowerArea(HWND hwnd) {
    const RECT rect { 0, WSC_HEADER, windowWidth - 20, windowClientHeight };
    InvalidateRect(hwnd, &rect, TRUE);
}

void g_redrawWindow(HWND hwnd) {
    const int oldScrollPos = g_windowScrollYPos;
    g_windowScrollYPos = 0;
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    g_windowScrollYPos = oldScrollPos;
}

int g_calculateTextWidth(HDC hdc, const std::wstring &text) {
    const auto oldFont = static_cast<HFONT>(SelectObject(hdc, g_hTableFont));
    SIZE sz;
    GetTextExtentPoint32(hdc, text.c_str(), static_cast<int>(text.size()), &sz);
    SelectObject(hdc, oldFont);
    return sz.cx;
}

void g_paintGrid(HDC hdc, const int sx, const int sy, const int rows) {
    const auto hPen = CreatePen(PS_SOLID, 1, WCP_FOREGROUND);
    const auto hOldPen = static_cast<HPEN>(SelectObject(hdc, hPen));

    const int gridWidth = std::accumulate(std::begin(g_tableColumnWidths), std::end(g_tableColumnWidths), 0, std::plus());
    const int gridHeight = rows * g_tableRowHeight;

    RECT rect = { sx, sy, gridWidth + sx, sy + g_tableRowHeight };
    FillRect(hdc, &rect, g_createBrush(WCP_BACKGROUND2));
    g_deleteLastBrush();

    // Draw horizontal lines
    for (int row = 0; row <= rows; row++) {
        const int y = row * g_tableRowHeight + sy;
        MoveToEx(hdc, sx, y, nullptr);
        LineTo(hdc, gridWidth + sx, y);
    }

    // Draw vertical lines
    int x = sx;
    for (int col = 0; col <= W_GRID_MAX_COLUMNS; col++) {
        MoveToEx(hdc, x, sy, nullptr);
        LineTo(hdc, x, gridHeight + sy);
        if (col < W_GRID_MAX_COLUMNS)
            x += g_tableColumnWidths[col];
    }

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

inline void g_calculateDefaultWidths(HDC hdc) {
    for (auto i = 0; i < std::size(g_tableHeaders); i++) {
        std::wstring header = g_tableHeaders[i];
        g_tableDefaultColumnWidths[i] = g_calculateTextWidth(hdc, header);
    }
}

std::wstring getWindowValue(const taskbar::WindowInfo &wInfo, const int col) {
    std::wstring value;
    switch (col) {
        case 0: {
            if (wInfo.wasExceptional)
                value = L"Exceptional";
            else if (wInfo.detected)
                value = L"Detected";
            else
                value = L"Ignored";
            break;
        }
        case 3: {
            value = L"[" + std::to_wstring(wInfo.rect.left) + L","
                    + std::to_wstring(wInfo.rect.top) + L","
                    + std::to_wstring(wInfo.rect.right) + L","
                    + std::to_wstring(wInfo.rect.bottom) + L"]";
            break;
        }
        case 1: value = wInfo.procFilename; break;
        case 2: value = wInfo.wndClass; break;
        case 4: value = wInfo.maximized == 1 ? L"Maximized" : wInfo.maximized == 0 ? L"Minimized" : L"Unknown"; break;
        case 5: value = wInfo.title; break;
        default: value = L"???"; break;
    }
    return value;
}

inline void g_calculateCurrentWidths(HDC hdc, const int rows) {
    int widths[6];
    std::copy(std::begin(g_tableDefaultColumnWidths), std::end(g_tableDefaultColumnWidths), std::begin(widths));

    for (auto row = 0; row < rows; row++) {
        taskbar::WindowInfo wInfo;
        if (row > 0)
            wInfo = taskbar::windows[row - 1];
        for (auto col = 0; col < W_GRID_MAX_COLUMNS; col++)
            if (const std::wstring value = row == 0 ? g_tableHeaders[col] : getWindowValue(wInfo, col); !value.empty())
                if (const int width = g_calculateTextWidth(hdc, value) + 2 * g_tableColumnXMargin; widths[col] < width)
                    widths[col] = width;
    }

    std::copy(std::begin(widths), std::end(widths), std::begin(g_tableColumnWidths));
}

inline void g_printDataToGrid(HDC hdc, const int sx, const int sy, const int rows) {
    for (auto row = 0; row < rows; row++) {
        const int y = sy + 7 + row * g_tableRowHeight;
        int x = sx;
        taskbar::WindowInfo wInfo;
        if (row > 0)
            wInfo = taskbar::windows[row - 1];
        for (auto col = 0; col < W_GRID_MAX_COLUMNS; col++) {
            std::wstring value;
            if (row == 0) { // Header
                value = g_tableHeaders[col];
                g_drawText(hdc, value, x + 10, y);
            } else {
                value = getWindowValue(wInfo, col);
                if (!value.empty())
                    g_drawText(hdc, value, x + 10, y);
            }
            x += g_tableColumnWidths[col];
        }
    }
}

void g_updateTable(HDC hdc) {
    if (g_tableDefaultColumnWidths[0] == 0)
        g_calculateDefaultWidths(hdc);
    const int y = WSC_GRID_Y - g_windowScrollYPos;
    const int rows = std::size(taskbar::windows) + 1; // +header

    SelectObject(hdc, g_hTableFont);
    SetBkMode(hdc, TRANSPARENT);
    g_calculateCurrentWidths(hdc, rows);
    g_paintGrid(hdc, 10, y, rows);
    g_printDataToGrid(hdc, 10, y, rows);
}

void g_updateScrollBarStyle() {
    SetLayeredWindowAttributes(g_hScrollBarOverlay, 0, config::darkMode ? 180 : 0, LWA_ALPHA);
}

void updateScrollBarInfo() {
    // Add header row
    const int contentHeight = (std::size(taskbar::windows) + 1) * g_tableRowHeight + WSC_GRID_Y + WSC_GRID_TOP_OFFSET;

    if (g_windowScrollYPos > WSC_MAX_SCROLL)
        g_windowScrollYPos = WSC_MAX_SCROLL;
    if (g_windowScrollYPos < 0)
        g_windowScrollYPos = 0;

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = contentHeight - WSC_GRID_Y;
    si.nPage  = WSC_VISIBLE_AREA;
    si.nPos   = g_windowScrollYPos;
    SetScrollInfo(g_hScrollBar, SB_CTL, &si, TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) {
    static NOTIFYICONDATA nid = {};

    switch (uMsg) {
        case WM_CREATE:
        {
            // Get icon for system tray
            const auto pcs = reinterpret_cast<CREATESTRUCT *>(lParam);
            const auto hTrayIcon = static_cast<HICON>(pcs->lpCreateParams);
            // Create system tray icon
            nid.cbSize = sizeof(nid);
            nid.hWnd = hwnd;
            nid.uID = 1;
            nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            nid.uCallbackMessage = WM_TRAY_ICON;
            nid.hIcon = hTrayIcon;
            // Set icon tip
            lstrcpy(nid.szTip, VER_FILEDESCRIPTION_STR);

            const auto create = reinterpret_cast<LPCREATESTRUCT>(lParam);

            g_hDefaultFont = CreateFont(
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
                L"Sonoran Sans Serif"
            );

            g_hTableFont = CreateFont(
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

            LONG_PTR lPtrHandCursor = reinterpret_cast<LONG_PTR>(LoadCursor(nullptr, IDC_HAND));

            const auto hButtonUpdate = CreateWindow(
                WMC_BUTTON, L"Update",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                10, 10, WSC_BUTTON_DEFAULT_W, WSC_BUTTON_DEFAULT_H,
                hwnd, reinterpret_cast<HMENU>(ID_BUTTON_UPDATE), create->hInstance, nullptr);
            SendMessage(hButtonUpdate, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), TRUE);
            SendMessage(hButtonUpdate, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
            SetClassLongPtr(hButtonUpdate, GCLP_HCURSOR, lPtrHandCursor);

            g_hSettingsButton = CreateWindow(
                WMC_BUTTON, L"Actions",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                0, 0, WSC_BUTTON_DEFAULT_W, WSC_BUTTON_DEFAULT_H,
                hwnd, reinterpret_cast<HMENU>(ID_BUTTON_ACTIONS), create->hInstance, nullptr);
            SendMessage(g_hSettingsButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), TRUE);
            SendMessage(g_hSettingsButton, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
            SetClassLongPtr(g_hSettingsButton, GCLP_HCURSOR, lPtrHandCursor);

            const auto hCheckBoxAutoUpdate = CreateWindowW(
                WMC_BUTTON, L"Auto Update",
                WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_OWNERDRAW,
                140, 10, 100, 30,
                hwnd, reinterpret_cast<HMENU>(ID_CHECKBOX_AUTO_UPDATE), create->hInstance, nullptr);
            SendMessage(hCheckBoxAutoUpdate, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), TRUE);
            SendMessage(hCheckBoxAutoUpdate, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
            SetClassLongPtr(hCheckBoxAutoUpdate, GCLP_HCURSOR, lPtrHandCursor);
            SetWindowLongPtr(hCheckBoxAutoUpdate, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&config::livePreview));

            const auto hCheckboxDarkMode = CreateWindow(
                WMC_BUTTON, utils::message(MSG_APP_WIN_DARK_MODE).c_str(),
                WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_OWNERDRAW,
                250, 10, 100, 30,
                hwnd, reinterpret_cast<HMENU>(ID_CHECKBOX_DARK_MODE), create->hInstance, nullptr);
            SendMessage(hCheckboxDarkMode, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), TRUE);
            SendMessage(hCheckboxDarkMode, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
            SetClassLongPtr(hCheckboxDarkMode, GCLP_HCURSOR, lPtrHandCursor);
            SetWindowLongPtr(hCheckboxDarkMode, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&config::darkMode));

            g_hScrollBar = CreateWindowEx(
                WS_VSCROLL, WMC_SCROLLBAR, nullptr,
                WS_CHILD | WS_VISIBLE | SBS_VERT,
                -WSC_SCROLLBAR_WIDTH, WSC_HEADER, WSC_SCROLLBAR_WIDTH, 0,
                hwnd, reinterpret_cast<HMENU>(ID_SCROLLBAR), create->hInstance, nullptr);
            SendMessage(g_hScrollBar, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), TRUE);
            SendMessage(hCheckboxDarkMode, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);

            g_hScrollBarOverlay = CreateWindowEx(
                WS_EX_LAYERED | WS_EX_TRANSPARENT, WMC_BUTTON, nullptr, // STATIC doesn't work, using BUTTON as I still override its functionality
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                0, 0, 200, 200,
                hwnd, reinterpret_cast<HMENU>(ID_SCROLLBAR_OVERLAY), create->hInstance, nullptr);
            SetClassLongPtr(g_hScrollBarOverlay, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(LoadCursor(nullptr, IDC_ARROW)));
            g_updateScrollBarStyle();

            // Add icon
            Shell_NotifyIcon(NIM_ADD, &nid);

            //Trigger table update
            taskbar::collectWindowsInfo = true;

            updateScrollBarInfo();
            break;
        }
        case WM_SIZE:
        {
            const int newWindowWidth = LOWORD(lParam);
            const int newWindowHeight = HIWORD(lParam);
            windowHeight = newWindowHeight;
            windowWidth = newWindowWidth;
            g_redrawWindow(hwnd);
            g_redrawLowerArea(hwnd);
            // Reposition scrollbar
            MoveWindow(g_hScrollBar, windowWidth - WSC_SCROLLBAR_WIDTH, WSC_HEADER, 20, windowHeight - WSC_HEADER, TRUE);
            MoveWindow(g_hScrollBarOverlay, windowWidth - WSC_SCROLLBAR_WIDTH, WSC_HEADER, 20, windowHeight - WSC_HEADER, TRUE);
            MoveWindow(g_hSettingsButton, windowWidth - WSC_BUTTON_DEFAULT_W - 10, 10, WSC_BUTTON_DEFAULT_W, WSC_BUTTON_DEFAULT_H, TRUE);
            RECT rect;
            GetClientRect(hwnd, &rect);
            windowClientHeight = rect.bottom - rect.top;

            updateScrollBarInfo();
            break;
        }
        case WM_ACTIVATE:
        {
            focused = LOWORD(wParam) != WA_INACTIVE;
            break;
        }
        case WM_DRAWITEM:
        {
            const auto draw = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
            wchar_t text[256];
            GetWindowText(draw->hwndItem, text, sizeof(text));
            PAINTSTRUCT ps;
            switch (draw->CtlID) {
                case ID_SCROLLBAR_OVERLAY:
                {
                    FillRect(draw->hDC, &draw->rcItem, CreateSolidBrush(RGB(0, 0, 0)));
                    g_deleteLastBrush();
                    break;
                }
                case ID_BUTTON_ACTIONS:
                case ID_BUTTON_UPDATE:
                {
                    HDC mHdc = g_doubleBuffering(hwnd, ps, draw->hDC, true);
                    COLORREF buttonBackgroundColor = WCP_BUTTON_BG;
                    if (draw->itemState & ODS_SELECTED)
                        buttonBackgroundColor = WCP_BUTTON_CLICKED_BG;
                    const auto buttonBackgroundBrush = CreateSolidBrush(buttonBackgroundColor);
                    const auto buttonBaseBackgroundBrush = CreateSolidBrush(WCP_BACKGROUND2);
                    const auto buttonBorderBrush = CreateSolidBrush(WCP_BUTTON_BORDER);

                    RECT rect = draw->rcItem;
                    FillRect(mHdc, &rect, buttonBaseBackgroundBrush);
                    FrameRect(mHdc, &rect, draw->itemState & ODS_SELECTED ? buttonBackgroundBrush : buttonBorderBrush);
                    rect.left += 3;
                    rect.top += 3;
                    rect.right -= 3;
                    rect.bottom -= 3;
                    FillRect(mHdc, &rect, buttonBackgroundBrush);

                    SetBkColor(mHdc, buttonBackgroundColor);
                    SetTextColor(mHdc, WCP_FOREGROUND);
                    SelectObject(mHdc, g_hDefaultFont);
                    DrawTextW(mHdc, text, -1, &draw->rcItem, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

                    DeleteObject(buttonBaseBackgroundBrush);
                    DeleteObject(buttonBackgroundBrush);
                    g_doubleBuffering(hwnd, ps, draw->hDC, false);
                    break;
                }
                case ID_CHECKBOX_DARK_MODE:
                case ID_CHECKBOX_AUTO_UPDATE:
                {
                    HDC mHdc = g_doubleBuffering(hwnd, ps, draw->hDC, true);

                    const auto background = CreateSolidBrush(WCP_BACKGROUND2);
                    const auto foreground = CreateSolidBrush(WCP_FOREGROUND);

                    // Background color
                    FillRect(mHdc, &draw->rcItem, background);

                    // Create checkbox rect
                    RECT boxRect = draw->rcItem;
                    boxRect.right = boxRect.left + 16;
                    boxRect.top += (boxRect.bottom - boxRect.top - 16 ) / 2;
                    boxRect.bottom = boxRect.top + 16;

                    // Draw checkbox rect
                    FillRect(mHdc, &boxRect, background);
                    FrameRect(mHdc, &boxRect, foreground);
                    if (const auto pState = reinterpret_cast<bool*>(GetWindowLongPtr(draw->hwndItem, GWLP_USERDATA)); pState ? *pState : false)
                    {
                        // Create a little rect inside checkbox rect
                        boxRect.left += 3;
                        boxRect.top += 3;
                        boxRect.right -= 3;
                        boxRect.bottom -= 3;
                        FillRect(mHdc, &boxRect, foreground);
                    }

                    RECT textRect = draw->rcItem;
                    textRect.left += 20;
                    SetBkColor(mHdc, WCP_BACKGROUND2);
                    SetTextColor(mHdc, WCP_FOREGROUND);
                    SelectObject(mHdc, g_hDefaultFont);
                    DrawTextW(mHdc, text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

                    DeleteObject(background);
                    DeleteObject(foreground);
                    g_doubleBuffering(hwnd, ps, draw->hDC, false);
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
            HDC mHdc = g_doubleBuffering(hwnd, ps, nullptr, true);

            // Update colors for window
            SetBkColor(mHdc, WCP_BACKGROUND);
            SetTextColor(mHdc, WCP_FOREGROUND);
            FillRect(mHdc, &ps.rcPaint, g_createBrush(WCP_BACKGROUND));
            g_deleteLastBrush();

            RECT wRect;
            GetWindowRect(hwnd, &wRect);
            wRect.left = 0;
            wRect.top = 0;
            wRect.bottom = WSC_HEADER;
            FillRect(mHdc, &wRect, g_createBrush(WCP_BACKGROUND2));
            g_deleteLastBrush();

            SelectObject(mHdc, g_hDefaultFont);
            SetBkColor(mHdc, WCP_BACKGROUND2);
            g_drawText(mHdc, L"Here you can see which window was ignored and which one was exceptional!", 10, 45);
            SetBkColor(mHdc, WCP_BACKGROUND);
            g_drawText(mHdc, L"Debug table:", 10, 80 - g_windowScrollYPos);

            g_updateTable(mHdc);

            g_doubleBuffering(hwnd, ps, nullptr, false);
            break;
        }
        case WM_TRAY_ICON:
        {
            if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP || lParam == WM_CONTEXTMENU) {
                HMENU hMenu = CreatePopupMenu();
                if (lParam != WM_CONTEXTMENU)
                    AppendMenu(hMenu, MF_STRING | MF_DISABLED, ID_TRAY_HEADER, TRAY_TITLE);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_OPEN_CONFIG, utils::message(MSG_TRAY_CONFIG_OPEN).c_str());
                AppendMenu(hMenu, MF_STRING, ID_TRAY_RELOAD_CONFIG, utils::message(MSG_TRAY_CONFIG_RELOAD).c_str());
                AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_PAUSE_HIDER, (globals::taskbarLoopRunState ? utils::message(MSG_TRAY_TB_PAUSE) : utils::message(MSG_TRAY_TB_RESUME)).c_str());
                AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_ADD_REMOVE_STARTUP, (utils::doesAutoStart() ? utils::message(MSG_TRAY_REM_STARTUP) : utils::message(MSG_TRAY_ADD_STARTUP)).c_str());
                AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_GITHUB, utils::message(MSG_TRAY_OPEN_GITHUB).c_str());
                if (lParam != WM_CONTEXTMENU) {
                    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
                    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, utils::message(MSG_TRAY_EXIT).c_str());
                }
                POINT p;
                GetCursorPos(&p);
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, p.x, p.y, 0, hwnd, nullptr);
                DestroyMenu(hMenu);
            }
            break;
        }
        case WM_UPDATE_GRID_REQUEST:
        {
            updateScrollBarInfo();
            g_redrawLowerArea(hwnd);
        }
        case WM_COMMAND:
        {
            switch (const auto lwParam = LOWORD(wParam)) {
                case ID_BUTTON_ACTIONS:
                {
                    SendMessage(hwnd, WM_TRAY_ICON, 0, WM_CONTEXTMENU);
                    break;
                }
                case ID_BUTTON_UPDATE:
                {
                    taskbar::collectWindowsInfo = true;
                    break;
                }
                case ID_CHECKBOX_AUTO_UPDATE:
                case ID_CHECKBOX_DARK_MODE:
                {
                    HWND hCheckbox = GetDlgItem(hwnd, lwParam);
                    if (const auto pState = reinterpret_cast<bool*>(GetWindowLongPtr(hCheckbox, GWLP_USERDATA))) {
                        *pState = !*pState;
                        if (lwParam == ID_CHECKBOX_DARK_MODE)
                            g_updateScrollBarStyle();
                        g_redrawWindow(hwnd);
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
                case SB_LINEUP:     g_windowScrollYPos -= 10;  break; // Arrow up
                case SB_LINEDOWN:   g_windowScrollYPos += 10;  break; // Arrow down
                case SB_PAGEUP:     g_windowScrollYPos -= 100; break; // Click upper thumb
                case SB_PAGEDOWN:   g_windowScrollYPos += 100; break; // Click lower thumb
                case SB_THUMBTRACK: {
                    SCROLLINFO si;
                    si.cbSize = sizeof(si);
                    si.fMask = SIF_TRACKPOS;
                    GetScrollInfo(g_hScrollBar, SB_CTL, &si);
                    g_windowScrollYPos = si.nTrackPos;
                    break;
                }
                default: break;
            }

            updateScrollBarInfo();
            g_redrawLowerArea(hwnd);
            break;
        }
        case WM_MOUSEWHEEL:
        {
            const int oldPos = g_windowScrollYPos;
            g_windowScrollYPos += -(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * (WSC_SCROLL_ROWS * g_tableRowHeight));
            updateScrollBarInfo();
            if (oldPos == g_windowScrollYPos)
                break;
            updateScrollBarInfo();
            g_redrawLowerArea(hwnd);
            break;
        }
        case WM_GETMINMAXINFO: {
            const auto lpMinMaxInfo = reinterpret_cast<LPMINMAXINFO>(lParam);
            lpMinMaxInfo->ptMinTrackSize.x = 620;
            lpMinMaxInfo->ptMinTrackSize.y = 400;
            break;
        }
        case WM_DESTROY: {
            if (g_hDefaultFont)
                DeleteObject(g_hDefaultFont);
            if (g_hScrollBar)
                DeleteObject(g_hScrollBar);
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
        crashInfo << (lpwstr != nullptr ? lpwstr : L"<Failed to translate error message!>") << std::endl;
        crashInfo << utils::message(MSG_UNCAUGHT_EXCEPTION_INFO) << std::endl;
        crashInfo << "  " << utils::message(MSG_UNCAUGHT_EXCEPTION_INFO_CODE) << " 0x" << std::hex << record->ExceptionCode << std::endl;
        crashInfo << "  " << utils::message(MSG_UNCAUGHT_EXCEPTION_INFO_ADDR) << " " << record->ExceptionAddress << std::endl;
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