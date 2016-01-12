//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#undef UNICODE
#undef _UNICODE

#include <windows.h>

#include <stdio.h>

#include "rl.h"


//#define TRACE


char *RLFEOpts = NULL;
BOOL FRLFE = FALSE;


static HANDLE HPipe = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION CS;
static BOOL FConnected = FALSE;


static void SendCommand(RLFE_COMMAND command, DWORD dataLen);


static int
SpawnRLFE(
    const char *options
)
{
    char cmd[1024];
    char *str = "Unexpected error";
    enum SD_STATUS {
        SD_NONE, SD_IN, SD_ERR, SD_OUT, SD_CREATE
    } status;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa;
    HANDLE NULStderr = 0, NULStdout = 0, NULStdin = 0;

    sprintf_s(cmd, "rlfe %s", options);
    if (strlen(cmd) > 1023) {
        printf("cmd buffer too small\n");
        return -2;
    }

    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    status = SD_IN;
    NULStdin = CreateFile("NUL", GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_ALWAYS, 0, INVALID_HANDLE_VALUE);
    if (NULStdin == INVALID_HANDLE_VALUE)
        goto cleanup;

    status = SD_ERR;
    NULStderr = CreateFile("NUL", GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_ALWAYS, 0, INVALID_HANDLE_VALUE);
    if (NULStderr == INVALID_HANDLE_VALUE)
        goto cleanup;

    status = SD_OUT;
    NULStdout = CreateFile("NUL", GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_ALWAYS, 0, INVALID_HANDLE_VALUE);
    if (NULStdout == INVALID_HANDLE_VALUE)
        goto cleanup;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = NULStdout;
    si.hStdError = NULStderr;
    si.hStdInput = NULStdin;

#ifdef TRACE
    printf("Spawning RLFE (%s)\n", cmd);
#endif

    status = SD_CREATE;
    if (!CreateProcess(NULL,
        cmd,
        NULL,
        NULL,
        TRUE,
        DETACHED_PROCESS,
        NULL,
        NULL,
        &si,
        &pi))
        goto cleanup;

    status = SD_NONE;

    // Close handles.

    CloseHandle(NULStdin);
    CloseHandle(NULStderr);
    CloseHandle(NULStdout);

#ifdef TRACE
    printf("Successful spawn\n");
#endif

    return 0;

cleanup:

    switch (status) {
    case SD_CREATE:
        CloseHandle(NULStdout);
    case SD_OUT:
        CloseHandle(NULStderr);
    case SD_ERR:
        CloseHandle(NULStdin);
    case SD_IN:
        break;
    }

    switch (status) {
    case SD_CREATE:
        str = "Unable to spawn RL";
        break;
    case SD_ERR:
    case SD_IN:
    case SD_OUT:
        str = "Unable to obtain file handle";
        break;
    }

    printf("%s\n", str);

    return 1;
}


// This function takes care of creating the pipe between RL and RLFE and
// starts RLFE.
BOOL
RLFEConnect(
    const char *prefix
)
{
    char pipeName[32];
    char options[512], *pOpt, *s;
    char pipeID[9], *pID;
    DWORD id = 0;

    pOpt = options;
    pID = NULL;

    // Parse RLFE options.

    while (RLFEOpts && *RLFEOpts) {
        s = strchr(RLFEOpts, ',');
        if (s)
            *s++ = '\0';

        if (!_stricmp(RLFEOpts, "min"))
            pOpt += sprintf_s(pOpt, REMAININGARRAYLEN(options, pOpt), " -min");
        else if (!_strnicmp(RLFEOpts, "pipe", 4))
            pID = RLFEOpts + 4;

        RLFEOpts = s;
    }

    if (pID == NULL) {
        id = GetCurrentProcessId();
        sprintf_s(pipeID, "%08x", id);
        pID = pipeID;
    }

    sprintf_s(pipeName, "%s%s", PIPE_NAME, pID);
    if (strlen(pipeName) > 31) {
        printf("RLFE pipename buffer too small\n");
        return FALSE;
    }

    pOpt += sprintf_s(pOpt, REMAININGARRAYLEN(options, pOpt), " -pipe %s", pID);
    if (prefix)
        pOpt += sprintf_s(pOpt, REMAININGARRAYLEN(options, pOpt), " -prefix \"%s\"", prefix);

    HPipe = CreateNamedPipe(
        pipeName,
        PIPE_ACCESS_OUTBOUND | FILE_FLAG_WRITE_THROUGH,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        2048,
        2048,
        5 * 1000,
        NULL);
    if (HPipe == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "RLFE pipe creation failed\n");
        return FALSE;
    }

    if (id) {
        if (strlen(options) > 511) {
            printf("RLFE options buffer too small\n");
            return FALSE;
        }
        if (SpawnRLFE(options)) {
            CloseHandle(HPipe);
            return FALSE;
        }
    }
    else {
        printf("Please launch RLFE manually with -pipe %s\n", pID);
    }

    if (!ConnectNamedPipe(HPipe, NULL)) {
        CloseHandle(HPipe);
        fprintf(stderr, "RLFE failed to connected\n");
        return FALSE;
    }

    FConnected = TRUE;

    return TRUE;
}

void
RLFEDisconnect(
    BOOL fKilled
)
{
    if (FConnected) {
        if (!fKilled) {
            SendCommand(RLFE_DONE, 0);
            FlushFileBuffers(HPipe);
        }
        DeleteCriticalSection(&CS);
        CloseHandle(HPipe);
        HPipe = INVALID_HANDLE_VALUE;
        FConnected = FALSE;
    }

    FRLFE = FALSE;
}

BOOL
RLFEEnterCritSec(
    void
)
{
    EnterCriticalSection(&CS);
    return FRLFE;
}

void
RLFELeaveCritSec(
    void
)
{
    LeaveCriticalSection(&CS);
}


static void
SendData(
    BYTE *buf,
    DWORD len
)
{
    DWORD wb;

    if (!WriteFile(HPipe, buf, len, &wb, NULL) || (wb != len)) {
        fprintf(stderr, "Write to RLFE pipe failed; RLFE output disabled\n");
        RLFEDisconnect(TRUE);
    }
}

static void
SendCommand(
    RLFE_COMMAND command,
    DWORD dataLen
)
{
    BYTE cmdBuf[3];

    cmdBuf[0] = (char)command;
    cmdBuf[1] = BYTE(dataLen / 256);
    cmdBuf[2] = BYTE(dataLen % 256);

    if (RLFEEnterCritSec()) {
        SendData(cmdBuf, 3);
        RLFELeaveCritSec();
    }
}

static void
SendDataWithLen(
    BYTE *data,
    DWORD len
)
{
    BYTE lenBuf[2];

    lenBuf[0] = BYTE(len / 256);
    lenBuf[1] = BYTE(len % 256);

    if (RLFEEnterCritSec()) {
        SendData(lenBuf, 2);
        if (len)
            SendData(data, len);
        RLFELeaveCritSec();
    }
}

void
RLFEInit(
    BYTE numThreads,
    int numDirs
)
{
    BYTE buf[4];

    ASSERTNR((SHORT)numDirs == numDirs);

    InitializeCriticalSection(&CS);

#ifdef TRACE
    printf("RLFEAddInit: %d, %d\n", numDirs, numThreads);
#endif

    *(SHORT *)buf = (SHORT)numDirs;
    buf[2] = numThreads;
    buf[3] = (BYTE)RLFE_VERSION;

    SendCommand(RLFE_INIT, 4);
    SendData(buf, 4);
}

static const char *
StatString(
    RL_STATS stat
)
{
    switch (stat) {
    case RLS_TOTAL:
        return "";

    case RLS_EXE:
        return "Exec";

    case RLS_BASELINES:
        return "Baselines";

    case RLS_DIFFS:
        return "Diffs";
    }

    return "RL_ERROR";
}

void
RLFEAddRoot(
    RL_STATS stat,
    DWORD total
)
{
    int len;
    const char *path;

    path = StatString(stat);
    len = (int)strlen(path) + 5;

#ifdef TRACE
    printf("RLFEAddRoot: %d, %d\n", stat, total);
#endif

    if (RLFEEnterCritSec()) {
        SendCommand(RLFE_ADD_ROOT, len);
        SendData((BYTE *)path, len - 5);
        SendData((BYTE *)&total, 4);
        SendData((BYTE *)&stat, 1);
        RLFELeaveCritSec();
    }
}

void
RLFEAddTest(
    RL_STATS stat,
    CDirectory *pDir
)
{
    char *path;
    int len, num;

    path = pDir->GetDirectoryName();
    num = pDir->GetDirectoryNumber();
    ASSERTNR((SHORT)num == num);

#ifdef TRACE
    printf("RLFEAddTest: %d, %s, %d, %d\n", stat,
        path, num, pDir->NumVariations);
#endif

    len = (int)strlen(path) + 7;

    if (RLFEEnterCritSec()) {
        SendCommand(RLFE_ADD_TEST, len);
        SendData((BYTE *)path, len - 7);
        SendData((BYTE *)&pDir->NumVariations, 4);
        SendData((BYTE *)&stat, 1);
        SendData((BYTE *)&num, 2);
        RLFELeaveCritSec();
    }
}

void
RLFEAddLog(
    CDirectory *pDir,
    RLFE_STATUS status,
    const char *testName,
    const char *subTestName,
    const char *logText
)
{
    int pathLen, logLen, num;
    BYTE buf[4];
    char pathBuf[BUFFER_SIZE], *path;

    ASSERTNR(testName || subTestName);
    ASSERTNR(logText);

    path = pathBuf;
    if (testName)
        path += sprintf_s(path, REMAININGARRAYLEN(pathBuf, path), "%c%s", PIPE_SEP_CHAR, testName);
    if (subTestName)
        path += sprintf_s(path, REMAININGARRAYLEN(pathBuf, path), "%c%s", PIPE_SEP_CHAR, subTestName);

    // String has a leading PIPE_SEP_CHAR, so chop that off.

    path = pathBuf + 1;
    pathLen = (int)strlen(path);
    ASSERTNR(pathLen < BUFFER_SIZE - 1);

    logLen = (int)strlen(logText);

    num = pDir->GetDirectoryNumber();
    ASSERTNR((SHORT)num == num);

    *(SHORT *)buf = (SHORT)num;
    buf[2] = (BYTE)pDir->stat;
    buf[3] = (BYTE)status;

#ifdef TRACE
    printf("RLFEAddLog: %d, %d, %s, %s\n", pDir->stat, num, path, logText);
#endif

    if (RLFEEnterCritSec()) {
        SendCommand(RLFE_ADD_LOG, 2 + pathLen + 4 + logLen);
        SendDataWithLen((BYTE *)path, pathLen);
        SendData(buf, 4);
        SendData((BYTE *)logText, logLen);
        RLFELeaveCritSec();
    }
}

void
RLFETestStatus(
    CDirectory *pDir
)
{
    DWORD *d;
    BYTE buf[3];
    BYTE statBuf[3 * 4];
    DWORD dataLen;
    int num;

    num = pDir->GetDirectoryNumber();
    ASSERTNR((SHORT)num == num);

    if (pDir->stat == RLS_DIFFS)
        dataLen = 3 * 4;
    else
        dataLen = 2 * 4;

    d = (DWORD *)statBuf;
    *d++ = (int) pDir->NumVariationsRun;
    *d++ = (int) pDir->NumFailures;

    if (pDir->stat == RLS_DIFFS)
        *d++ = (int) pDir->NumDiffs;

    *(SHORT *)buf = (SHORT)num;
    buf[2] = (BYTE)pDir->stat;

#ifdef TRACE
    printf("RLFETestStatus: %d, %d, %d, %d, %d\n",
        pDir->stat, num, pDir->NumVariationsRun, pDir->NumFailures,
        pDir->NumDiffs);
#endif

    if (RLFEEnterCritSec()) {
        SendCommand(RLFE_TEST_STATUS, 2 + 1 + dataLen);
        SendData((BYTE *)&buf, 3);
        SendData(statBuf, dataLen);
        RLFELeaveCritSec();
    }
}

void
RLFEThreadDir(
    CDirectory *pDir,
    BYTE threadNum
)
{
    BYTE buf[4];
    int num;

    if (pDir) {
        num = pDir->GetDirectoryNumber();
        ASSERTNR((SHORT)num == num);
    }
    else {
        num = 0;
    }

    --threadNum; // RL is 1-based, RLFE is 0-based

    *(SHORT *)buf = (SHORT)num;
    buf[2] = threadNum;
    if (pDir)
        buf[3] = (BYTE)pDir->stat;

#ifdef TRACE
    printf("RLFEThreadDir: %d, %d, %d\n", num, threadNum, buf[3]);
#endif

    if (RLFEEnterCritSec()) {
        SendCommand(RLFE_THREAD_DIR, 4);
        SendData(buf, 4);
        RLFELeaveCritSec();
    }
}

void
RLFEThreadStatus(
    BYTE num,
    const char *text
)
{
    DWORD len;

    --num; // RL is 1-based, RLFE is 0-based

    len = (DWORD)strlen(text);

#ifdef TRACE
    printf("RLFEThreadStatus: %d, %s\n", num, text);
#endif

    if (RLFEEnterCritSec()) {
        SendCommand(RLFE_THREAD_STATUS, len + 1);
        SendData(&num, 1);
        SendData((BYTE *)text, len);
        RLFELeaveCritSec();
    }
}
