cmake_minimum_required(VERSION 3.10)
project(BackgroundProcessApp)

# Установка стандарта C++ (используем C++11 для совместимости)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Создание исполняемого файла
add_executable(${PROJECT_NAME}
    main.cpp
    source/backgroundProcessHandlerLib.cpp
)

# Включаем директории с заголовками
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/header
)

# Настройки для Windows
if(WIN32)
    # Windows требует дополнительные настройки для CreateProcess
    add_definitions(-D_WIN32_WINNT=0x0600)  # Минимальная версия Windows Vista
else()
    # Настройки для Unix/Linux/MacOS
    add_definitions(-D_POSIX_C_SOURCE=200809L)
endif()