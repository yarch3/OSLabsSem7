#pragma once

#ifdef _WIN32
    #include <windows.h>
#else
    #include <termios.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <dirent.h>
#endif

#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <memory>
#include <condition_variable>

using namespace std;
using namespace chrono;

struct TemperatureData {
    system_clock::time_point timestamp;
    float temperature;
    
    TemperatureData(system_clock::time_point ts, float temp)
        : timestamp(ts), temperature(temp) {}
};

class DataCollector {
private:
    string portName;
    atomic<bool> testMode;
    atomic<bool> running;
    thread collectorThread;
    thread processorThread;
    
    #ifdef _WIN32
        HANDLE hSerial;
    #else
        int serialPort;
    #endif
    
    vector<TemperatureData> hourlyData;
    vector<float> dailyTemperatures;
    mutex dataMutex;
    condition_variable dataCV;
    
    // log paths
    string logPath;
    string hourlyLogPath;
    string dailyLogPath;
    
    // file utilities
    void rotateLogFile(const string& filename, size_t maxLines);
    void cleanupOldHourlyData();
    void cleanupOldDailyData();
    
    bool readTemperature(float& temperature);
    
    void collectData();
    void processData();
    
    // avg
    float calculateHourlyAverage();
    float calculateDailyAverage();
    
    void writeToLog(const TemperatureData& data);
    void writeToHourlyLog(float avgTemp);
    void writeToDailyLog(float avgTemp);
    
    string timestampToString(const system_clock::time_point& tp);
    string getCurrentDateString();
    string getCurrentHourString();
    
public:
    DataCollector(const string& port, 
                  const string& rawLog = "temperature.log",
                  const string& hourlyLog = "temperature_hourly.log",
                  const string& dailyLog = "temperature_daily.log");
    
    ~DataCollector();
    
    bool initialize();
    void start();
    void stop();
};