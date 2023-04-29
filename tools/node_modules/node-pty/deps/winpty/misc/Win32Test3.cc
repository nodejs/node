/*
 * Creates a window station and starts a process under it.  The new process
 * also gets a new console.
 */

#include <windows.h>
#include <string.h>
#include <stdio.h>

int main()
{
    BOOL success;

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.bInheritHandle = TRUE;

    HWINSTA originalStation = GetProcessWindowStation();
    printf("originalStation == 0x%x\n", originalStation);
    HWINSTA station = CreateWindowStation(NULL,
                                          0,
                                          WINSTA_ALL_ACCESS,
                                          &sa);
    printf("station == 0x%x\n", station);
    if (!SetProcessWindowStation(station))
        printf("SetWindowStation failed!\n");
    HDESK desktop = CreateDesktop("Default", NULL, NULL,
                                  /*dwFlags=*/0, GENERIC_ALL,
                                  &sa);
    printf("desktop = 0x%x\n", desktop);

    char stationName[256];
    stationName[0] = '\0';
    success = GetUserObjectInformation(station, UOI_NAME, 
                                       stationName, sizeof(stationName),
                                       NULL);
    printf("stationName = [%s]\n", stationName);

    char startupDesktop[256];
    sprintf(startupDesktop, "%s\\Default", stationName);

    STARTUPINFO sui;
    PROCESS_INFORMATION pi;
    memset(&sui, 0, sizeof(sui));
    memset(&pi, 0, sizeof(pi));
    sui.cb = sizeof(STARTUPINFO);
    sui.lpDesktop = startupDesktop;

    // Start a cmd subprocess, and have it start its own cmd subprocess.
    // Both subprocesses will connect to the same non-interactive window
    // station.

    const char program[] = "c:\\windows\\system32\\cmd.exe";
    char cmdline[256];
    sprintf(cmdline, "%s /c cmd", program);
    success = CreateProcess(program,
                            cmdline,
                            NULL,
                            NULL,
                            /*bInheritHandles=*/FALSE,
                            /*dwCreationFlags=*/CREATE_NEW_CONSOLE,
                            NULL, NULL,
                            &sui,
                            &pi);

    printf("pid == %d\n", pi.dwProcessId);

    // This sleep is necessary.  We must give the child enough time to
    // connect to the specified window station.
    Sleep(5000);

    SetProcessWindowStation(originalStation);
    CloseWindowStation(station);
    CloseDesktop(desktop);
    Sleep(5000);

    return 0;
}
