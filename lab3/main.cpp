#include "process/process_manager.h"

int main(int argument_count, char* argument_values[]) {
    ProcessManager manager;
    manager.run(argument_count, argument_values);
    return 0;
}