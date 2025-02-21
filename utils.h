#ifndef UTILS_H
#define UTILS_H

#include <dwmapi.h>
#include <functional>
#include <string>

class utils {
public:
    static std::string getProcessName(HWND hwnd);
    static std::string getProgramVersion();
    static bool processArguments(int argc, char* argv[]);
    static HICON loadExeIcon(LPCSTR pszExeFileName, UINT nIconIndex);
    static void showExceptionMessageBox(const std::function<void(std::stringstream&)>& callback);
    static LPSTR NTStatusMessageToText(DWORD NTStatusMessage);
    static void throwIfNoDLLIcons(const std::string &dllPath);
    static std::string joinString(const std::vector<std::string> &vec, const std::string &delimiter);
    static std::vector<std::string> splitString(const std::string &str, char delimiter);
};

#endif //UTILS_H
