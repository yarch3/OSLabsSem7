#include "data_collector.h"
#include <iostream>
#include <string>
#include <csignal>

std::atomic<bool> running(true);

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal received. Stopping...\n";
    running = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Используем тестовый режим по умолчанию
    std::string port = "test";  // "test" режим вместо реального COM порта
    
    if (argc > 1) {
        port = argv[1];
    }
    
    std::cout << "Temperature Data Collector\n";
    std::cout << "==========================\n";
    std::cout << "Port: " << port << "\n\n";
    
    DataCollector collector(port, 
                           "temperature_raw.log",
                           "temperature_hourly.log",
                           "temperature_daily.log");
    
    if (!collector.initialize()) {
        std::cerr << "Failed to initialize data collector!\n";
        return 1;
    }
    
    collector.start();
    
    std::cout << "\nSystem is running. Press Ctrl+C to stop.\n";
    std::cout << "Log files:\n";
    std::cout << "  - temperature_raw.log (last 24 hours)\n";
    std::cout << "  - temperature_hourly.log (last month)\n";
    std::cout << "  - temperature_daily.log (current year)\n\n";
    
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "\nStopping system...\n";
    collector.stop();
    
    std::cout << "System stopped successfully.\n";
    return 0;
}