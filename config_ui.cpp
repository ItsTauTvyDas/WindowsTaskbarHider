#include "config_ui.h"

#include "globals.h"
#include "resources.h"
#include "language.h"
#include "win_draw.h"

#define CONFIG_CLASS_NAME PROJECT_NAME"Config"

bool config_ui::initOk = false;
int config_ui::winClientW;
int config_ui::winClientH;
HWND config_ui::windowHandle;
win_draw::scrollable_content config_ui::scrollable(getContentWidth, getContentHeight, &winClientW, &winClientH, WSC_CONFIG_HEADER);
config_ui::SettingList config_ui::settings;

void config_ui::buildSettings() {
    settings.clear();

    addSetting<SettingSeparator>(L"Window");
    addSetting<SettingCheckbox>(L"Dark mode", L"Window.DarkMode", config::darkMode);
    addSetting<SettingCheckbox>(L"Auto update", L"Window.AutoUpdate", config::autoUpdate);

    addSetting<SettingSeparator>(L"Taskbar");
    addSetting<SettingNumber>(L"Update interval (ms)", L"Taskbar.UpdateInterval", config::taskbarUpdateInterval, 1, 1000);
    addSetting<SettingNumber>(L"Opacity when hidden", L"Taskbar.OpacityWhenHidden", config::opacityWhenHidden, 0, 255);
}

int config_ui::getContentHeight() {
    return 1000;
}

int config_ui::getContentWidth() {
    return 0;
}

LRESULT CALLBACK config_ui::WndProc(HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            const auto create = reinterpret_cast<LPCREATESTRUCT>(lParam);
            scrollable.createScrollbars(hWnd, create->hInstance, false, true);
            break;
        }
        case WM_SIZE: {
            if (wParam != SIZE_MINIMIZED) {
                winClientW = LOWORD(lParam);
                winClientH = HIWORD(lParam);
                scrollable.onWindowMove();
                scrollable.updateXYScrollBarsInfo();
                InvalidateRect(hWnd, nullptr, TRUE);
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
            if (!scrollable.onMouseWheel(wParam, CONFIG_UI_SETTINGS_DRAW_ROW_HEIGHT))
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

            // Drawing settings
            long start = CONFIG_UI_SETTINGS_DRAW_Y;
            HBRUSH checkboxBg = CreateSolidBrush(WCP_BACKGROUND);
            HBRUSH checkboxFg = CreateSolidBrush(WCP_FOREGROUND);
            for (const auto &rawSetting : settings) {
                const long y = start - scrollable.scrollYPos;
                const long x = CONFIG_UI_SETTINGS_DRAW_X/* - scrollable.scrollXPos*/;
                const std::wstring *label = &rawSetting->label;

                HGDIOBJ oldFont = SelectObject(mHdc, *globals::hDefaultFontP);
                SetBkColor(mHdc, WCP_BACKGROUND);
                COLORREF oldColor = SetTextColor(mHdc, WCP_FOREGROUND);
                if (rawSetting->type != SettingType::separator) {
                    // Draw label for every control except separator
                    win_draw::drawText(mHdc, *label, x + 20, y);
                }
                switch (rawSetting->type) {
                    case SettingType::separator: {
                        SIZE textSize;
                        win_draw::calculateTextSize(mHdc, *label, *globals::hDefaultFontP, &textSize);
                        // Line
                        HPEN hPen = CreatePen(PS_SOLID, 1, WCP_FOREGROUND);
                        HPEN hOldPen = (HPEN)SelectObject(mHdc, hPen);
                        const long lineY = y + textSize.cy / 2;
                        MoveToEx(mHdc, x, lineY, nullptr);
                        LineTo(mHdc, winClientW - x - WSC_SCROLLBAR_WIDTH, lineY);
                        SelectObject(mHdc, hOldPen);
                        DeleteObject(hPen);
                        // Text
                        win_draw::drawText(mHdc, *label, winClientW / 2 - textSize.cx / 2, y);
                        break;
                    }
                    case SettingType::checkbox: {
                        auto &setting = dynamic_cast<SettingCheckbox &>(*rawSetting);
                        // Checkbox
                        const long checkboxX = winClientW - x - WSC_SCROLLBAR_WIDTH - CONFIG_UI_SETTINGS_DRAW_ROW_HEIGHT;
                        const RECT rect{
                            .left = checkboxX,
                            .top = y,
                            .right = checkboxX + CONFIG_UI_SETTINGS_DRAW_ROW_HEIGHT,
                            .bottom = y + CONFIG_UI_SETTINGS_DRAW_ROW_HEIGHT
                        };
                        win_draw::drawCheckBox(mHdc, setting.value, rect, L"", checkboxBg, checkboxFg);
                        break;
                    }
                    case SettingType::number: {
                        auto &setting = dynamic_cast<SettingNumber &>(*rawSetting);
                        break;
                    }
                    case SettingType::select: {
                        // auto &setting = dynamic_cast<SettingSelect &>(*rawSetting);
                        break;
                    }
                }
                start += CONFIG_UI_SETTINGS_DRAW_ROW_HEIGHT;
                SelectObject(mHdc, oldFont);
                SetTextColor(mHdc, oldColor);
            }
            DeleteObject(checkboxBg);
            DeleteObject(checkboxFg);

            // Header
            RECT wRect;
            GetWindowRect(hWnd, &wRect);
            wRect.left = 0;
            wRect.top = 0;
            wRect.bottom = WSC_CONFIG_HEADER;
            FillRect(mHdc, &wRect, win_draw::createBrush(WCP_BACKGROUND2));
            win_draw::deleteLastBrush();

            // Scrollbar
            scrollable.drawScrollBars(mHdc, *globals::hDefaultFontBoldP);

            win_draw::doubleBuffering(hWnd, ps, nullptr, false, &winClientW, &winClientH);
            break;
        }
        default:
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    return 1;
}

void config_ui::redrawWindow() {
    RECT rect;
    GetClientRect(windowHandle, &rect);
    InvalidateRect(windowHandle, &rect, true);
}

void config_ui::init(HICON hIcon) {
    WNDCLASSEX wc    = {};
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

    buildSettings();

    initOk = windowHandle != nullptr;
}

void config_ui::open() {
    if (!initOk)
        return;

    scrollable.scrollXPos = 0;
    scrollable.scrollYPos = 0;
    scrollable.updateXYScrollBarsInfo();

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