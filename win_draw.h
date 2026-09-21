#ifndef WINDOWSTASKBARHIDER_WIN_DRAW_H
#define WINDOWSTASKBARHIDER_WIN_DRAW_H

#include <windows.h>
#include <string>
#include "config.h"
#include <functional>

constexpr COLORREF darkColorPalette[] = {
    RGB(  0,   0,   0), // Base color
    RGB(255, 255, 255), // Text color
    RGB( 30,  30,  30), // Background color
    RGB( 25,  25,  25), // Second background color
    RGB( 50,  50,  50), // Button background color
    RGB( 55,  55,  55), // Button border color
    RGB( 80,  80,  80), // Button clicked background color
    RGB( 60,  60,  60), // Scrollbar color
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

#define WCP_BASE_COLOR            (config::darkMode ? darkColorPalette[0] : lightColorPalette[0])
#define WCP_FOREGROUND            (config::darkMode ? darkColorPalette[1] : lightColorPalette[1])
#define WCP_BACKGROUND            (config::darkMode ? darkColorPalette[2] : lightColorPalette[2])
#define WCP_BACKGROUND2           (config::darkMode ? darkColorPalette[3] : lightColorPalette[3])
#define WCP_BUTTON_BG             (config::darkMode ? darkColorPalette[4] : lightColorPalette[4])
#define WCP_BUTTON_BORDER         (config::darkMode ? darkColorPalette[5] : lightColorPalette[5])
#define WCP_BUTTON_CLICKED_BG     (config::darkMode ? darkColorPalette[6] : lightColorPalette[6])
#define WCP_SCROLLBAR_COLOR       (config::darkMode ? darkColorPalette[7] : lightColorPalette[7])

#define WSC_BUTTON_DEFAULT_W      120
#define WSC_BUTTON_DEFAULT_H      30
#define WSC_HEADER                70
#define WSC_CONFIG_HEADER         50
#define WSC_SCROLLBAR_WIDTH       17
#define WSC_GRID_Y                100
#define WSC_GRID_TOP_OFFSET       40
#define WSC_SCROLL_ROWS           1
#define WSC_CHECKBOX_TEXT_OFFSET  20
#define WSC_BUTTON_X_MARGIN       15
#define WSC_CHECKBOX_SPACING      10

constexpr DWORD MY_DWMWA_CAPTION_COLOR = 35;

class win_draw {
public:
    static void redrawWindow(HWND hWnd);
    static int calculateTextWidth(HDC hdc, const std::wstring &text, HFONT font);
    static void drawText(HDC hdc, const std::wstring &text, int x, int y);
    static void drawCheckBox(HDC mHdc, bool pState, RECT oRect, LPCWSTR text, HBRUSH &bg, HBRUSH &fg);
    static HDC doubleBuffering(HWND hWnd, PAINTSTRUCT &ps, HDC oHdc, bool start, const int *winClientW, const int *winClientH);
    static void updateTitlebarColors(HWND hWnd);
    static HBRUSH createBrush(COLORREF color);
    static void deleteLastBrush();

    class scrollable_content {
    public:
        HWND hXScrollBar;
        HWND hYScrollBar;
        const int *winClientW;
        const int *winClientH;
        long scrollYPos;
        long scrollXPos;
        long topOffset;

        scrollable_content(std::function<int()> getContentWidth,
                           std::function<int()> getContentHeight,
                           const int *winClientW,
                           const int *winClientH,
                           long topOffset);

        void updateXScrollBarInfo();
        void updateYScrollBarInfo();
        void updateXYScrollBarsInfo();
        void createScrollbars(HWND hWnd, HINSTANCE hInstance, bool allowXScrollBar = true, bool allowYScrollBar = true);
        void onWindowMove() const;
        bool onScroll(LPARAM lParam, WPARAM wParam);
        bool onMouseWheel(WPARAM wParam, int rowHeight);
        void drawScrollBars(HDC hdc, HFONT hFontBold) const;
        void destroy() const;
    private:
        const std::function<int()> getContentWidth;
        const std::function<int()> getContentHeight;

        [[nodiscard]] int getMaxXScroll(int contentWidth) const;
        [[nodiscard]] int getMaxYScroll(int contentHeight) const;
        bool getYScrollBarMiddleThumb(RECT &rect, SCROLLBARINFO &sbi) const;
        bool getXScrollBarMiddleThumb(RECT &rect, SCROLLBARINFO &sbi) const;
    };
private:
    static HBRUSH lastBrush;
};

#endif //WINDOWSTASKBARHIDER_WIN_DRAW_H
