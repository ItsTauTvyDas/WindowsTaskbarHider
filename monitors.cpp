#include "monitors.h"

#include <algorithm>
#include <vector>

#include "taskbar.h"

HMONITOR monitors::indexedMonitors[64];
int monitors::monitorCount;
std::mutex monitors::monitorsMutex;

void monitors::indexMonitors() {
    std::lock_guard lock(monitorsMutex);
    memset(indexedMonitors, 0, sizeof(indexedMonitors));
    std::vector<MonitorRect> monitors;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMonitor, HDC, LPRECT lpMonitorRect, LPARAM dwData) -> BOOL {
        reinterpret_cast<std::vector<MonitorRect>*>(dwData)->push_back({ hMonitor, *lpMonitorRect });
        return TRUE;
    }, reinterpret_cast<LPARAM>(&monitors));
    std::ranges::sort(monitors, [](const MonitorRect& a, const MonitorRect& b) {
        return a.rect.left < b.rect.left;
    });
    for(auto i = 0; i < monitors.size(); i++)
        indexedMonitors[i] = monitors[i].hMonitor;
    monitorCount = static_cast<int>(std::size(monitors));
}