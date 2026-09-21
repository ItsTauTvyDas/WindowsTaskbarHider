#ifndef WINDOWSTASKBARHIDER_CONFIG_UI_H
#define WINDOWSTASKBARHIDER_CONFIG_UI_H

#include <memory>
#include <windows.h>

#include "win_draw.h"

#define CONFIG_UI_WINDOW_MIN_WIDTH         500
#define CONFIG_UI_WINDOW_MIN_HEIGHT        400
#define CONFIG_UI_SETTINGS_DRAW_Y          50
#define CONFIG_UI_SETTINGS_DRAW_X          5
#define CONFIG_UI_SETTINGS_DRAW_ROW_HEIGHT 20
#define ID_CONFIG_SETTING_BASE             5000

class config_ui {
public:
    static bool initOk;

    static void init(HICON icon);
    static void open();
    static void close();
    static void save();
private:
    enum class SettingType {
        separator,
        checkbox,
        text,
        number,
        select,
    };

    struct Setting {
        SettingType type = SettingType::separator;
        const wchar_t* langKey = nullptr;
        const wchar_t* iniKey = nullptr;
        union {
            std::atomic<bool>* boolean = nullptr;
            std::atomic<int>* number;
            std::wstring* text;
            std::vector<std::wstring>* select;
        };
        int min = 0;
        int max = 0;
        const wchar_t* unit = nullptr;
    };

    static const Setting settings[];
    static const std::size_t settingsCount;

    static std::vector<HWND> controls;
    static void createControls(HWND hWnd);
    static void layoutControls();

    static win_draw::scrollable_content scrollable;
    static HWND windowHandle;
    static int winClientW, winClientH;
    static int getContentHeight();
    static int getContentWidth();
    static void redrawWindow();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif //WINDOWSTASKBARHIDER_CONFIG_UI_H
