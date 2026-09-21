#ifndef WINDOWSTASKBARHIDER_CONFIG_UI_H
#define WINDOWSTASKBARHIDER_CONFIG_UI_H

#include <memory>
#include <windows.h>

#include "win_draw.h"

#define CONFIG_UI_WINDOW_MIN_WIDTH         500
#define CONFIG_UI_WINDOW_MIN_HEIGHT        400
#define CONFIG_UI_SETTINGS_DRAW_Y          55
#define CONFIG_UI_SETTINGS_DRAW_X          5
#define CONFIG_UI_SETTINGS_DRAW_ROW_HEIGHT 20

class config_ui {
public:
    static bool initOk;

    static void init(HICON icon);
    static void open();
    static void close();
    static void save();

    enum class SettingType {
        separator,
        checkbox,
        number,
        select,
    };
    
    struct SettingBase {
        SettingType type;
        std::wstring label;
        std::wstring description;
        std::wstring iniKey;
        bool enabled = true;
        std::function<void()> onChange = nullptr;
        explicit SettingBase(const SettingType type, std::wstring label = L"", std::wstring iniKey = L"")
                : type(type),
                label(std::move(label)),
                iniKey(std::move(iniKey)) {}
        virtual ~SettingBase() = default;
    };

    struct SettingSeparator : SettingBase {
        explicit SettingSeparator(std::wstring title = L"")
                : SettingBase(
                    SettingType::separator,
                    std::move(title)
                ) {}
    };

    struct SettingCheckbox : SettingBase {
        std::atomic<bool> &value;

        SettingCheckbox(std::wstring label, std::wstring iniKey, std::atomic<bool> &value)
                : SettingBase(
                    SettingType::checkbox,
                    std::move(label),
                    std::move(iniKey)
                ), value(value) {}
    };

    struct SettingNumber : SettingBase {
        std::atomic<int> &value;
        int min;
        int max;

        SettingNumber(std::wstring label, std::wstring iniKey, std::atomic<int> &value, const int min, const int max)
                : SettingBase(
                    SettingType::number,
                    std::move(label),
                    std::move(iniKey)
                ), value(value), min(min), max(max) {}
    };

    struct setting_select : SettingBase {
        struct option {
            std::wstring label;
            int value;
        };

        std::atomic<int> &value;
        std::vector<option> options;

        setting_select(std::wstring label, std::wstring iniKey, std::atomic<int> &value, std::vector<option> options)
                : SettingBase(
                    SettingType::select,
                    std::move(label),
                    std::move(iniKey)
                ), value(value), options(std::move(options)) {}
    };

    using SettingList = std::vector<std::unique_ptr<SettingBase>>;
private:
    static win_draw::scrollable_content scrollable;
    static HWND windowHandle;
    static int winClientW, winClientH;
    static int getContentHeight();
    static int getContentWidth();
    static void redrawWindow();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static SettingList settings;
    template<typename T, typename... Args>
    static void addSetting(Args &&...args) {
        settings.push_back(std::make_unique<T>(std::forward<Args>(args)...));
    }
    static void buildSettings();
};

#endif //WINDOWSTASKBARHIDER_CONFIG_UI_H
