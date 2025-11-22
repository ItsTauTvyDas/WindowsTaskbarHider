#ifndef WINDOWSTASKBARHIDER_WIN32THREAD_H
#define WINDOWSTASKBARHIDER_WIN32THREAD_H

#include <windows.h>

class win32thread {
public:
    win32thread();
    explicit win32thread(const LPTHREAD_START_ROUTINE& proc);

    win32thread& operator=(const win32thread&) = delete;
    win32thread& operator=(win32thread&& other) noexcept;

    void join();

    [[nodiscard]] bool joinable() const noexcept
    {
        return handle != nullptr;
    }

    [[nodiscard]] HANDLE getHandle() const noexcept {
        return handle;
    }

private:
    HANDLE handle;
};

#endif