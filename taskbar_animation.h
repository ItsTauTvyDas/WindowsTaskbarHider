#ifndef TASKBAR_ANIMATION_H
#define TASKBAR_ANIMATION_H

#include <mutex>
#include <windows.h>
#include <thread>

class taskbar_animation {
public:
    static std::thread animationThread;
    static std::mutex animationMutex;

    static void initThread();
    static bool isAnimating(HWND taskbar);
    static void animate(HWND taskbar, bool causedByMaximizedWindow);
};

#endif //TASKBAR_ANIMATION_H
