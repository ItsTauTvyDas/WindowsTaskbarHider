#ifndef MONITORS_H
#define MONITORS_H

#include <windows.h>

class monitors {
public:
    struct MonitorRect {
        HMONITOR hMonitor;
        RECT rect;
    };

    static HMONITOR indexedMonitors[64];
    static void indexMonitors();
};

#endif //MONITORS_H
