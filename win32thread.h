#ifndef WINDOWSTASKBARHIDER_WIN32THREAD_H
#define WINDOWSTASKBARHIDER_WIN32THREAD_H

class win32thread {
public:
    HANDLE handle;
    win32thread& operator=(const win32thread&) = delete;

    explicit win32thread() {
        handle = nullptr;
    }

    win32thread& operator=(win32thread&& other) noexcept
    {
        if (this != &other) {
            CloseHandle(other.handle);
            other.handle = nullptr;
            handle = other.handle;
        }
        return *this;
    }

    explicit win32thread(const LPTHREAD_START_ROUTINE& proc) {
        handle = CreateThread(
            nullptr,
            0,
            proc,
            nullptr,
            0,
            nullptr
        );
    }

    [[nodiscard]] bool joinable() const noexcept {
        return handle != nullptr;
    }

    void join() {
        if (!joinable())
            return;
        WaitForSingleObject(handle, INFINITE);
        CloseHandle(handle);
        handle = nullptr;
    }
};


#endif