#ifndef BACKGROUND_PROCESS_HANDLER
#define BACKGROUND_PROCESS_HANDLER

#ifdef _WIN32
    #include <windows.h>
#else
//designed for unix/posix
    #include <sys/wait.h>
    #include <unistd.h>
    #include <signal.h>
#endif

struct ProcessHandler {
#ifdef _WIN32
    PROCESS_INFORMATION procInfo;
#else
    pid_t procId;
#endif
    int exitCode;
    bool isFinished;
};

int runBackgroundProcess(const char* cmd, ProcessHandler* handler);
int waitProcessEnd(ProcessHandler* handler);

#endif