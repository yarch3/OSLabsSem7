#include "data_collector.h"
#include <filesystem>
#include <numeric>
#include <random>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

DataCollector::DataCollector(const std::string& port, 
                             const std::string& rawLog,
                             const std::string& hourlyLog,
                             const std::string& dailyLog)
    : portName(port), running(false), testMode(false),
      logPath(rawLog), hourlyLogPath(hourlyLog), dailyLogPath(dailyLog) {}

DataCollector::~DataCollector() {
    stop();
}

bool DataCollector::initialize() {
    std::cout << "Initializing data collector on port: " << portName << "\n";
    
    // Если порт пустой или "test", используем тестовый режим
    if (portName.empty() || portName == "test" || portName == "TEST") {
        testMode = true;
        std::cout << "Using TEST mode (no real COM port)\n";
        return true;
    }
    
    #ifdef _WIN32
        // На Windows добавляем префикс для COM портов выше COM9
        std::string fullPortName = portName;
        if (portName.find("COM") == 0 && portName.length() > 4) {
            fullPortName = "\\\\.\\" + portName;
        }
        
        std::cout << "Trying to open port: " << fullPortName << "\n";
        
        hSerial = CreateFileA(
            fullPortName.c_str(),
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        if (hSerial == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            
            if (error == ERROR_FILE_NOT_FOUND) {
                std::cout << "Port " << portName << " not found. Switching to TEST mode.\n";
                testMode = true;
                return true;
            }
            
            std::cerr << "Error opening port " << portName 
                      << " on Windows. Error code: " << error << "\n";
            
            // Пробуем другие COM порты
            std::cout << "Trying alternative COM ports...\n";
            for (int i = 1; i <= 8; i++) {
                std::string altPort = "COM" + std::to_string(i);
                std::string altFullPort = "\\\\.\\" + altPort;
                
                hSerial = CreateFileA(
                    altFullPort.c_str(),
                    GENERIC_READ,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL
                );
                
                if (hSerial != INVALID_HANDLE_VALUE) {
                    portName = altPort;
                    std::cout << "Successfully opened alternative port: " << portName << "\n";
                    break;
                }
            }
            
            if (hSerial == INVALID_HANDLE_VALUE) {
                std::cout << "All COM ports failed. Switching to TEST mode.\n";
                testMode = true;
                return true;
            }
        }
        
        // Настройка параметров порта
        DCB dcbSerialParams = {0};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        
        if (!GetCommState(hSerial, &dcbSerialParams)) {
            std::cout << "Warning: Could not get port state. Using default settings.\n";
            // Не выходим с ошибкой, продолжаем в тестовом режиме
            testMode = true;
            return true;
        }
        
        dcbSerialParams.BaudRate = CBR_9600;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;
        dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;
        
        if (!SetCommState(hSerial, &dcbSerialParams)) {
            std::cout << "Warning: Could not set port parameters. Using TEST mode.\n";
            testMode = true;
            return true;
        }
        
        // Настройка таймаутов
        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        timeouts.WriteTotalTimeoutConstant = 50;
        timeouts.WriteTotalTimeoutMultiplier = 10;
        
        if (!SetCommTimeouts(hSerial, &timeouts)) {
            std::cout << "Warning: Could not set timeouts. Continuing anyway.\n";
        }
        
    #else
        // Linux/Mac код
        std::cout << "Trying to open port: " << portName << " (POSIX)\n";
        
        serialPort = open(portName.c_str(), O_RDONLY | O_NOCTTY);
        if (serialPort < 0) {
            std::cout << "Port " << portName << " not available on POSIX. Switching to TEST mode.\n";
            testMode = true;
            return true;
        }
        
        struct termios tty;
        if (tcgetattr(serialPort, &tty) != 0) {
            std::cout << "Warning: Could not get port attributes. Using TEST mode.\n";
            close(serialPort);
            testMode = true;
            return true;
        }
        
        cfsetospeed(&tty, B9600);
        cfsetispeed(&tty, B9600);
        
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;
        tty.c_cflag &= ~CRTSCTS;
        tty.c_cflag |= CREAD | CLOCAL;
        
        tty.c_lflag &= ~ICANON;
        tty.c_lflag &= ~ECHO;
        tty.c_lflag &= ~ECHOE;
        tty.c_lflag &= ~ECHONL;
        tty.c_lflag &= ~ISIG;
        
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);
        tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
        
        tty.c_oflag &= ~OPOST;
        tty.c_oflag &= ~ONLCR;
        
        tty.c_cc[VTIME] = 1;
        tty.c_cc[VMIN] = 0;
        
        if (tcsetattr(serialPort, TCSANOW, &tty) != 0) {
            std::cout << "Warning: Could not set port attributes. Using TEST mode.\n";
            close(serialPort);
            testMode = true;
            return true;
        }
    #endif
    
    std::cout << "Data collector initialized on " << portName << std::endl;
    if (testMode) {
        std::cout << "Running in TEST mode (generating simulated data)\n";
    }
    return true;
}

std::string DataCollector::timestampToString(const std::chrono::system_clock::time_point& tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string DataCollector::getCurrentDateString() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d");
    return ss.str();
}

std::string DataCollector::getCurrentHourString() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:00");
    return ss.str();
}

bool DataCollector::readTemperature(float& temperature) {
    if (testMode) {
        // Генерируем тестовые данные с реалистичными уличными температурами
        static std::random_device rd;
        static std::mt19937 gen(rd());
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm* localTime = std::localtime(&time);
        
        int hour = localTime->tm_hour;
        int month = localTime->tm_mon + 1;
        
        // Базовые температуры по месяцам (в градусах Цельсия)
        float baseTemp;
        float variation;
        
        if (month >= 12 || month <= 2) {  // Зима
            baseTemp = -5.0f;
            variation = 8.0f;
        } else if (month >= 3 && month <= 5) {  // Весна
            baseTemp = 10.0f;
            variation = 6.0f;
        } else if (month >= 6 && month <= 8) {  // Лето
            baseTemp = 22.0f;
            variation = 5.0f;
        } else {  // Осень
            baseTemp = 8.0f;
            variation = 7.0f;
        }
        
        // Суточные колебания (+5 днем, -5 ночью)
        float dailyOffset = 5.0f * sin(2.0f * 3.14159f * (hour - 6) / 24.0f);
        
        std::normal_distribution<float> dist(baseTemp + dailyOffset, variation);
        float temp = dist(gen);
        
        // Ограничиваем диапазон
        if (temp < -30.0f) temp = -30.0f;
        if (temp > 40.0f) temp = 40.0f;
        
        temperature = temp;
        
        // Имитируем задержку чтения
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return true;
    }
    
    // Реальный код чтения из порта
    char buffer[256];
    DWORD bytesRead = 0;
    
    #ifdef _WIN32
        if (!ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
            return false;
        }
    #else
        bytesRead = read(serialPort, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0) {
            return false;
        }
    #endif
    
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        try {
            temperature = std::stof(buffer);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    return false;
}

void DataCollector::rotateLogFile(const std::string& filename, size_t maxLines) {
    std::ifstream inFile(filename);
    if (!inFile) return;
    
    std::vector<std::string> lines;
    std::string line;
    
    while (std::getline(inFile, line)) {
        lines.push_back(line);
    }
    inFile.close();
    
    if (lines.size() > maxLines) {
        std::ofstream outFile(filename);
        size_t startIdx = lines.size() - maxLines;
        
        for (size_t i = startIdx; i < lines.size(); ++i) {
            outFile << lines[i] << '\n';
        }
    }
}

void DataCollector::cleanupOldHourlyData() {
    auto now = std::chrono::system_clock::now();
    auto oneMonthAgo = now - std::chrono::hours(24 * 30);
    
    std::ifstream inFile(hourlyLogPath);
    if (!inFile) return;
    
    std::vector<std::string> lines;
    std::string line;
    
    while (std::getline(inFile, line)) {
        try {
            std::tm tm = {};
            std::stringstream ss(line.substr(0, 19));
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            
            if (ss.fail()) continue;
            
            auto timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            if (timestamp >= oneMonthAgo) {
                lines.push_back(line);
            }
        } catch (...) {
            continue;
        }
    }
    inFile.close();
    
    std::ofstream outFile(hourlyLogPath);
    for (const auto& l : lines) {
        outFile << l << '\n';
    }
}

void DataCollector::cleanupOldDailyData() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&time);
    int currentYear = localTime->tm_year + 1900;    
    std::ifstream inFile(dailyLogPath);
    if (!inFile) return;
    
    std::vector<std::string> lines;
    std::string line;
    
    while (std::getline(inFile, line)) {
        try {
            int year = std::stoi(line.substr(0, 4));
            if (year == static_cast<int>(currentYear)) {
                lines.push_back(line);
            }
        } catch (...) {
            continue;
        }
    }
    inFile.close();
    
    std::ofstream outFile(dailyLogPath);
    for (const auto& l : lines) {
        outFile << l << '\n';
    }
}

void DataCollector::writeToLog(const TemperatureData& data) {
    std::ofstream file(logPath, std::ios::app);
    if (file) {
        file << timestampToString(data.timestamp) << "," 
             << std::fixed << std::setprecision(2) << data.temperature << "\n";
    }
    
    // Ротация: сохраняем только последние 24 часа данных (макс 1440 записей при 1 мин интервале)
    rotateLogFile(logPath, 1440);
}

void DataCollector::writeToHourlyLog(float avgTemp) {
    std::ofstream file(hourlyLogPath, std::ios::app);
    if (file) {
        file << getCurrentHourString() << ":00," 
             << std::fixed << std::setprecision(2) << avgTemp << "\n";
    }
    
    // Очистка старых данных (более месяца)
    cleanupOldHourlyData();
}

void DataCollector::writeToDailyLog(float avgTemp) {
    std::ofstream file(dailyLogPath, std::ios::app);
    if (file) {
        file << getCurrentDateString() << "," 
             << std::fixed << std::setprecision(2) << avgTemp << "\n";
    }
    
    // Очистка старых данных (не текущего года)
    cleanupOldDailyData();
}

float DataCollector::calculateHourlyAverage() {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (hourlyData.empty()) return 0.0f;
    
    float sum = 0.0f;
    for (const auto& data : hourlyData) {
        sum += data.temperature;
    }
    
    return sum / hourlyData.size();
}

float DataCollector::calculateDailyAverage() {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (dailyTemperatures.empty()) return 0.0f;
    
    float sum = 0.0f;
    for (const auto& temp : dailyTemperatures) {
        sum += temp;
    }
    
    return sum / dailyTemperatures.size();
}

void DataCollector::collectData() {
    std::cout << "Starting data collection (every minute)...\n";
    
    auto lastMeasurementTime = std::chrono::system_clock::now();
    
    while (running) {
        float temp;
        if (readTemperature(temp)) {
            auto now = std::chrono::system_clock::now();
            TemperatureData data(now, temp);
            
            {
                std::lock_guard<std::mutex> lock(dataMutex);
                hourlyData.push_back(data);
                writeToLog(data);
            }
            
            dataCV.notify_one();
            
            // Логируем в консоль для информации
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::cout << "[" << std::put_time(std::localtime(&t), "%H:%M:%S") 
                      << "] Temperature: " << std::fixed << std::setprecision(1) 
                      << temp << "°C\n";
            
            lastMeasurementTime = now;
        }
        
        // Ждем до следующей минуты
        auto now = std::chrono::system_clock::now();
        auto timeSinceLast = std::chrono::duration_cast<std::chrono::seconds>(now - lastMeasurementTime);
        
        if (timeSinceLast < std::chrono::seconds(60)) {
            auto sleepTime = std::chrono::seconds(60) - timeSinceLast;
            std::this_thread::sleep_for(sleepTime);
        }
    }
}

void DataCollector::processData() {
    auto lastHourCheck = std::chrono::system_clock::now();
    auto lastDayCheck = std::chrono::system_clock::now();
    
    // Получаем начальный день
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&time);
    int currentDay = localTime->tm_mday;
    int currentMonth = localTime->tm_mon;
    
    std::cout << "Data processor started. Current day: " << currentDay 
              << ", Month: " << (currentMonth + 1) << "\n";
    
    while (running) {
        now = std::chrono::system_clock::now();
        
        // Каждый час считаем среднюю температуру
        if (std::chrono::duration_cast<std::chrono::minutes>(now - lastHourCheck).count() >= 60) {
            float hourlyAvg = calculateHourlyAverage();
            if (hourlyAvg != 0.0f || !hourlyData.empty()) {
                writeToHourlyLog(hourlyAvg);
                
                {
                    std::lock_guard<std::mutex> lock(dataMutex);
                    dailyTemperatures.push_back(hourlyAvg);
                    hourlyData.clear();
                }
                
                std::cout << "[" << getCurrentHourString() 
                          << "] Hourly average: " << std::fixed << std::setprecision(1) 
                          << hourlyAvg << "°C\n";
            }
            
            lastHourCheck = now;
        }
        
        // Проверяем смену дня
        time = std::chrono::system_clock::to_time_t(now);
        localTime = std::localtime(&time);
        int day = localTime->tm_mday;
        
        if (day != currentDay) {
            float dailyAvg = calculateDailyAverage();
            if (dailyAvg != 0.0f || !dailyTemperatures.empty()) {
                writeToDailyLog(dailyAvg);
                
                {
                    std::lock_guard<std::mutex> lock(dataMutex);
                    dailyTemperatures.clear();
                }
                
                std::cout << "[" << getCurrentDateString() 
                          << "] Daily average: " << std::fixed << std::setprecision(1) 
                          << dailyAvg << "°C (Day changed)\n";
            }
            
            currentDay = day;
            lastDayCheck = now;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

void DataCollector::start() {
    running = true;
    collectorThread = std::thread(&DataCollector::collectData, this);
    processorThread = std::thread(&DataCollector::processData, this);
    
    std::cout << "Data collector started\n";
    std::cout << "Raw log: " << logPath << "\n";
    std::cout << "Hourly log: " << hourlyLogPath << "\n";
    std::cout << "Daily log: " << dailyLogPath << "\n";
    
    if (testMode) {
        std::cout << "Mode: TEST (simulated data)\n";
    } else {
        std::cout << "Mode: REAL (reading from port " << portName << ")\n";
    }
}

void DataCollector::stop() {
    running = false;
    
    if (collectorThread.joinable()) {
        collectorThread.join();
    }
    
    if (processorThread.joinable()) {
        processorThread.join();
    }
    
    if (!testMode) {
        #ifdef _WIN32
            if (hSerial != INVALID_HANDLE_VALUE) {
                CloseHandle(hSerial);
                hSerial = INVALID_HANDLE_VALUE;
            }
        #else
            if (serialPort >= 0) {
                close(serialPort);
                serialPort = -1;
            }
        #endif
    }
    
    std::cout << "Data collector stopped\n";
}