#include "taskbar_animation.h"

#include <algorithm>
#include <iostream>
#include <windows.h>
#include <thread>

#include "config.h"
#include "globals.h"

std::unordered_map<HWND, int> taskbar_animation::animatingState = {};
std::thread taskbar_animation::animationThread;
std::mutex animationMutex;

void animationLoop() {
    while (true) {
        {
            std::lock_guard lock(animationMutex);

            for (auto it = taskbar_animation::animatingState.begin(); it != taskbar_animation::animatingState.end();) {
                HWND taskbar = it->first;
                const int visibleOpacityPoint = it->second;

                POINT cursorPos;
                RECT taskbarRect;

                GetCursorPos(&cursorPos);
                GetWindowRect(taskbar, &taskbarRect);

                BYTE opacityBytes = 0;
                if (GetLayeredWindowAttributes(taskbar, nullptr, &opacityBytes, nullptr)) {
                    int currentOpacity = opacityBytes;

                    const int delta = (PtInRect(&taskbarRect, cursorPos) ? 1 : -1) * config::animationOpacityStep;
                    currentOpacity = std::clamp(currentOpacity + delta, visibleOpacityPoint, config::opacityWhenHoveredInternal);

                    if (currentOpacity <= visibleOpacityPoint || currentOpacity >= config::opacityWhenHoveredInternal) {
                        it = taskbar_animation::animatingState.erase(it);
                        continue;
                    }

                    SetLayeredWindowAttributes(taskbar, 0, static_cast<BYTE>(currentOpacity), LWA_ALPHA);
                }
                ++it;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(config::animationStepDelay));
        if (globals::isShuttingDown)
            break;
    }
}

void taskbar_animation::initThread() {
    if (animationThread.joinable())
        return;
    animationThread = std::thread(animationLoop);
}

bool taskbar_animation::isAnimating(HWND taskbar) {
    return animatingState.contains(taskbar);
}

void taskbar_animation::animate(HWND taskbar, const bool causedByMaximizedWindow) {
    // int opacity = 0;
    // GetLayeredWindowAttributes(taskbar, nullptr, reinterpret_cast<BYTE *>(&opacity), nullptr);
    // opacity += hoveredOver ? config::animationOpacityStep : -config::animationOpacityStep;
    // if (opacity <= config::opacityWhenHiddenInternal || opacity >= config::opacityWhenHoveredInternal)
    //     return;

    std::lock_guard lock(animationMutex);
    if (!animatingState.contains(taskbar)) {
        animatingState[taskbar] = causedByMaximizedWindow ? config::opacityWhenShownInternal : config::opacityWhenHiddenInternal;
    }
}