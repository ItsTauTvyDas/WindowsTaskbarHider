#include "utils.h"

#include <shlobj.h>
#include <fstream>
#include <iostream>
#include "windows.h"
#include "resources.h"
#include "config.h"
#include "taskbar.h"
#include <sstream>
#include <tlhelp32.h>
#include "globals.h"
#include <sys/stat.h>
#include "stdcerr.h"

std::string utils::getProcessName(HWND hwnd) {
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return "Unknown";

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    std::string processName = "Unknown";
    if (Process32First(hProcessSnap, &pe32))
        do {
            if (pe32.th32ProcessID == processId) {
                processName = pe32.szExeFile;
                break;
            }
        } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return processName;
}

void utils::killProcessByName(const char* processName, DWORD currentPid) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnap, &pe))
        do {
            if (currentPid == pe.th32ProcessID)
                continue;
            if (_stricmp(pe.szExeFile, processName) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                }
            }
        } while (Process32Next(hSnap, &pe));
    CloseHandle(hSnap);
}

std::string utils::getProgramVersion() {
    char exePath[MAX_PATH];
    if (GetModuleFileName(nullptr, exePath, MAX_PATH) == 0)
        return "Failed to get executable path.";

    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSize(exePath, &handle);
    if (size == 0)
        return "Failed to get version info size.";

    std::vector<char> data(size);
    if (!GetFileVersionInfo(exePath, handle, size, data.data()))
        return "Failed to get version info.";

    VS_FIXEDFILEINFO* versionInfo = nullptr;
    UINT len = 0;
    if (!VerQueryValue(data.data(), "\\", reinterpret_cast<LPVOID *>(&versionInfo), &len))
        return "Failed to query version info.";

    const DWORD major = HIWORD(versionInfo->dwFileVersionMS);
    const DWORD minor = LOWORD(versionInfo->dwFileVersionMS);
    const DWORD build = HIWORD(versionInfo->dwFileVersionLS);
    const DWORD revision = LOWORD(versionInfo->dwFileVersionLS);

    char versionStr[50];
    snprintf(versionStr, sizeof(versionStr), "%d.%d.%d.%d", major, minor, build, revision);
    return versionStr;
}

bool utils::processArguments(const int argc, char* argv[]) {
    if (argc < 2) return true;
    attachConsoleWindow();
    std::string arg = argv[1];
    if (arg == "--help" || arg == "-h") {
        std::cout << "Usage: " << globals::exe << " [OPTIONS]" << std::endl
                  << "Options:" << std::endl
                  << "  -h,       --help                 Show this help message" << std::endl
                  << "  -nmb,     --no-msgbox            Disable message boxes" << std::endl
                  << "  -d,       --debug                Enable debug output" << std::endl
                  << "  -nc,      --no-config            Don't create/load config file" << std::endl
                  << "  -rtb,     --reset-taskbar        Reset taskbar visibility and exit" << std::endl
                  << "  -c:<key>, --config:<key> <value> Set values for configuration (doesn't save)" << std::endl
                  << "  -i,       --info                 Show the information about this program and exit" << std::endl;
        return false;
    }

    if (arg == "--info" || arg == "-i") {
        std::cout << "Executable Version: " << getProgramVersion() << std::endl
                  << "Author:             ItsTauTvyDas (https://itstatutvydas.me)" << std::endl
                  << "Github Repository:  https://github.com/ItsTauTvyDas/WindowsTaskbarHider" << std::endl;
        return false;
    }

    if (arg == "--reset-taskbar" || arg == "-rtb") {
        taskbar::resetTaskbar();
        return false;
    }

    for (int i = 1; i < argc; i++) {
        arg = argv[i];
        if (arg == "--debug" || arg == "-d") {
            config::debug = true;
        } else if (arg == "--no-config" || arg == "-nc") {
            globals::noConfigFile = true;
        } else if ((arg.rfind("-c:", 0) == 0 || arg.rfind("-config:", 0) == 0) && i + 1 < argc && arg.rfind(':', 0) + 1 < arg.size()) {
            config::processSingle(arg.substr(arg.rfind(':', 0)), argv[i + 1]);
        } else {
            std::cerr << "Invalid argument: " + arg << std::endl;
            return false;
        }
    }
    return true;
}

HICON utils::loadExeIcon(const LPCSTR pszExeFileName, const UINT nIconIndex) {
    return ExtractIconA(GetModuleHandle(nullptr), pszExeFileName, nIconIndex);
}

void utils::showExceptionMessageBox(const std::function<void(std::stringstream&)>& callback) {
    std::stringstream crashInfo;
    crashInfo << "The application has crashed!" << std::endl;
    crashInfo << std::endl;
    callback(crashInfo);
    MessageBoxA(globals::hWnd, crashInfo.str().c_str(), PROJECT_NAME, MB_ICONERROR | MB_OK);
}

LPSTR utils::NTStatusMessageToText(const DWORD NTStatusMessage)
{
    // https://web.archive.org/web/20150121053632/http://support.microsoft.com/kb/259693
    LPSTR message;
    const HMODULE Hand = LoadLibrary("NTDLL.DLL");
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_FROM_HMODULE,
        Hand,
        NTStatusMessage,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&message),
        sizeof(message),
        nullptr);
    FreeLibrary(Hand);
    return message;
}

BOOL CALLBACK EnumIconsProc([[maybe_unused]] HMODULE hModule, [[maybe_unused]] LPCTSTR lpszType, [[maybe_unused]] LPTSTR lpszName, const LONG_PTR lParam) {
    *reinterpret_cast<bool *>(lParam) = true;
    return FALSE;
}

std::string utils::joinString(const std::vector<std::string>& vec, const std::string& delimiter) {
    std::ostringstream result;
    for (size_t i = 0; i < vec.size(); ++i) {
        result << vec[i];
        if (i < vec.size() - 1)
            result << delimiter;
    }
    return result.str();
}

std::vector<std::string> utils::splitString(const std::string& str, const char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter))
        result.push_back(item);

    return result;
}

void utils::attachConsoleWindow() {
    if (!AttachConsole(ATTACH_PARENT_PROCESS) && AllocConsole()) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

        const int hCrt = _open_osfhandle(reinterpret_cast<intptr_t>(hConsole), 0x4000);
        const FILE* fp = _fdopen(hCrt, "w");
        *stdout = *fp;
        *stderr = *fp;

        setvbuf(stdout, nullptr, _IONBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);

        std::cout.clear();
        std::cerr.clear();

        static stdcerr _;

        SetConsoleTitleW(L"WindowsTaskbarHider (debugging)");

        std::cout << "Console successfully attached." << std::endl;
        Sleep(100);
        return;
    }
    MessageBoxA(globals::hWnd, "Console is already attached!", PROJECT_NAME, MB_ICONERROR | MB_OK);
}

bool utils::fileExists(const char *path) {
    struct stat buffer{};
    return stat(path, &buffer) == 0;
}

void utils::toUnicode(const LPCCH string, const LPWSTR str) {
    MultiByteToWideChar(CP_ACP, 0, string, -1, str, MAX_PATH);
}

void utils::clearConsole(const COORD startCoord) {
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO screen;
    DWORD written;

    GetConsoleScreenBufferInfo(console, &screen);
    FillConsoleOutputCharacterA(
        console, ' ', (screen.dwSize.X - startCoord.X) * (screen.dwSize.Y - startCoord.Y), startCoord, &written
    );
    SetConsoleCursorPosition(console, startCoord);
}

std::string utils::createShortcutLinkPath() {
    char startupPath[MAX_PATH];
    if (!SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_STARTUP, nullptr, 0, startupPath))) {
        MessageBoxA(globals::hWnd, "Failed to get startup folder location!", PROJECT_NAME, MB_ICONERROR | MB_OK);
        return nullptr;
    }
    std::string name = globals::exe;
    name = name.substr(0, name.find_last_of('.'));
    return std::string(std::string(startupPath) + "\\" + name + ".lnk");
}

bool utils::doesAutoStart() {
    return fileExists(createShortcutLinkPath().c_str());
}

void utils::toggleStartup() {
    char appPath[MAX_PATH];
    GetModuleFileName(nullptr, appPath, MAX_PATH);
    const std::string shortcutPath = createShortcutLinkPath();

    if (fileExists(shortcutPath.c_str())) {
        remove(shortcutPath.c_str());
        return;
    }

    if (std::ofstream shortcut(shortcutPath); shortcut.is_open()) {
        CoInitialize(nullptr);

        IShellLinkW* psl;
        HRESULT result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl));
        if (SUCCEEDED(result))
        {
            IPersistFile* ppf;

            WCHAR pszFile[MAX_PATH];
            toUnicode(appPath, pszFile);

            psl->SetPath(pszFile);
            psl->SetArguments(L"");
            psl->SetDescription(L"");

            result = psl->QueryInterface(IID_PPV_ARGS(&ppf));
            if (SUCCEEDED(result))
            {
                WCHAR pszFileName[MAX_PATH];
                toUnicode(shortcutPath.c_str(), pszFileName);
                result = ppf->Save(pszFileName, TRUE);
                ppf->Release();
            }
            psl->Release();
        }
        CoUninitialize();

        if (!SUCCEEDED(result)) {
            MessageBoxA(globals::hWnd, "Failed to create a shortcut at startup directory!", PROJECT_NAME, MB_ICONERROR | MB_OK);
            remove(shortcutPath.c_str());
        }
    }
}

void utils::toggleConsoleWindow(const PHANDLER_ROUTINE handler, const bool status) {
    if (status) {
        attachConsoleWindow();
        SetConsoleCtrlHandler(handler, TRUE);
        return;
    }
    SetConsoleCtrlHandler(handler, FALSE);
    HWND hwnd = GetConsoleWindow();
    FreeConsole();
    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);
    if (const HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, process_id)) {
        SendMessage(hwnd, WM_CLOSE, 0, 0L);
        CloseHandle(hProcess);
    }
    CloseWindow(hwnd);
}