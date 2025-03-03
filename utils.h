#ifndef UTILS_H
#define UTILS_H

#include <dwmapi.h>
#include <functional>
#include <string>

class utils {
public:
    static void getProcessInfo(HWND hwnd, std::wstring &processExeName);
    static bool killProcessByName(const wchar_t *processName, DWORD currentPid);
    static bool processArguments(int argc, const LPWSTR *argv, LPWSTR commandLine);
    static void showExceptionMessageBox(const std::function<void(std::wstringstream&)>& callback, bool allowRetry);
    static LPWSTR replaceCharacterWithText(LPWSTR lpstr, wchar_t target, const std::wstring &replacement, int skip = 0);
    static std::string exceptionNameA(DWORD exceptionCode);
    static std::wstring exceptionName(DWORD exceptionCode);
    static LPWSTR NTStatusMessageToText(DWORD NTStatusMessage);
    static std::wstring joinString(const std::vector<std::wstring> &vec, const std::wstring &delimiter);
    static std::vector<std::wstring> splitString(const std::wstring &str, wchar_t delimiter);
    static void ltrim(std::wstring &s);
    static void rtrim(std::wstring &s);
    static void trim(std::wstring &s);
    static std::vector<std::wstring> splitToGroups(const std::wstring& s, unsigned int length);
    static bool fileExists(const wchar_t *path);
    static bool doesAutoStart();
    static void toggleStartup();
    static RECT rect(int x, int y, int width, int height);
    static bool mouseInRect(const RECT *rect, int vKey);
    static std::wstring getFormattedTime();
    static bool processIniFileLine(const std::wstring& line, std::wstring *prefix, std::wstring &key, std::wstring &value);
    static void logcLangString(unsigned int mType, std::wstring *string);
    static std::wstring formatLangString(const std::wstring &rStr, const std::vector<std::wstring> &values);
    static int messageBox(unsigned int mType, unsigned int uType, const std::vector<std::wstring> &values = {});
    static int messageBox(const std::wstring &mText, unsigned int uType);
    static std::wstring message(unsigned int mType, const std::vector<std::wstring> &values);
    static std::wstring message(unsigned int mType);
    static bool isUserUsingDarkTheme();
    static void toUnicode(LPCCH string, LPWSTR str);
private:
    static std::wstring createShortcutLinkPath();
};

#endif //UTILS_H
