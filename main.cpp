#include "taskbar.h"
#include "utils.h"
#include "config.h"
#include "globals.h"
#include "resources.h"
#include <dwmapi.h>
#include <format>
#include <windowsx.h>
#include <iostream>
#include <sstream>
#include <numeric>
#include "language.h"
#include "monitors.h"
#include "taskbar_animation.h"
#include "win_draw.h"

#pragma comment(lib, "Dwmapi.lib")

#define EVENT_OBJECT_CLOAKED      0x8017
#define EVENT_OBJECT_UNCLOAKED    0x8018

#define C_RED                     RGB(255, 0, 0)

#define APP_WINDOW_MIN_WIDTH      800
#define APP_WINDOW_MIN_HEIGHT     550

#define WMC_BUTTON                L"BUTTON"
#define WMC_SCROLLBAR             L"SCROLLBAR"

#define W_GRID_MAX_COLUMNS        9

std::wstring g_tableHeaders[W_GRID_MAX_COLUMNS] = {};
std::vector<HWND> g_childWindows;

constexpr bool g_tableCollapsableHeaders[] = {
    false, true, true, true, true, false, false, false, true
};

bool g_focused = false,
     g_tableHeaderCheckboxCollapseStates[W_GRID_MAX_COLUMNS] = {};
RECT g_trackableCheckBoxes[W_GRID_MAX_COLUMNS] = {};
HWND g_hCBShowAllWindows = nullptr,
#ifdef IS_PORTABLE
     g_hInstallButton    = nullptr,
#endif
     g_hSettingsButton   = nullptr;
HFONT g_hDefaultFont     = nullptr,
      g_hDefaultFontBold = nullptr,
      g_hTableFontBold   = nullptr,
      g_hTableFont       = nullptr;
HBRUSH g_lastCreatedBrush = nullptr;
int g_windowWidth                                  = APP_WINDOW_MIN_WIDTH,
    g_windowHeight                                 = APP_WINDOW_MIN_HEIGHT,
    g_windowClientHeight                           = 0,
    g_windowClientWidth                            = 0,
    g_tableDefaultColumnWidths[W_GRID_MAX_COLUMNS] = {},
    g_tableColumnWidths[W_GRID_MAX_COLUMNS]        = {},
    g_tableColumnXMargin                           = 10,
    g_tableRowHeight                               = 30,
    g_lastUpdatedTimeTextWidth                     = 0;
std::wstring g_lastTableUpdateTime = L"00:00:00.000";

int WTBH_getContentHeight();
int WTBH_getContentWidth();

win_draw::scrollable_content g_gridScroll(WTBH_getContentWidth,
    WTBH_getContentHeight,
    &g_windowClientWidth,
    &g_windowClientHeight,
    &g_windowWidth,
    &g_windowHeight,
    WSC_HEADER);

std::vector<taskbar::WindowInfo> g_windowsCache;
int g_windowsCacheSize = 0;

HHOOK g_hKeyboardHook = nullptr;

inline int WTBH_getContentHeight() {
    // +1 for header row
    return (g_windowsCacheSize + 1) * g_tableRowHeight + WSC_GRID_Y + WSC_GRID_TOP_OFFSET + (config::autoUpdate ? g_windowClientHeight : 0);
}

inline int WTBH_getContentWidth() {
    return std::accumulate(std::begin(g_tableColumnWidths), std::end(g_tableColumnWidths), config::autoUpdate ? g_windowClientWidth : 0, std::plus());
}

inline void WTBH_resizeChildWindows(HWND hWnd);

inline void WTBH_updateLanguage(HWND hWnd) {
    for (auto i = 0; i < W_GRID_MAX_COLUMNS; i++)
        g_tableHeaders[i] = utils::message(MSG_WND_DEBUG_TABLE_STATUS + i);
    if (hWnd) {
        WTBH_resizeChildWindows(hWnd);
        SetWindowTextW(hWnd, utils::message(MSG_APPLICATION_NAME).c_str());
    }
}

HBRUSH WTBH_createBrush(const COLORREF color) {
    g_lastCreatedBrush = CreateSolidBrush(color);
    return g_lastCreatedBrush;
}

void WTBH_deleteLastBrush() {
    DeleteObject(g_lastCreatedBrush);
    g_lastCreatedBrush = nullptr;
}

inline void WTBH_redrawLowerArea(HWND hWnd) {
    RECT rect { 0, WSC_HEADER, g_windowWidth, g_windowClientHeight };
    InvalidateRect(hWnd, &rect, true);
    rect = utils::rect(g_windowClientWidth - g_lastUpdatedTimeTextWidth - 10, 45, g_lastUpdatedTimeTextWidth, 30);
    InvalidateRect(hWnd, &rect, true);
}

inline void WTBH_paintGrid(HDC hdc, const int sx, const int sy, const int rows) {
    const int gridWidth = std::accumulate(std::begin(g_tableColumnWidths), std::end(g_tableColumnWidths), 0, std::plus());
    const int gridHeight = rows * g_tableRowHeight;

    const RECT rect = { sx, sy, gridWidth + sx, sy + g_tableRowHeight };
    FillRect(hdc, &rect, WTBH_createBrush(WCP_BACKGROUND2));
    WTBH_deleteLastBrush();

    auto hPen = CreatePen(PS_SOLID, 1, WCP_FOREGROUND);
    auto hOldPen = static_cast<HPEN>(SelectObject(hdc, hPen));

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

    // Draw horizontal lines
    for (int row = 0; row <= rows; row++) {
        const bool detected = config::showAllWindows && row > 0 && row < rows ? g_windowsCache[row - 1].detected : false;
        hPen = CreatePen(PS_SOLID, 1, detected ? C_RED : WCP_FOREGROUND);
        hOldPen = static_cast<HPEN>(SelectObject(hdc, hPen));
        int y = row * g_tableRowHeight + sy;

        MoveToEx(hdc, sx, y, nullptr);
        LineTo(hdc, gridWidth + sx, y);
        if (detected && row < rows) {
            row++;
            x = sx;
            for (int col = 0; col <= W_GRID_MAX_COLUMNS; col++) {
                MoveToEx(hdc, x, y, nullptr);
                LineTo(hdc, x, g_tableRowHeight + y);
                if (col < W_GRID_MAX_COLUMNS)
                    x += g_tableColumnWidths[col];
            }
            y += g_tableRowHeight;
            MoveToEx(hdc, sx, y, nullptr);
            LineTo(hdc, gridWidth + sx, y);
        }

        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);
    }
}

inline void WTBH_calculateDefaultWidths(HDC hdc) {
    for (auto i = 0; i < W_GRID_MAX_COLUMNS; i++) {
        std::wstring header = g_tableHeaders[i];
        g_tableDefaultColumnWidths[i] = win_draw::calculateTextWidth(hdc, header, g_hTableFont);
        if (g_tableCollapsableHeaders[i])
            g_tableDefaultColumnWidths[i] += WSC_CHECKBOX_TEXT_OFFSET;
    }
}

std::wstring WTBH_getWindowValue(const taskbar::WindowInfo &wInfo, const int col) {
    std::wstring value;
    switch (col) {
        case 0: { // Status
            if (wInfo.wasExceptional)
                value = utils::message(MSG_WND_DEBUG_TABLE_STATUS_EXCEPTION);
            else if (wInfo.detected)
                value = utils::message(MSG_WND_DEBUG_TABLE_STATUS_DETECTED);
            else if (wInfo.initiallyIgnored)
                value = utils::message(MSG_WND_DEBUG_TABLE_STATUS_SKIPPED);
            else
                value = utils::message(MSG_WND_DEBUG_TABLE_STATUS_IGNORED);
            break;
        }
        case 1: { // Triggered by
            if (wInfo.initiallyIgnored)
                value += utils::message(MSG_WND_DEBUG_TABLE_TB_INT_NMAXIMIZED);
            else if (wInfo.fault.empty() && wInfo.detected)
                value = utils::message(MSG_WND_DEBUG_TABLE_TB_INT_NEGATED);
            else
                value = wInfo.fault;
            break;
        }
        case 2: value = wInfo.procFilename; break; // Process filename
        case 3: value = wInfo.wndClass; break; // Class
        case 4: { // Rect
            value = wInfo.rect ? L"[" + std::to_wstring(wInfo.rect->left) + L","
                    + std::to_wstring(wInfo.rect->top) + L","
                    + std::to_wstring(wInfo.rect->right) + L","
                    + std::to_wstring(wInfo.rect->bottom) + L"]" : L"[?,?,?,?]";
            break;
        }
        case 5: { // State
            value = utils::message(wInfo.maximized == 1 ? MSG_WND_DEBUG_TABLE_STATE_MAXIMIZED : MSG_WND_DEBUG_TABLE_STATE_MINIMIZED);
            break;
        }
        case 6: { // Monitor
            value = L"?";
            for (auto i = 0; i < monitors::monitorCount; i++) {
                if (wInfo.hMonitor == monitors::monitor(i)) {
                    value = std::to_wstring(i);
                    break;
                }
            }
            break;
        }
        case 7: { // Focused
            value = wInfo.focused ? utils::message(*wInfo.focused ? MSG_WND_YES : MSG_WND_NO) : L"?";
            break;
        }
        case 8: value = wInfo.title; break; // Title
        default: value = L"???"; break; // Unknown
    }
    return value;
}

inline void WTBH_calculateCurrentWidths(HDC hdc, const int rows) {
    int widths[W_GRID_MAX_COLUMNS];
    std::ranges::copy(g_tableDefaultColumnWidths, std::begin(widths));

    for (auto row = 0; row < rows; row++) {
        taskbar::WindowInfo wInfo;
        if (row > 0)
            wInfo = g_windowsCache[row - 1];
        for (auto col = 0; col < W_GRID_MAX_COLUMNS; col++)
            if (const std::wstring value = row == 0 ? g_tableHeaders[col] : WTBH_getWindowValue(wInfo, col); !value.empty()) {
                int width = win_draw::calculateTextWidth(hdc, value, g_hTableFont) + 2 * g_tableColumnXMargin;
                if (row == 0 && g_tableCollapsableHeaders[col])
                    width += WSC_CHECKBOX_TEXT_OFFSET;
                if (g_tableHeaderCheckboxCollapseStates[col]) {
                    widths[col] = g_tableDefaultColumnWidths[col] + WSC_CHECKBOX_TEXT_OFFSET;
                    continue;
                }
                if (widths[col] < width)
                    widths[col] = width;
            }
    }

    std::ranges::copy(widths, std::begin(g_tableColumnWidths));
}

inline void WTBH_printDataToGrid(HDC hdc, const int sx, const int sy, const int rows) {
    HBRUSH background = nullptr, foreground = nullptr;
    for (auto row = 0; row < rows; row++) {
        const int y = sy + row * g_tableRowHeight;
        int x = sx;
        taskbar::WindowInfo wInfo;
        if (row > 0)
            wInfo = g_windowsCache[row - 1];
        for (auto col = 0; col < W_GRID_MAX_COLUMNS; col++) {
            constexpr int textLeftMargin = 10;
            std::wstring value;
            RECT rect = utils::rect(x + textLeftMargin, y + 1, g_tableColumnWidths[col] - textLeftMargin, g_tableRowHeight - 1);
            if (row == 0) { // Header
                value = g_tableHeaders[col];
                const auto oldFont = SelectObject(hdc, g_hTableFontBold);
                if (g_tableCollapsableHeaders[col]) {
                    win_draw::drawCheckBox(hdc, !g_tableHeaderCheckboxCollapseStates[col], rect, value.c_str(), background, foreground);
                    g_trackableCheckBoxes[col] = rect;
                } else {
                    DrawTextW(hdc, value.c_str(), -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
                SelectObject(hdc, oldFont);
            } else {
                value = WTBH_getWindowValue(wInfo, col);
                if (!value.empty()) {
                    if (wInfo.detected) {
                        const auto oldTextColor = SetTextColor(hdc, C_RED);
                        DrawTextW(hdc, value.c_str(), -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                        SetTextColor(hdc, oldTextColor);
                    } else {
                        DrawTextW(hdc, value.c_str(), -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                    }
                }
            }
            x += g_tableColumnWidths[col];
        }
    }
    DeleteObject(background);
    DeleteObject(foreground);
}

inline void WTBH_updateTable(HDC hdc, const bool onlyPaintGrid) {
    if (g_tableDefaultColumnWidths[0] == 0)
        WTBH_calculateDefaultWidths(hdc);

    const int y = WSC_GRID_Y - g_gridScroll.scrollYPos;
    const int rows = g_windowsCacheSize + 1; // +header

    SelectObject(hdc, g_hTableFont);
    SetBkMode(hdc, TRANSPARENT);
    if (!onlyPaintGrid)
        WTBH_calculateCurrentWidths(hdc, rows);
    WTBH_paintGrid(hdc, 10 - g_gridScroll.scrollXPos, y, rows);
    if (!onlyPaintGrid)
        WTBH_printDataToGrid(hdc, 10 - g_gridScroll.scrollXPos, y, rows);

    g_gridScroll.updateXYScrollBarsInfo();
}

struct WTBH_CHECKBOX_DATA {
    std::atomic<bool>* pState;
    bool disabled;
};

inline LONG_PTR WTBH_makeCheckboxData(std::atomic<bool> *pState, const bool disabled = false) {
    return reinterpret_cast<LONG_PTR>(new WTBH_CHECKBOX_DATA {
        pState,
        disabled
    });
}

inline void WTBH_resizeChildWindows(HWND hWnd) {
    HCURSOR lPtrHandCursor = LoadCursor(nullptr, IDC_HAND);
    const auto hdc = GetDC(hWnd);
    int i = 0;

    const bool createWindows = g_childWindows.empty();

    // Update button
    std::wstring text = utils::message(MSG_WND_HEADER_TABLE_UPDATE);
    int lastWidth = win_draw::calculateTextWidth(hdc, text, g_hDefaultFont) + WSC_BUTTON_X_MARGIN * 2;
    RECT rect = { 10, 10, lastWidth, WSC_BUTTON_DEFAULT_H };
    if (createWindows) {
        const auto hButtonUpdate = CreateWindowExW(
            0, WMC_BUTTON, text.c_str(),
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rect.left, rect.top, rect.right, rect.bottom,
            hWnd, reinterpret_cast<HMENU>(ID_BUTTON_UPDATE), globals::hIns, nullptr);
        SendMessageW(hButtonUpdate, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), true);
        SendMessageW(hButtonUpdate, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
        SetClassLongPtrW(hButtonUpdate, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(lPtrHandCursor));
        g_childWindows.push_back(hButtonUpdate);
    } else {
        SetWindowTextW(g_childWindows[i], text.c_str());
        MoveWindow(g_childWindows[i], rect.left, rect.top, rect.right, rect.bottom, true);
        i++;
    }

    // Actions button
    text = utils::message(MSG_WND_HEADER_ACTIONS);
    rect = { 0, 0, win_draw::calculateTextWidth(hdc, text, g_hDefaultFont) + WSC_BUTTON_X_MARGIN * 2, WSC_BUTTON_DEFAULT_H };
    if (createWindows) {
        g_hSettingsButton = CreateWindowExW(
            0, WMC_BUTTON, text.c_str(),
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rect.left, rect.top, rect.right, rect.bottom,
            hWnd, reinterpret_cast<HMENU>(ID_BUTTON_ACTIONS), globals::hIns, nullptr);
        SendMessageW(g_hSettingsButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), true);
        SendMessageW(g_hSettingsButton, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
        SetClassLongPtrW(g_hSettingsButton, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(lPtrHandCursor));
        g_childWindows.push_back(g_hSettingsButton);
    } else {
        SetWindowTextW(g_childWindows[i], text.c_str());
        MoveWindow(g_childWindows[i], g_windowWidth - rect.right - 10, 10, rect.right, rect.bottom, true);
        i++;
    }

#if IS_PORTABLE
    // Install button
    text = utils::message(MSG_WND_HEADER_INSTALL);
    rect = { 0, 0, win_draw::calculateTextWidth(hdc, text, g_hDefaultFont) + WSC_BUTTON_X_MARGIN * 2, WSC_BUTTON_DEFAULT_H };
    if (createWindows) {
        g_hInstallButton = CreateWindowExW(
            0, WMC_BUTTON, text.c_str(),
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rect.left, rect.top, rect.right, rect.bottom,
            hWnd, reinterpret_cast<HMENU>(ID_BUTTON_INSTALL), globals::hIns, nullptr);
        SendMessageW(g_hInstallButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), true);
        SendMessageW(g_hInstallButton, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
        SetClassLongPtrW(g_hInstallButton, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(lPtrHandCursor));
        g_childWindows.push_back(g_hInstallButton);
    } else {
        SetWindowTextW(g_childWindows[i], text.c_str());
        RECT sRect;
        GetWindowRect(g_hSettingsButton, &sRect);
        MoveWindow(g_childWindows[i], g_windowWidth - rect.right - 20 - (sRect.right - sRect.left), 10, rect.right, rect.bottom, true);
        i++;
    }
#endif
    int lastX = 10 + lastWidth + WSC_CHECKBOX_SPACING;

    // Auto update check box
    text = utils::message(MSG_WND_HEADER_AUTO_UPDATE);
    lastWidth = win_draw::calculateTextWidth(hdc, text, g_hDefaultFont) + WSC_CHECKBOX_TEXT_OFFSET;
    rect = { lastX, 10, lastWidth, WSC_BUTTON_DEFAULT_H };
    if (createWindows) {
        const auto hCheckBoxAutoUpdate = CreateWindowExW(
            0, WMC_BUTTON, text.c_str(),
            WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_OWNERDRAW,
            rect.left, rect.top, rect.right, rect.bottom,
            hWnd, reinterpret_cast<HMENU>(ID_CHECKBOX_AUTO_UPDATE), globals::hIns, nullptr);
        SendMessageW(hCheckBoxAutoUpdate, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), true);
        SendMessageW(hCheckBoxAutoUpdate, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
        SetClassLongPtrW(hCheckBoxAutoUpdate, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(lPtrHandCursor));
        SetWindowLongPtrW(hCheckBoxAutoUpdate, GWLP_USERDATA, WTBH_makeCheckboxData(&config::autoUpdate));
        g_childWindows.push_back(hCheckBoxAutoUpdate);
    } else {
        SetWindowTextW(g_childWindows[i], text.c_str());
        MoveWindow(g_childWindows[i], rect.left, rect.top, rect.right, rect.bottom, true);
        i++;
    }
    lastX += lastWidth + WSC_CHECKBOX_SPACING;

    // Dark mode check box
    text = utils::message(MSG_WND_HEADER_DARK_MODE);
    lastWidth = win_draw::calculateTextWidth(hdc, text, g_hDefaultFont) + WSC_CHECKBOX_TEXT_OFFSET;
    rect = { lastX, 10, lastWidth, WSC_BUTTON_DEFAULT_H };
    if (createWindows) {
        const auto hCheckboxDarkMode = CreateWindowExW(
            0,
            WMC_BUTTON, text.c_str(),
            WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_OWNERDRAW,
            rect.left, rect.top, rect.right, rect.bottom,
            hWnd, reinterpret_cast<HMENU>(ID_CHECKBOX_DARK_MODE), globals::hIns, nullptr);
        SendMessageW(hCheckboxDarkMode, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), true);
        SendMessageW(hCheckboxDarkMode, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
        SetClassLongPtrW(hCheckboxDarkMode, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(lPtrHandCursor));
        SetWindowLongPtrW(hCheckboxDarkMode, GWLP_USERDATA, WTBH_makeCheckboxData(&config::darkMode));
        g_childWindows.push_back(hCheckboxDarkMode);
    } else {
        SetWindowTextW(g_childWindows[i], text.c_str());
        MoveWindow(g_childWindows[i], rect.left, rect.top, rect.right, rect.bottom, true);
        i++;
    }
    lastX += lastWidth + WSC_CHECKBOX_SPACING;

    // Show all windows check box
    text = utils::message(MSG_WND_HEADER_SHOW_ALL_WINDOWS);
    lastWidth = win_draw::calculateTextWidth(hdc, text, g_hDefaultFont) + WSC_CHECKBOX_TEXT_OFFSET;
    rect = { lastX, 10, lastWidth, WSC_BUTTON_DEFAULT_H };
    if (createWindows) {
        g_hCBShowAllWindows = CreateWindowExW(
            0, WMC_BUTTON, text.c_str(),
            WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_OWNERDRAW,
            rect.left, rect.top, rect.right, rect.bottom,
            hWnd, reinterpret_cast<HMENU>(ID_CHECKBOX_SHOW_ALL_WINDOWS), globals::hIns, nullptr);
        SendMessageW(g_hCBShowAllWindows, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), true);
        SendMessageW(g_hCBShowAllWindows, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
        SetClassLongPtrW(g_hCBShowAllWindows, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(lPtrHandCursor));
        SetWindowLongPtrW(g_hCBShowAllWindows, GWLP_USERDATA, WTBH_makeCheckboxData(&config::showAllWindows));
        g_childWindows.push_back(g_hCBShowAllWindows);
    } else {
        SetWindowTextW(g_childWindows[i], text.c_str());
        MoveWindow(g_childWindows[i], rect.left, rect.top, rect.right, rect.bottom, true);
    }

    DeleteObject(hdc);
}

inline void WTBH_update(const bool ignorePreviousWindows, const bool ignoreGUIChecks) {
    taskbar::clearForcedVisibilityStates();
    taskbar::collectWindowData(ignorePreviousWindows, ignoreGUIChecks);
}

bool WTBH_getWindowsBuild(DWORD& build) {
    typedef LONG (WINAPI *RtlGetVersion_t)(PRTL_OSVERSIONINFOW);

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        return false;
    }

    auto rtl = reinterpret_cast<RtlGetVersion_t>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (!rtl) {
        return false;
    }

    RTL_OSVERSIONINFOW vi = { sizeof(vi) };
    if (rtl(&vi) != 0) {
        return false;
    }

    build = vi.dwBuildNumber;
    return true;
}

LRESULT CALLBACK WndProc(HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) {
    static NOTIFYICONDATA nid = {};
    static bool minimizedNotifShowed = false;
    long *scrollPosition;

    switch (uMsg) {
        case WM_CREATE:
        {
            WTBH_updateLanguage(nullptr);

            // Get icon for system tray
            const auto pcs = reinterpret_cast<CREATESTRUCT *>(lParam);
            const auto hTrayIcon = static_cast<HICON>(pcs->lpCreateParams);
            // Create system tray icon
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hWnd;
            nid.uID = 1;
            nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            nid.uCallbackMessage = WM_TRAY_ICON;
            nid.hIcon = hTrayIcon;
            // Set icon tip
            lstrcpyW(nid.szTip, VER_FILEDESCRIPTION_STR);

            const auto create = reinterpret_cast<LPCREATESTRUCT>(lParam);

            // Create base (default) font
            g_hDefaultFont = CreateFontW(
                16,
                0,
                0,
                0,
                FW_NORMAL,
                false,
                false,
                false,
                ANSI_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Sonoran Sans Serif"
            );

            LOGFONT lf;
            // Re-create default font but bold
            GetObjectW(g_hDefaultFont, sizeof(LOGFONT), &lf);
            lf.lfWeight = FW_BOLD;
            g_hDefaultFontBold = CreateFontIndirectW(&lf);

            // Re-create default font for table but with different face name
            GetObjectW(g_hDefaultFont, sizeof(LOGFONT), &lf);
            // By doing this character widths might be incorrect, but for now I didn't notice anything (yet)
            wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Consolas");
            g_hTableFont = CreateFontIndirectW(&lf);

            // Re-create table font but bold
            GetObjectW(g_hTableFont, sizeof(LOGFONT), &lf);
            lf.lfWeight = FW_BOLD;
            g_hTableFontBold = CreateFontIndirectW(&lf);

            WTBH_resizeChildWindows(hWnd);

            g_gridScroll.hYScrollBar = CreateWindowExW(
                WS_EX_LAYERED, WMC_SCROLLBAR, nullptr,
                WS_CHILD | WS_VISIBLE | SBS_VERT,
                -WSC_SCROLLBAR_WIDTH, WSC_HEADER, WSC_SCROLLBAR_WIDTH, 0,
                hWnd, reinterpret_cast<HMENU>(ID_SCROLLBAR_Y), create->hInstance, nullptr);
            SendMessageW(g_gridScroll.hYScrollBar, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), true);
            SendMessageW(g_gridScroll.hYScrollBar, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
            SetLayeredWindowAttributes(g_gridScroll.hYScrollBar, 0, 1, LWA_ALPHA);

            g_gridScroll.hXScrollBar = CreateWindowExW(
                WS_EX_LAYERED, WMC_SCROLLBAR, nullptr,
                WS_CHILD | WS_VISIBLE | SBS_HORZ,
                0, 0, 0, 0,
                hWnd, reinterpret_cast<HMENU>(ID_SCROLLBAR_X), create->hInstance, nullptr);
            SendMessageW(g_gridScroll.hXScrollBar, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
            SetLayeredWindowAttributes(g_gridScroll.hXScrollBar, 0, 1, LWA_ALPHA);

            // Add icon
            Shell_NotifyIconW(NIM_ADD, &nid);

            WTBH_update(true, true);
            break;
        }
        case WM_SIZE:
        {
            if (wParam == SIZE_MINIMIZED) {
                if (config::minimizeToTray) {
                    ShowWindow(hWnd, SW_HIDE);
                    if (!minimizedNotifShowed) {
                        utils::showTrayNotification(utils::message(MSG_MINIMIZED_TO_TRAY));
                        minimizedNotifShowed = true;
                    }
                } else
                    return DefWindowProc(hWnd, uMsg, wParam, lParam);
            } else {
                g_windowWidth = LOWORD(lParam);
                g_windowHeight = HIWORD(lParam);
                // Reposition scrollbar
                MoveWindow(g_gridScroll.hYScrollBar, g_windowWidth - WSC_SCROLLBAR_WIDTH, WSC_HEADER, WSC_SCROLLBAR_WIDTH, g_windowHeight - WSC_HEADER, true);
                MoveWindow(g_gridScroll.hXScrollBar, 0, g_windowHeight - WSC_SCROLLBAR_WIDTH, g_windowWidth - WSC_SCROLLBAR_WIDTH, WSC_SCROLLBAR_WIDTH, true);
                RECT rect;
                GetWindowRect(g_hSettingsButton, &rect);
                int width = rect.right - rect.left;
                MoveWindow(g_hSettingsButton, g_windowWidth - width - 10, 10, width, rect.bottom - rect.top, true);
#if IS_PORTABLE
                GetWindowRect(g_hInstallButton, &rect);
                MoveWindow(g_hInstallButton, g_windowWidth - (rect.right - rect.left) - 20 - width, 10, rect.right - rect.left, rect.bottom - rect.top, true);
#endif
                GetClientRect(hWnd, &rect);
                g_windowClientHeight = rect.bottom - rect.top;
                g_windowClientWidth = rect.right - rect.left;

                g_gridScroll.updateXYScrollBarsInfo();
                win_draw::redrawWindow(hWnd);
            }
            break;
        }
        case WM_LBUTTONDOWN:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (pt.y <= WSC_HEADER) // Checkbox is not visible
                break;
            for (auto i = 0; i < W_GRID_MAX_COLUMNS; i++) {
                const RECT rect = g_trackableCheckBoxes[i];
                if (PtInRect(&rect, pt)) {
                    g_tableHeaderCheckboxCollapseStates[i] = !g_tableHeaderCheckboxCollapseStates[i];
                    WTBH_redrawLowerArea(hWnd);
                    g_gridScroll.updateXScrollBarInfo();
                    break;
                }
            }
            break;
        }
        case WM_ACTIVATE:
        {
            g_focused = LOWORD(wParam) != WA_INACTIVE;
            break;
        }
        case WM_SHOWWINDOW:
        {
            if (!config::autoUpdateOnOpen)
                break;
            if (wParam && !(lParam & SW_PARENTCLOSING)) {
                WTBH_update(false, true);
            }
            break;
        }
        case WM_DRAWITEM:
        {
            const auto draw = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
            wchar_t text[256];
            GetWindowTextW(draw->hwndItem, text, sizeof(text));
            PAINTSTRUCT ps;
            switch (draw->CtlID) {
                case ID_BUTTON_ACTIONS:
#if IS_PORTABLE
                case ID_BUTTON_INSTALL:
#endif
                case ID_BUTTON_UPDATE:
                {
                    HDC mHdc = win_draw::doubleBuffering(hWnd, ps, draw->hDC, true, &g_windowClientWidth, &g_windowClientHeight);
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
                    win_draw::doubleBuffering(hWnd, ps, draw->hDC, false, &g_windowClientWidth, &g_windowClientHeight);
                    break;
                }
                case ID_CHECKBOX_TABLE_HEADER:
                case ID_CHECKBOX_SHOW_ALL_WINDOWS:
                case ID_CHECKBOX_DARK_MODE:
                case ID_CHECKBOX_AUTO_UPDATE:
                {
                    HDC mHdc = win_draw::doubleBuffering(hWnd, ps, draw->hDC, true, &g_windowClientWidth, &g_windowClientHeight);

                    HBRUSH background = nullptr, foreground = nullptr;
                    const auto pData = reinterpret_cast<WTBH_CHECKBOX_DATA*>(GetWindowLongPtrW(draw->hwndItem, GWLP_USERDATA));
                    SelectObject(mHdc, g_hDefaultFont);
                    win_draw::drawCheckBox(mHdc, pData && pData->pState->load(), draw->rcItem, text, background, foreground);
                    DeleteObject(background);
                    DeleteObject(foreground);
                    win_draw::doubleBuffering(hWnd, ps, draw->hDC, false, &g_windowClientWidth, &g_windowClientHeight);
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
            HDC mHdc = win_draw::doubleBuffering(hWnd, ps, nullptr, true, &g_windowClientWidth, &g_windowClientHeight);

            // Update colors for window
            SetBkColor(mHdc, WCP_BACKGROUND);
            SetTextColor(mHdc, WCP_FOREGROUND);
            FillRect(mHdc, &ps.rcPaint, WTBH_createBrush(WCP_BACKGROUND));
            WTBH_deleteLastBrush();

            // Above table text
            SetBkColor(mHdc, WCP_BACKGROUND);
            SelectObject(mHdc, g_hDefaultFont);
            win_draw::drawText(mHdc, utils::message(MSG_WND_DEBUG_TABLE_HEADER_TEXT), 10 - g_gridScroll.scrollXPos, 80 - g_gridScroll.scrollYPos);

            // Table
            WTBH_updateTable(mHdc, false);

            // Header
            RECT wRect;
            GetWindowRect(hWnd, &wRect);
            wRect.left = 0;
            wRect.top = 0;
            wRect.bottom = WSC_HEADER;
            FillRect(mHdc, &wRect, WTBH_createBrush(WCP_BACKGROUND2));
            WTBH_deleteLastBrush();

            SetBkColor(mHdc, WCP_BACKGROUND2);
            SelectObject(mHdc, g_hDefaultFont);

            // Header text
            win_draw::drawText(mHdc, utils::message(MSG_WND_HEADER_TEXT), 10, 45);

            // Table update time text
            std::wstring text = utils::message(MSG_WND_LAST_UPDATE_AT) + L" ";
            if (g_lastUpdatedTimeTextWidth == 0)
                g_lastUpdatedTimeTextWidth = win_draw::calculateTextWidth(mHdc, g_lastTableUpdateTime, g_hDefaultFontBold);
            win_draw::drawText(mHdc, text, g_windowClientWidth - win_draw::calculateTextWidth(mHdc, text, g_hDefaultFont) - g_lastUpdatedTimeTextWidth - 10, 45);
            auto oldFont = SelectObject(mHdc, g_hDefaultFontBold);
            win_draw::drawText(mHdc, g_lastTableUpdateTime, g_windowClientWidth - g_lastUpdatedTimeTextWidth - 10, 45);
            SelectObject(mHdc, oldFont);

            // Draw scrollbars
            g_gridScroll.drawScrollBars(mHdc, g_hDefaultFontBold);

            win_draw::doubleBuffering(hWnd, ps, nullptr, false, &g_windowClientWidth, &g_windowClientHeight);
            break;
        }
        case WM_TRAY_ICON:
        {
            if (lParam == WM_LBUTTONUP) {
                if (!IsWindowVisible(hWnd))
                    ShowWindow(hWnd, SW_SHOWNORMAL);
                else if (IsIconic(hWnd))
                    ShowWindow(hWnd, SW_RESTORE);
                else
                    ShowWindow(hWnd, SW_SHOW);
                SetForegroundWindow(hWnd);
            } else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                HMENU hMenu = CreatePopupMenu();
                if (lParam != WM_CONTEXTMENU)
                    AppendMenuW(hMenu, MF_STRING | MF_DISABLED, ID_TRAY_HEADER, TRAY_TITLE);
                AppendMenuW(hMenu, MF_STRING | (config::exists() ? 0 : MF_DISABLED), ID_TRAY_OPEN_CONFIG, utils::message(MSG_TRAY_CONFIG_OPEN).c_str());
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_RELOAD_CONFIG, utils::message(MSG_TRAY_CONFIG_RELOAD).c_str());
                if (lParam == WM_CONTEXTMENU)
                    AppendMenuW(hMenu, MF_STRING | (config::exists() ? 0 : MF_DISABLED), ID_TRAY_EXPOSE_INTERNALS, utils::message(MSG_TRAY_CONFIG_EXPOSE_INTERNALS).c_str());
                AppendMenuW(hMenu, MF_STRING | (config::directoryExists() ? 0 : MF_DISABLED), ID_TRAY_OPEN_CONFIG_DIR, utils::message(MSG_TRAY_CONFIG_OPEN_DIR).c_str());
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_PAUSE_HIDER, utils::message(globals::taskbarLoopRunState ?  MSG_TRAY_TB_PAUSE : MSG_TRAY_TB_RESUME).c_str());
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
#if IS_PORTABLE
                AppendMenuW(hMenu, MF_STRING | MF_DISABLED, ID_TRAY_ADD_REMOVE_STARTUP, std::wstring(utils::message(MSG_TRAY_ADD_STARTUP) + L" (" + utils::message(MSG_NOT_SUPPORTED) + L")").c_str());
#else
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_ADD_REMOVE_STARTUP, utils::message(utils::doesAutoStart() ? MSG_TRAY_REM_STARTUP : MSG_TRAY_ADD_STARTUP).c_str());
#endif
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, ID_TRAY_GITHUB, utils::message(MSG_TRAY_OPEN_GITHUB).c_str());
                if (lParam != WM_CONTEXTMENU) {
                    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, utils::message(MSG_TRAY_EXIT).c_str());
                }
                POINT p;
                if (lParam == WM_CONTEXTMENU) {
                    RECT rect;
                    GetWindowRect(g_hSettingsButton, &rect);
                    p.x = rect.left;
                    p.y = rect.top + WSC_BUTTON_DEFAULT_H;
                } else {
                    GetCursorPos(&p);
                }
                SetForegroundWindow(hWnd);
                TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, p.x, p.y, 0, hWnd, nullptr);
                DestroyMenu(hMenu);
            }
            break;
        }
        case WM_TASKBAR_THREAD_ERROR: {
            auto what = reinterpret_cast<std::string*>(lParam);
            std::wstring message;
            if (*what == "stoi") {
                message = utils::message(MSG_CONFIG_NOT_NUMBER);
            } else {
                message = std::wstring(what->begin(), what->end());
            }
            delete what;
            utils::messageBox(MSG_TASKBAR_THREAD_EXCEPTION_OCCURRED, MB_ICONERROR | MB_OK, { message });
            break;
        }
        case WM_UPDATE_GRID_REQUEST:
        {
            const auto pData = reinterpret_cast<WTBH_CHECKBOX_DATA*>(GetWindowLongPtrW(g_hCBShowAllWindows, GWLP_USERDATA));
            pData->disabled = false;
            const auto unpackedLParam = static_cast<UINT_PTR>(lParam);
            const bool ignoreChecks = unpackedLParam & 1u;
            auto pWindows = reinterpret_cast<std::vector<taskbar::WindowInfo>*>(wParam);
            if (!pWindows || (!ignoreChecks && (!IsWindowVisible(hWnd) || (config::autoUpdate && !g_focused && config::disableAutoUpdateWhenUnfocused)))) {
                delete pWindows;
                break;
            }
            g_lastTableUpdateTime = utils::getFormattedTime();
            g_windowsCacheSize = static_cast<int>(unpackedLParam) >> 1;
            g_windowsCache = std::move(*pWindows);
            delete pWindows;
            WTBH_redrawLowerArea(hWnd);
            g_gridScroll.updateXYScrollBarsInfo();
            break;
        }
        case WM_WTSSESSION_CHANGE: {
            if (wParam == WTS_SESSION_LOCK) {
                // Pause taskbar loop when user locks the session
                globals::taskbarLoopRunState = false;
                globals::sessionLocked = true;
            } else if (wParam == WTS_SESSION_UNLOCK) {
                // Continue taskbar loop when user log-ins into the same session where the application was originally ran from
                globals::taskbarLoopRunState = true;
                globals::sessionLocked = false;
            }
            break;
        }
        case WM_POWERBROADCAST: {
            if (globals::sessionLocked)
                break; // No need if session is locked
            if (wParam == PBT_APMSUSPEND) {
                // Pause taskbar loop when computer is about to go to a suspend (sleep)
                globals::taskbarLoopRunState = false;
            } else if (wParam == PBT_APMRESUMESUSPEND || wParam == PBT_APMRESUMEAUTOMATIC) {
                // Continue taskbar loop when computer wakes up from normal suspend or due to timed/LAN wake up
                globals::taskbarLoopRunState = true;
            }
            break;
        }
        case WM_COMMAND:
        {
            WTBH_CHECKBOX_DATA* pData = nullptr;
            const auto lwParam = LOWORD(wParam);
            if (lwParam >= IDS_CHECKBOX_MIN && lwParam <= IDS_CHECKBOX_MAX) { // Check if clicked component falls into the range of checkbox IDs
                HWND hCheckbox = GetDlgItem(hWnd, lwParam);
                pData = reinterpret_cast<WTBH_CHECKBOX_DATA*>(GetWindowLongPtrW(hCheckbox, GWLP_USERDATA));
                if (pData) {
                    if (pData->disabled)
                        break;
                    // Inverse checkbox state
                    pData->pState->store(!pData->pState->load());
                    if (lwParam != ID_CHECKBOX_DARK_MODE) // No need for updating, as dark mode check already redraws the window
                        win_draw::redrawWindow(hCheckbox);
                    else
                        win_draw::updateTitlebarColors(globals::hWnd);
                }
            }
            switch (lwParam) {
                case ID_BUTTON_ACTIONS:
                {
                    SendMessageW(hWnd, WM_TRAY_ICON, 0, WM_CONTEXTMENU);
                    break;
                }
                case ID_CHECKBOX_SHOW_ALL_WINDOWS:
                {
                    if (pData)
                        pData->disabled = true;
                }
                case ID_BUTTON_UPDATE:
                {
                    WTBH_update(true, true);
                    break;
                }
#if IS_PORTABLE
                case ID_BUTTON_INSTALL:
                {
                    ShellExecute(nullptr, L"open", GITHUB_RELEASES_LINK, nullptr, nullptr, SW_SHOWNORMAL);
                    break;
                }
#endif
                case ID_CHECKBOX_AUTO_UPDATE:
                {
                    g_gridScroll.scrollXPos = 0;
                    g_gridScroll.scrollYPos = 0;
                    win_draw::redrawWindow(hWnd);
                    g_gridScroll.updateXYScrollBarsInfo();
                    break;
                }
                case ID_CHECKBOX_DARK_MODE:
                {
                    win_draw::redrawWindow(hWnd);
                    break;
                }
                // Next ones are for system tray and actions button menu
                case ID_TRAY_EXIT:
                {
                    DestroyWindow(hWnd);
                    break;
                }
                case ID_TRAY_OPEN_CONFIG:
                {
                    config::open();
                    break;
                }
                case ID_TRAY_OPEN_CONFIG_DIR: {
                    config::openDirectory();
                    break;
                }
                case ID_TRAY_EXPOSE_INTERNALS:
                {
                    config::save(true);
                    // No need for break
                }
                case ID_TRAY_RELOAD_CONFIG:
                {
#if IS_PORTABLE
                    if (!config::exists()) {
                        config::save(false);
                        utils::messageBox(MSG_CONFIG_RELOADED, MB_ICONINFORMATION | MB_OK);
                        break;
                    }
#endif
                    if (config::load())
                        utils::messageBox(MSG_CONFIG_RELOADED, MB_ICONINFORMATION | MB_OK);
                    else
                        utils::messageBox(MSG_CONFIG_RELOADED_WITH_ERRORS, MB_ICONWARNING | MB_OK);
                    minimizedNotifShowed = false;
                    WTBH_updateLanguage(hWnd);
                    win_draw::redrawWindow(hWnd);
                    // taskbar::resetTaskbar();
                    taskbar::clearErrorState();
                    break;
                }
                case ID_TRAY_PAUSE_HIDER:
                {
                    globals::taskbarLoopRunState = !globals::taskbarLoopRunState;
                    if (globals::taskbarLoopRunState)
                        taskbar::resumeTaskbar();
                    break;
                }
#ifndef IS_PORTABLE
                case ID_TRAY_ADD_REMOVE_STARTUP:
                {
                    utils::toggleStartup();
                    break;
                }
#endif
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
        case WM_HSCROLL:
        case WM_VSCROLL: {
            HWND hScrollBar = reinterpret_cast<HWND>(lParam);
            if (!hScrollBar)
                break;
            if (int scrollBarID = GetDlgCtrlID(hScrollBar); scrollBarID == ID_SCROLLBAR_X) {
                scrollPosition = &g_gridScroll.scrollXPos;
            } else if (scrollBarID == ID_SCROLLBAR_Y) {
                scrollPosition = &g_gridScroll.scrollYPos;
            } else {
                break;
            }

            switch (LOWORD(wParam))
            {
                case SB_LINEUP:     *scrollPosition -= 10;  break; // Arrow up
                case SB_LINEDOWN:   *scrollPosition += 10;  break; // Arrow down
                case SB_PAGEUP:     *scrollPosition -= 100; break; // Click upper thumb
                case SB_PAGEDOWN:   *scrollPosition += 100; break; // Click lower thumb
                case SB_THUMBTRACK: {
                    SCROLLINFO si;
                    si.cbSize = sizeof(SCROLLINFO);
                    si.fMask = SIF_TRACKPOS;
                    GetScrollInfo(hScrollBar, SB_CTL, &si);
                    *scrollPosition = si.nTrackPos;
                    break;
                }
                default: break;
            }

            g_gridScroll.updateXYScrollBarsInfo();
            WTBH_redrawLowerArea(hWnd);
            break;
        }
        case WM_MOUSEWHEEL:
        {
            scrollPosition = GetKeyState(VK_SHIFT) & KF_UP ? &g_gridScroll.scrollXPos : &g_gridScroll.scrollYPos;
            const int oldPos = *scrollPosition;
            *scrollPosition += -(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * (WSC_SCROLL_ROWS * g_tableRowHeight));
            g_gridScroll.updateXYScrollBarsInfo();
            if (oldPos == *scrollPosition)
                break;
            WTBH_redrawLowerArea(hWnd);
            break;
        }
        case WM_GETMINMAXINFO: {
            const auto lpMinMaxInfo = reinterpret_cast<LPMINMAXINFO>(lParam);
            lpMinMaxInfo->ptMinTrackSize.x = APP_WINDOW_MIN_WIDTH;
            lpMinMaxInfo->ptMinTrackSize.y = APP_WINDOW_MIN_HEIGHT;
            break;
        }
        case WM_DISPLAYCHANGE: {
            // When new display gets (dis)connected
            monitors::indexMonitors();
            taskbar::findTaskbarHandles();
            taskbar::clearForcedVisibilityStates();
            break;
        }
        case WM_CLOSE: {
            if (config::closeConfirmMessage && !config::closeToTray) {
                if (utils::messageBox(MSG_WINDOW_CLOSE_CONFIRMATION, MB_ICONQUESTION | MB_YESNO) == 7)
                    break;
            } else if (config::closeToTray) {
                ShowWindow(hWnd, SW_HIDE);
                if (!minimizedNotifShowed) {
                    utils::showTrayNotification(utils::message(MSG_MINIMIZED_TO_TRAY));
                    minimizedNotifShowed = true;
                }
                break;
            }
            DestroyWindow(hWnd);
            break;
        }
        case WM_DESTROY: {
            DeleteObject(g_hDefaultFont);
            DeleteObject(g_hDefaultFontBold);
            DeleteObject(g_hTableFont);
            DeleteObject(g_hTableFontBold);
            DeleteObject(g_gridScroll.hYScrollBar);
            DeleteObject(g_gridScroll.hXScrollBar);
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
        }
        // case WM_ERASEBKGND: {
        //     return 1;
        // }
        case WM_TRIGGER: {
            ShowWindow(hWnd, SW_SHOWNORMAL);
            SetForegroundWindow(hWnd);
            return 727;
        }
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 1;
}

void signalHandler(const int signum) {
    MessageBox(nullptr, L"CRITICAL: Segmentation fault (SIGSEGV) occurred!", VER_FILEDESCRIPTION_STR, MB_OK | MB_ICONERROR);
    exit(signum);
}

LONG WINAPI CrashHandler(const EXCEPTION_POINTERS* pException) {
    signal(SIGSEGV, signalHandler);
    globals::hWnd = nullptr;
    utils::showExceptionMessageBox([pException](std::wstringstream& crashInfo) {
        const EXCEPTION_RECORD* record = pException->ExceptionRecord;
        LPWSTR lpwstr = utils::NTStatusMessageToText(record->ExceptionCode);
        // A workaround, EXCEPTION_ACCESS_VIOLATION returns this message:
        // "The instruction at 0xp referenced memory at 0xp. The memory could not be s."
        // There are missing %, but p and s letters are not being used in many words, so we can just replace them (by skipping few first letters)
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
            if (record->NumberParameters > 0)
                lpwstr = utils::replaceCharacterWithText(lpwstr, 'p', std::to_wstring(record->ExceptionInformation[1]));
            if (record->NumberParameters > 1)
                lpwstr = utils::replaceCharacterWithText(lpwstr, 'p', std::to_wstring(record->ExceptionInformation[2]));
        } else if (record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {
            // I hope these are correct
            // Message: "The instruction at 0xp referenced memory at 0xp. The required data was not placed into memory because of an I/O error status of 0xx."
            if (record->NumberParameters > 0)
                lpwstr = utils::replaceCharacterWithText(lpwstr, 'p', std::to_wstring(record->ExceptionInformation[0]));
            if (record->NumberParameters > 1)
                lpwstr = utils::replaceCharacterWithText(lpwstr, 'p', std::to_wstring(record->ExceptionInformation[1]));
            if (record->NumberParameters > 2)
                lpwstr = utils::replaceCharacterWithText(lpwstr, 'x', std::to_wstring(record->ExceptionInformation[2]), 3);
        }
        crashInfo << utils::message(MSG_UNCAUGHT_EXCEPTION_WILL_TERMINATE) << std::endl;
        crashInfo << utils::message(MSG_UNCAUGHT_EXCEPTION_TRANSLATED_MSG, {utils::exceptionName(record->ExceptionCode)}) << std::endl;
        crashInfo << std::endl;
        crashInfo << (lpwstr != nullptr ? lpwstr : utils::message(MSG_UNCAUGHT_EXCEPTION_TRANSLATION_FAILED)) << std::endl;
        crashInfo << utils::message(MSG_UNCAUGHT_EXCEPTION_INFO) << std::endl;
        crashInfo << "  " << utils::message(MSG_UNCAUGHT_EXCEPTION_INFO_CODE) << " 0x" << std::hex << record->ExceptionCode << std::endl;
        crashInfo << "  " << utils::message(MSG_UNCAUGHT_EXCEPTION_INFO_ADDR) << " " << record->ExceptionAddress << std::endl;
        LocalFree(lpwstr);
    }, true);
    taskbar::resetTaskbar();
    return EXCEPTION_EXECUTE_HANDLER;
}

LRESULT CALLBACK KeyboardEventProc(const int nCode, const WPARAM wParam, const LPARAM lParam) {
    if (nCode == HC_ACTION) {
        const auto pKeyboard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && pKeyboard->vkCode == VK_ESCAPE)
            taskbar::clearForcedVisibilityStates();
    }
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hWnd, LONG idObject, LONG, DWORD, DWORD)
{
    if (idObject != OBJID_WINDOW)
        return;
    if (event != EVENT_OBJECT_SHOW && event != EVENT_OBJECT_HIDE && event != EVENT_OBJECT_CLOAKED && event != EVENT_OBJECT_UNCLOAKED)
        return;
    std::wstring className(256, L'\0');
    const int len = GetClassName(hWnd, className.data(), static_cast<int>(className.size()));
    className.resize(len);
    std::wstring proc;
    utils::getProcessInfo(hWnd, proc, true);
    // ahk_class Windows.UI.Core.CoreWindow -- search
    // ahk_exe SearchHost.exe

    // ahk_class Windows.UI.Core.CoreWindow -- start menu
    // ahk_exe StartMenuExperienceHost.exe

    // ahk_class Windows.UI.Core.CoreWindow -- context menu of app (jump lists), time, ethernet
    // ahk_exe ShellExperienceHost.exe

    // ahk_class Shell_InputSwitchTopLevelWindow -- language
    // ahk_exe explorer.exe

    // ahk_class Xaml_WindowedPopupClass -- context menu of taskbar, popups for app names
    // ahk_exe explorer.exe

    // ahk_class TopLevelWindowForOverflowXamlIsland -- tray icons
    // ahk_exe explorer.exe

    // ahk_class XamlExplorerHostIslandWindow -- app preview, volume
    // ahk_exe explorer.exe

    // ahk_class Xaml_WindowedPopupClass -- popup for various elements on taskbar
    // ahk_exe explorer.exe

    switch (event) {
        // TODO: For Windows 10 but this was before I knew cloak/uncloak events existed
        // Detecting start menu via hide/show events won't work, it uses DWM attributes,
        // and it seems they trigger location change? (the location is always the same)
        // case EVENT_OBJECT_LOCATIONCHANGE: {
        //     if (className == L"Windows.UI.Core.CoreWindow" && config::exceptTaskbarPopups) {
        //         utils::getProcessInfo(hWnd, proc);
        //         if (proc == L"StartMenuExperienceHost.exe" || proc == L"SearchApp.exe" || proc == L"SearchHost.exe") {
        //             HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        //             DWORD cloaked = 0;
        //             if (const HRESULT hr = DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)); SUCCEEDED(hr)) {
        //                 std::lock_guard lock(taskbar::taskbarMutex);
        //                 taskbar::taskbarForcedVisibilityStates[hMonitor] = cloaked == 0; // 0 when opened
        //             }
        //         }
        //     }
        //     break;
        // }
        case EVENT_OBJECT_SHOW: // 32770
        case EVENT_OBJECT_HIDE: // 32771
        case /* Invisible to user */ EVENT_OBJECT_CLOAKED: // 32791
        case /* Visible to user   */ EVENT_OBJECT_UNCLOAKED: { // 32792
            if (config::exceptTaskbarPopups) {
                if (std::ranges::find(config::I_ExceptionalWindows, proc + L":" + className) != config::I_ExceptionalWindows.end()) {
                    HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                    std::lock_guard lock(taskbar::taskbarMutex);
                    taskbar::taskbarForcedVisibilityStates[hMonitor] = event == EVENT_OBJECT_UNCLOAKED;
                    break;
                }
            }
        //     break;
        // }
        // case EVENT_OBJECT_SHOW: // 32770
        // case EVENT_OBJECT_HIDE: { // 32771
            // Fix taskbar app icon hover bug
            if (!IS_WINDOWS_11(globals::sysBuildNumber)) {
                if (config::fixTaskbarHoverGlitch && event == EVENT_OBJECT_SHOW && proc == L"explorer.exe" && className == L"tooltips_class32") {
                    HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                    std::lock_guard lock(taskbar::taskbarMutex);
                    if (!utils::mouseInWindow(taskbar::taskbarHandles[hMonitor]))
                        ShowWindow(hWnd, SW_HIDE);
                    break;
                }
                if (config::exceptTaskbarPopups) {
                    if (std::ranges::find(config::I_ExceptionalWindows, proc + L":" + className) != config::I_ExceptionalWindows.end()) {
                        HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                        std::lock_guard lock(taskbar::taskbarMutex);
                        taskbar::taskbarForcedVisibilityStates[hMonitor] = event == EVENT_OBJECT_SHOW;
                    } else if (LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE); style & WS_POPUP || className == L"#32768") {
                        // #32768 class seems to be used for menus
                        HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                        HWND taskbar = taskbar::taskbarHandles[hMonitor];
                        if (hWnd == taskbar) break;
                        RECT taskbarRect; GetWindowRect(taskbar, &taskbarRect); // Get taskbar location
                        RECT windowRect; GetWindowRect(hWnd, &windowRect); // Get window location
                        if (event == EVENT_OBJECT_SHOW &&
                            taskbarRect.left < windowRect.right &&
                            taskbarRect.right > windowRect.left &&
                            taskbarRect.top < windowRect.bottom &&
                            taskbarRect.bottom > windowRect.top) { // Check if two rects overlaps each other
                            std::lock_guard lock(taskbar::taskbarMutex);
                            taskbar::taskbarForcedVisibilityStates[hMonitor] = true;
                            break;
                            }
                        std::lock_guard lock(taskbar::taskbarMutex);
                        taskbar::taskbarForcedVisibilityStates[hMonitor] = false;
                    }
                }
            }
        }
        default: break;
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, const int nShowCmd) {
    HANDLE hMutex = CreateMutexW(nullptr, true, PROJECT_NAME);
    const DWORD hMutexLastError = GetLastError();

    WTBH_getWindowsBuild(globals::sysBuildNumber);
    config::init();

    globals::hIns = hInstance;
    SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(CrashHandler));

    int argc;
    LPWSTR commandLine = GetCommandLineW();
    LPWSTR *argv = CommandLineToArgvW(commandLine, &argc);
    if (!utils::processArguments(argc, argv, commandLine)) {
        LocalFree(argv);
        return 0;
    }
    LocalFree(argv);

    // Check user's preferences
    config::darkMode = utils::isUserUsingDarkTheme();

#ifndef IS_PORTABLE
    utils::exportLanguageFiles();
#endif

    if (!hMutex)
        utils::messageBox(MSG_MUTEX_FAILED, MB_ICONWARNING | MB_OK, { utils::NTStatusMessageToText(GetLastError()) });

    if (!hMutex || hMutexLastError == ERROR_ALREADY_EXISTS) {
        std::vector<DWORD> processIds;
        utils::getProcessesByName(globals::exe.c_str(), GetCurrentProcessId(), [&processIds](const PROCESSENTRY32W* pe) {
            processIds.push_back(pe->th32ProcessID);
        });

        const auto answer = utils::messageBox(MSG_APP_ALREADY_RUNNING, MB_ICONQUESTION | MB_YESNO);
        if (answer == IDYES) {
            for (const DWORD &id : processIds) {
                if (HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, false, id)) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                }
            }
            taskbar::resetTaskbar();
            CloseHandle(hMutex);
            return 0;
        }
        if (answer == IDNO) {
            bool success = false;
            EnumWindows([](HWND hWnd, const LPARAM lParam) -> BOOL {
                const auto terminateOthers = reinterpret_cast<bool*>(lParam);
                std::wstring className(256, L'\0');
                const int len = GetClassNameW(hWnd, className.data(), static_cast<int>(className.size()));
                className.resize(len);

                if (className == PROJECT_NAME) {
                    if (*terminateOthers) {
                        DWORD id;
                        GetWindowThreadProcessId(hWnd, &id);
                        if (HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, false, id)) {
                            TerminateProcess(hProcess, 0);
                            CloseHandle(hProcess);
                        }
                        return true;
                    }
                    DWORD_PTR ret;
                    SendMessageTimeoutW(hWnd, WM_TRIGGER, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK | SMTO_NOTIMEOUTIFNOTHUNG | SMTO_ERRORONEXIT, 0, &ret);
                    if (ret == 727) {
                        *terminateOthers = true;
                    }
                }
                return true;
            }, reinterpret_cast<LPARAM>(&success));
            CloseHandle(hMutex);
        }
        return 1;
    }

    if (!globals::noConfigFile) {
        config::load();
    }

    // Load icon
    HICON hIcon = LoadIconW(globals::hIns, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!hIcon) {
        utils::showExceptionMessageBox([](std::wstringstream& crashInfo) {
            crashInfo << utils::message(MSG_WINDOW_ICON_FAILED);
        }, true);
        CloseHandle(hMutex);
        // return 1;
    }

    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon         = hIcon;
    wc.lpszClassName = PROJECT_NAME;

    if (!RegisterClassExW(&wc)) {
        utils::showExceptionMessageBox([](std::wstringstream& crashInfo) {
            crashInfo << utils::message(MSG_WINDOW_REGISTER_FAILED);
        }, true);
        CloseHandle(hMutex);
        DestroyIcon(hIcon);
        return 1;
    }

    // Get monitor size
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Create window
    globals::hWnd = CreateWindowExW(
        WS_EX_CLIENTEDGE, PROJECT_NAME, utils::message(MSG_APPLICATION_NAME).c_str(),
        WS_OVERLAPPEDWINDOW,
        static_cast<short>((screenWidth - g_windowWidth) / 2),
        static_cast<short>((screenHeight - g_windowHeight) / 2),
        g_windowWidth, g_windowHeight,
        nullptr, nullptr,
        hInstance, hIcon);

    if (globals::hWnd == nullptr) {
        utils::showExceptionMessageBox([](std::wstringstream& crashInfo) {
            crashInfo << utils::message(MSG_WINDOW_CREATION_FAILED);
        }, true);
        CloseHandle(hMutex);
        DestroyIcon(hIcon);
        return 1;
    }

    win_draw::updateTitlebarColors(globals::hWnd);

    const auto windowPreviewsEventHook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_HIDE, nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    const auto windowCloakEventHook = SetWinEventHook(EVENT_OBJECT_CLOAKED, EVENT_OBJECT_UNCLOAKED, nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    monitors::indexMonitors();
    taskbar::findTaskbarHandles();

    taskbar::initThread();
    taskbar_animation::initThread();

    // Hook global keyboard listener
    g_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardEventProc, nullptr, 0);

    ShowWindow(globals::hWnd, !config::openOnStart ? SW_HIDE : nShowCmd);
    UpdateWindow(globals::hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (windowPreviewsEventHook)
        UnhookWinEvent(windowPreviewsEventHook);
    if (windowCloakEventHook)
        UnhookWinEvent(windowCloakEventHook);

    globals::isShuttingDown = true;
    if (taskbar::updateThread.joinable())
        taskbar::updateThread.join();

    if (taskbar_animation::animationThread.joinable())
        taskbar_animation::animationThread.join();

    if (g_hKeyboardHook)
        UnhookWindowsHookEx(g_hKeyboardHook);

    taskbar::resetTaskbar();
    DestroyIcon(hIcon);
    CloseHandle(hMutex);
    DestroyWindow(globals::hWnd);
    return static_cast<int>(msg.wParam);
}