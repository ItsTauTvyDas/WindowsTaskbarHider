#include "taskbar_animation.h"

#include <algorithm>
#include <iostream>
#include <windows.h>

#include "config.h"
#include "globals.h"
#include "utils.h"
#include "win32thread.h"

std::unordered_map<HWND, int> animatingState = {};
// std::thread taskbar_animation::animationThread;
win32thread taskbar_animation::animationThread;

std::mutex taskbar_animation::animationMutex;

inline int nextOpacity(HWND taskbar, const int visibleOpacityPoint) {
    BYTE opacityBytes = 0;
    if (!GetLayeredWindowAttributes(taskbar, nullptr, &opacityBytes, nullptr))
        return 255;
    const int currentOpacity = opacityBytes;
    const int delta = (utils::mouseInWindow(taskbar) ? 1 : -1) * config::animationOpacityStep;
    return std::clamp(currentOpacity + delta, visibleOpacityPoint, config::opacityWhenHoveredInternal.load());
}

DWORD WINAPI animationLoop(LPVOID) {
    while (true) {
        if (globals::isShuttingDown)
            break;
        std::vector<std::pair<HWND, int>> states;
        {
            std::lock_guard lock(taskbar_animation::animationMutex);
            states.assign(animatingState.begin(), animatingState.end());
        }
        for (auto [taskbar, visibleOpacityPoint] : states) {
            const int currentOpacity = nextOpacity(taskbar, visibleOpacityPoint);
            SetLayeredWindowAttributes(taskbar, 0, static_cast<BYTE>(currentOpacity), LWA_ALPHA);
            if (currentOpacity == visibleOpacityPoint || currentOpacity == config::opacityWhenHoveredInternal) {
                std::lock_guard lock(taskbar_animation::animationMutex);
                animatingState.erase(taskbar);
            }
        }
        // std::this_thread::sleep_for(std::chrono::milliseconds(config::animationStepDelay));
        Sleep(config::animationStepDelay);
    }
    return 0;
}

void taskbar_animation::initThread() {
    // animationThread = std::thread(animationLoop);
    animationThread = win32thread(animationLoop);
}

bool taskbar_animation::isAnimating(HWND taskbar) {
    std::lock_guard lock(animationMutex);
    return animatingState.contains(taskbar);
}

void taskbar_animation::animate(HWND taskbar, const bool causedByMaximizedWindow) {
    if (animatingState.contains(taskbar))
        return;
    const int visibleOpacityPoint = causedByMaximizedWindow ? config::opacityWhenShownInternal : config::opacityWhenHiddenInternal;
    const int currentOpacity = nextOpacity(taskbar, visibleOpacityPoint);
    SetLayeredWindowAttributes(taskbar, 0, static_cast<BYTE>(currentOpacity), LWA_ALPHA);
    if (currentOpacity == visibleOpacityPoint || currentOpacity == config::opacityWhenHoveredInternal)
        return;
    std::lock_guard lock(animationMutex);
    animatingState[taskbar] = visibleOpacityPoint;
}