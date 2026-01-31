cmake_minimum_required(VERSION 3.10)
project(TemperatureMonitor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Настройка для Windows
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# Основная программа
add_executable(temperature_monitor
    main.cpp
    thermometer_simulator.h
    data_collector.cpp 
)

# Для Windows нужны дополнительные библиотеки
if(WIN32)
    target_link_libraries(temperature_monitor Setupapi)
endif()

