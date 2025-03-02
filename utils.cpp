#include "utils.h"

#include <cmath>
#include <format>
#include <shlobj.h>
#include <fstream>
#include <iomanip>
#include "windows.h"
#include "resources.h"
#include "config.h"
#include "taskbar.h"
#include <sstream>
#include <tlhelp32.h>
#include "globals.h"
#include <sys/stat.h>
#include <unordered_map>
#include <mutex>
#include <bits/ranges_algo.h>
#include "language.h"

bool utils::killProcessByName(const wchar_t* processName, DWORD currentPid) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    bool success = false;

    if (Process32First(hSnap, &pe))
        do {
            if (currentPid == pe.th32ProcessID)
                continue;
            if (wcschr(pe.szExeFile, *processName) == nullptr) {
                if (HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID)) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                    success = true;
                }
            }
        } while (Process32Next(hSnap, &pe));
    CloseHandle(hSnap);
    return success;
}

void utils::getProcessInfo(HWND hwnd, std::wstring &processExeName) {
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    processExeName = L"_unknown";

    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE)
        return;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hProcessSnap, &pe32))
        do {
            if (pe32.th32ProcessID == processId) {
                processExeName = pe32.szExeFile;
                break;
            }
        } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
}

bool utils::processArguments(const int argc, wchar_t* argv[]) {
    if (argc < 2) return true;
    std::wstring arg = argv[1];

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
    // Use the wide vformat + wide args
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
            messageBox(MSG_UNCAUGHT_EXCEPTION_RETRY_FAILED, MB_ICONERROR | MB_OK, {NTStatusMessageToText(GetLastError())});
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
    const HMODULE Hand = LoadLibrary(L"NTDLL.DLL");
    const DWORD length = FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE,
        Hand,
        NTStatusMessage,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPTSTR>(&lpMessageBuffer),
        0,
        nullptr);
    FreeLibrary(Hand);
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

bool utils::fileExists(const wchar_t *path) {
    struct _stat buffer = {};
    return _wstat(path, &buffer) == 0;
}

void utils::toUnicode(const LPCCH string, LPWSTR str) {
    MultiByteToWideChar(CP_ACP, 0, string, -1, str, MAX_PATH);
}

std::wstring utils::createShortcutLinkPath() {
    WCHAR startupPath[260];
    SHGetFolderPath(nullptr, CSIDL_STARTUP, nullptr, 0, startupPath);
    const std::wstring name = globals::exe.substr(0, globals::exe.find_last_of('.'));
    return std::wstring(std::wstring(startupPath) + L"\\" + name + L".lnk");
}

bool utils::doesAutoStart() {
    return fileExists(createShortcutLinkPath().c_str());
}

void utils::toggleStartup() {
    WCHAR appPath[260];
    GetModuleFileName(nullptr, appPath, MAX_PATH);
    const std::wstring shortcutPath = createShortcutLinkPath();
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

        IShellLinkW* psl;
        HRESULT result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl));
        if (SUCCEEDED(result))
        {
            IPersistFile* ppf;

            psl->SetPath(appPath);
            psl->SetArguments(L"");
            psl->SetDescription(VER_FILEDESCRIPTION_STR);

            result = psl->QueryInterface(IID_PPV_ARGS(&ppf));
            if (SUCCEEDED(result))
            {
                result = ppf->Save(shortcutPathC, TRUE);
                ppf->Release();
            }
            psl->Release();
        }
        CoUninitialize();

        if (!SUCCEEDED(result)) {
            messageBox(MSG_SHORTCUT_CREATION_FAILED, MB_ICONERROR | MB_OK);
            _wremove(shortcutPathC);
        }
    } else {
        messageBox(MSG_OUTPUT_STREAM_FAILED, MB_ICONERROR | MB_OK);
    }
}

RECT utils::rect(const int x, const int y, const int width, const int height) {
    return { x, y, x + width, y + height };
}

bool utils::mouseInRect(const RECT *rect, int vKey) {
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(globals::hWnd, &pt);
    return PtInRect(rect, pt) && GetAsyncKeyState(vKey) & 0x8000;
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

bool utils::processIniFileLine(const std::wstring &line, std::wstring *prefix, std::wstring &key, std::wstring &value) {
    if (line.empty())
        return false;

    if (line.rfind('[', 0) == 0) {
        if (!prefix)
            return false;
        *prefix = line.substr(1, line.size() - 2);
        std::ranges::replace(*prefix, ' ', '_');
        *prefix += '.';
        return false;
    }

    if (line.rfind(';', 0) == 0)
        return false;

    const size_t pos = line.find('=');
    if (pos == std::string::npos)
        return false;

    key = line.substr(0, pos);
    trim(key);
    value = line.substr(pos + 1);
    trim(value);
    return true;
}

// Load or get cached string
void utils::logcLangString(const unsigned int mType, std::wstring &string) {
    static std::unordered_map<unsigned int, std::wstring> messages;
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    if (messages.empty()) {
        HRSRC hRes = FindResource(globals::hIns, MAKEINTRESOURCE(IDR_INI_LANG_EN), IDI_RES_INI);
        const HGLOBAL hData = LoadResource(globals::hIns, hRes);
        const int dataSize = static_cast<int>(SizeofResource(globals::hIns, hRes));
        const auto content = static_cast<const char*>(LockResource(hData));
        if (const int wCharsCount = MultiByteToWideChar(CP_UTF8, 0, content, dataSize, nullptr, 0); wCharsCount > 0) {
            std::wstring winiContent(wCharsCount, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, content, dataSize, &winiContent[0], wCharsCount);
            std::wistringstream input(winiContent);
            std::wstring line;
            while (std::getline(input, line)) {
                std::wstring key, value;
                if (!processIniFileLine(line, nullptr, key, value))
                    continue;
                const auto mId = std::ranges::find_if(language::messageTypeMap, [&key](const auto &pair) {
                    return pair.second == key;
                });
                if (mId != language::messageTypeMap.end())
                    messages[mId->first] = value;
            }
        }
    }
    string = messages[mType];
    if (string.empty())
        string = L"<untranslated>";
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
            if (size_t index = std::stoul(indexStr); index < values.size()) {
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

std::wstring utils::message(const unsigned int mType, const std::vector<std::wstring> &values) {
    std::wstring s;
    logcLangString(mType, s);
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