#ifndef STDCERR_H
#define STDCERR_H

#include <windows.h>
#include <iostream>
#include <streambuf>
#include <fcntl.h>

class stdcerr final : public std::streambuf {
    HANDLE hCon;
    std::streambuf* oldBuf;
public:
    stdcerr() {
        hCon = GetStdHandle(STD_ERROR_HANDLE);
        oldBuf = std::cerr.rdbuf(this);
    }
    ~stdcerr() override {
        std::cerr.rdbuf(oldBuf);
    }
protected:
    int overflow(const int c) override {
        if (c == EOF) return c;
        SetConsoleTextAttribute(hCon, FOREGROUND_RED | FOREGROUND_INTENSITY);
        DWORD written;
        WriteConsoleA(hCon, &c, 1, &written, nullptr);
        SetConsoleTextAttribute(hCon, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        return c;
    }
};

#endif //STDCERR_H
