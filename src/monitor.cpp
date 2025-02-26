#include "monitor.h"
#include <cassert>

PowerMonitor::PowerMonitor() {
    PWR_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_APP, "ExaMiniMD", &cntxt);
    PWR_CntxtGetEntryPoint(cntxt, &self);
    startTime = std::chrono::high_resolution_clock::now();

    debugMode = getEnvBool("PWRAPI_DEBUG");
    exportData = getEnvBool("PWRAPI_EXPORT_DATA");
}

PowerMonitor::~PowerMonitor() {
    if (exportData) exportLogs();
    report();

    PWR_CntxtDestroy(cntxt);
}

bool PowerMonitor::getEnvBool(const std::string &envVar, bool defaultVal) {
    const char* val = std::getenv(envVar.c_str());
    if (val == nullptr) return defaultVal;
    
    if (debugMode)
        printf("[INFO     ] Environment variable %s set to %d by the user\n", envVar.c_str(), std::atoi(val));

    return (std::string(val) == "1");
}

void PowerMonitor::logData(const std::string &label) {
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
}

void PowerMonitor::stopStatTracking() {
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

void PowerMonitor::exportLogs(const std::string &fileName) {
    FILE* logFile = fopen(fileName.c_str(), "w");
    if (!logFile) {
        printf("[WARN     ] Could not open log file %s\n", fileName.c_str());
        return;
    }

    fprintf(logFile, "# Power Monitoring Log\n");
    fprintf(logFile, "#---------------------------------------------------------------------------------------------------------\n");
    fprintf(logFile, "# %-10s | %-20s | %-6s | %-10s | %-15s | %-15s | %-10s \n",
            "Time(s)", "Label", "Type", "Power(W)", "Energy(J)", "Frequency(Hz)", "Voltage(V)");
    fprintf(logFile, "#---------------------------------------------------------------------------------------------------------\n");

    for (const auto &entry : logEntries) {
        char buffer[256];
        if (entry.isStat) {
            sprintf(buffer, "  %-10.4f | %-20s | %-6s | %-10.2f | %-15.2f | %-15s | %-10s\n",
                entry.time, entry.label.c_str(), "STAT", entry.values.power, entry.values.energy, "-", "-");
        } else {
            sprintf(buffer, "  %-10.4f | %-20s | %-6s | %-10.2f | %-15.2f | %-15.2f | %-10.2f\n",
                entry.time, entry.label.c_str(), "INST", entry.values.power, entry.values.energy, entry.values.frequency, entry.values.voltage);
        }
        fprintf(logFile, "%s", buffer);
    }

    fclose(logFile);
    printf("[INFO     ] Power montitoring logs saved to %s\n", fileName.c_str());
}