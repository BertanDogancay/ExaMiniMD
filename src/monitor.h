#ifndef MONITOR_H
#define MONITOR_H

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <chrono>

#include <pwr.h>

enum PowerDataType {
    POWER_DATA_TYPE_POWER,
    POWER_DATA_TYPE_ENERGY,
    POWER_DATA_TYPE_FREQUENCY,
    POWER_DATA_TYPE_VOLTAGE,
    POWER_NUM_DATA_TYPES
};

struct LogEntry {
    std::string label;
    struct {
        double power;
        double energy;
        double frequency;
        double voltage;
    } values;
    double time;
    bool isStat;
};

class PowerMonitor {
public:
    // Constructor
    PowerMonitor();

    // Deconstructor
    ~PowerMonitor();

    // Get instantaneous power/energy value
    void logData(const std::string &label);

    // Start state-based power tracking
    void startStatTracking(const std::string &label);

    // Stop state-based power tracking
    void stopStatTracking();

    // Print the monitoring report
    void report();

    // Export logs to a file
    void exportLogs(const std::string &fileName = "power_monitor.log");

private:
    PWR_Cntxt cntxt;
    PWR_Obj self;
    PWR_Time ts;
    std::vector<PWR_Stat> statStack;
    std::vector<std::string> statLabels;
    std::vector<LogEntry> logEntries;
    std::chrono::high_resolution_clock::time_point startTime, endTime;

    bool debugMode;
    bool exportData;
    bool getEnvBool(const std::string &envVar, bool defaultVal = 0);
};

#endif // MONITOR_H