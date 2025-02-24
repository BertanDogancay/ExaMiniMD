#include "monitor.h"
#include <cassert>

PowerMonitor::PowerMonitor() {
    PWR_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_APP, "ExaMiniMD", &cntxt);
    PWR_CntxtGetEntryPoint(cntxt, &self);
    startTime = std::chrono::high_resolution_clock::now();
}

PowerMonitor::~PowerMonitor() {
    report();
    PWR_CntxtDestroy(cntxt);
}

void PowerMonitor::logData(const std::string &label, bool debugMode) {
    LogEntry logEntry;

    logEntry.isStat = false;
    logEntry.label = label;

    PWR_ObjAttrGetValue(self, PWR_ATTR_POWER, &logEntry.values.power, &ts);
    PWR_ObjAttrGetValue(self, PWR_ATTR_ENERGY, &logEntry.values.energy, &ts);
    PWR_ObjAttrGetValue(self, PWR_ATTR_FREQ, &logEntry.values.frequency, &ts);
    PWR_ObjAttrGetValue(self, PWR_ATTR_VOLTAGE, &logEntry.values.voltage, &ts);

    auto timestamp = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = timestamp - startTime;
    logEntry.time = duration.count();

    logEntries.push_back(logEntry);

    if (debugMode) {
        printf("[INSTANT  ] %s - Power: %lf (W) Energy: %lf (J) Frequency: %lf (Hz) Voltage: %lf (V) Time: %lf (s)\n", label.c_str(), logEntry.values.power, 
            logEntry.values.energy, logEntry.values.frequency, logEntry.values.voltage, logEntry.time);
    }
}

void PowerMonitor::startStatTracking(const std::string &label) {
    PWR_Grp grp;
    PWR_Stat stat;

    PWR_CntxtGetGrpByType(cntxt, PWR_OBJ_CORE, &grp);
    PWR_GrpGetObjByIndx(grp, 0, &self);
    PWR_ObjCreateStat(self, PWR_ATTR_ENERGY, PWR_ATTR_STAT_AVG, &stat);
    PWR_StatStart(stat);

    statStack.push_back(stat);
    statLabels.push_back(label);

    // printf("# Started power statistics tracking for %s...\n", label.c_str());
}

void PowerMonitor::stopStatTracking(bool debugMode) {
    if (statStack.empty()) {
        printf("[WARN     ] No active stat tracking session to stop\n");
        return;
    }

    PWR_Stat stat = statStack.back();
    std::string label = statLabels.back();
    statStack.pop_back();
    statLabels.pop_back();

    PWR_StatStop(stat);

    PWR_TimePeriod statTimes;
    statTimes.start = statTimes.stop = PWR_TIME_UNINIT;

    double avg_energy, avg_power;
    PWR_StatGetValue(stat, &avg_power, &statTimes);
    PWR_StatGetValue(stat, &avg_energy, &statTimes);
    PWR_StatDestroy(stat);

    auto timestamp = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = timestamp - startTime;

    LogEntry logEntry;
    logEntry.label = label;
    logEntry.isStat = true;
    logEntry.time = duration.count();
    logEntry.values.energy = avg_energy;
    logEntry.values.power = avg_power;

    logEntries.push_back(logEntry);

    if (debugMode) {
        printf("\n# Power Statistics Summary for %s:\n", label.c_str());
        printf("[STAT     ] Average Energy Consumption: %lf Joules\n", avg_energy);
        printf("Measurement Start: %lf s, Stop: %lf s\n", 
            (double)statTimes.start / 1e9, (double)statTimes.stop / 1e9);
    }
}

void PowerMonitor::report() {
    printf("\n# Energy Usage Log:\n");
    for (const auto& entry : logEntries) {
        if (entry.isStat) {
            printf("[STAT     ] %s - Avg Power: %lf (W) Avg Energy: %lf (J) Time: %lf (s)\n", 
                   entry.label.c_str(), entry.values.power, entry.values.energy, entry.time);
        } else {
            printf("[INSTANT  ] %s - Power: %lf (W) Energy: %lf (J) Frequency: %lf (Hz) Voltage: %lf (V) Time: %lf (s)\n", 
                   entry.label.c_str(), entry.values.power, entry.values.energy, entry.values.frequency, entry.values.voltage, entry.time);
        }
    }
}