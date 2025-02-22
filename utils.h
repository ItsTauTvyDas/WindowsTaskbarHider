#ifndef UTILS_H
#define UTILS_H

#include <dwmapi.h>
#include <functional>
#include <string>

class utils {
public:
    static std::string getProcessName(HWND hwnd);
    static void killProcessByName(const char *filename, DWORD currentPid);
    static std::string getProgramVersion();
    static bool processArguments(int argc, char* argv[]);
    static HICON loadExeIcon(LPCSTR pszExeFileName, UINT nIconIndex);
    static void showExceptionMessageBox(const std::function<void(std::stringstream&)>& callback);
    static LPSTR NTStatusMessageToText(DWORD NTStatusMessage);
    static std::string joinString(const std::vector<std::string> &vec, const std::string &delimiter);
    static std::vector<std::string> splitString(const std::string &str, char delimiter);
    static bool fileExists(const char *path);
    static bool doesAutoStart();
    static void toggleStartup();
    static void toggleConsoleWindow(PHANDLER_ROUTINE handler, bool status);
    static void toUnicode(LPCCH string, LPWSTR str);
    static void clearConsole(COORD startCoord);
private:
    static std::string createShortcutLinkPath();
    static void attachConsoleWindow();
};

#endif //UTILS_H
