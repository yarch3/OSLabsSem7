if(WIN32)
    set(MINGW_PATH "C:/mingw64/bin")
    set(CMAKE_C_COMPILER "${MINGW_PATH}/gcc.exe")
    set(CMAKE_CXX_COMPILER "${MINGW_PATH}/g++.exe")
endif()

cmake_minimum_required(VERSION 3.28)
project(lab CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)

add_library(backgroundProcessHandlerLib source/backgroundProcessHandlerLib.cpp)
target_include_directories(backgroundProcessHandlerLib PUBLIC include)

if(WIN32)
    target_compile_definitions(backgroundProcessHandlerLib PRIVATE _WIN32)
endif()

add_executable(lab2 main.cpp)
target_link_libraries(lab2 backgroundProcessHandlerLib)