#ifndef UTILS_H
#define UTILS_H

#include <dwmapi.h>
#include <functional>
#include <string>

class utils {
public:
    static void getProcessInfo(HWND hwnd, std::string &processExeName);
    static bool killProcessByName(const char *filename, DWORD currentPid);
    static bool processArguments(int argc, char* argv[]);
    static void showExceptionMessageBox(const std::function<void(std::stringstream&)>& callback, bool allowRetry);
    static LPSTR replaceCharacterWithText(LPSTR lpstr, char target, const std::string &replacement, int skip = 0);
    static std::string exceptionName(DWORD exceptionCode);
    static LPSTR NTStatusMessageToText(DWORD NTStatusMessage);
    static std::string joinString(const std::vector<std::string> &vec, const std::string &delimiter);
    static std::vector<std::string> splitString(const std::string &str, char delimiter);
    static void ltrim(std::string &s);
    static void rtrim(std::string &s);
    static void trim(std::string &s);
    static std::vector<std::string> splitToGroups(std::string s, unsigned int length);
    static bool fileExists(const char *path);
    static bool doesAutoStart();
    static void toggleStartup();
    static bool toggleConsoleWindow(PHANDLER_ROUTINE handler, bool status);
    static void toUnicode(LPCCH string, LPWSTR str);
    static void clearConsole(COORD startCoord, bool setPosAfter);
    static bool attachConsoleWindow();
private:
    static std::string createShortcutLinkPath();
};

#endif //UTILS_H
