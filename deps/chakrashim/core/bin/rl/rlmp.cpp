//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//
// rlmp.c -- Code to help implement multi-threaded support
//

#include "rl.h"

/////////////////////////////////////////////////////////////////////////

int
FilterThread(
    FILE* ChildOutput,
    DWORD millisecTimeout
    );

FILE *
PipeSpawn(
    char* cmdstring,
    void* localEnvVars = NULL
);

int
PipeSpawnClose(
    FILE* pstream,
    bool timedOut
);

/////////////////////////////////////////////////////////////////////////

bool CDirectory::TryLock()
{
    char full[BUFFER_SIZE];
    sprintf_s(full, "%s\\%s", this->GetDirectoryPath(), DIR_LOCKFILE);
    _dirLock = CreateFile(full, GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_DELETE_ON_CLOSE,
                    NULL);
    if (_dirLock == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_SHARING_VIOLATION) {
            return false;
        } else {
            LogError("Creating lock file %s - error %d\n", full, GetLastError());
            return false;
        }
    }
    else
        return true;
}

void CDirectory::WaitLock()
{
    char full[BUFFER_SIZE];
    sprintf_s(full, "%s\\%s", this->GetDirectoryPath(), DIR_LOCKFILE);
    int sec = 0;
    for (;;)
    {
        _dirLock = CreateFile(full, GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_DELETE_ON_CLOSE,
                    NULL);
        if (_dirLock != INVALID_HANDLE_VALUE)
            break;
        if (GetLastError() != ERROR_SHARING_VIOLATION) {
            LogError("Creating lock file %s - error %d\n", full, GetLastError());
            // Continue execution regardless
            break;
        }
        Message("Waiting for directory %s (%d seconds elapsed)",
            this->GetDirectoryPath(), sec);

        // Force this message to be seen immediately. This breaks our usual
        // rules for interleaving output, but that is probably okay in this
        // rare situation.
        FlushOutput();

        Sleep(1000);
        sec++;
    }
}

CDirectory::~CDirectory()
{
    // Directory lock _dirLock gets released automagically by CHandle. Since
    // we use FILE_FLAG_DELETE_ON_CLOSE, the lock file goes away when the
    // handle is closed.

    FreeTestList(&_testList);

    DeleteCriticalSection(&_cs);
}

int CDirectory::Count()
{
    int count = 0;
    Test * pTest;

    for (pTest = _testList.first; pTest != NULL; pTest = pTest->next)
    {
       for (TestVariant * variant = pTest->variants;
            variant != NULL;
            variant = variant->next)
       {
          ++count;
       }
    }
    return count;
}

void CDirectory::InitStats(int num)
{
    if (num) {
        NumVariations = num;

        if (Mode == RM_EXE) {
            stat = RLS_EXE;
            ::NumVariationsTotal[RLS_EXE] += num;
            ::NumVariationsTotal[RLS_TOTAL] += num;
        }
        else {
            if (FBaseDiff || FBaseline) {
                stat = RLS_BASELINES; // initial stat
                ::NumVariationsTotal[RLS_BASELINES] += num;
                ::NumVariationsTotal[RLS_TOTAL] += num;
            }
            if (FBaseDiff || !FBaseline) {
                if (!FBaseDiff)
                    stat = RLS_DIFFS; // initial stat
                ::NumVariationsTotal[RLS_DIFFS] += num;
                ::NumVariationsTotal[RLS_TOTAL] += num;
            }
        }
    }
    else {
        ASSERTNR(FBaseDiff && !IsBaseline());
        ASSERTNR(stat == RLS_BASELINES);
        ASSERTNR(NumVariationsRun == NumVariations);
        stat = RLS_DIFFS;
    }

    NumVariationsRun = NumFailures = NumDiffs = 0;
}

long CDirectory::IncRun(int inc)
{
    ::NumVariationsRun[RLS_TOTAL] += inc; // overall count
    ::NumVariationsRun[stat] += inc; // mode stat
    return NumVariationsRun += inc; // per directory count
}

long CDirectory::IncFailures(int inc)
{
    ::NumFailuresTotal[RLS_TOTAL] += inc; // overall count
    ::NumFailuresTotal[stat] += inc; // mode stat
    return NumFailures += inc; // per directory count
}

long CDirectory::IncDiffs()
{
    ::NumDiffsTotal[RLS_TOTAL]++; // overall count
    ::NumDiffsTotal[stat]++; // mode stat
    return NumDiffs++; // per directory count
}

void CDirectory::TryBeginDirectory()
{
    UpdateState();
}

void CDirectory::TryEndDirectory()
{
    UpdateState();
}

// Notify CDirectory that run counts have been changed.
// Check to see if directory is just being started or finished.
// Output summary if needed and TODO: enter/end directory
void CDirectory::UpdateState()
{
    EnterCriticalSection(&_cs);

    if (_isDirStarted)
    {
        // After we set elapsed_dir, we won't write the finish summary again.
        if (NumVariationsRun == NumVariations && elapsed_dir == 0)
        {
            elapsed_dir = time(NULL) - start_dir;
            WriteDirectoryFinishSummary();
        }
    }
    else
    {
        start_dir = time(NULL);
        _isDirStarted = true;
        WriteDirectoryBeginSummary();
    }

    LeaveCriticalSection(&_cs);
}

void CDirectory::WriteDirectoryBeginSummary()
{
    Message("Running %s %s in directory %s",
        ModeNames[Mode],
        (Mode == RM_ASM)
        ? (IsBaseline() ? "baselines" : "diffs")
        : "tests",
        GetDirectoryName());

    if (FSyncTest || FSyncVariation)
    {
        FlushOutput();
    }
}

void CDirectory::WriteDirectoryFinishSummary()
{
    WriteSummary(GetDirectoryName(), IsBaseline(), NumVariations, NumDiffs, NumFailures);

    if (Timing & TIME_DIR)
    {
        Message("RL: Directory elapsed time (%s): %02d:%02d", GetDirectoryName(), elapsed_dir / 60, elapsed_dir % 60);
    }

    if (FSyncDir)
    {
       FlushOutput();
    }
}

#ifndef NODEBUG
void CDirectory::Dump()
{
    printf("==== Directory %s (%s)\n", _pDir->name, _pDir->fullPath);
    DumpTestList(&_testList);
}
#endif

/////////////////////////////////////////////////////////////////////////

CDirectoryQueue::~CDirectoryQueue()
{
    // We must clean up the directory list rooted at _head if FSingleThreadPerDir is
    // false. Might as well just always clean up if there is anything here.
    CDirectory* prev = NULL;
    CDirectory* temp = _head;

    while (temp != NULL)
    {
        prev = temp;
        temp = temp->_next;

        delete prev;
    }

    // The CHandle objects clean up after themselves.

    DeleteCriticalSection(&_cs);
}

CDirectory* CDirectoryQueue::Pop()
{
    bool force_wait = false;
    CDirectory* tmp;
    CDirectory* tmpPrev;

    EnterCriticalSection(&_cs);
    tmpPrev = NULL;
    tmp = _head;
    while (tmp != NULL) {
        if (tmp->TryLock()) {
            if (tmp == _head)
                _head = _head->_next;
            if (tmpPrev != NULL)
                tmpPrev->_next = tmp->_next; // unlink it
            if (tmp == _tail)
                _tail = tmpPrev;
            --_length;
            break;
        }
        Warning("Skipping locked directory %s (will return to it)",
            tmp->GetDirectoryName());
        tmpPrev = tmp;
        tmp = tmp->_next;
    }
    if (tmp == NULL && _head != NULL) {
        // There's at least one directory, but they're all in use by another
        // copy of rl. Just grab the first one and wait on it.
        tmp = _head;
        _head = _head->_next;
        if (tmp == _tail)
            _tail = NULL;
        --_length;
        // wait until it's no longer in use..... However, don't wait inside the
        // critical section.
        force_wait = true;
    }
    ReleaseSemaphore(_hMaxWorkQueueSem, 1, NULL);
    LeaveCriticalSection(&_cs);
    if (tmp != NULL)    // might be empty diff queue before masters are run
        tmp->_next = NULL;
    if (force_wait)
        tmp->WaitLock();
    return tmp;
}

/////////////////////////////////////////////////////////////////////////

CDirectoryAndTestCase* CDirectoryAndTestCaseQueue::Pop()
{
    CDirectoryAndTestCase* tmp;

    EnterCriticalSection(&_cs);
    tmp = _head;

    if (tmp != NULL) {
        if (tmp == _head)
            _head = _head->_next;
        if (tmp == _tail)
            _tail = NULL;
        --_length;
    }

    ReleaseSemaphore(_hMaxWorkQueueSem, 1, NULL);
    LeaveCriticalSection(&_cs);

    return tmp;
}

/////////////////////////////////////////////////////////////////////////

void CThreadInfo::Done()
{
    // The thread is exiting, so set appropriate state
    EnterCriticalSection(&_cs);
    _isDone = true;
    strcpy_s(_currentTest, "done");
    if (FRLFE)
        RLFEThreadDir(NULL, (BYTE)ThreadId);
    LeaveCriticalSection(&_cs);
}

void CThreadInfo::SetCurrentTest(char* dir, char* test, bool isBaseline)
{
    char* tmp = "";

    EnterCriticalSection(&_cs);

    if (FRLFE) {
        if (test[0] != ' ') {
            sprintf_s(_currentTest, "\\%s", test);
            RLFEThreadStatus((BYTE)ThreadId, _currentTest);
        }
        else {
            RLFEThreadStatus((BYTE)ThreadId, test);
        }
    }

    if (FBaseDiff) { // indicate either baseline or diff compile
        if (isBaseline)
            tmp = "b:";
        else
            tmp = "d:";
    }
    sprintf_s(_currentTest, "%s%s\\%s", tmp, dir, test);

    LeaveCriticalSection(&_cs);
}

void CThreadInfo::AddToTmpFileList(char* fullPath)
{
    EnterCriticalSection(&_cs);
    _head = new TmpFileList(_head, fullPath);
    LeaveCriticalSection(&_cs);
}

void CThreadInfo::ClearTmpFileList()
{
    EnterCriticalSection(&_cs);
    while (_head != NULL) {
        TmpFileList* tmp = _head;
        _head = _head->_next;
        delete tmp;
    }
    _head = NULL;
    LeaveCriticalSection(&_cs);
}

void CThreadInfo::DeleteTmpFileList()
{
    EnterCriticalSection(&_cs);
    while (_head != NULL) {
        DeleteFileIfFound(_head->_fullPath);
        _head = _head->_next;
    }
    ClearTmpFileList();
    LeaveCriticalSection(&_cs);
}

/////////////////////////////////////////////////////////////////////////

#define BUFFER_CHUNK 512

void COutputBuffer::Reset()
{
    // leave buffer alone until destructed
    _end = _start;
    *_end = '\0';
    _textGrabbed = false;
}

void COutputBuffer::Flush(FILE* pfile)
{
    if (pfile != NULL) {
        EnterCriticalSection(&csStdio);
        fprintf(pfile, "%s", _start);
        fflush(pfile);
        _textGrabbed = false;
        LeaveCriticalSection(&csStdio);
    }
    Reset();
}

COutputBuffer::COutputBuffer(char* logfile, bool buffered)
    : _bufSize(512)
    , _buffered(buffered)
    , _textGrabbed(false)
    , _type(OUT_FILENAME)
{
    _end = _start = new char[_bufSize];
    *_end = '\0';
    if (logfile == NULL) {
        _filename = NULL;
    } else {
        _filename = _strdup(logfile);
    }
}

COutputBuffer::COutputBuffer(FILE* pfile, bool buffered)
    : _bufSize(512)
    , _buffered(buffered)
    , _textGrabbed(false)
    , _type(OUT_FILE)
{
    _end = _start = new char[_bufSize];
    *_end = '\0';
    _pfile = pfile;
}

COutputBuffer::~COutputBuffer()
{
    Flush();

    delete[] _start;
    _start = _end = NULL;
    _bufSize = 0;
    if (_type == OUT_FILENAME) {
        delete[] _filename;
        _filename = NULL;
    }
}

// Add without doing varargs formatting (avoids local buffer size problems)
void COutputBuffer::AddDirect(char* string)
{
    ASSERTNR(!_textGrabbed);

    size_t len = strlen(string);
    while ((_end - _start) + len >= _bufSize) {
        char* pNew = new char[_bufSize * 2];
        memcpy(pNew, _start, _end - _start + 1); // copy null
        _end = pNew + (_end - _start);
        delete[] _start;
        _start = pNew;
        _bufSize *= 2;
    }

    memcpy(_end, string, len + 1); // copy null
    _end += len;

    if (!_buffered || (!FRLFE && (NumberOfThreads == 1))) {
        // no need to synchronize; flush immediately to get faster interaction
        Flush();
    }
}

void COutputBuffer::Add(const char* fmt, ...)
{
    const int MESSAGE_MAX = 60000;  // maximum allowed message size
    va_list arg_ptr;
    char tempBuf[MESSAGE_MAX];

    va_start(arg_ptr, fmt);
    vsprintf_s(tempBuf, fmt, arg_ptr);
    ASSERT(strlen(tempBuf) < MESSAGE_MAX); // better not have written past tempBuf
    AddDirect(tempBuf);
}

void COutputBuffer::Flush()
{
    FILE* fp;

    if (_type == OUT_FILE) {
        Flush(_pfile);
    }
    else {
        ASSERTNR(_type == OUT_FILENAME);
        if (_filename != NULL) {
            fp = fopen_unsafe(_filename, "a");
            if (fp == NULL) {
                // We might not be able to open the log or full log output
                // files because someone is grepping or otherwise looking
                // through them while rl is active. In that case, we don't
                // want to just kill rl, so just keep accumulating output
                // and try again next time. Output a warning to the log so
                // they know it happened (but they won't see it unless the
                // output is flushed). We could consider having a maximum,
                // after which we "turn off" this output buffer, but we're
                // unlikely to ever have so much output that it causes a
                // problem.

                Warning("Cannot open '%s' for appending with error '%s'", _filename, strerror_unsafe(errno));
            }
            else {
                Flush(fp);
                fclose(fp);
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////

//+---------------------------------------------------------------------------
//
//  Function:   ExecuteCommand
//
//  Synopsis:   Execute the given command and filter its output.
//
//  Arguments:  [path]          -- directory in which to execute the command
//              [CommandLine]   --
//
//  Returns:    ERROR_SUCCESS, ERROR_NOTENOUGHMEMORY, or return code from
//              PipeSpawnClose.
//
//  Notes:      On a multiprocessor machine, this will spawn a new thread
//              and then return, letting the thread run asynchronously.  Use
//              WaitForParallelThreads() to ensure all threads are finished.
//              By default, this routine will spawn as many threads as the
//              machine has processors.  This can be overridden with the
//              -threads option.
//
//----------------------------------------------------------------------------

int
ExecuteCommand(
    char* path,
    char* CommandLine,
    DWORD millisecTimeout,
    void* envFlags)
{
    int rc;
    FILE* childOutput = NULL;
    char putEnvStr[BUFFER_SIZE];
    char ExecuteProgramCmdLine[BUFFER_SIZE];
    UINT prevmode;

    prevmode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

    // Always flush output buffers before executing a command. Note: this
    // shouldn't really be necessary because all output should be buffered
    // by the COutputBuffer class on a per-thread basis.
    fflush(stdout);
    fflush(stderr);

    strcpy_s(ExecuteProgramCmdLine, "cmd.exe /c "); // for .cmd/.bat scripts
    strcat_s(ExecuteProgramCmdLine, CommandLine);

    EnterCriticalSection(&csCurrentDirectory);

    // If we're doing executable tests, set the TARGET_VM environment variable.
    // We must do this inside a critical section since the environment is
    // not thread specific.

    if ((Mode == RM_EXE) && TargetVM) {
        sprintf_s(putEnvStr, "TARGET_VM=%s", TargetVM);
        _putenv(putEnvStr);
    }

    rc = _chdir(path);
    if (rc == 0) {
        childOutput = PipeSpawn(ExecuteProgramCmdLine, envFlags);
    }
    LeaveCriticalSection(&csCurrentDirectory);

    if (rc != 0) {
        LogError("Could not change directory to '%s' - errno == %d\n", path, errno);
    } else if (childOutput != NULL) {
        rc = FilterThread(childOutput, millisecTimeout);
        rc = PipeSpawnClose(childOutput, rc == WAIT_TIMEOUT);
    }

    SetErrorMode(prevmode);

    return rc;
}


//+---------------------------------------------------------------------------
//
//  Function:   FilterThread
//
//  Synopsis:   Capture the output of the thread and process it.
//
//----------------------------------------------------------------------------

// Don't malloc this up every time; use thread-local storage to keep it around.
#define FILTER_READ_CHUNK 512
__declspec(thread) char* StartPointer = NULL;
__declspec(thread) unsigned BufSize = 512;

struct FilterThreadData {
    int ThreadId;
    unsigned* pBufSize;
    char** pStartPointer;
    COutputBuffer* ThreadOut;
    COutputBuffer* ThreadFull;
    FILE* ChildOutput;
};

unsigned WINAPI
FilterWorker(
    void* param
    );

int
FilterThread(
    FILE* ChildOutput,
    DWORD millisecTimeout
    )
{
    FilterThreadData data = {
        ThreadId,
        &BufSize,
        &StartPointer,
        ThreadOut,
        ThreadFull,
        ChildOutput
    };

    HANDLE hThreadWorker = (HANDLE)_beginthreadex(nullptr, 0, FilterWorker, &data, 0, nullptr);

    if (hThreadWorker == 0) {
        LogError("Failed to create FilterWorker thread - error = %d", GetLastError());
        return -1;
    }

    DWORD waitresult = WaitForSingleObject(hThreadWorker, millisecTimeout);

    int rc = 0;

    if (waitresult == WAIT_TIMEOUT) {
        // Abort the worker thread by cancelling the IO operations on the pipe
        CancelIoEx((HANDLE)_get_osfhandle(_fileno(ChildOutput)), nullptr);
        WaitForSingleObject(hThreadWorker, INFINITE);
        rc = WAIT_TIMEOUT;
    }
    else {
        DWORD ec;
        if (!GetExitCodeThread(hThreadWorker, &ec)) {
            LogError("Could not get exit code from FilterWorker worker thread - error = %d", GetLastError());
            rc = -1;
        }
        if (ec != 0) {
            LogError("Pipe read failed - errno = %d\n", ec);
            rc = -1;
        }
    }

    CloseHandle(hThreadWorker);

    return rc;
}

unsigned WINAPI
FilterWorker(
    void* param
    )
{
    FilterThreadData* data = static_cast<FilterThreadData*>(param);
    size_t CountBytesRead;
    char* EndPointer;
    char* NewPointer;
    char buf[50];

    buf[0] = '\0';
    if (!FNoThreadId && data->ThreadId != 0 && NumberOfThreads > 1) {
        sprintf_s(buf, "%d>", data->ThreadId);
    }

    if (*data->pStartPointer == NULL)
        *data->pStartPointer = (char*)malloc(*data->pBufSize);

    while (TRUE) {
        EndPointer = *data->pStartPointer;
        do {
            if (*data->pBufSize - (EndPointer - *data->pStartPointer) < FILTER_READ_CHUNK) {
                NewPointer = (char*)malloc(*data->pBufSize*2);
                memcpy(NewPointer, *data->pStartPointer,
                    EndPointer - *data->pStartPointer + 1);     // copy null byte, too
                EndPointer = NewPointer + (EndPointer - *data->pStartPointer);
                free(*data->pStartPointer);
                *data->pStartPointer = NewPointer;
                *data->pBufSize *= 2;
            }
            if (NULL == fgets(EndPointer, FILTER_READ_CHUNK, data->ChildOutput)) {
                if (ferror(data->ChildOutput)) {
                    return errno;
                }
                return 0;
            }
            CountBytesRead = strlen(EndPointer);
            EndPointer = EndPointer + CountBytesRead;
        } while ((CountBytesRead == FILTER_READ_CHUNK - 1)
                && *(EndPointer - 1) != '\n');

        CountBytesRead = EndPointer - *data->pStartPointer;
        if (CountBytesRead != 0) {
            data->ThreadOut->AddDirect(buf);
            data->ThreadOut->AddDirect(*data->pStartPointer);
            data->ThreadFull->AddDirect(buf);
            data->ThreadFull->AddDirect(*data->pStartPointer);
        }
    }
    
    return 0;
}


// PipeSpawn variables.  We can get away with one copy per thread.

__declspec(thread) HANDLE ProcHandle = INVALID_HANDLE_VALUE;

//+---------------------------------------------------------------------------
//
//  PipeSpawn.  Similar to _popen, but captures both stdout and stderr
//
//----------------------------------------------------------------------------

FILE *
PipeSpawn(
    char* cmdstring,
    void* localEnvVars
)
{
    int PipeHandle[2];
    HANDLE WriteHandle, ErrorHandle;
    STARTUPINFO StartupInfo;
    PROCESS_INFORMATION ProcessInformation;
    BOOL Status;
    FILE* pstream;

    ASSERT(cmdstring != NULL);

    // Open the pipe where we'll collect the output.

    _pipe(PipeHandle, 20 * 1024, _O_TEXT|_O_NOINHERIT);   // 20K bytes buffer

    DuplicateHandle(GetCurrentProcess(),
                    (HANDLE)_get_osfhandle(PipeHandle[1]),
                    GetCurrentProcess(),
                    &WriteHandle,
                    0L,
                    TRUE,
                    DUPLICATE_SAME_ACCESS);

    DuplicateHandle(GetCurrentProcess(),
                    (HANDLE)_get_osfhandle(PipeHandle[1]),
                    GetCurrentProcess(),
                    &ErrorHandle,
                    0L,
                    TRUE,
                    DUPLICATE_SAME_ACCESS);

    _close(PipeHandle[1]);

    pstream = _fdopen(PipeHandle[0], "rt");
    if (pstream == NULL) {
        LogError("Creation of I/O filter stream failed - error = %d\n",
            cmdstring, errno);
        CloseHandle(WriteHandle);
        CloseHandle(ErrorHandle);
        WriteHandle = INVALID_HANDLE_VALUE;
        ErrorHandle = INVALID_HANDLE_VALUE;
        _close(PipeHandle[0]);
        return NULL;
    }

    memset(&StartupInfo, 0, sizeof(STARTUPINFO));
    StartupInfo.cb = sizeof(STARTUPINFO);
    StartupInfo.hStdOutput = WriteHandle;
    StartupInfo.hStdError = ErrorHandle;
    StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    StartupInfo.dwFlags = STARTF_USESTDHANDLES;

    memset(&ProcessInformation, 0, sizeof(PROCESS_INFORMATION));
    ProcessInformation.hThread  = INVALID_HANDLE_VALUE;
    ProcessInformation.hProcess = INVALID_HANDLE_VALUE;

    // And start the process.

#ifndef NODEBUG
    if (FDebug) {
        printf("Creating process '%s'\n", cmdstring);
        fflush(stdout);
    }
#endif

    Status = CreateProcess(NULL, cmdstring, NULL, NULL, TRUE, 0, localEnvVars, NULL,
                &StartupInfo, &ProcessInformation);

    CloseHandle(WriteHandle);
    CloseHandle(ErrorHandle);
    WriteHandle = INVALID_HANDLE_VALUE;
    ErrorHandle = INVALID_HANDLE_VALUE;

    if (Status == 0) {
        LogError("Exec of '%s' failed - error = %d\n",
            cmdstring, GetLastError());
        fclose(pstream);        // This will close the read handle
        pstream = NULL;
        ProcHandle = INVALID_HANDLE_VALUE;
    } else {
        CloseHandle(ProcessInformation.hThread);
        ProcessInformation.hThread = INVALID_HANDLE_VALUE;

        ProcHandle = ProcessInformation.hProcess;
    }

    return pstream;
}


//+---------------------------------------------------------------------------
//
//  Function:   PipeSpawnClose (similar to _pclose)
//
//----------------------------------------------------------------------------

int
PipeSpawnClose(
    FILE *pstream,
    bool timedOut
    )
{
    DWORD retval = (DWORD) -1;

    if (pstream == NULL) {
        return -1;
    }

    fclose(pstream);
    pstream = NULL;

    if (timedOut) {
        retval = WAIT_TIMEOUT;
        // TerminateProcess doesn't kill child processes of the specified process.
        // Use taskkill.exe command instead using its /T option to kill child
        // processes as well.
        char cmdbuf[50];
        sprintf_s(cmdbuf, "taskkill.exe /PID %d /T /F", GetProcessId(ProcHandle));
        int rc = ExecuteCommand(REGRESS, cmdbuf);
        if (rc != 0) {
            LogError("taskkill failed - exit code == %d\n", rc);
        }
    }
    else {
        if (WaitForSingleObject(ProcHandle, INFINITE) == WAIT_OBJECT_0) {
            if (!GetExitCodeProcess(ProcHandle, &retval)) {
                LogError("Getting process exit code - error == %d\n", GetLastError());
                retval = (DWORD) -1;
            }
        }
        else {
            LogError("Wait for process termination failed - error == %d\n", GetLastError());
            retval = (DWORD) -1;
        }
    }
    CloseHandle(ProcHandle);
    ProcHandle = INVALID_HANDLE_VALUE;
    return (int)retval;
}
