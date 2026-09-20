#include "monitors.h"

#include <algorithm>
#include <vector>

#include "taskbar.h"

HMONITOR monitors::indexedMonitorsUnsafe[];
std::atomic<int> monitors::monitorCount;
std::mutex monitors::monitorsMutex;

HMONITOR monitors::monitor(const int i) {
    std::lock_guard lock(monitorsMutex);
    if (i < monitorCount && i >= 0)
        return indexedMonitorsUnsafe[i];
    return nullptr;
}

void monitors::indexMonitors() {
    {
        std::lock_guard lock(monitorsMutex);
        memset(indexedMonitorsUnsafe, 0, sizeof(indexedMonitorsUnsafe));
    }
    
    std::vector<MonitorRect> monitors;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMonitor, HDC, LPRECT lpMonitorRect, LPARAM dwData) -> BOOL {
        reinterpret_cast<std::vector<MonitorRect>*>(dwData)->push_back({ hMonitor, *lpMonitorRect });
        return TRUE;
    }, reinterpret_cast<LPARAM>(&monitors));
    std::ranges::sort(monitors, [](const MonitorRect& a, const MonitorRect& b) {
        return a.rect.left < b.rect.left;
    });

    std::lock_guard lock(monitorsMutex);
    for(auto i = 0; i < monitors.size(); i++)
        indexedMonitorsUnsafe[i] = monitors[i].hMonitor;
    monitorCount = static_cast<int>(std::size(monitors));
}