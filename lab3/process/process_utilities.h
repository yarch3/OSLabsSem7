#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
#   include <windows.h>
#else
#   include <unistd.h>
#   include <sys/wait.h>
#   include <signal.h>
#endif

// Платформо-зависимые типы
#ifdef _WIN32
using ProcessHandle = HANDLE;
using ProcessId = DWORD;
#else
using ProcessHandle = pid_t;
using ProcessId = pid_t;
#endif

using namespace std;

class ProcessUtilities {
public:
    static bool create_child_process(const string& program_path, const string& argument);
    
    static bool is_process_alive(ProcessHandle process_handle);
    
    static void terminate_process(ProcessHandle process_handle);
    
    static ProcessId get_current_process_id();
    
    static void cleanup_processes(vector<ProcessHandle>& processes);
};

#ifdef _WIN32

bool ProcessUtilities::create_child_process(const string& program_path, const string& argument) {
    STARTUPINFOA startup_info;
    PROCESS_INFORMATION process_info;
    
    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    ZeroMemory(&process_info, sizeof(process_info));
    
    string command_line = "\"" + program_path + "\" " + argument;
    
    if (CreateProcessA(NULL, command_line.data(), NULL, NULL, FALSE, 0, NULL, NULL,&startup_info, &process_info)) {
        CloseHandle(process_info.hThread);
        return true;
    }
    return false;
}

bool ProcessUtilities::is_process_alive(ProcessHandle process_handle) {
    DWORD exit_code;
    if (GetExitCodeProcess(process_handle, &exit_code)) {
        return exit_code == STILL_ACTIVE;
    }
    return false;
}

void ProcessUtilities::terminate_process(ProcessHandle process_handle) {
    TerminateProcess(process_handle, 0);
    CloseHandle(process_handle);
}

ProcessId ProcessUtilities::get_current_process_id() {
    return GetCurrentProcessId();
}

void ProcessUtilities::cleanup_processes(vector<ProcessHandle>& processes) {
    for (auto process : processes) {
        if (is_process_alive(process)) {
            terminate_process(process);
        }
    }
    processes.clear();
}

#else

bool ProcessUtilities::create_child_process(const string& program_path, const string& argument) {
    pid_t child_pid = fork();
    
    if (child_pid == 0) {
        // Дочерний процесс
        execl(program_path.c_str(), "program", argument.c_str(), nullptr);
        _exit(1);
    } else if (child_pid > 0) {
        return true;
    }
    return false;
}

bool ProcessUtilities::is_process_alive(ProcessHandle process_handle) {
    return kill(process_handle, 0) == 0;
}

void ProcessUtilities::terminate_process(ProcessHandle process_handle) {
    kill(process_handle, SIGTERM);
    waitpid(process_handle, nullptr, 0);
}

ProcessId ProcessUtilities::get_current_process_id() {
    return getpid();
}

void ProcessUtilities::cleanup_processes(vector<ProcessHandle>& processes) {
    for (auto process : processes) {
        if (is_process_alive(process)) {
            terminate_process(process);
        }
    }
    processes.clear();
}

#endif