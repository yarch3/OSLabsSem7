#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <memory>
#include <iomanip>
#include <ctime>

#include "../shared/sharedData.h"
#include "process_utilities.h"
#include <algorithm>

using namespace std;
using namespace cplib;

class ProcessManager {
private:
    atomic<bool> should_continue_running;
    atomic<bool> is_current_master;
    
    thread counter_timer_thread;
    thread one_second_logger_thread;
    thread three_seconds_manager_thread;
    
    SharedMemory<SharedData> shared_memory;
    
    vector<ProcessHandle> child_processes;
    ProcessId current_process_id;

public:
    ProcessManager() 
        : should_continue_running(true)
        , is_current_master(false)
        , shared_memory(SHARED_MEMORY_NAME, true)
        , current_process_id(ProcessUtilities::get_current_process_id())
    {
        determine_master_status();
        
        if (is_current_master && shared_memory.IsValid()) {
            shared_memory.Lock();
            SharedData* shared_data = shared_memory.Data();
            shared_data->counter = 0;
            shared_memory.Unlock();
        }
    }

    ~ProcessManager() {
        should_continue_running = false;
        
        if (counter_timer_thread.joinable()) counter_timer_thread.join();
        if (one_second_logger_thread.joinable()) one_second_logger_thread.join();
        if (three_seconds_manager_thread.joinable()) three_seconds_manager_thread.join();
        
        if (is_current_master && shared_memory.IsValid()) {
            shared_memory.Lock();
            SharedData* shared_data = shared_memory.Data();
            if (shared_data->master_process_id == current_process_id) {
                shared_data->master_exists = false;
                shared_data->master_process_id = 0;
            }
            shared_memory.Unlock();
        }
        
        ProcessUtilities::cleanup_processes(child_processes);
    }

    string get_formatted_timestamp() {
        auto current_time = chrono::system_clock::now();
        auto time_as_time_t = chrono::system_clock::to_time_t(current_time);
        auto milliseconds = chrono::duration_cast<chrono::milliseconds>(
            current_time.time_since_epoch()) % 1000;
        
        char time_buffer[100];
        tm* local_time = localtime(&time_as_time_t);
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", local_time);
        
        char full_timestamp[110];
        snprintf(full_timestamp, sizeof(full_timestamp), "%s.%03d", 
                     time_buffer, static_cast<int>(milliseconds.count()));
        return string(full_timestamp);
    }

    void log_message(const string& message) {
        static SharedMemory<int> log_mutex(LOG_MUTEX_NAME, true);
        
        if (log_mutex.IsValid()) {
            log_mutex.Lock();
            ofstream log_file(LOG_FILE_NAME, ios::app);
            if (log_file.is_open()) {
                log_file << get_formatted_timestamp() << "; PID: " << current_process_id 
                        << "; " << message << endl;
            }
            log_file.close();
            log_mutex.Unlock();
        }
    }

    void determine_master_status() {
        if (shared_memory.IsValid()) {
            shared_memory.Lock();
            SharedData* shared_data = shared_memory.Data();
            
            if (!shared_data->master_exists) {
                shared_data->master_exists = true;
                shared_data->master_process_id = current_process_id;
                is_current_master = true;
            } else {
                is_current_master = (shared_data->master_process_id == current_process_id);
            }
            
            shared_memory.Unlock();
        }
    }

    void start_counter_increment_thread() {
        counter_timer_thread = thread([this]() {
            while (should_continue_running) {
                this_thread::sleep_for(chrono::milliseconds(300));
                if (shared_memory.IsValid()) {
                    shared_memory.Lock();
                    shared_memory.Data()->counter++;
                    shared_memory.Unlock();
                }
            }
        });
    }

    void start_periodic_logging_thread() {
        one_second_logger_thread = thread([this]() {
            while (should_continue_running) {
                this_thread::sleep_for(chrono::seconds(1));
                if (shared_memory.IsValid() && is_current_master) {
                    shared_memory.Lock();
                    int current_value = shared_memory.Data()->counter;
                    log_message("Counter value: " + to_string(current_value));
                    shared_memory.Unlock();
                }
            }
        });
    }

    void start_child_process_manager_thread() {
        three_seconds_manager_thread = thread([this]() {
            while (should_continue_running) {
                this_thread::sleep_for(chrono::seconds(3));
                
                if (!is_current_master) continue;
                
                // Удаляем завершенные процессы
                child_processes.erase(
                    remove_if(child_processes.begin(), child_processes.end(),
                        [](ProcessHandle handle) { return !ProcessUtilities::is_process_alive(handle); }),
                    child_processes.end()
                );
                
                if (!child_processes.empty()) continue;
                
                // Получаем путь к текущему исполняемому файлу
                #ifdef _WIN32
                    char exe_path[MAX_PATH];
                    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
                    string program_path = exe_path;
                #else
                    string program_path = "/proc/self/exe";
                #endif
                
                ProcessUtilities::create_child_process(program_path, "copy1");
                ProcessUtilities::create_child_process(program_path, "copy2");
            }
        });
    }

    void execute_copy1_operations() {
        log_message("Copy +10 started");
        if (shared_memory.IsValid()) {
            shared_memory.Lock();
            shared_memory.Data()->counter += 10;
            shared_memory.Unlock();
        }
        log_message("Copy +10 finished");
    }

    void execute_copy2_operations() {
        log_message("Copy x2 started");
        if (shared_memory.IsValid()) {
            shared_memory.Lock();
            SharedData* shared_data = shared_memory.Data();
            int original_value = shared_data->counter;
            shared_data->counter = original_value * 2;
            shared_memory.Unlock();
            
            this_thread::sleep_for(chrono::seconds(2));
            
            shared_memory.Lock();
            shared_data = shared_memory.Data();
            int current_value = shared_data->counter;
            shared_data->counter = current_value / 2;
            shared_memory.Unlock();
        }
        log_message("Copy x2 finished");
    }

    int get_counter_value() {
        if (shared_memory.IsValid()) {
            shared_memory.Lock();
            int value = shared_memory.Data()->counter;
            shared_memory.Unlock();
            return value;
        }
        return -1;
    }

    void set_counter_value(int new_value) {
        if (shared_memory.IsValid()) {
            shared_memory.Lock();
            shared_memory.Data()->counter = new_value;
            shared_memory.Unlock();
        }
    }

    void handle_user_commands() {
        string user_input;
        
        while (should_continue_running) {
            if (!getline(cin, user_input)) break;
            
            if (user_input == "g") {
                cout << "Current counter: " << get_counter_value() << endl;
            }
            else if (user_input.find("s ") == 0) {
                int new_value = stoi(user_input.substr(2));
                set_counter_value(new_value);
            }
            else if (!user_input.empty()) {
                cout << "Unknown command" << endl;
            }
        }
    }

    void run(int argument_count, char* argument_values[]) {
        if (argument_count > 1) {
            string mode = argument_values[1];
            if (mode == "copy1") {
                execute_copy1_operations();
                return;
            } else if (mode == "copy2") {
                execute_copy2_operations();
                return;
            }
        }
        
        log_message("Process started" + string(is_current_master ? " as MASTER" : " as SLAVE"));
        log_message("Initial counter: " + to_string(get_counter_value()));
        
        start_counter_increment_thread();
        start_periodic_logging_thread();
        
        if (is_current_master) {
            start_child_process_manager_thread();
        }
        
        handle_user_commands();
    }
};