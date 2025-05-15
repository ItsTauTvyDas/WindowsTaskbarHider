#ifndef TASKBAR_ANIMATION_H
#define TASKBAR_ANIMATION_H

#include <unordered_map>
#include <vector>
#include <windows.h>
#include <bits/std_thread.h>

class taskbar_animation {
public:
    static std::unordered_map<HWND, int> animatingState;
    static std::thread animationThread;

    static void initThread();
    static bool isAnimating(HWND taskbar);
    static void animate(HWND taskbar, bool causedByMaximizedWindow);
};

#endif //TASKBAR_ANIMATION_H
