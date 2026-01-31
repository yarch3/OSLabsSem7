#pragma once

#ifdef _WIN32
    #include <windows.h>
#else
    #include <termios.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cmath>

class DeviceSimulator {
private:
    std::string portName;
    bool running;
    std::thread simulatorThread;
    
    #ifdef _WIN32
        HANDLE hSerial;
    #else
        int serialPort;
    #endif

    struct ClimateParams {
        float baseAnnualTemp;    
        float annualAmplitude;   
        float dailyAmplitude;  
    };
    
    ClimateParams climate;
    //gen
    float generateTemperature() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm* localTime = std::localtime(&t);
        int month = localTime->tm_mon + 1;
        
        struct MonthlyTemp {
            int month;
            float avgTemp;
            float variation;
        };
        
        static const MonthlyTemp monthlyTemps[] = {
            {1, -5.0f, 8.0f},   // Январь: -5°C ± 8°C
            {2, -4.0f, 7.0f},   // Февраль: -4°C ± 7°C
            {3, 1.0f, 6.0f},    // Март: 1°C ± 6°C
            {4, 8.0f, 5.0f},    // Апрель: 8°C ± 5°C
            {5, 15.0f, 4.0f},   // Май: 15°C ± 4°C
            {6, 19.0f, 3.0f},   // Июнь: 19°C ± 3°C
            {7, 21.0f, 3.0f},   // Июль: 21°C ± 3°C
            {8, 20.0f, 3.0f},   // Август: 20°C ± 3°C
            {9, 14.0f, 4.0f},   // Сентябрь: 14°C ± 4°C
            {10, 8.0f, 5.0f},   // Октябрь: 8°C ± 5°C
            {11, 2.0f, 6.0f},   // Ноябрь: 2°C ± 6°C
            {12, -3.0f, 7.0f}   // Декабрь: -3°C ± 7°C
        };
        
        float avgTemp = 10.0f;
        float variation = 5.0f;
        
        for (const auto& mt : monthlyTemps) {
            if (mt.month == month) {
                avgTemp = mt.avgTemp;
                variation = mt.variation;
                break;
            }
        }
        
        std::normal_distribution<float> dist(avgTemp, variation);
        float temp = dist(gen);
        
        // constraints
        constexpr float MIN_TEMP = -30.0f;
        constexpr float MAX_TEMP = 40.0f;
        
        if (temp < MIN_TEMP) temp = MIN_TEMP;
        if (temp > MAX_TEMP) temp = MAX_TEMP;
        
        return temp;
    }

    void simulate() {
    
    while (running) {
        float temp = generateTemperature();
        std::string data = std::to_string(temp) + "\n";  // Просто число

        #ifdef _WIN32
            DWORD bytesWritten;
            WriteFile(hSerial, data.c_str(), data.length(), &bytesWritten, NULL);
        #else
            write(serialPort, data.c_str(), data.length());
        #endif
        
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
}

public:
    DeviceSimulator(const std::string& port, 
                    float baseAnnualTemp = 10.0f,    // Среднегодовая для умеренного климата
                    float annualAmplitude = 15.0f,   // Зима-лето разница ~30°C
                    float dailyAmplitude = 5.0f,     // День-ночь разница ~10°C
                    float latitudeEffect = 1.0f)     // Коэффициент для широты
        : portName(port), running(false) {
        
        climate.baseAnnualTemp = baseAnnualTemp;
        climate.annualAmplitude = annualAmplitude;
        climate.dailyAmplitude = dailyAmplitude;
    }
    
    ~DeviceSimulator() {
        stop();
    }
    
    
    bool initialize() {

        #ifdef _WIN32
            std::string fullPortName = portName;
            if (portName.find("COM") == 0 && portName.length() > 4) {
                fullPortName = "\\\\.\\" + portName;
            }
            
            hSerial = CreateFileA(
                fullPortName.c_str(),
                GENERIC_WRITE,
                0,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
            
            if (hSerial == INVALID_HANDLE_VALUE) {
                std::cerr << "Failed to open port " << portName << "\n";
                return false;
            }
            
            // Настройка порта...
            DCB dcbSerialParams = {0};
            dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
            dcbSerialParams.BaudRate = CBR_9600;
            dcbSerialParams.ByteSize = 8;
            dcbSerialParams.StopBits = ONESTOPBIT;
            dcbSerialParams.Parity = NOPARITY;
            dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;
            
            if (!SetCommState(hSerial, &dcbSerialParams)) {
                std::cerr << "Error setting port parameters\n";
                CloseHandle(hSerial);
                return false;
            }
            
        #else
            serialPort = open(portName.c_str(), O_WRONLY | O_NOCTTY);
            if (serialPort < 0) {
                std::cerr << "Failed to open port " << portName << "\n";
                return false;
            }
            
            struct termios tty;
            if (tcgetattr(serialPort, &tty) != 0) {
                std::cerr << "Error getting port attributes\n";
                close(serialPort);
                return false;
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
            
            tty.c_cc[VTIME] = 5;
            tty.c_cc[VMIN] = 0;
            
            if (tcsetattr(serialPort, TCSANOW, &tty) != 0) {
                std::cerr << "Error setting port attributes\n";
                close(serialPort);
                return false;
            }
        #endif
        
        std::cout << " Temperature Sensor Simulator initialized\n";
        std::cout << "Port: " << portName << "\n";
        return true;
    }
    
    void start() {
        running = true;
        simulatorThread = std::thread(&DeviceSimulator::simulate, this);
        std::cout << "Simulating temperature sensor...\n";
    }
    
    void stop() {
        running = false;
        if (simulatorThread.joinable()) {
            simulatorThread.join();
        }
        
        #ifdef _WIN32
            if (hSerial != INVALID_HANDLE_VALUE) {
                CloseHandle(hSerial);
            }
        #else
            if (serialPort >= 0) {
                close(serialPort);
            }
        #endif
        
        std::cout << "Sensor simulator stopped\n";
    }
    
    // Ручная отправка конкретной температуры
    bool sendTemperature(float temperatureCelsius) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << temperatureCelsius << "\n";
        std::string data = ss.str();
        
        #ifdef _WIN32
            DWORD bytesWritten;
            if (WriteFile(hSerial, data.c_str(), static_cast<DWORD>(data.length()), &bytesWritten, NULL)) {
                std::cout << ss.str() << '\n';
                return true;
            }
        #else
            if (write(serialPort, data.c_str(), data.length()) > 0) {
                std::cout << ss.str() << '\n';
                return true;
            }
        #endif
        
        return false;
    }
};