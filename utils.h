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
    static void throwIfNoDLLIcons(const std::string &dllPath);
    static std::string joinString(const std::vector<std::string> &vec, const std::string &delimiter);
    static std::vector<std::string> splitString(const std::string &str, char delimiter);
    static void attachConsoleWindow();
    static bool fileExists(const char *path);
    static bool doesAutoStart();
    static void toggleStartup();
    static void toggleConsoleWindow();
    static void toUnicode(LPCCH string, LPWSTR str);
private:
    static std::string createShortcutLinkPath(char appPath[]);
};

#endif //UTILS_H
