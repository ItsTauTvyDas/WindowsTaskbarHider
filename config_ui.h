#ifndef WINDOWSTASKBARHIDER_CONFIG_UI_H
#define WINDOWSTASKBARHIDER_CONFIG_UI_H

#include <windows.h>

#include "win_draw.h"

#define CONFIG_UI_WINDOW_MIN_WIDTH  500
#define CONFIG_UI_WINDOW_MIN_HEIGHT 400

class config_ui {
public:
    static bool initOk;

    static void init(HICON icon);
    static void open();
    static void close();
    static void save();
private:
    static win_draw::scrollable_content scrollable;
    static HWND windowHandle;
    static int winClientW, winClientH;
    static int getContentHeight();
    static int getContentWidth();
    static void redrawWindow();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif //WINDOWSTASKBARHIDER_CONFIG_UI_H
