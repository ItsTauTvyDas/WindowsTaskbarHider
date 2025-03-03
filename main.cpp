#include "taskbar.h"
#include "utils.h"
#include "config.h"
#include "globals.h"
#include "resources.h"
#include <dwmapi.h>
#include <windowsx.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <numeric>
#include "language.h"
#include "monitors.h"

#define WM_TRAY_ICON           (WM_USER + 1)

#define WCP_BASE_COLOR         config::darkMode ? darkColorPalette [0] : lightColorPalette[0]
#define WCP_FOREGROUND         config::darkMode ? darkColorPalette [1] : lightColorPalette[1]
#define WCP_BACKGROUND         config::darkMode ? darkColorPalette [2] : lightColorPalette[2]
#define WCP_BACKGROUND2        config::darkMode ? darkColorPalette [3] : lightColorPalette[3]
#define WCP_BUTTON_BG          config::darkMode ? darkColorPalette [4] : lightColorPalette[4]
#define WCP_BUTTON_BORDER      config::darkMode ? darkColorPalette [5] : lightColorPalette[5]
#define WCP_BUTTON_CLICKED_BG  config::darkMode ? darkColorPalette [6] : lightColorPalette[6]
#define WCP_SCROLLBAR_COLOR    config::darkMode ? darkColorPalette [7] : lightColorPalette[7]
#define WCP_SCROLLBAR_BG       config::darkMode ? darkColorPalette [8] : lightColorPalette[8]

#define WSC_BUTTON_DEFAULT_W      120
#define WSC_BUTTON_DEFAULT_H      30
#define WSC_HEADER                70
#define WSC_SCROLLBAR_WIDTH       17
#define WSC_GRID_Y                100
#define WSC_GRID_TOP_OFFSET       40
#define WSC_SCROLL_ROWS           1
#define WSC_CHECKBOX_TEXT_OFFSET  20
#define WSC_BUTTON_X_MARGIN       15
#define WSC_CHECKBOX_SPACING      10

#define APP_WINDOW_MIN_WIDTH      800
#define APP_WINDOW_MIN_HEIGHT     550

#define WMC_BUTTON                L"BUTTON"
#define WMC_SCROLLBAR             L"SCROLLBAR"

#define W_GRID_MAX_COLUMNS        9

constexpr COLORREF darkColorPalette[] = {
    RGB(  0,   0,   0), // Base color
    RGB(255, 255, 255), // Text color
    RGB( 30,  30,  30), // Background color
    RGB( 25,  25,  25), // Second background color
    RGB( 50,  50,  50), // Button background color
    RGB( 55,  55,  55), // Button border color
    RGB( 80,  80,  80), // Button clicked background color
    RGB( 60,  60,  60), // Scrollbar color
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
    RGB(200, 200, 200), // Scrollbar color
    RGB(150, 150, 150), // Scrollbar background color
};

std::wstring g_tableHeaders[W_GRID_MAX_COLUMNS] = {};
std::vector<HWND> g_childWindows;

constexpr bool g_tableCollapsableHeaders[] = {
    false, true, true, true, true, false, false, false, true
};

bool quitting                = false,
     focused                 = false,
     g_tableSizesInitialized = false,
     g_tableHeaderCheckboxCollapseStates[W_GRID_MAX_COLUMNS] = {};
RECT g_trackableCheckBoxes[W_GRID_MAX_COLUMNS] = {};
HWND   g_hYScrollBar       = nullptr,
       g_hXScrollBar       = nullptr,
       g_hSettingsButton   = nullptr;
HFONT  g_hDefaultFont      = nullptr,
       g_hDefaultFontBold  = nullptr,
       g_hTableFontBold    = nullptr,
       g_hTableFont        = nullptr;
HBRUSH g_lastCreatedBrush  = nullptr;
int windowWidth                = APP_WINDOW_MIN_WIDTH,
    windowHeight               = APP_WINDOW_MIN_HEIGHT,
    windowClientHeight         = 0,
    windowClientWidth          = 0,
    g_windowScrollYPos         = 0,
    g_windowScrollXPos         = 0,
    g_tableDefaultColumnWidths[W_GRID_MAX_COLUMNS] = {},
    g_tableColumnWidths[W_GRID_MAX_COLUMNS] = {},
    g_tableColumnXMargin       = 10,
    g_tableRowHeight           = 30,
    g_lastUpdatedTimeTextWidth = 0;
std::wstring g_lastTableUpdateTime = L"00:00:00.000";

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

inline int getContentHeight() {
    // +1 for header row
    return (static_cast<int>(std::size(taskbar::windows)) + 1) * g_tableRowHeight + WSC_GRID_Y + WSC_GRID_TOP_OFFSET + (config::autoUpdate ? windowClientHeight : 0);
}

inline int getContentWidth() {
    return std::accumulate(std::begin(g_tableColumnWidths), std::end(g_tableColumnWidths), config::autoUpdate ? windowClientWidth : 0, std::plus());
}

inline int getMaxYScroll(const int contentHeight) {
    return contentHeight - (windowClientHeight - WSC_HEADER) - WSC_GRID_Y;
}

inline int getMaxXScroll(const int contentWidth) {
    const int visibleWidth = windowClientWidth - WSC_SCROLLBAR_WIDTH;
    return contentWidth - visibleWidth;
}

inline void updateYScrollBarInfo() {
    const int contentHeight = getContentHeight() + WSC_SCROLLBAR_WIDTH;
    if (const int maxScroll = getMaxYScroll(contentHeight); g_windowScrollYPos > maxScroll)
        g_windowScrollYPos = maxScroll;
    if (g_windowScrollYPos < 0)
        g_windowScrollYPos = 0;

    SCROLLINFO si = {};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = contentHeight - WSC_GRID_Y;
    si.nPage  = windowClientHeight - WSC_HEADER;
    si.nPos   = g_windowScrollYPos;
    SetScrollInfo(g_hYScrollBar, SB_CTL, &si, TRUE);
}

inline void updateXScrollBarInfo() {
    const int contentWidth = getContentWidth() + 20;
    if (const int maxScroll = getMaxXScroll(contentWidth); g_windowScrollXPos > maxScroll)
        g_windowScrollXPos = maxScroll;
    if (g_windowScrollXPos < 0)
        g_windowScrollXPos = 0;

    SCROLLINFO si = {};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = contentWidth;
    si.nPage  = windowClientWidth - WSC_SCROLLBAR_WIDTH;
    si.nPos   = g_windowScrollXPos;
    SetScrollInfo(g_hXScrollBar, SB_CTL, &si, TRUE);
}

inline void updateScrollBarsInfo() {
    updateYScrollBarInfo();
    updateXScrollBarInfo();
}

inline void resizeChildWindows(HWND hwnd, LPCREATESTRUCT create);

inline void updateLanguage(HWND hwnd) {
    for (auto i = 0; i < W_GRID_MAX_COLUMNS; i++)
        g_tableHeaders[i] = utils::message(MSG_WND_DEBUG_TABLE_STATUS + i);
    if (hwnd) {
        resizeChildWindows(hwnd, nullptr);
        SetWindowText(hwnd, utils::message(MSG_APPLICATION_NAME).c_str());
    }
}

inline bool getYScrollBarMiddleThumb(RECT &rect, SCROLLBARINFO &sbi) {
    sbi.cbSize = sizeof(SCROLLBARINFO);
    GetScrollBarInfo(g_hYScrollBar, OBJID_CLIENT, &sbi);
    rect = utils::rect(windowWidth - WSC_SCROLLBAR_WIDTH, WSC_HEADER + sbi.xyThumbTop, WSC_SCROLLBAR_WIDTH, sbi.xyThumbBottom - sbi.xyThumbTop);
    if (getMaxYScroll(getContentHeight()) <= -WSC_SCROLLBAR_WIDTH)
        return false;
    return true;
}

inline bool getXScrollBarMiddleThumb(RECT &rect, SCROLLBARINFO &sbi) {
    sbi.cbSize = sizeof(SCROLLBARINFO);
    GetScrollBarInfo(g_hXScrollBar, OBJID_CLIENT, &sbi);
    rect = utils::rect(sbi.xyThumbTop, windowHeight - WSC_SCROLLBAR_WIDTH, sbi.xyThumbBottom - sbi.xyThumbTop, WSC_SCROLLBAR_WIDTH);
    if (getMaxXScroll(getContentWidth()) <= -20)
        return false;
    return true;
}

HBRUSH g_createBrush(const COLORREF color) {
    g_lastCreatedBrush = CreateSolidBrush(color);
    return g_lastCreatedBrush;
}

void g_deleteLastBrush() {
    DeleteObject(g_lastCreatedBrush);
    g_lastCreatedBrush = nullptr;
}

inline void g_drawScrollBars(HDC hdc) {
    #define SELECT_BRUSH utils::mouseInRect(&rect, VK_LBUTTON) ? brushClicked : brush

    HBRUSH brush = CreateSolidBrush(WCP_SCROLLBAR_COLOR);
    HBRUSH brushClicked = CreateSolidBrush(WCP_BUTTON_CLICKED_BG);

    RECT rect;
    SCROLLBARINFO sbi = {};

    SelectObject(hdc, g_hDefaultFontBold);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, WCP_FOREGROUND);

    // Y scrollbar middle thumb
    bool paintScrollBarMiddleThumb = getYScrollBarMiddleThumb(rect, sbi);
    if (paintScrollBarMiddleThumb)
        FillRect(hdc, &rect, SELECT_BRUSH);

    // Y scrollbar thumbs
    rect.top = WSC_HEADER;
    rect.bottom = rect.top + 16;
    FillRect(hdc, &rect, SELECT_BRUSH);
    DrawText(hdc, L"\u02C4", -1, &rect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

    rect.top = windowClientHeight - 17;
    rect.bottom = windowClientHeight;
    FillRect(hdc, &rect, SELECT_BRUSH);
    DrawText(hdc, L"\u02C5", -1, &rect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

    // X scrollbar middle thumb
    paintScrollBarMiddleThumb = getXScrollBarMiddleThumb(rect, sbi);
    if (paintScrollBarMiddleThumb)
        FillRect(hdc, &rect, SELECT_BRUSH);

    // X scrollbar thumbs
    rect.left = 0;
    rect.right = 16;
    FillRect(hdc, &rect, SELECT_BRUSH);
    DrawText(hdc, L"\u02C2", -1, &rect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

    rect.left = windowClientWidth - WSC_SCROLLBAR_WIDTH - rect.right;
    rect.right = rect.left + 16;
    FillRect(hdc, &rect, SELECT_BRUSH);
    DrawText(hdc, L"\u02C3", -1, &rect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

    DeleteObject(brush);
    DeleteObject(brushClicked);
}

inline HDC g_doubleBuffering(HWND hwnd, PAINTSTRUCT &ps, HDC oHdc, const bool start) {
    static HBITMAP memBitmap;
    static HGDIOBJ oldBitmap;
    static HDC mHdc, hdc;
    if (start) {
        if (!oHdc)
            hdc = BeginPaint(hwnd, &ps);
        else
            hdc = oHdc;
        mHdc = CreateCompatibleDC(hdc);

        memBitmap = CreateCompatibleBitmap(hdc, windowClientWidth, windowClientHeight);
        oldBitmap = SelectObject(mHdc, memBitmap);
        return mHdc;
    }

    BitBlt(hdc, 0, 0, windowClientWidth, windowClientHeight, mHdc, 0, 0, SRCCOPY);

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
    return nullptr;
}

inline void g_drawText(HDC hdc, const std::wstring &text, const int x, const int y) {
    TextOut(hdc, x, y, text.c_str(), static_cast<int>(text.length()));
}

inline void g_drawCheckBox(HDC mHdc, bool pState, const RECT oRect, const LPCWSTR text, HBRUSH &background, HBRUSH &foreground, const bool reverse, const int *id) {
    if (!background)
        background = CreateSolidBrush(WCP_BACKGROUND2);
    if (!foreground)
        foreground = CreateSolidBrush(WCP_FOREGROUND);

    // Background color
    FillRect(mHdc, &oRect, background);

    if (id)
        g_trackableCheckBoxes[*id] = oRect;

    // Create checkbox rect
    RECT boxRect = oRect;
    boxRect.right = boxRect.left + 16;
    boxRect.top += (boxRect.bottom - boxRect.top - 16 ) / 2;
    boxRect.bottom = boxRect.top + 16;

    // Draw checkbox rect
    FillRect(mHdc, &boxRect, background);
    FrameRect(mHdc, &boxRect, foreground);
    if (reverse)
        pState = !pState;
    if (pState)
    {
        // Create a little rect inside checkbox rect
        boxRect.left += 3;
        boxRect.top += 3;
        boxRect.right -= 3;
        boxRect.bottom -= 3;
        FillRect(mHdc, &boxRect, foreground);
    }

    RECT textRect = oRect;
    textRect.left += WSC_CHECKBOX_TEXT_OFFSET;
    const auto oldBkColor = SetBkColor(mHdc, WCP_BACKGROUND2);
    const auto oldTextColor = SetTextColor(mHdc, WCP_FOREGROUND);
    DrawTextW(mHdc, text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SetBkColor(mHdc, oldBkColor);
    SetTextColor(mHdc, oldTextColor);
}

inline int g_calculateTextWidth(HDC hdc, const std::wstring &text, HFONT font) {
    HGDIOBJ oldFont = nullptr;
    if (font)
        oldFont = SelectObject(hdc, font);
    SIZE sz;
    GetTextExtentPoint32(hdc, text.c_str(), static_cast<int>(text.size()), &sz);
    if (font)
        SelectObject(hdc, oldFont);
    return sz.cx;
}

inline void g_redrawLowerArea(HWND hwnd) {
    RECT rect { 0, WSC_HEADER, windowWidth, windowClientHeight };
    InvalidateRect(hwnd, &rect, TRUE);
    rect = utils::rect(windowClientWidth - g_lastUpdatedTimeTextWidth - 10, 45, g_lastUpdatedTimeTextWidth, 30);
    InvalidateRect(hwnd, &rect, TRUE);
}

inline void g_redrawHeader(HWND hwnd) {
    const RECT rect { 0, 0, windowWidth, WSC_HEADER };
    InvalidateRect(hwnd, &rect, TRUE);
}

inline void g_redrawWindow(HWND hwnd) {
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

inline void g_paintGrid(HDC hdc, const int sx, const int sy, const int rows) {
    const int gridWidth = std::accumulate(std::begin(g_tableColumnWidths), std::end(g_tableColumnWidths), 0, std::plus());
    const int gridHeight = rows * g_tableRowHeight;

    const RECT rect = { sx, sy, gridWidth + sx, sy + g_tableRowHeight };
    FillRect(hdc, &rect, g_createBrush(WCP_BACKGROUND2));
    g_deleteLastBrush();

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
        const bool detected = config::showAllWindows && row > 0 && row < rows ? taskbar::windows[row - 1].detected : false;
        hPen = CreatePen(PS_SOLID, 1, detected ? RGB(255, 0, 0) : WCP_FOREGROUND);
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

inline void g_calculateDefaultWidths(HDC hdc) {
    for (auto i = 0; i < W_GRID_MAX_COLUMNS; i++) {
        std::wstring header = g_tableHeaders[i];
        g_tableDefaultColumnWidths[i] = g_calculateTextWidth(hdc, header, g_hTableFont);
        if (g_tableCollapsableHeaders[i])
            g_tableDefaultColumnWidths[i] += WSC_CHECKBOX_TEXT_OFFSET;
    }
}

inline std::wstring getWindowValue(const taskbar::WindowInfo &wInfo, const int col) {
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
            value = L"[" + std::to_wstring(wInfo.rect.left) + L","
                    + std::to_wstring(wInfo.rect.top) + L","
                    + std::to_wstring(wInfo.rect.right) + L","
                    + std::to_wstring(wInfo.rect.bottom) + L"]";
            break;
        }
        case 5: value = utils::message(wInfo.maximized == 1 ? MSG_WND_DEBUG_TABLE_STATE_MAXIMIZED : MSG_WND_DEBUG_TABLE_STATE_MINIMIZED); break; // State
        case 6: { // Monitor
            for (auto i = 0; i < monitors::monitorCount; i++) {
                if (wInfo.hMonitor == monitors::indexedMonitors[i]) {
                    value = std::to_wstring(i);
                    break;
                }
            }
            break;
        }
        case 7: { // Title
            if (wInfo.focused == 1)
                value = utils::message(MSG_WND_YES);
            else if (wInfo.focused == 0)
                value = utils::message(MSG_WND_NO);
            else
                value = std::to_wstring(wInfo.focused);
            break;
        }
        case 8: value = wInfo.title; break; // Title
        default: value = L"???"; break; // unknown
    }
    return value;
}

inline void g_calculateCurrentWidths(HDC hdc, const int rows) {
    int widths[W_GRID_MAX_COLUMNS];
    std::copy(std::begin(g_tableDefaultColumnWidths), std::end(g_tableDefaultColumnWidths), std::begin(widths));

    for (auto row = 0; row < rows; row++) {
        taskbar::WindowInfo wInfo;
        if (row > 0)
            wInfo = taskbar::windows[row - 1];
        for (auto col = 0; col < W_GRID_MAX_COLUMNS; col++)
            if (const std::wstring value = row == 0 ? g_tableHeaders[col] : getWindowValue(wInfo, col); !value.empty()) {
                int width = g_calculateTextWidth(hdc, value, g_hTableFont) + 2 * g_tableColumnXMargin;
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

    std::copy(std::begin(widths), std::end(widths), std::begin(g_tableColumnWidths));
}

inline void g_printDataToGrid(HDC hdc, const int sx, const int sy, const int rows) {
    HBRUSH background = nullptr, foreground = nullptr;
    for (auto row = 0; row < rows; row++) {
        const int y = sy + row * g_tableRowHeight;
        int x = sx;
        taskbar::WindowInfo wInfo;
        if (row > 0)
            wInfo = taskbar::windows[row - 1];
        for (auto col = 0; col < W_GRID_MAX_COLUMNS; col++) {
            constexpr int textLeftMargin = 10;
            std::wstring value;
            RECT rect = utils::rect(x + textLeftMargin, y + 1, g_tableColumnWidths[col] - textLeftMargin, g_tableRowHeight - 1);
            if (row == 0) { // Header
                value = g_tableHeaders[col];
                const auto oldFont = SelectObject(hdc, g_hTableFontBold);
                if (g_tableCollapsableHeaders[col]) {
                    g_drawCheckBox(hdc, g_tableHeaderCheckboxCollapseStates[col], rect, value.c_str(), background, foreground, true, &col);
                } else {
                    DrawText(hdc, value.c_str(), -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
                SelectObject(hdc, oldFont);
            } else {
                value = getWindowValue(wInfo, col);
                if (!value.empty()) {
                    DrawText(hdc, value.c_str(), -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
            }
            x += g_tableColumnWidths[col];
        }
    }
    DeleteObject(background);
    DeleteObject(foreground);
}

inline void g_updateTable(HDC hdc, const bool onlyPaintGrid) {
    if (g_tableDefaultColumnWidths[0] == 0)
        g_calculateDefaultWidths(hdc);

    const int y = WSC_GRID_Y - g_windowScrollYPos;
    const int rows = static_cast<int>(std::size(taskbar::windows)) + 1; // +header

    SelectObject(hdc, g_hTableFont);
    SetBkMode(hdc, TRANSPARENT);
    if (!onlyPaintGrid)
        g_calculateCurrentWidths(hdc, rows);
    g_paintGrid(hdc, 10 - g_windowScrollXPos, y, rows);
    if (!onlyPaintGrid)
        g_printDataToGrid(hdc, 10 - g_windowScrollXPos, y, rows);

    updateScrollBarsInfo();
}

inline void resizeChildWindows(HWND hwnd, const LPCREATESTRUCT create) {
    HCURSOR lPtrHandCursor = LoadCursor(nullptr, IDC_HAND);
    const auto hdc = GetDC(hwnd);
    int i = 0;

    const bool createWindows = g_childWindows.empty();

    std::wstring text = utils::message(MSG_WND_HEADER_TABLE_UPDATE);
    int lastWidth = g_calculateTextWidth(hdc, text, g_hDefaultFont) + WSC_BUTTON_X_MARGIN * 2;
    RECT rect = { 10, 10, lastWidth, WSC_BUTTON_DEFAULT_H };
    if (createWindows) {
        const auto hButtonUpdate = CreateWindow(
            WMC_BUTTON, text.c_str(),
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rect.left, rect.top, rect.right, rect.bottom,
            hwnd, reinterpret_cast<HMENU>(ID_BUTTON_UPDATE), create->hInstance, nullptr);
        SendMessage(hButtonUpdate, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), TRUE);
        SendMessage(hButtonUpdate, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
        SetClassLongPtr(hButtonUpdate, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(lPtrHandCursor));
        g_childWindows.push_back(hButtonUpdate);
    } else {
        SetWindowText(g_childWindows[i], text.c_str());
        MoveWindow(g_childWindows[i], rect.left, rect.top, rect.right, rect.bottom, TRUE);
        i++;
    }

    text = utils::message(MSG_WND_HEADER_ACTIONS);
    rect = { 0, 0, g_calculateTextWidth(hdc, text, g_hDefaultFont) + WSC_BUTTON_X_MARGIN * 2, WSC_BUTTON_DEFAULT_H };
    if (createWindows) {
        g_hSettingsButton = CreateWindow(
            WMC_BUTTON, text.c_str(),
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rect.left, rect.top, rect.right, rect.bottom,
            hwnd, reinterpret_cast<HMENU>(ID_BUTTON_ACTIONS), create->hInstance, nullptr);
        SendMessage(g_hSettingsButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), TRUE);
        SendMessage(g_hSettingsButton, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
        SetClassLongPtr(g_hSettingsButton, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(lPtrHandCursor));
        g_childWindows.push_back(g_hSettingsButton);
    } else {
        SetWindowText(g_childWindows[i], text.c_str());
        MoveWindow(g_childWindows[i], windowWidth - rect.right - 10, 10, rect.right, rect.bottom, TRUE);
        i++;
    }

    int lastX = 10 + lastWidth + WSC_CHECKBOX_SPACING;

    text = utils::message(MSG_WND_HEADER_AUTO_UPDATE);
    lastWidth = g_calculateTextWidth(hdc, text, g_hDefaultFont) + WSC_CHECKBOX_TEXT_OFFSET;
    rect = { lastX, 10, lastWidth, WSC_BUTTON_DEFAULT_H };
    if (createWindows) {
        const auto hCheckBoxAutoUpdate = CreateWindowW(
            WMC_BUTTON, text.c_str(),
            WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_OWNERDRAW,
            rect.left, rect.top, rect.right, rect.bottom,
            hwnd, reinterpret_cast<HMENU>(ID_CHECKBOX_AUTO_UPDATE), create->hInstance, nullptr);
        SendMessage(hCheckBoxAutoUpdate, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), TRUE);
        SendMessage(hCheckBoxAutoUpdate, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
        SetClassLongPtr(hCheckBoxAutoUpdate, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(lPtrHandCursor));
        SetWindowLongPtr(hCheckBoxAutoUpdate, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&config::autoUpdate));
        g_childWindows.push_back(hCheckBoxAutoUpdate);
    } else {
        SetWindowText(g_childWindows[i], text.c_str());
        MoveWindow(g_childWindows[i], rect.left, rect.top, rect.right, rect.bottom, TRUE);
        i++;
    }
    lastX += lastWidth + WSC_CHECKBOX_SPACING;

    text = utils::message(MSG_WND_HEADER_DARK_MODE);
    lastWidth = g_calculateTextWidth(hdc, text, g_hDefaultFont) + WSC_CHECKBOX_TEXT_OFFSET;
    rect = { lastX, 10, lastWidth, WSC_BUTTON_DEFAULT_H };
    if (createWindows) {
        const auto hCheckboxDarkMode = CreateWindow(
            WMC_BUTTON, text.c_str(),
            WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_OWNERDRAW,
            rect.left, rect.top, rect.right, rect.bottom,
            hwnd, reinterpret_cast<HMENU>(ID_CHECKBOX_DARK_MODE), create->hInstance, nullptr);
        SendMessage(hCheckboxDarkMode, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), TRUE);
        SendMessage(hCheckboxDarkMode, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
        SetClassLongPtr(hCheckboxDarkMode, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(lPtrHandCursor));
        SetWindowLongPtr(hCheckboxDarkMode, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&config::darkMode));
        g_childWindows.push_back(hCheckboxDarkMode);
    } else {
        SetWindowText(g_childWindows[i], text.c_str());
        MoveWindow(g_childWindows[i], rect.left, rect.top, rect.right, rect.bottom, TRUE);
        i++;
    }
    lastX += lastWidth + WSC_CHECKBOX_SPACING;

    text = utils::message(MSG_WND_HEADER_SHOW_ALL_WINDOWS);
    lastWidth = g_calculateTextWidth(hdc, text, g_hDefaultFont) + WSC_CHECKBOX_TEXT_OFFSET;
    rect = { lastX, 10, lastWidth, WSC_BUTTON_DEFAULT_H };
    if (createWindows) {
        const auto hCheckBoxShowAllWindows = CreateWindowW(
            WMC_BUTTON, text.c_str(),
            WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_OWNERDRAW,
            rect.left, rect.top, rect.right, rect.bottom,
            hwnd, reinterpret_cast<HMENU>(ID_CHECKBOX_SHOW_ALL_WINDOWS), create->hInstance, nullptr);
        SendMessage(hCheckBoxShowAllWindows, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), TRUE);
        SendMessage(hCheckBoxShowAllWindows, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
        SetClassLongPtr(hCheckBoxShowAllWindows, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(lPtrHandCursor));
        SetWindowLongPtr(hCheckBoxShowAllWindows, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&config::showAllWindows));
        g_childWindows.push_back(hCheckBoxShowAllWindows);
    } else {
        SetWindowText(g_childWindows[i], text.c_str());
        MoveWindow(g_childWindows[i], rect.left, rect.top, rect.right, rect.bottom, TRUE);
    }

    DeleteObject(hdc);
}

LRESULT CALLBACK WndProc(HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) {
    static NOTIFYICONDATA nid = {};
    int *scrollPosition;

    switch (uMsg) {
        case WM_CREATE:
        {
            updateLanguage(nullptr);

            // Get icon for system tray
            const auto pcs = reinterpret_cast<CREATESTRUCT *>(lParam);
            const auto hTrayIcon = static_cast<HICON>(pcs->lpCreateParams);
            // Create system tray icon
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hwnd;
            nid.uID = 1;
            nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            nid.uCallbackMessage = WM_TRAY_ICON;
            nid.hIcon = hTrayIcon;
            // Set icon tip
            lstrcpy(nid.szTip, VER_FILEDESCRIPTION_STR);

            const auto create = reinterpret_cast<LPCREATESTRUCT>(lParam);

            // Create base (default) font
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

            LOGFONT lf;
            // Re-create default font but bold
            GetObject(g_hDefaultFont, sizeof(lf), &lf);
            lf.lfWeight = FW_BOLD;
            g_hDefaultFontBold = CreateFontIndirect(&lf);

            // Re-create default font for table but with different face name
            GetObject(g_hDefaultFont, sizeof(lf), &lf);
            // By doing this character widths might be incorrect, but for now I didn't notice anything (yet)
            wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Consolas");
            g_hTableFont = CreateFontIndirect(&lf);

            // Re-create table font but bold
            GetObject(g_hTableFont, sizeof(lf), &lf);
            lf.lfWeight = FW_BOLD;
            g_hTableFontBold = CreateFontIndirect(&lf);

            resizeChildWindows(hwnd, create);

            g_hYScrollBar = CreateWindowEx(
                WS_EX_LAYERED, WMC_SCROLLBAR, nullptr,
                WS_CHILD | WS_VISIBLE | SBS_VERT,
                -WSC_SCROLLBAR_WIDTH, WSC_HEADER, WSC_SCROLLBAR_WIDTH, 0,
                hwnd, reinterpret_cast<HMENU>(ID_SCROLLBAR_Y), create->hInstance, nullptr);
            SendMessage(g_hYScrollBar, WM_SETFONT, reinterpret_cast<WPARAM>(g_hDefaultFont), TRUE);
            SendMessage(g_hYScrollBar, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
            SetLayeredWindowAttributes(g_hYScrollBar, 0, 1, LWA_ALPHA);

            g_hXScrollBar = CreateWindowEx(
                WS_EX_LAYERED, WMC_SCROLLBAR, nullptr,
                WS_CHILD | WS_VISIBLE | SBS_HORZ,
                0, 0, 0, 0,
                hwnd, reinterpret_cast<HMENU>(ID_SCROLLBAR_X), create->hInstance, nullptr);
            SendMessage(g_hXScrollBar, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
            SetLayeredWindowAttributes(g_hXScrollBar, 0, 1, LWA_ALPHA);

            // Add icon
            Shell_NotifyIcon(NIM_ADD, &nid);

            //Trigger table update
            taskbar::collectWindowsInfo = true;
            break;
        }
        case WM_SIZE:
        {
            if (wParam == SIZE_MINIMIZED) {
                if (config::minimizeToTray)
                    ShowWindow(hwnd, SW_HIDE);
                else
                    return DefWindowProc(hwnd, uMsg, wParam, lParam);
            } else {
                windowWidth = LOWORD(lParam);
                windowHeight = HIWORD(lParam);
                // Reposition scrollbar
                MoveWindow(g_hYScrollBar, windowWidth - WSC_SCROLLBAR_WIDTH, WSC_HEADER, WSC_SCROLLBAR_WIDTH, windowHeight - WSC_HEADER, TRUE);
                MoveWindow(g_hXScrollBar, 0, windowHeight - WSC_SCROLLBAR_WIDTH, windowWidth - WSC_SCROLLBAR_WIDTH, WSC_SCROLLBAR_WIDTH, TRUE);
                RECT rect;
                GetWindowRect(g_hSettingsButton, &rect);
                MoveWindow(g_hSettingsButton, windowWidth - (rect.right - rect.left) - 10, 10, rect.right - rect.left, rect.bottom - rect.top, TRUE);

                GetClientRect(hwnd, &rect);
                windowClientHeight = rect.bottom - rect.top;
                windowClientWidth = rect.right - rect.left;

                updateScrollBarsInfo();
                g_redrawWindow(hwnd);
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
                    g_redrawLowerArea(hwnd);
                    updateXScrollBarInfo();
                    break;
                }
            }
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
                case ID_CHECKBOX_TABLE_HEADER:
                case ID_CHECKBOX_SHOW_ALL_WINDOWS:
                case ID_CHECKBOX_DARK_MODE:
                case ID_CHECKBOX_AUTO_UPDATE:
                {
                    HDC mHdc = g_doubleBuffering(hwnd, ps, draw->hDC, true);

                    HBRUSH background = nullptr, foreground = nullptr;
                    const auto pState = reinterpret_cast<bool*>(GetWindowLongPtr(draw->hwndItem, GWLP_USERDATA));
                    SelectObject(mHdc, g_hDefaultFont);
                    g_drawCheckBox(mHdc, pState ? *pState : false, draw->rcItem, text, background, foreground, false, nullptr);

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

            // Above table text
            SetBkColor(mHdc, WCP_BACKGROUND);
            SelectObject(mHdc, g_hDefaultFont);
            g_drawText(mHdc, utils::message(MSG_WND_DEBUG_TABLE_HEADER_TEXT), 10 - g_windowScrollXPos, 80 - g_windowScrollYPos);

            // Table
            g_updateTable(mHdc, false);

            // Header
            RECT wRect;
            GetWindowRect(hwnd, &wRect);
            wRect.left = 0;
            wRect.top = 0;
            wRect.bottom = WSC_HEADER;
            FillRect(mHdc, &wRect, g_createBrush(WCP_BACKGROUND2));
            g_deleteLastBrush();

            SetBkColor(mHdc, WCP_BACKGROUND2);
            SelectObject(mHdc, g_hDefaultFont);

            // Header text
            g_drawText(mHdc, utils::message(MSG_WND_HEADER_TEXT), 10, 45);

            // Table update time text
            std::wstring text = utils::message(MSG_WND_LAST_UPDATE_AT) + L" ";
            if (g_lastUpdatedTimeTextWidth == 0)
                g_lastUpdatedTimeTextWidth = g_calculateTextWidth(mHdc, g_lastTableUpdateTime, g_hDefaultFontBold);
            g_drawText(mHdc, text, windowClientWidth - g_calculateTextWidth(mHdc, text, g_hDefaultFont) - g_lastUpdatedTimeTextWidth - 10, 45);
            auto oldFont = SelectObject(mHdc, g_hDefaultFontBold);
            g_drawText(mHdc, g_lastTableUpdateTime, windowClientWidth - g_lastUpdatedTimeTextWidth - 10, 45);
            SelectObject(mHdc, oldFont);

            // Draw scrollbars
            g_drawScrollBars(mHdc);

            g_doubleBuffering(hwnd, ps, nullptr, false);
            break;
        }
        case WM_TRAY_ICON:
        {
            if (lParam == WM_LBUTTONUP) {
                if (!IsWindowVisible(hwnd))
                    ShowWindow(hwnd, SW_SHOWNORMAL);
                else if (IsIconic(hwnd))
                    ShowWindow(hwnd, SW_RESTORE);
                else
                    ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
            } else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                HMENU hMenu = CreatePopupMenu();
                if (lParam != WM_CONTEXTMENU)
                    AppendMenu(hMenu, MF_STRING | MF_DISABLED, ID_TRAY_HEADER, TRAY_TITLE);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_OPEN_CONFIG, utils::message(MSG_TRAY_CONFIG_OPEN).c_str());
                AppendMenu(hMenu, MF_STRING, ID_TRAY_RELOAD_CONFIG, utils::message(MSG_TRAY_CONFIG_RELOAD).c_str());
                if (lParam == WM_CONTEXTMENU) {
                    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXPOSE_INTERNALS, utils::message(MSG_TRAY_CONFIG_EXPOSE_INTERNALS).c_str());
                }
                AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_PAUSE_HIDER, utils::message(globals::taskbarLoopRunState ?  MSG_TRAY_TB_PAUSE : MSG_TRAY_TB_RESUME).c_str());
                AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_ADD_REMOVE_STARTUP, utils::message(utils::doesAutoStart() ? MSG_TRAY_REM_STARTUP : MSG_TRAY_ADD_STARTUP).c_str());
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
        case WM_TASKBAR_THREAD_ERROR: {
            auto what = reinterpret_cast<std::string*>(lParam);
            std::wstring message;
            if (*what == "stoi") {
                message = utils::message(MSG_CONFIG_NOT_NUMBER);
            } else {
                message = std::wstring(what->begin(), what->end());
            }
            utils::messageBox(MSG_TASKBAR_THREAD_EXCEPTION_OCCURRED, MB_ICONERROR | MB_OK, { message });
            delete what;
            break;
        }
        case WM_UPDATE_GRID_REQUEST:
        {
            if (config::autoUpdate) {
                if (!focused && config::disableAutoUpdateWhenUnfocused)
                    break;
            }
            g_lastTableUpdateTime = utils::getFormattedTime();
            g_redrawLowerArea(hwnd);
            updateScrollBarsInfo();
            break;
        }
        case WM_COMMAND:
        {
            const auto lwParam = LOWORD(wParam);
            if (lwParam >= IDS_CHECKBOX_MIN && lwParam <= IDS_CHECKBOX_MAX) {
                HWND hCheckbox = GetDlgItem(hwnd, lwParam);
                if (const auto pState = reinterpret_cast<bool*>(GetWindowLongPtr(hCheckbox, GWLP_USERDATA))) {
                    // Inverse checkbox state
                    *pState = !*pState;
                    if (lwParam != ID_CHECKBOX_DARK_MODE) // No need for updating, as dark mode check already redraws the window
                        g_redrawWindow(hCheckbox);
                }
            }
            switch (lwParam) {
                case ID_BUTTON_ACTIONS:
                {
                    SendMessage(hwnd, WM_TRAY_ICON, 0, WM_CONTEXTMENU);
                    break;
                }
                case ID_CHECKBOX_SHOW_ALL_WINDOWS: {
                    taskbar::forceToCollect = true;
                    // No need to break
                }
                case ID_BUTTON_UPDATE:
                {
                    taskbar::collectWindowsInfo = true;
                    break;
                }
                case ID_CHECKBOX_AUTO_UPDATE:
                {
                    g_windowScrollXPos = 0;
                    g_windowScrollYPos = 0;
                    g_redrawWindow(hwnd);
                    updateScrollBarsInfo();
                    break;
                }
                case ID_CHECKBOX_DARK_MODE:
                {
                    g_redrawWindow(hwnd);
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
                case ID_TRAY_EXPOSE_INTERNALS: {
                    config::save(true);
                    // No need for break
                }
                case ID_TRAY_RELOAD_CONFIG:
                {
                    if (config::load())
                        utils::messageBox(MSG_CONFIG_RELOADED, MB_ICONINFORMATION | MB_OK);
                    else
                        utils::messageBox(MSG_CONFIG_RELOADED_WITH_ERRORS, MB_ICONWARNING | MB_OK);
                    updateLanguage(hwnd);
                    g_redrawWindow(hwnd);
                    taskbar::resetTaskbar();
                    taskbar::clearErrorState();
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
                scrollPosition = &g_windowScrollXPos;
            } else if (scrollBarID == ID_SCROLLBAR_Y) {
                scrollPosition = &g_windowScrollYPos;
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

            updateScrollBarsInfo();
            g_redrawLowerArea(hwnd);
            break;
        }
        case WM_MOUSEWHEEL:
        {
            scrollPosition = GetKeyState(VK_SHIFT) & 0x8000 ? &g_windowScrollXPos : &g_windowScrollYPos;
            const int oldPos = *scrollPosition;
            *scrollPosition += -(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * (WSC_SCROLL_ROWS * g_tableRowHeight));
            updateScrollBarsInfo();
            if (oldPos == *scrollPosition)
                break;
            g_redrawLowerArea(hwnd);
            break;
        }
        case WM_GETMINMAXINFO: {
            const auto lpMinMaxInfo = reinterpret_cast<LPMINMAXINFO>(lParam);
            lpMinMaxInfo->ptMinTrackSize.x = APP_WINDOW_MIN_WIDTH;
            lpMinMaxInfo->ptMinTrackSize.y = APP_WINDOW_MIN_HEIGHT;
            break;
        }
        case WM_DISPLAYCHANGE: {
            monitors::indexMonitors();
            taskbar::findTaskbarHandles();
            break;
        }
        case WM_CLOSE: {
            if (config::closeConfirmMessage && !config::closeToTray) {
                if (utils::messageBox(MSG_WINDOW_CLOSE_CONFIRMATION, MB_ICONQUESTION | MB_YESNO) == 7)
                    break;
            } else if (config::closeToTray) {
                ShowWindow(hwnd, SW_HIDE);
                break;
            }
            DestroyWindow(hwnd);
            break;
        }
        case WM_DESTROY: {
            DeleteObject(g_hDefaultFont);
            DeleteObject(g_hDefaultFontBold);
            DeleteObject(g_hTableFont);
            DeleteObject(g_hTableFontBold);
            DeleteObject(g_hYScrollBar);
            DeleteObject(g_hXScrollBar);
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
        }
        case WM_ERASEBKGND: {
            return 0;
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
            if (record->NumberParameters > 1)
                lpwstr = utils::replaceCharacterWithText(lpwstr, 'p', std::to_wstring(record->ExceptionInformation[1]));
            if (record->NumberParameters > 2)
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

void signalHandler(const int signum) {
    MessageBox(nullptr, L"CRITICAL: Segmentation fault (SIGSEGV) occurred!", VER_FILEDESCRIPTION_STR, MB_OK | MB_ICONERROR);
    exit(signum);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, const int nShowCmd) {
    signal(SIGSEGV, signalHandler);
    globals::hIns = hInstance;
    SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(CrashHandler));

    int argc;
    LPWSTR commandLine = GetCommandLineW();
    if (const LPWSTR *argv = CommandLineToArgvW(commandLine, &argc); !utils::processArguments(argc, argv, commandLine))
        return 0;

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
            crashInfo << utils::message(MSG_WINDOW_ICON_FAILED);
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

    // Get monitor size
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Create window
    globals::hWnd = CreateWindowEx(
        WS_EX_CLIENTEDGE, PROJECT_NAME, utils::message(MSG_APPLICATION_NAME).c_str(),
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

    monitors::indexMonitors();
    taskbar::findTaskbarHandles();
    auto taskbarLoopThread = std::thread(taskbarLoop);

    ShowWindow(globals::hWnd, !config::openOnStart ? SW_HIDE : nShowCmd);
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