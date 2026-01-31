#pragma once

#include <string>

#ifdef _WIN32
#   include <windows.h>
#else
#   include <sys/types.h>
#   include <unistd.h>
#endif

#include "sharedMemory.h"

// const names
constexpr const char* SHARED_MEMORY_NAME = "global_counter";
constexpr const char* LOG_MUTEX_NAME = "log_mutex";
constexpr const char* LOG_FILE_NAME = "program.log";

struct SharedData {
    int counter = 0;
    bool master_exists = false;
#ifdef _WIN32
    DWORD master_process_id = 0;
#else
    pid_t master_process_id = 0;
#endif
};