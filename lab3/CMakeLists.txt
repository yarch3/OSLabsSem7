cmake_minimum_required(VERSION 3.10)
project(SharedMemoryProcessManager)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES main.cpp)

# Создаем исполняемый файл
add_executable(lab3 ${SOURCES})

# Устанавливаем выходную папку
set_target_properties(lab3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output
)

if(WIN32)
    target_compile_definitions(lab3 PRIVATE _WIN32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(lab3 Threads::Threads)
    
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(lab3 rt)
    endif()
endif()