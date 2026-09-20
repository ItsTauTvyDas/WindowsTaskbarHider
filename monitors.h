#ifndef MONITORS_H
#define MONITORS_H

#include <atomic>
#include <mutex>
#include <windows.h>

class monitors {
public:
    struct MonitorRect {
        HMONITOR hMonitor;
        RECT rect;
    };

    static std::atomic<int> monitorCount;
    static std::mutex monitorsMutex;

    static HMONITOR monitor(int i);
    static void indexMonitors();
private:
    static HMONITOR indexedMonitorsUnsafe[64];
};

#endif //MONITORS_H
