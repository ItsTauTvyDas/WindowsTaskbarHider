#ifndef UTILS_H
#define UTILS_H

#include <dwmapi.h>
#include <filesystem>
#include <functional>
#include <string>
#include <map>
#include <tlhelp32.h>

class utils {
public:
    template <typename K, typename V> struct pair {
        const K key;
        const V value;
    };

    static void getProcessInfo(HWND hWnd, std::wstring &processExeName, bool lowercase = false);
    static bool getProcessesByName(const wchar_t *processName, DWORD currentPid, const std::function<void (PROCESSENTRY32W*)> &func);
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
    static bool showTrayNotification(const std::wstring &message);
    static bool fileExists(const wchar_t *path, bool dir = false);
    static int stoi(const wchar_t* str, bool* success = nullptr);
#ifndef IS_PORTABLE
    static bool doesAutoStart();
    static void toggleStartup();
    static bool updateLanguageFile();
    static void exportLanguageFiles();
#endif
    static RECT rect(int x, int y, int width, int height);
    static bool mouseInRect(const RECT *rect, int vKey);
    static std::wstring getFormattedTime();
    static void loadIfNeededAndGetCachedLanguageString(unsigned int mType, std::wstring *string);
    static std::wstring formatLangString(const std::wstring &rStr, const std::vector<std::wstring> &values);
    static int messageBox(unsigned int mType, unsigned int uType, const std::vector<std::wstring> &values = {});
    static int messageBox(const std::wstring &mText, unsigned int uType);
    static int messageBoxRT(const std::wstring &mText, unsigned int uType);
    static std::wstring message(unsigned int mType, const std::vector<std::wstring> &values);
    static std::wstring message(unsigned int mType);
    static bool isUserUsingDarkTheme();
    static void toUnicode(LPCCH string, LPWSTR str);
    static bool processIniFileLine(const std::wstring &orgLine, std::wstring *prefix, std::wstring &key, std::wstring &value);
    static std::wstring utf8ToWide(const std::string& str);
    static bool mouseInWindow(HWND window);
    static bool hasBitmaskStrW(std::wstring str, const DWORD currentBitmask, const pair<const std::wstring_view, const DWORD>* entries, size_t count);
#ifndef IS_PORTABLE
    static std::filesystem::path toDataPath(const std::filesystem::path& relativePath);
#endif
private:
#ifndef IS_PORTABLE
    struct INILine {
        std::wstring key;
        std::wstring value;
        bool isComment;

        bool operator==(const INILine &other) const {
            return key == other.key &&
                   value == other.value &&
                   isComment == other.isComment;
        }
    };
    static bool loadInternalLanguageStringsIntoMap(std::map<int, INILine> &map);
    static void mapIniContent(std::map<int, INILine> &map, std::wistream &stream);
    static bool loadLanguageFromName(const std::wstring &shortName, std::wstringstream &wss);
#endif
};

#endif //UTILS_H
