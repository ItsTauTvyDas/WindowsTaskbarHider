#include "config_ui.h"

#include "globals.h"
#include "resources.h"
#include "language.h"
#include "win_draw.h"

#define CONFIG_CLASS_NAME PROJECT_NAME"Config"

bool config_ui::initOk = false;
int config_ui::winClientW;
int config_ui::winClientH;
int config_ui::winW;
int config_ui::winH;
HWND config_ui::windowHandle;
win_draw::scrollable_content config_ui::scrollable(getContentWidth,
    getContentHeight,
    &winClientW,
    &winClientH,
    &winW,
    &winH,
    WSC_HEADER);

int config_ui::getContentHeight() {
    return 500;
}

int config_ui::getContentWidth() {
    return 0;
}


LRESULT CALLBACK config_ui::WndProc(HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            const auto create = reinterpret_cast<LPCREATESTRUCT>(lParam);
            scrollable.createScrollbars(hWnd, create->hInstance);
            break;
        }
        case WM_SIZE: {
            if (wParam != SIZE_MINIMIZED) {
                scrollable.onWindowMove();
            }
            break;
        }
        case WM_HSCROLL:
        case WM_VSCROLL: {
            if (!scrollable.onScroll(lParam, wParam))
                break;
            redrawWindow();
            break;
        }
        case WM_MOUSEWHEEL:
        {
            if (!scrollable.onMouseWheel(wParam, 30))
                break;
            redrawWindow();
            break;
        }
        case WM_CLOSE: {
            close();
            return 0;
        }
        case WM_GETMINMAXINFO: {
            const auto mmi = reinterpret_cast<LPMINMAXINFO>(lParam);
            mmi->ptMinTrackSize.x = mmi->ptMaxTrackSize.x = CONFIG_UI_WINDOW_MIN_WIDTH;
            mmi->ptMinTrackSize.y = mmi->ptMaxTrackSize.y = CONFIG_UI_WINDOW_MIN_HEIGHT;
            return 0;
        }
        case WM_NCHITTEST: {
            switch (const LRESULT hit = DefWindowProc(hWnd, uMsg, wParam, lParam)) {
                case HTLEFT: case HTRIGHT: case HTTOP: case HTBOTTOM:
                case HTTOPLEFT: case HTTOPRIGHT: case HTBOTTOMLEFT: case HTBOTTOMRIGHT:
                    return HTBORDER;   // border no longer acts as a resize handle
                default:
                    return hit;
            }
        }
        case WM_ERASEBKGND: {
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC mHdc = win_draw::doubleBuffering(hWnd, ps, nullptr, true, &winClientW, &winClientH);

            // Update colors
            SetBkColor(mHdc, WCP_BACKGROUND);
            SetTextColor(mHdc, WCP_FOREGROUND);
            FillRect(mHdc, &ps.rcPaint, win_draw::createBrush(WCP_BACKGROUND));
            win_draw::deleteLastBrush();

            // Header
            RECT wRect;
            GetWindowRect(hWnd, &wRect);
            wRect.left = 0;
            wRect.top = 0;
            wRect.bottom = WSC_CONFIG_HEADER;
            FillRect(mHdc, &wRect, win_draw::createBrush(WCP_BACKGROUND2));
            win_draw::deleteLastBrush();

            scrollable.drawScrollBars(mHdc, *globals::hDefaultFontBoldP);

            win_draw::doubleBuffering(hWnd, ps, nullptr, false, &winClientW, &winClientH);
            break;
        }
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 1;
}

void config_ui::redrawWindow() {
    RECT rect;
    GetClientRect(windowHandle, &rect);
    InvalidateRect(windowHandle, &rect, true);
}

void config_ui::init(HICON hIcon) {
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = globals::hIns;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon         = hIcon;
    wc.lpszClassName = CONFIG_CLASS_NAME;

    if (!RegisterClassExW(&wc)) {
        utils::showExceptionMessageBox([](std::wstringstream& crashInfo) {
            crashInfo << utils::message(MSG_WINDOW_REGISTER_FAILED);
        }, true);
        return;
    }

    windowHandle = CreateWindowExW(
        WS_EX_CLIENTEDGE, CONFIG_CLASS_NAME, L"Settings",
        WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX),
        0, 0, CONFIG_UI_WINDOW_MIN_WIDTH, CONFIG_UI_WINDOW_MIN_HEIGHT,
        globals::hWnd, nullptr, globals::hIns, nullptr);

    RECT rect;
    GetClientRect(windowHandle, &rect);
    winClientH = rect.bottom - rect.top;
    winClientW = rect.right - rect.left;

    GetWindowRect(windowHandle, &rect);
    winH = rect.bottom - rect.top;
    winW = rect.right - rect.left;

    initOk = windowHandle != nullptr;
}

void config_ui::open() {
    if (!initOk)
        return;

    win_draw::updateTitlebarColors(windowHandle);

    RECT o, w;
    GetWindowRect(globals::hWnd, &o);
    GetWindowRect(windowHandle, &w);
    SetWindowPos(windowHandle, nullptr,
        o.left + ((o.right - o.left) - (w.right - w.left)) / 2,
        o.top  + ((o.bottom - o.top) - (w.bottom - w.top)) / 2,
        0, 0, SWP_NOSIZE | SWP_NOZORDER);

    EnableWindow(globals::hWnd, false);
    ShowWindow(windowHandle, SW_SHOW);
    SetForegroundWindow(windowHandle);
}

void config_ui::close() {
    EnableWindow(globals::hWnd, true);
    ShowWindow(windowHandle, SW_HIDE);
    SetForegroundWindow(globals::hWnd);
}

void config_ui::save() {

}