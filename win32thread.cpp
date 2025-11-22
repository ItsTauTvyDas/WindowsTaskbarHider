#include "win32thread.h"

win32thread::win32thread() : handle(nullptr) {}

win32thread::win32thread(const LPTHREAD_START_ROUTINE& proc)
{
    handle = CreateThread(
        nullptr,
        0,
        proc,
        nullptr,
        0,
        nullptr
    );
}

win32thread& win32thread::operator=(win32thread&& other) noexcept
{
    if (this != &other) {
        if (other.handle != nullptr)
            CloseHandle(other.handle);

        handle = other.handle;
        other.handle = nullptr;
    }
    return *this;
}

void win32thread::join()
{
    if (!joinable())
        return;

    WaitForSingleObject(handle, INFINITE);
    CloseHandle(handle);
    handle = nullptr;
}
