#include <iostream>
#include "header/backgroundProcessHandlerLib.h"

using namespace std;


int main(int argc, char* argv[])
 {
    ProcessHandler handler;
    const char* command;
    
    if (argc == 1)
     {
        command = "sleep 2";

    #ifdef _WIN32
        command = "timeout 2";
    #endif
    }
    else 
    {
        string commandBuffer = argv[1];
        for (int i = 2; i < argc; i++)
         {
            commandBuffer += " ";
            commandBuffer += argv[i];
        }

        command = commandBuffer.c_str();
    }

    cout << "Start process:" << command << endl;

    if (runBackgroundProcess(command, &handler) != 0) 
    {
           cerr << "Failed to run process" << endl;
        return 1;
    }

    cout << "Waiting for finishing process" << endl;

    int exitCode = waitProcessEnd(&handler);
    cout << "Process has finished with exit code: " << exitCode << endl;

    return 0;
}