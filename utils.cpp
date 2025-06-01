#include "utils.h"

#include <cmath>
#include <format>
#include <shlobj.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <tchar.h>
#include <windows.h>
#include "resources.h"
#include "config.h"
#include "taskbar.h"
#include <sstream>
#include <tlhelp32.h>
#include "globals.h"
#include <unordered_map>
#include <mutex>
#include <psapi.h>
#include "language.h"
#include <strsafe.h>
#include <algorithm>
#include <ranges>
#include <vector>
#include <filesystem>

bool utils::killProcessByName(const wchar_t* processName, DWORD currentPid) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnap, &pe)) {
        CloseHandle(hSnap);
        return false;
    }

    bool success = false;
    do {
        if (currentPid == pe.th32ProcessID)
            continue;
        if (_wcsicmp(pe.szExeFile, processName) != 0)
            continue;
        if (HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID)) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
            success = true;
        }
    } while (Process32Next(hSnap, &pe));
    CloseHandle(hSnap);
    return success;
}

void utils::getProcessInfo(HWND hwnd, std::wstring &processExeName, const bool lowercase) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        processExeName = L"#_unknown(?)";
        return;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) {
        processExeName = L"#_unknown(0)";
        return;
    }

    HMODULE hMod;
    DWORD cbNeeded;
    if (!EnumProcessModules(hProcess, &hMod, sizeof(HMODULE), &cbNeeded)) {
        CloseHandle(hProcess);
        processExeName = L"#_unknown(1)";
        return;
    }

    wchar_t processName[MAX_PATH] = {};
    if (GetModuleBaseNameW(hProcess, hMod, processName, MAX_PATH) == 0)
    {
        CloseHandle(hProcess);
        processExeName = L"#_unknown(2)";
        return;
    }

    CloseHandle(hProcess);
    processExeName = std::wstring(processName);
    if (lowercase)
        std::ranges::transform(processExeName,processExeName.begin(),[](const wchar_t c) {
            return std::towlower(c);
        });
}

bool utils::processArguments(const int argc, const LPWSTR *argv, LPWSTR commandLine) {
    if (argv == nullptr) {
        messageBox(MSG_ARGS_PARSE_FAILED, MB_ICONERROR | MB_OK, { std::wstring(commandLine) });
        return false;
    }

    const auto processExeName = std::wstring(argv[0]);
    globals::exe = processExeName.substr(processExeName.find_last_of(L"/\\") + 1);
    if (argc < 2) return true;
    auto arg = std::wstring(argv[1]);

    if (arg == L"--reset-taskbar" || arg == L"-rtb") {
        taskbar::resetTaskbar();
        messageBox(MSG_TASKBAR_VISIBILITY_FIXED, MB_ICONINFORMATION | MB_OK);
        return false;
    }

    for (int i = 1; i < argc; i++) {
        arg = argv[i];
        if (arg == L"--no-config" || arg == L"-nc") {
            globals::noConfigFile = true;
        } else if ((arg.compare(0, 3, L"-c:") == 0 || arg.compare(0, 9, L"--config:") == 0) && i + 1 < argc) {
            if (const size_t colonPos = arg.find(':'); colonPos != std::string::npos && colonPos + 1 < arg.size()) {
                config::processSingle(arg.substr(colonPos), argv[i + 1]);
                i++;
            }
        } else {
            messageBox(MSG_ARGS_INVALID, MB_ICONERROR | MB_OK, {arg});
            return false;
        }
    }
    return true;
}

std::wstring replaceLastErrorPlaceholder(const std::wstring& message) {
    LPWSTR errorText = utils::NTStatusMessageToText(GetLastError());
    std::wstring_view errorTextView(errorText ? errorText : L"");
    return std::vformat(message, std::wformat_args(std::make_wformat_args(errorTextView)));
}

void utils::showExceptionMessageBox(const std::function<void(std::wstringstream&)>& callback, const bool allowRetry) {
    std::wstringstream crashInfo;
    crashInfo << message(MSG_UNCAUGHT_EXCEPTION_HEADER) << std::endl;
    crashInfo << std::endl;
    callback(crashInfo);
    if (!allowRetry) {
        messageBox(replaceLastErrorPlaceholder(crashInfo.str()), MB_ICONERROR | MB_OK);
        return;
    }
    crashInfo << std::endl << message(MSG_UNCAUGHT_EXCEPTION_RETRY);
    if (messageBox(replaceLastErrorPlaceholder(crashInfo.str()), MB_ICONERROR | MB_RETRYCANCEL) == 4) {
        WCHAR path[MAX_PATH];
        if (GetModuleFileName(nullptr, path, MAX_PATH) == 0) {
            messageBox(MSG_UNCAUGHT_EXCEPTION_RETRY_FAILED, MB_ICONERROR | MB_OK, { NTStatusMessageToText(GetLastError()) });
            return;
        }
        ShellExecute(nullptr, L"open", path, globals::args.c_str(), nullptr, SW_SHOWNORMAL);
    }
}

LPWSTR utils::replaceCharacterWithText(LPWSTR lpstr, const wchar_t target, const std::wstring &replacement, const int skip) {
    if (!lpstr)
        return nullptr;
    const wchar_t* pos = lpstr;
    for (int i = 0; i < skip; ++i) {
        pos = wcschr(pos, target);
        if (!pos)
            return lpstr;
        pos++;
    }
    pos = wcschr(pos, target);
    if (!pos)
        return lpstr;
    const auto index = pos - lpstr;
    std::wstring str(lpstr);
    str.replace(index, 1, replacement);
    const auto size = (str.size() + 1) * sizeof(wchar_t);
    const auto newStr = static_cast<LPWSTR>(LocalAlloc(LMEM_FIXED, size));
    if (!newStr)
        return lpstr;
    wcscpy_s(newStr, str.size() + 1, str.c_str());
    LocalFree(lpstr);
    return newStr;
}

std::wstring utils::exceptionName(const DWORD exceptionCode) {
    std::string ex = exceptionNameA(exceptionCode);
    std::wstring w(ex.begin(), ex.end());
    return w;
}

std::string utils::exceptionNameA(const DWORD exceptionCode) {
    switch (exceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:         return STRINGIFY(EXCEPTION_ACCESS_VIOLATION);
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return STRINGIFY(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
        case EXCEPTION_BREAKPOINT:               return STRINGIFY(EXCEPTION_BREAKPOINT);
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return STRINGIFY(EXCEPTION_DATATYPE_MISALIGNMENT);
        case EXCEPTION_FLT_DENORMAL_OPERAND:     return STRINGIFY(EXCEPTION_FLT_DENORMAL_OPERAND);
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return STRINGIFY(EXCEPTION_FLT_DIVIDE_BY_ZERO);
        case EXCEPTION_FLT_INEXACT_RESULT:       return STRINGIFY(EXCEPTION_FLT_INEXACT_RESULT);
        case EXCEPTION_FLT_INVALID_OPERATION:    return STRINGIFY(EXCEPTION_FLT_INVALID_OPERATION);
        case EXCEPTION_FLT_OVERFLOW:             return STRINGIFY(EXCEPTION_FLT_OVERFLOW);
        case EXCEPTION_FLT_STACK_CHECK:          return STRINGIFY(EXCEPTION_FLT_STACK_CHECK);
        case EXCEPTION_FLT_UNDERFLOW:            return STRINGIFY(EXCEPTION_FLT_UNDERFLOW);
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return STRINGIFY(EXCEPTION_ILLEGAL_INSTRUCTION);
        case EXCEPTION_IN_PAGE_ERROR:            return STRINGIFY(EXCEPTION_IN_PAGE_ERROR);
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return STRINGIFY(EXCEPTION_INT_DIVIDE_BY_ZERO);
        case EXCEPTION_INT_OVERFLOW:             return STRINGIFY(EXCEPTION_INT_OVERFLOW);
        case EXCEPTION_INVALID_DISPOSITION:      return STRINGIFY(EXCEPTION_INVALID_DISPOSITION);
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return STRINGIFY(EXCEPTION_NONCONTINUABLE_EXCEPTION);
        case EXCEPTION_PRIV_INSTRUCTION:         return STRINGIFY(EXCEPTION_PRIV_INSTRUCTION);
        case EXCEPTION_SINGLE_STEP:              return STRINGIFY(EXCEPTION_SINGLE_STEP);
        case EXCEPTION_STACK_OVERFLOW:           return STRINGIFY(EXCEPTION_STACK_OVERFLOW);
        default:                                 return "EXCEPTION_UNKNOWN";
    }
}

LPWSTR utils::NTStatusMessageToText(const DWORD NTStatusMessage)
{
    // https://web.archive.org/web/20150121053632/http://support.microsoft.com/kb/259693
    LPWSTR lpMessageBuffer = nullptr;
    const HMODULE module = LoadLibrary(L"NTDLL.DLL");
    const DWORD length = FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE,
        module,
        NTStatusMessage,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPTSTR>(&lpMessageBuffer),
        0,
        nullptr);
    FreeLibrary(module);
    return length > 0 ? lpMessageBuffer : nullptr;
}

std::wstring utils::joinString(const std::vector<std::wstring> &vec, const std::wstring &delimiter) {
    std::wostringstream result;
    for (size_t i = 0; i < vec.size(); ++i) {
        result << vec[i];
        if (i < vec.size() - 1)
            result << delimiter;
    }
    return result.str();
}

std::vector<std::wstring> utils::splitString(const std::wstring &str, const wchar_t delimiter) {
    std::vector<std::wstring> result;
    std::wstringstream ss(str);
    std::wstring item;
    while (std::getline(ss, item, delimiter))
        result.push_back(item);
    return result;
}

void utils::ltrim(std::wstring &s) {
    s.erase(s.begin(), std::ranges::find_if(s, [](const unsigned char ch) {
        return !std::isspace(ch);
    }));
}

void utils::rtrim(std::wstring &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](const unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

void utils::trim(std::wstring &s) {
    rtrim(s);
    ltrim(s);
}

std::vector<std::wstring> utils::splitToGroups(const std::wstring& s, const unsigned int length) {
    if (s.length() <= length)
        return { s };
    if (length == 0)
        return {};
    std::vector<std::wstring> result = {};
    int splitCount = 0;
    while (splitCount * length <= s.length()) {
        std::wstring lastStr = s.substr(splitCount * length, length);
        splitCount++;
        result.push_back(lastStr);
    }
    return result;
}

bool utils::showTrayNotification(const std::wstring &message)
{
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = globals::hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    StringCchCopy(nid.szInfo, std::size(nid.szInfo), message.c_str());
    StringCchCopy(nid.szInfoTitle, std::size(nid.szInfoTitle), utils::message(MSG_APPLICATION_NAME).c_str());
    nid.dwInfoFlags = NIIF_INFO;
    return Shell_NotifyIcon(NIM_MODIFY, &nid) == TRUE;
}

bool utils::fileExists(const wchar_t *path, const bool dir) {
    std::error_code errC;
    const std::filesystem::path fsPath(path);
    return dir ? is_directory(fsPath, errC) : is_regular_file(fsPath, errC);
}

bool mkdir(const wchar_t* path) {
    std::error_code errC;
    std::filesystem::create_directory(path, errC);
    return !errC || errC == std::errc::file_exists;
}

void utils::toUnicode(const LPCCH string, LPWSTR str) {
    MultiByteToWideChar(CP_ACP, 0, string, -1, str, MAX_PATH);
}

#if IS_PORTABLE == 0
std::wstring createShortcutLinkPath(const bool global) {
    WCHAR startupPath[260];
    if (FAILED(SHGetFolderPath(nullptr, global ? CSIDL_COMMON_STARTUP : CSIDL_STARTUP, nullptr, 0, startupPath)))
        return L"";
    const std::wstring name = globals::exe.substr(0, globals::exe.find_last_of('.'));
    return std::wstring(std::wstring(startupPath) + L"\\" + name + L".lnk");
}

bool GetWorkingDirectory(LPWSTR* pDirectory) {
    if (pDirectory == nullptr) {
        return false;
    }
    const DWORD requiredSize = GetCurrentDirectoryW(0, nullptr);
    if (requiredSize == 0)
        return false;
    const auto buffer = new wchar_t[requiredSize];
    if (const DWORD length = GetCurrentDirectoryW(requiredSize, buffer); length == 0 || length >= requiredSize) {
        delete[] buffer;
        return false;
    }
    *pDirectory = buffer;
    return true;
}

int isInProgramFiles(WCHAR appPath[MAX_PATH]) {
    TCHAR programFilesPath[MAX_PATH] = {};
    if (FAILED(SHGetFolderPath(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, programFilesPath)))
        return -1;
    if (appPath[0] == L'\0')
        GetModuleFileName(nullptr, appPath, MAX_PATH);
    return _tcsnicmp(appPath, programFilesPath, _tcslen(programFilesPath)) == 0 ? TRUE : FALSE;
}

bool utils::doesAutoStart() {
    WCHAR appPath[MAX_PATH];
    return fileExists(createShortcutLinkPath(isInProgramFiles(appPath)).c_str());
}

void utils::toggleStartup() {
    WCHAR appPath[260];
    GetModuleFileName(nullptr, appPath, MAX_PATH);
    const int inProgramFiles = isInProgramFiles(appPath);
    if (inProgramFiles == -1) {
        messageBox(MSG_SHORTCUT_CREATION_FAILED, MB_ICONERROR | MB_OK);
        return;
    }
    const std::wstring shortcutPath = createShortcutLinkPath(inProgramFiles == TRUE);
    if (shortcutPath.empty()) {
        messageBox(MSG_SHORTCUT_CREATION_FAILED, MB_ICONERROR | MB_OK);
        return;
    }
    const wchar_t *shortcutPathC = shortcutPath.c_str();
    if (fileExists(shortcutPathC)) {
        if (messageBox(MSG_SHORTCUT_REMOVE_VERIFY, MB_ICONQUESTION | MB_YESNO) == 6)
            _wremove(shortcutPathC);
        return;
    }

    if (messageBox(MSG_SHORTCUT_ADD_VERIFY, MB_ICONQUESTION | MB_YESNO) == 7)
        return;

    if (std::wofstream shortcut((shortcutPath.data())); shortcut.is_open()) {
        CoInitialize(nullptr);

        IShellLinkW *psl;
        HRESULT result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl));
        if (SUCCEEDED(result))
        {
            LPWSTR pszDir;
            GetWorkingDirectory(&pszDir);

            psl->SetPath(appPath);
            psl->SetArguments(L"");
            psl->SetWorkingDirectory(pszDir);
            psl->SetDescription(VER_FILEDESCRIPTION_STR);

            IPersistFile *ppf;
            result = psl->QueryInterface(IID_PPV_ARGS(&ppf));
            if (SUCCEEDED(result))
            {
                result = ppf->Save(shortcutPathC, TRUE);
                ppf->Release();
            }
            psl->Release();
        }
        CoUninitialize();

        if (FAILED(result)) {
            messageBox(MSG_SHORTCUT_CREATION_FAILED, MB_ICONERROR | MB_OK);
            _wremove(shortcutPathC);
        }
    } else {
        messageBox(MSG_OUTPUT_STREAM_FAILED, MB_ICONERROR | MB_OK);
    }
}
#endif

RECT utils::rect(const int x, const int y, const int width, const int height) {
    return { x, y, x + width, y + height };
}

bool utils::mouseInRect(const RECT *rect, const int vKey) {
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(globals::hWnd, &pt);
    return PtInRect(rect, pt) && GetAsyncKeyState(vKey) & KF_UP;
}

bool utils::mouseInWindow(HWND window) {
    POINT cursorPos;
    RECT windowRect;
    GetCursorPos(&cursorPos);
    GetWindowRect(window, &windowRect);
    return PtInRect(&windowRect, cursorPos);
}

std::wstring utils::getFormattedTime() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    std::wostringstream oss;
    oss << std::setfill(L'0') << std::setw(2) << st.wHour << L":"
        << std::setfill(L'0') << std::setw(2) << st.wMinute << L":"
        << std::setfill(L'0') << std::setw(2) << st.wSecond << L"."
        << std::setfill(L'0') << std::setw(3) << st.wMilliseconds;
    return oss.str();
}

bool utils::processIniFileLine(const std::wstring &orgLine, std::wstring *prefix, std::wstring &key, std::wstring &value) {
    std::wstring line = orgLine;
    trim(line);

    if (line.empty())
        return false;

    if (line.front() == '[') {
        const size_t end = line.find(']');
        if (end == std::wstring_view::npos)
            return false;

        if (!prefix)
            return false;

        auto name = line.substr(1, end-1);
        trim(name);
        *prefix = std::wstring{name} + L".";
        return false;
    }

    if (line.front() == L';' || line.front() == L'#')
        return false;

    const size_t pos = line.find(L'=');
    if (pos == std::string::npos)
        return false;

    key = line.substr(0, pos);
    trim(key);

    if (key.empty())
        return false;

    value = line.substr(pos + 1);
    trim(value);
    return true;
}

#if IS_PORTABLE == 0
bool utils::updateLanguageFile() {
    if (config::languageCode == IDR_INI_LANG_CUSTOM)
        return true;
    std::map<int, INILine> internalMessages = {};
    loadInternalLanguageStringsIntoMap(internalMessages);

    std::wstringstream wss;
    loadLanguageFromName(config::languageShortName, wss);
    std::map<int, INILine> modifiedMessages = {};
    mapIniContent(modifiedMessages, wss);

    bool needsModification = false;
    for (const auto &ini: internalMessages | std::views::values) {
        if (ini.isComment)
            continue;
        const auto exists = std::ranges::any_of(modifiedMessages, [&ini](const auto &pair) {
            return !pair.second.isComment && pair.second.key == ini.key;
        });
        if (exists)
            continue;
        needsModification = true;
        break;
    }

    if (needsModification) {
        std::wofstream file(std::wstring(L"languages/language." + config::languageShortName + L".ini").c_str(), std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            messageBox(MSG_FAILED_TO_UPDATE_LANGUAGE_FILE, MB_ICONERROR | MB_OK);
            return false;
        }
        for (const auto &ini: internalMessages | std::views::values) {
            if (ini.isComment)
                file << ini.value;
            else {
                const auto modified = std::ranges::find_if(modifiedMessages, [&ini](const auto &pair) {
                    return pair.second.key == ini.key;
                });
                file << ini.key << "=" << (modified != std::ranges::end(modifiedMessages) ? modified->second.value : ini.value) << std::endl;
            }
        }
    }
    return needsModification;
}

bool utils::loadLanguageFromName(const std::wstring &shortName, std::wstringstream &wss) {
    if (FILE* f = _wfopen(std::wstring(L"languages/language." + shortName + L".ini").c_str(), L"rb")) {
        if (fseek(f, 0, SEEK_END) == 0) {
            if (const long size = ftell(f); size >= 0) {
                rewind(f);
                std::vector<char> buffer(static_cast<size_t>(size));
                if (fread(buffer.data(), 1, buffer.size(), f) == buffer.size()) {
                    // UTF-8 -> UTF16 (wchar_t)
                    const int wideLen = MultiByteToWideChar(
                        CP_UTF8, 0,
                        buffer.data(), static_cast<int>(buffer.size()),
                        nullptr, 0
                    );
                    if (wideLen > 0) {
                        std::wstring wContent(wideLen, L'\0');
                        MultiByteToWideChar(
                            CP_UTF8, 0,
                            buffer.data(), static_cast<int>(buffer.size()),
                            &wContent[0], wideLen
                        );
                        // Strip UTF16 BOM
                        if (!wContent.empty() && wContent[0] == 0xFEFF)
                            wContent.erase(0, 1);
                        wss.str(L"");
                        wss.clear();
                        wss << wContent;
                        fclose(f);
                        return true;
                    }
                }
            }
        }
        fclose(f);
    }

    config::languageShortName = L"en";
    config::languageCode = IDR_INI_LANG_EN;
    // Can't be bothered to load internal messages again
    messageBoxRT(L"Failed to open/read languages/language." + shortName + L".ini file, using default language instead.", MB_ICONERROR | MB_OK);
    return false;
}

void utils::exportLanguageFiles() {
    mkdir(L"languages");
    bool success = true;
    for (const std::unordered_map<std::wstring, int> languages = APP_DEFAULT_LANGUAGES; const auto &[language, languageCode]: languages) {
        const std::wstring fileName = L"languages/language." + language + L".ini";
        if (fileExists(fileName.c_str())) {
            if (std::ifstream file(fileName.c_str(), std::ios::binary | std::ios::ate); file.tellg() != 0)
                continue;
        }

        const auto hRes = FindResource(globals::hIns, MAKEINTRESOURCE(languageCode), L"INI");
        if (!hRes) {
            success = false;
            continue;
        }

        const auto hData = LoadResource(globals::hIns, hRes);
        if (!hData) {
            success = false;
            continue;
        }

        const int dataSize = static_cast<int>(SizeofResource(globals::hIns, hRes));
        const auto content = static_cast<const char*>(LockResource(hData));
        if (!content) {
            success = false;
            continue;
        }

        static constexpr unsigned char UTF8_BOM[] = { 0xEF, 0xBB, 0xBF };
        if (FILE* out = _wfopen(fileName.c_str(), L"wb"); !out) {
            success = false;
        } else {
            fwrite(UTF8_BOM, sizeof(UTF8_BOM), 1, out);
            fwrite(content, dataSize, 1, out);
            fclose(out);
        }
    }
    if (!success)
        messageBox(MSG_FAILED_TO_EXPORT_LANGUAGES, MB_ICONERROR | MB_OK);
}

bool utils::loadInternalLanguageStringsIntoMap(std::map<int, INILine> &map) {
    if (const auto hRes = FindResource(globals::hIns, MAKEINTRESOURCE(config::languageCode), L"INI")) {
        if (const HGLOBAL hData = LoadResource(globals::hIns, hRes)) {
            const int dataSize = static_cast<int>(SizeofResource(globals::hIns, hRes));
            const auto content = static_cast<const char*>(LockResource(hData));
            if (const int wCharsCount = MultiByteToWideChar(CP_UTF8, 0, content, dataSize, nullptr, 0); wCharsCount > 0) {
                std::wstring winiContent(wCharsCount, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, content, dataSize, &winiContent[0], wCharsCount);
                std::wstringstream input;
                input << winiContent;
                mapIniContent(map, input);
                return true;
            }
        }
    }
    return false;
}

void utils::mapIniContent(std::map<int, INILine> &map, std::wistream &stream) {
    std::wstring line;
    int i = 0;
    while (std::getline(stream, line)) {
        if (std::wstring key, value; !processIniFileLine(line, nullptr, key, value))
            map[i] = { L"", line, true };
        else
            map[i] = { key, value, false };
        i++;
    }
}
#endif

void utils::loadIfNeededAndGetCachedLanguageString(const unsigned int mType, std::wstring *string) {
    static std::mutex mutex;
    static std::unordered_map<unsigned int, std::wstring> cachedMessages;
    std::lock_guard lock(mutex); // Thread safety
    if (cachedMessages.empty() || mType == 0) {
        cachedMessages.clear(); // Clear cached messages
        std::wstringstream input;
#if IS_PORTABLE == 0
        if (config::languageCode == IDR_INI_LANG_CUSTOM)
            loadLanguageFromName(config::languageShortName, input);
#endif

        if (input.str().empty()) {
            if (const auto hRes = FindResource(globals::hIns, MAKEINTRESOURCE(config::languageCode), L"INI")) {
                if (const HGLOBAL hData = LoadResource(globals::hIns, hRes)) {
                    const int dataSize = static_cast<int>(SizeofResource(globals::hIns, hRes));
                    const auto content = static_cast<const char*>(LockResource(hData));
                    if (const int wCharsCount = MultiByteToWideChar(CP_UTF8, 0, content, dataSize, nullptr, 0); wCharsCount > 0) {
                        std::wstring winiContent(wCharsCount, L'\0');
                        MultiByteToWideChar(CP_UTF8, 0, content, dataSize, &winiContent[0], wCharsCount);
                        input << winiContent;
                    }
                }
            }
        }

        if (!input.str().empty()) {
            std::wstring line;
            while (std::getline(input, line)) {
                std::wstring key, value;
                if (!processIniFileLine(line, nullptr, key, value))
                    continue;
                const auto mId = std::ranges::find_if(language::messageTypeMap, [&key](const auto &pair) {
                    return pair.second == key;
                });
                if (mId != language::messageTypeMap.end()) {
                    cachedMessages[mId->first].clear();
                    cachedMessages[mId->first].reserve(value.size());
                    for (size_t i = 0; i < value.size(); ++i) {
                        if (value[i] == L'\\' && i + 1 < value.size() && value[i + 1] == L'n') {
                            cachedMessages[mId->first].push_back(L'\n');
                            i++;
                        } else
                            cachedMessages[mId->first].push_back(value[i]);
                    }
                }
            }
        }
    }
    if (string) {
        const auto it = cachedMessages.find(mType);
        *string = it != cachedMessages.end() ? it->second : L"<untranslated>";
    }
}

// Format raw string
std::wstring utils::formatLangString(const std::wstring& rStr, const std::vector<std::wstring>& values) {
    if (values.empty())
        return rStr;
    std::wstring result;
    size_t pos = 0;
    while (pos < rStr.size()) {
        const size_t openBrace = rStr.find(L'{', pos);
        if (openBrace == std::wstring::npos) {
            result.append(rStr, pos, std::wstring::npos);
            break;
        }
        result.append(rStr, pos, openBrace - pos);
        const size_t closeBrace = rStr.find(L'}', openBrace);
        if (closeBrace == std::wstring::npos) {
            result.append(rStr, openBrace, std::wstring::npos);
            break;
        }
        std::wstring indexStr = rStr.substr(openBrace + 1, closeBrace - openBrace - 1);
        bool isNumber = !indexStr.empty();
        for (const wchar_t ch : indexStr) {
            if (!iswdigit(ch)) {
                isNumber = false;
                break;
            }
        }
        if (isNumber) {
            if (const size_t index = std::stoul(indexStr); index < values.size()) {
                result.append(values[index]);
            } else {
                result.append(rStr, openBrace, closeBrace - openBrace + 1);
            }
        } else {
            result.append(rStr, openBrace, closeBrace - openBrace + 1);
        }
        pos = closeBrace + 1;
    }
    return result;
}

int utils::messageBox(const unsigned int mType, const unsigned int uType, const std::vector<std::wstring> &values) {
    return MessageBox(globals::hWnd, message(mType, values).c_str(), message(MSG_APPLICATION_NAME).c_str(), uType);
}

int utils::messageBox(const std::wstring &mText, const unsigned int uType) {
    return MessageBox(globals::hWnd, mText.c_str(), message(MSG_APPLICATION_NAME).c_str(), uType);
}

int utils::messageBoxRT(const std::wstring &mText, const unsigned int uType) {
    return MessageBox(globals::hWnd, mText.c_str(), PROJECT_NAME, uType);
}

std::wstring utils::message(const unsigned int mType, const std::vector<std::wstring> &values) {
    std::wstring s;
    loadIfNeededAndGetCachedLanguageString(mType, &s);
    s = formatLangString(s, values);
    return s;
}

std::wstring utils::message(const unsigned int mType) {
    return message(mType, {});
}

bool utils::isUserUsingDarkTheme() {
    auto buffer = std::vector<char>(4);
    auto cbData = static_cast<DWORD>(buffer.size() * sizeof(char));
    const auto res = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        buffer.data(),
        &cbData);

    if (res != ERROR_SUCCESS)
        return false; // Default to false

    return (buffer[3] << 24 | buffer[2] << 16 | buffer[1] << 8 | buffer[0]) == 0;
}

std::wstring utils::utf8ToWide(const std::string& string) {
    if (string.empty())
        return {};

    const int required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        string.data(),
        static_cast<int>(string.size()),
        nullptr, 0
    );

    if (required == 0)
        throw std::runtime_error("zero-size");

    std::wstring result(required, L'\0');
    int got = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        string.data(),
        static_cast<int>(string.size()),
        &result[0],
        required
    );

    if (got == 0)
        throw std::runtime_error("conversion");

    return result;
}

