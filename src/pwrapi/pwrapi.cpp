#include "pwrapi.h"
#include <cassert>

PwrAPI::PwrAPI() {
    PWR_CntxtInit(PWR_CNTXT_DEFAULT, PWR_ROLE_APP, "ExaMiniMD", &cntxt);
    PWR_CntxtGetEntryPoint(cntxt, &self);
    startTime = std::chrono::high_resolution_clock::now();

    debugMode = getEnvBool("PWRAPI_DEBUG");
    exportData = getEnvBool("PWRAPI_EXPORT_DATA");
}

PwrAPI::~PwrAPI() {
    if (exportData) exportLogs();
    report();

    PWR_CntxtDestroy(cntxt);
}

bool PwrAPI::getEnvBool(const std::string &envVar, bool defaultVal) {
    const char* val = std::getenv(envVar.c_str());
    if (val == nullptr) return defaultVal;
    
    if (debugMode)
        PWR_INFO("Environment variable %s set to %d by the user\n", envVar.c_str(), std::atoi(val));

    return (std::string(val) == "1");
}

PwrErrCode PwrAPI::getPowerAttr(const std::string &label) {
    LogEntry logEntry;

    logEntry.isStat = false;
    logEntry.label = label;

    //CHECK_PWR_CALL(PWR_ObjAttrGetValue(self, PWR_ATTR_POWER, &logEntry.values.power, &ts));
    CHECK_PWR_CALL(PWR_ObjAttrGetValue(self, PWR_ATTR_ENERGY, &logEntry.values.energy, &ts));
    //CHECK_PWR_CALL(PWR_ObjAttrGetValue(self, PWR_ATTR_FREQ, &logEntry.values.frequency, &ts));
    //CHECK_PWR_CALL(PWR_ObjAttrGetValue(self, PWR_ATTR_VOLTAGE, &logEntry.values.voltage, &ts));

    auto timestamp = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = timestamp - startTime;
    logEntry.time = duration.count();

    logEntries.push_back(logEntry);

    if (debugMode) {
        PWR_INFO("%s - Type: INST Power: %lf (W) Energy: %lf (J) Frequency: %lf (Hz) Voltage: %lf (V) Time: %lf (s)\n", label.c_str(), logEntry.values.power, 
            logEntry.values.energy, logEntry.values.frequency, logEntry.values.voltage, logEntry.time);
    }

    return PWR_SUCCESS;
}

PwrErrCode PwrAPI::startStatTracking(const std::string &label) {
    PWR_Grp grp;
    PWR_Stat stat;

    CHECK_PWR_CALL(PWR_CntxtGetGrpByType(cntxt, PWR_OBJ_CORE, &grp));
    CHECK_PWR_CALL(PWR_GrpGetObjByIndx(grp, 0, &self));
    CHECK_PWR_CALL(PWR_ObjCreateStat(self, PWR_ATTR_ENERGY, PWR_ATTR_STAT_AVG, &stat));
    CHECK_PWR_CALL(PWR_StatStart(stat));

    statStack.push_back(stat);
    statLabels.push_back(label);

    return PWR_SUCCESS;
}

PwrErrCode PwrAPI::stopStatTracking() {
    if (statStack.empty()) {
        PWR_ERROR("No active stat tracking session to stop\n");
        return PWR_FAIL;
    }

    PWR_Stat stat = statStack.back();
    std::string label = statLabels.back();
    statStack.pop_back();
    statLabels.pop_back();

    CHECK_PWR_CALL(PWR_StatStop(stat));

    PWR_TimePeriod statTimes;
    statTimes.start = statTimes.stop = PWR_TIME_UNINIT;

    double avg_energy, avg_power;
    CHECK_PWR_CALL(PWR_StatGetValue(stat, &avg_power, &statTimes));
    CHECK_PWR_CALL(PWR_StatGetValue(stat, &avg_energy, &statTimes));
    CHECK_PWR_CALL(PWR_StatDestroy(stat));

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
        PWR_INFO("%s - Type: STAT, Avg Power: %lf (W), Avg Energy: %lf (J), Time: %lf (s)\n", label.c_str(), 
            logEntry.values.power, logEntry.values.energy, logEntry.time);
    }

    return PWR_SUCCESS;
}

PwrErrCode PwrAPI::setPowerAttr(PwrAPIType_t type, double val) {
    switch (type) {
        case PWRAPI_TYPE_POWER:
            CHECK_PWR_CALL(PWR_ObjAttrSetValue(self, PWR_ATTR_POWER_LIMIT_MAX, &val));
            break;
        case PWRAPI_TYPE_FREQUENCY:
            CHECK_PWR_CALL(PWR_ObjAttrSetValue(self, PWR_ATTR_FREQ, &val));
            break;
        case PWRAPI_TYPE_VOLTAGE:
            CHECK_PWR_CALL(PWR_ObjAttrSetValue(self, PWR_ATTR_VOLTAGE, &val));
            break;
        default:
            PWR_ERROR("Unsupported power data type.\n");
            return PWR_FAIL;
    }

    if (debugMode) {
        PWR_INFO("%s set to %lf\n", typeToString(type).c_str(), val);
    }

    return PWR_SUCCESS;
}

PwrErrCode PwrAPI::exportLogs(const std::string &fileName) {
    FILE* logFile = fopen(fileName.c_str(), "w");
    if (!logFile) {
        PWR_ERROR("Could not open log file %s\n", fileName.c_str());
        return PWR_FAIL;
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
    PWR_INFO("Power montitoring logs saved to %s\n", fileName.c_str());

    return PWR_SUCCESS;
}

void PwrAPI::clearLogs() {
    if (debugMode)
        PWR_INFO("Cleared %ld stored power monitoring logs\n", logEntries.size());
    
    logEntries.clear();
}

void PwrAPI::report() {
    printf("\n# Energy Usage Log:\n");
    for (const auto& entry : logEntries) {
        if (entry.isStat) {
            printf("[INFO     ] %s - Avg Power: %lf (W) Avg Energy: %lf (J) Time: %lf (s)\n", 
                   entry.label.c_str(), entry.values.power, entry.values.energy, entry.time);
        } else {
            printf("[INFO     ] %s - Power: %lf (W) Energy: %lf (J) Frequency: %lf (Hz) Voltage: %lf (V) Time: %lf (s)\n", 
                   entry.label.c_str(), entry.values.power, entry.values.energy, entry.values.frequency, entry.values.voltage, entry.time);
        }
    }
}

std::string PwrAPI::typeToString(PwrAPIType_t type) {
    switch (type)
    {
        case PWRAPI_TYPE_POWER:
            return "Power (W)";
        case PWRAPI_TYPE_ENERGY:
            return "Energy (J)";
        case PWRAPI_TYPE_FREQUENCY:
            return "Frequency (Hz)";
        case PWRAPI_TYPE_VOLTAGE:
            return "Voltage (V)";
        default:
            return "Unknown Type";
    }
}