#ifndef PWR_API_H
#define PWR_API_H

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <chrono>

#include <pwr.h>
#include "errcode.h"

#define CPU_FREQ_PATH "/sys/devices/system/cpu/"
#define MAX_CPUS 128

enum PwrAPIType_t {
    PWRAPI_TYPE_POWER,
    PWRAPI_TYPE_ENERGY,
    PWRAPI_TYPE_FREQUENCY,
    PWRAPI_TYPE_VOLTAGE,
    PWRAPI_NUM_TYPES
};

struct LogEntry {
    std::string label;
    struct {
        double power = 0.0;
        double energy = 0.0;
        double frequency = 0.0;
        double voltage = 0.0;
    } values;
    double time;
    bool isStat;
};

class PwrAPI {
public:
    // Constructor
    PwrAPI();

    // Deconstructor
    ~PwrAPI();

    // Get instantaneous power/energy value
    PwrErrCode getPowerAttr(const std::string &label);

    // Start state-based power tracking
    PwrErrCode startStatTracking(const std::string &label);

    // Stop state-based power tracking
    PwrErrCode stopStatTracking();

    // Set power attr
    PwrErrCode setPowerAttr(PwrAPIType_t type, double val, int *cpuList = nullptr, int cpuCount = 0);

    // Export logs to a file
    PwrErrCode exportLogs(const std::string &fileName = "power_monitor.log");

    // Clear all the stored logs
    void clearLogs();

    // Print the monitoring report
    void report();

    // Convert PWRAPI attr type to tring
    std::string typeToString(PwrAPIType_t type);

private:
    PWR_Cntxt cntxt;
    PWR_Obj self;
    PWR_Time ts;
    std::vector<PWR_Stat> statStack;
    std::vector<std::string> statLabels;
    std::vector<LogEntry> logEntries;
    std::chrono::high_resolution_clock::time_point startTime, endTime;

    char cpuMask[MAX_CPUS] = {0};

    bool debugMode;
    bool exportData;
    bool getEnvBool(const std::string &envVar, bool defaultVal = 0);

    // Helper function for cpu mask
    void selectCpus(int* cpuList, int count);

    // Helper function to write to CPU sysfs
    PwrErrCode writeToCpuSysfs(const char *filename, const char *val);

    // Read CPU frequency (avg all cores)
    PwrErrCode getAvgCpuFreq(double *avgFreq);

    // Set CPU frequency (all cores)
    // freq == 0 to reset
    PwrErrCode setCpuFreq(double freq, int *cpuList, int cpuCount);
};

#endif // PWR_API_H