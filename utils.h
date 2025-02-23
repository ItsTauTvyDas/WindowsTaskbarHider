#ifndef UTILS_H
#define UTILS_H

#include <dwmapi.h>
#include <functional>
#include <string>

class utils {
public:
    static std::string getProcessName(HWND hwnd);
    static bool killProcessByName(const char *filename, DWORD currentPid);
    static bool processArguments(int argc, char* argv[]);
    static void showExceptionMessageBox(const std::function<void(std::stringstream&)>& callback, bool allowRetry);
    static LPSTR replaceCharacterWithText(LPSTR lpstr, char target, const std::string &replacement, int skip = 0);
    static std::string exceptionName(DWORD exceptionCode);
    static LPSTR NTStatusMessageToText(DWORD NTStatusMessage);
    static std::string joinString(const std::vector<std::string> &vec, const std::string &delimiter);
    static std::vector<std::string> splitString(const std::string &str, char delimiter);
    static bool fileExists(const char *path);
    static bool doesAutoStart();
    static void toggleStartup();
    static bool toggleConsoleWindow(PHANDLER_ROUTINE handler, bool status);
    static void toUnicode(LPCCH string, LPWSTR str);
    static void clearConsole(COORD startCoord);
    static bool attachConsoleWindow();
private:
    static std::string createShortcutLinkPath();
};

#endif //UTILS_H
