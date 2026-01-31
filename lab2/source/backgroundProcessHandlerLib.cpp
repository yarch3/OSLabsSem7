#include "../header/backgroundProcessHandlerLib.h"
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
    #include <tchar.h>
#else
    #include <sys/wait.h>
    #include <errno.h>
#endif

int runBackgroundProcess(const char* command, ProcessHandler* handler) {
#ifdef _WIN32
    STARTUPINFO startInfo;
    ZeroMemory(&startInfo, sizeof(startInfo));
    startInfo.cb = sizeof(startInfo);
    ZeroMemory(&handler->procInfo, sizeof(handler->procInfo));

    if (!CreateProcess(
        NULL, 
        (LPSTR)command, 
        NULL, 
        NULL, 
        FALSE, 
        0, 
        NULL, 
        NULL, 
        &startInfo, 
        &handler->procInfo)
    ) {
        return -1;
    }
    handler->isFinished = false;
    return 0;
#else
    handler->procId = fork();
    if (handler->procId == 0) {
        execl("/bin/sh", "/bin/sh", "-c", command, (char *)NULL);
        _exit(127);
    } else if (handler->procId > 0) {
        handler->isFinished = false;
        return 0;
    } else {
        return -1;
    }
#endif
}

int waitProcessEnd(ProcessHandler* handler) {
#ifdef _WIN32
    DWORD result = WaitForSingleObject(handler->procInfo.hProcess, INFINITE);
    if (result == WAIT_OBJECT_0) {
        GetExitCodeProcess(handler->procInfo.hProcess, (LPDWORD)&handler->exitCode);
        CloseHandle(handler->procInfo.hProcess);
        CloseHandle(handler->procInfo.hThread);
        handler->isFinished = true;
        return handler->exitCode;
    }
    return -1;
#else
    int status;
    pid_t result = waitpid(handler->procId, &status, 0);
    if (result == handler->procId) {
        if (WIFEXITED(status)) {
            handler->exitCode = WEXITSTATUS(status);
        } else {
            handler->exitCode = -1;
        }
        handler->isFinished = true;
        return handler->exitCode;
    }
    return -1;
#endif
}