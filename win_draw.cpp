#include "win_draw.h"

#include "globals.h"
#include "resources.h"
#include "utils.h"

namespace win_draw {
    void redrawWindow(HWND hWnd) {
        RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    }

    int calculateTextWidth(HDC hdc, const std::wstring &text, HFONT font) {
        HGDIOBJ oldFont = nullptr;
        if (font)
            oldFont = SelectObject(hdc, font);
        SIZE sz;
        GetTextExtentPoint32W(hdc, text.c_str(), static_cast<int>(text.size()), &sz);
        if (font)
            SelectObject(hdc, oldFont);
        return sz.cx;
    }

    void drawText(HDC hdc, const std::wstring &text, const int x, const int y) {
        TextOut(hdc, x, y, text.c_str(), static_cast<int>(text.length()));
    }

    void drawCheckBox(HDC mHdc, bool pState, const RECT oRect, const LPCWSTR text, HBRUSH &bg, HBRUSH &fg) {
        if (!bg)
            bg = CreateSolidBrush(WCP_BACKGROUND2);
        if (!fg)
            fg = CreateSolidBrush(WCP_FOREGROUND);

        // Background color
        FillRect(mHdc, &oRect, bg);

        // Create checkbox rect
        RECT boxRect = oRect;
        boxRect.right = boxRect.left + 16;
        boxRect.top += (boxRect.bottom - boxRect.top - 16 ) / 2;
        boxRect.bottom = boxRect.top + 16;

        // Draw checkbox rect
        FillRect(mHdc, &boxRect, bg);
        FrameRect(mHdc, &boxRect, fg);
        if (pState) {
            // Create a little rect inside checkbox rect
            boxRect.left += 3;
            boxRect.top += 3;
            boxRect.right -= 3;
            boxRect.bottom -= 3;
            FillRect(mHdc, &boxRect, fg);
        }

        RECT textRect = oRect;
        textRect.left += WSC_CHECKBOX_TEXT_OFFSET;
        const auto oldBkColor = SetBkColor(mHdc, WCP_BACKGROUND2);
        const auto oldTextColor = SetTextColor(mHdc, WCP_FOREGROUND);
        DrawTextW(mHdc, text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        SetBkColor(mHdc, oldBkColor);
        SetTextColor(mHdc, oldTextColor);
    }

    HDC doubleBuffering(HWND hWnd, PAINTSTRUCT &ps, HDC oHdc, const bool start, const int *winClientW, const int *winClientH) {
        static HBITMAP memBitmap;
        static HGDIOBJ oldBitmap;
        static HDC mHdc, hdc;
        if (start) {
            if (!oHdc)
                hdc = BeginPaint(hWnd, &ps);
            else
                hdc = oHdc;
            mHdc = CreateCompatibleDC(hdc);

            memBitmap = CreateCompatibleBitmap(hdc, *winClientW, *winClientH);
            oldBitmap = SelectObject(mHdc, memBitmap);
            return mHdc;
        }

        BitBlt(hdc, 0, 0, *winClientW, *winClientH, mHdc, 0, 0, SRCCOPY);

        SelectObject(mHdc, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(mHdc);
        if (!oHdc) {
            EndPaint(hWnd, &ps);
            hdc = nullptr;
        }
        memBitmap = nullptr;
        oldBitmap = nullptr;
        mHdc = nullptr;
        return nullptr;
    }

    void updateTitlebarColors(HWND hWnd) {
        if (IS_WINDOWS_11(globals::sysBuildNumber))
            DwmSetWindowAttribute(hWnd, DWMWA_CAPTION_COLOR, &WCP_BACKGROUND2, sizeof(WCP_BACKGROUND2));
    }

    // scrollable_content

    scrollable_content::scrollable_content(std::function<int()> getContentWidth,
                                           std::function<int()> getContentHeight,
                                           const int *winClientW,
                                           const int *winClientH,
                                           const int *winW,
                                           const int *winH,
                                           const long topOffset)
        : winClientW(winClientW),
          winClientH(winClientH),
          winW(winW),
          winH(winH),
          topOffset(topOffset),
          getContentWidth(std::move(getContentWidth)),
          getContentHeight(std::move(getContentHeight)) {}

    int scrollable_content::getMaxYScroll(const int contentHeight) const {
        return contentHeight - (*winClientH + WSC_SCROLLBAR_WIDTH);
    }

    int scrollable_content::getMaxXScroll(const int contentWidth) const {
        return contentWidth - (*winClientW - WSC_SCROLLBAR_WIDTH);
    }

    void scrollable_content::updateYScrollBarInfo() {
        const int contentHeight = getContentHeight() + WSC_SCROLLBAR_WIDTH;
        if (const int maxScroll = getMaxYScroll(contentHeight); scrollYPos > maxScroll)
            scrollYPos = maxScroll;
        if (scrollYPos < 0)
            scrollYPos = 0;

        SCROLLINFO si = {};
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin   = 0;
        si.nMax   = contentHeight - topOffset;
        si.nPage  = *winClientH - topOffset + WSC_SCROLLBAR_WIDTH;
        si.nPos   = scrollYPos;
        SetScrollInfo(hYScrollBar, SB_CTL, &si, true);
    }

    void scrollable_content::updateXScrollBarInfo() {
        const int contentWidth = getContentWidth() + WSC_SCROLLBAR_WIDTH + 3;
        if (const int maxScroll = getMaxXScroll(contentWidth); scrollXPos > maxScroll)
            scrollXPos = maxScroll;
        if (scrollXPos < 0)
            scrollXPos = 0;

        SCROLLINFO si = {};
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin   = 0;
        si.nMax   = contentWidth;
        si.nPage  = *winClientW - WSC_SCROLLBAR_WIDTH;
        si.nPos   = scrollXPos;
        SetScrollInfo(hXScrollBar, SB_CTL, &si, true);
    }


    void scrollable_content::updateXYScrollBarsInfo() {
        updateYScrollBarInfo();
        updateXScrollBarInfo();
    }

    bool scrollable_content::getYScrollBarMiddleThumb(RECT &rect, SCROLLBARINFO &sbi) const {
        sbi.cbSize = sizeof(SCROLLBARINFO);
        GetScrollBarInfo(hYScrollBar, OBJID_CLIENT, &sbi);
        rect = utils::rect(*winW - WSC_SCROLLBAR_WIDTH, topOffset + sbi.xyThumbTop, WSC_SCROLLBAR_WIDTH, sbi.xyThumbBottom - sbi.xyThumbTop);
        if (getMaxYScroll(getContentHeight()) <= -WSC_SCROLLBAR_WIDTH)
            return false;
        return true;
    }

    bool scrollable_content::getXScrollBarMiddleThumb(RECT &rect, SCROLLBARINFO &sbi) const {
        sbi.cbSize = sizeof(SCROLLBARINFO);
        GetScrollBarInfo(hXScrollBar, OBJID_CLIENT, &sbi);
        rect = utils::rect(sbi.xyThumbTop, *winH - WSC_SCROLLBAR_WIDTH, sbi.xyThumbBottom - sbi.xyThumbTop, WSC_SCROLLBAR_WIDTH);
        if (getMaxXScroll(getContentWidth()) <= -20)
            return false;
        return true;
    }

    void scrollable_content::drawScrollBars(HDC hdc, HFONT hFontBold) const {
        RECT rect;
        SCROLLBARINFO sbi = {};

        HBRUSH selectBrush = CreateSolidBrush(utils::mouseInRect(&rect, VK_LBUTTON) ? WCP_BUTTON_CLICKED_BG : WCP_SCROLLBAR_COLOR);

        SelectObject(hdc, hFontBold);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, WCP_FOREGROUND);

        // Y scrollbar middle thumb
        bool paintScrollBarMiddleThumb = getYScrollBarMiddleThumb(rect, sbi);
        if (paintScrollBarMiddleThumb)
            FillRect(hdc, &rect, selectBrush);

        // Y scrollbar thumbs
        rect.top = topOffset;
        rect.bottom = rect.top + 16;
        FillRect(hdc, &rect, selectBrush);
        DrawTextW(hdc, L"\u02C4", -1, &rect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

        rect.top = *winClientH - 17;
        rect.bottom = *winClientH;
        FillRect(hdc, &rect, selectBrush);
        DrawTextW(hdc, L"\u02C5", -1, &rect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

        // X scrollbar middle thumb
        paintScrollBarMiddleThumb = getXScrollBarMiddleThumb(rect, sbi);
        if (paintScrollBarMiddleThumb)
            FillRect(hdc, &rect, selectBrush);

        // X scrollbar thumbs
        rect.left = 0;
        rect.right = 16;
        FillRect(hdc, &rect, selectBrush);
        DrawTextW(hdc, L"\u02C2", -1, &rect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

        rect.left = *winClientW - WSC_SCROLLBAR_WIDTH - rect.right;
        rect.right = rect.left + 16;
        FillRect(hdc, &rect, selectBrush);
        DrawTextW(hdc, L"\u02C3", -1, &rect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

        DeleteObject(selectBrush);
    }
}
