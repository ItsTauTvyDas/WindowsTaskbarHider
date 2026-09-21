#include "config_ui.h"

#include "globals.h"
#include "resources.h"
#include "language.h"
#include "win_draw.h"
#include "_compiletime_helper.hpp"

#define CONFIG_CLASS_NAME PROJECT_NAME"Config"


#define SETTING_SEPARATOR(cat)          \
    {                                   \
        .type = SettingType::separator, \
        .langKey = cat                  \
    }

#define SETTING_CHECKBOX(cat, key, var)   \
    {                                     \
        .type = SettingType::checkbox,    \
        .langKey = TO_TITLE_CASE(key),    \
        .iniKey = CONFIG_KEY(cat, key),   \
        .boolean = &config::var           \
    }

#define SETTING_NUMBER(cat, key, var, lo, hi, ...) \
    {                                              \
        .type = SettingType::number,               \
        .langKey = TO_TITLE_CASE(key),             \
        .iniKey = CONFIG_KEY(cat, key),            \
        .number = &config::var,                    \
        .min = lo,                                 \
        .max = hi                                  \
        __VA_OPT__(, .unit = __VA_ARGS__)          \
    }

constexpr config_ui::Setting config_ui::settings[] = {
    SETTING_SEPARATOR(CONFIG_CAT_WINDOW),
    SETTING_CHECKBOX(CONFIG_CAT_WINDOW, CONFIG_KEY_DARK_MODE, darkMode),
    SETTING_CHECKBOX(CONFIG_CAT_WINDOW, CONFIG_KEY_AUTO_UPDATE, autoUpdate),
    SETTING_CHECKBOX(CONFIG_CAT_WINDOW, CONFIG_KEY_SHOW_ALL_WINDOWS, showAllWindows),

    SETTING_SEPARATOR(CONFIG_CAT_WINDOW_BEHAVIOUR),
    SETTING_CHECKBOX(CONFIG_CAT_WINDOW_BEHAVIOUR, CONFIG_KEY_OPEN_ON_START, openOnStart),
    SETTING_CHECKBOX(CONFIG_CAT_WINDOW_BEHAVIOUR, CONFIG_KEY_CLOSE_TO_TRAY, closeToTray),
    SETTING_CHECKBOX(CONFIG_CAT_WINDOW_BEHAVIOUR, CONFIG_KEY_MINIMIZE_TO_TRAY, minimizeToTray),
    SETTING_CHECKBOX(CONFIG_CAT_WINDOW_BEHAVIOUR, CONFIG_KEY_CLOSE_CONFIRM_MESSAGE, closeConfirmMessage),
    SETTING_CHECKBOX(CONFIG_CAT_WINDOW_BEHAVIOUR, CONFIG_KEY_DISABLE_AUTO_UPDATE_WHEN_UNFOCUSED, disableAutoUpdateWhenUnfocused),
    SETTING_CHECKBOX(CONFIG_CAT_WINDOW_BEHAVIOUR, CONFIG_KEY_AUTO_UPDATE_ON_OPEN, autoUpdateOnOpen),

    SETTING_NUMBER(CONFIG_CAT_TASKBAR, CONFIG_KEY_UPDATE_INTERVAL, taskbarUpdateInterval, 1, 1000, L"ms"),
    SETTING_CHECKBOX(CONFIG_CAT_TASKBAR, CONFIG_KEY_USE_REAL_OPACITY_VALUES, useRealOpacityValues),
    SETTING_NUMBER(CONFIG_CAT_TASKBAR, CONFIG_KEY_OPACITY_WHEN_HIDDEN, opacityWhenHidden, 0, 255),
    SETTING_NUMBER(CONFIG_CAT_TASKBAR, CONFIG_KEY_OPACITY_WHEN_SHOWN, opacityWhenShown, 1, 255),
    SETTING_NUMBER(CONFIG_CAT_TASKBAR, CONFIG_KEY_OPACITY_WHEN_HOVERED_OVER, opacityWhenHovered, 1, 255),

    SETTING_SEPARATOR(CONFIG_CAT_TASKBAR_HOVER_ANIMATION),
    SETTING_CHECKBOX(CONFIG_CAT_TASKBAR_HOVER_ANIMATION, CONFIG_KEY_ANIMATION_ENABLED, animationsEnabled),
    SETTING_NUMBER(CONFIG_CAT_TASKBAR_HOVER_ANIMATION, CONFIG_KEY_ANIMATION_STEP_DELAY, animationStepDelay, 1, 1000),
    SETTING_NUMBER(CONFIG_CAT_TASKBAR_HOVER_ANIMATION, CONFIG_KEY_ANIMATION_OPACITY_STEP, animationOpacityStep, 1, 255),

    SETTING_SEPARATOR(CONFIG_CAT_IGNORED_WINDOWS),
    SETTING_CHECKBOX(CONFIG_CAT_IGNORED_WINDOWS, CONFIG_KEY_ALWAYS_IGNORE_WHEN_NOT_MAXIMIZED, alwaysIgnoreWhenNotMaximized),
    SETTING_CHECKBOX(CONFIG_CAT_IGNORED_WINDOWS, CONFIG_KEY_EXCEPT_TASKBAR_POPUPS, exceptTaskbarPopups),

    SETTING_SEPARATOR(CONFIG_CAT_WINDOWS_10_FIXES),
    SETTING_CHECKBOX(CONFIG_CAT_WINDOWS_10_FIXES, CONFIG_KEY_FIX_TASKBAR_HOVER_GLITCH, fixTaskbarHoverGlitch),
};

constexpr std::size_t config_ui::settingsCount = std::size(settings);
bool config_ui::initOk = false;
HWND config_ui::windowHandle;
int config_ui::winClientW;
int config_ui::winClientH;
std::vector<HWND> config_ui::controls;
win_draw::scrollable_content config_ui::scrollable(getContentWidth, getContentHeight, &winClientW, &winClientH, WSC_CONFIG_HEADER);


void config_ui::redrawWindow() {
    RECT rect;
    GetClientRect(windowHandle, &rect);
    InvalidateRect(windowHandle, &rect, true);
}

void config_ui::layoutControls() {
    constexpr int rowH = CONFIG_UI_SETTINGS_DRAW_ROW_HEIGHT;
    const int checkboxX = winClientW - CONFIG_UI_SETTINGS_DRAW_X - WSC_SCROLLBAR_WIDTH - rowH;

    HDWP dwp = BeginDeferWindowPos(settingsCount);
    if (!dwp)
        return;
    for (std::size_t i = 0; i < settingsCount; i++) {
        if (!controls[i])
            continue;
        const int y = CONFIG_UI_SETTINGS_DRAW_Y + static_cast<int>(i) * rowH - scrollable.scrollYPos;
        const bool visible = y >= WSC_CONFIG_HEADER && y + rowH <= winClientH;
        const bool wasVisible = IsWindowVisible(controls[i]) != FALSE;
        UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS;
        if (visible && !wasVisible) flags |= SWP_SHOWWINDOW;
        else if (!visible && wasVisible) flags |= SWP_HIDEWINDOW;
        dwp = DeferWindowPos(dwp, controls[i], nullptr, checkboxX, y, rowH, rowH, flags);
        if (!dwp)
            return;
    }
    EndDeferWindowPos(dwp);
}

LRESULT CALLBACK config_ui::WndProc(HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            const auto create = reinterpret_cast<LPCREATESTRUCT>(lParam);
            scrollable.createScrollbars(hWnd, create->hInstance, false, true);
            // Create controls
            controls.assign(settingsCount, nullptr);
            for (std::size_t i = 0; i < settingsCount; i++) {
                const Setting &s = settings[i];
                if (s.type != SettingType::checkbox)
                    continue;

                HWND handle = CreateWindowExW(
                    0, WMC_BUTTON, L"",
                    WS_CHILD | WS_VISIBLE | BS_CHECKBOX | BS_OWNERDRAW,
                    0, 0, CONFIG_UI_SETTINGS_DRAW_ROW_HEIGHT, CONFIG_UI_SETTINGS_DRAW_ROW_HEIGHT,
                    hWnd, reinterpret_cast<HMENU>(ID_CONFIG_SETTING_BASE + i),
                    globals::hIns, nullptr);
                SendMessageW(handle, WM_SETFONT, reinterpret_cast<WPARAM>(*globals::hDefaultFontP), true);
                SendMessageW(handle, WM_UPDATEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);
                SetClassLongPtrW(handle, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(LoadCursorW(nullptr, IDC_HAND)));
                SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&s));
                controls[i] = handle;
            }
            break;
        }
        case WM_SIZE: {
            if (wParam != SIZE_MINIMIZED) {
                winClientW = LOWORD(lParam);
                winClientH = HIWORD(lParam);
                scrollable.onWindowMove();
                scrollable.updateXYScrollBarsInfo();
                layoutControls();
                InvalidateRect(hWnd, nullptr, TRUE);
            }
            break;
        }
        case WM_HSCROLL:
        case WM_VSCROLL: {
            if (!scrollable.onScroll(lParam, wParam))
                break;
            layoutControls();
            redrawWindow();
            break;
        }
        case WM_MOUSEWHEEL:
        {
            if (!scrollable.onMouseWheel(wParam, CONFIG_UI_SETTINGS_DRAW_ROW_HEIGHT))
                break;
            layoutControls();
            redrawWindow();
            break;
        }
        case WM_COMMAND: {
            if (const int id = LOWORD(wParam); id >= ID_CONFIG_SETTING_BASE && id < ID_CONFIG_SETTING_BASE + static_cast<int>(settingsCount)) {
                if (const Setting &s = settings[id - ID_CONFIG_SETTING_BASE]; s.type == SettingType::checkbox) {
                    if (s.boolean == &config::autoUpdate) {
                        SendMessageW(globals::hWnd, WM_COMMAND, MAKEWPARAM(ID_CHECKBOX_AUTO_UPDATE, BN_CLICKED), 0);
                    } else if (s.boolean == &config::showAllWindows) {
                        SendMessageW(globals::hWnd, WM_COMMAND, MAKEWPARAM(ID_CHECKBOX_SHOW_ALL_WINDOWS, BN_CLICKED), 0);
                    } else {
                        s.boolean->store(!s.boolean->load());
                    }

                    if (&config::darkMode == s.boolean) {
                        win_draw::updateTitlebarColors(globals::hWnd);
                        win_draw::updateTitlebarColors(hWnd);
                        RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
                        win_draw::redrawWindow(globals::hWnd);
                    } else {
                        win_draw::redrawWindow(controls[id - ID_CONFIG_SETTING_BASE]);
                    }
                }
            }
            break;
        }
        case WM_DRAWITEM: {
            const auto d = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
            const auto s = reinterpret_cast<const Setting*>(GetWindowLongPtrW(d->hwndItem, GWLP_USERDATA));
            if (!s || s->type != SettingType::checkbox)
                break;

            PAINTSTRUCT ps;
            HDC mHdc = win_draw::doubleBuffering(hWnd, ps, d->hDC, true, &winClientW, &winClientH);
            HBRUSH bg = CreateSolidBrush(WCP_BACKGROUND);
            HBRUSH fg = CreateSolidBrush(WCP_FOREGROUND);
            SelectObject(mHdc, *globals::hDefaultFontP);
            win_draw::drawCheckBox(mHdc, s->boolean->load(), d->rcItem, L"", bg, fg);
            DeleteObject(bg);
            DeleteObject(fg);

            win_draw::doubleBuffering(hWnd, ps, d->hDC, false, &winClientW, &winClientH);
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
                    return HTBORDER;
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
            for (const auto &setting : settings) {
                const long y = start - scrollable.scrollYPos;
                const long x = CONFIG_UI_SETTINGS_DRAW_X/* - scrollable.scrollXPos*/;
                const wchar_t *label = setting.langKey;

                HGDIOBJ oldFont = SelectObject(mHdc, *globals::hDefaultFontP);
                SetBkColor(mHdc, WCP_BACKGROUND);
                COLORREF oldColor = SetTextColor(mHdc, WCP_FOREGROUND);
                if (setting.type != SettingType::separator) {
                    // Draw label for every control except separator
                    win_draw::drawText(mHdc, label, x + 20, y);
                }
                switch (setting.type) {
                    case SettingType::separator: {
                        SIZE textSize;
                        win_draw::calculateTextSize(mHdc, label, *globals::hDefaultFontP, &textSize);
                        // Line
                        HPEN hPen = CreatePen(PS_SOLID, 1, WCP_FOREGROUND);
                        HPEN hOldPen = (HPEN)SelectObject(mHdc, hPen);
                        const long lineY = y + textSize.cy / 2;
                        MoveToEx(mHdc, x, lineY, nullptr);
                        LineTo(mHdc, winClientW - x - WSC_SCROLLBAR_WIDTH, lineY);
                        SelectObject(mHdc, hOldPen);
                        DeleteObject(hPen);
                        // Text
                        win_draw::drawText(mHdc, label, winClientW / 2 - textSize.cx / 2, y);
                        break;
                    }
                    case SettingType::checkbox: {
                        break;
                    }
                    case SettingType::number: {
                        break;
                    }
                    case SettingType::select: {
                        break;
                    }
                    case SettingType::text:
                        break;
                }
                start += CONFIG_UI_SETTINGS_DRAW_ROW_HEIGHT;
                SelectObject(mHdc, oldFont);
                SetTextColor(mHdc, oldColor);
            }

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

int config_ui::getContentHeight() {
    return (settingsCount + 1) * CONFIG_UI_SETTINGS_DRAW_ROW_HEIGHT + CONFIG_UI_SETTINGS_DRAW_Y;
}

int config_ui::getContentWidth() {
    return 0;
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
        WS_EX_COMPOSITED | WS_EX_CLIENTEDGE, CONFIG_CLASS_NAME, L"Settings",
        WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX) | WS_CLIPCHILDREN,
        0, 0, CONFIG_UI_WINDOW_MIN_WIDTH, CONFIG_UI_WINDOW_MIN_HEIGHT,
        globals::hWnd, nullptr, globals::hIns, nullptr);

    RECT rect;
    GetClientRect(windowHandle, &rect);
    winClientH = rect.bottom - rect.top;
    winClientW = rect.right - rect.left;

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