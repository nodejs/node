#include <windows.h>

#include "TestUtil.cc"

const char *g_prefix = "";

static void dumpHandles() {
    trace("%sSTDIN=0x%I64x STDOUT=0x%I64x STDERR=0x%I64x",
        g_prefix,
        (long long)GetStdHandle(STD_INPUT_HANDLE),
        (long long)GetStdHandle(STD_OUTPUT_HANDLE),
        (long long)GetStdHandle(STD_ERROR_HANDLE));
}

static HANDLE createBuffer() {

    // If sa isn't provided, the handle defaults to not-inheritable.
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    trace("%sCreating a new buffer...", g_prefix);
    HANDLE conout = CreateConsoleScreenBuffer(
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                &sa,
                CONSOLE_TEXTMODE_BUFFER, NULL);

    trace("%sCreating a new buffer... 0x%I64x", g_prefix, (long long)conout);
    return conout;
}

static const char *successOrFail(BOOL ret) {
    return ret ? "ok" : "FAILED";
}

static void setConsoleActiveScreenBuffer(HANDLE conout) {
    trace("%sSetConsoleActiveScreenBuffer(0x%I64x) called...",
        g_prefix, (long long)conout);
    trace("%sSetConsoleActiveScreenBuffer(0x%I64x) called... %s",
        g_prefix, (long long)conout,
        successOrFail(SetConsoleActiveScreenBuffer(conout)));
}

static void writeTest(HANDLE conout, const char *msg) {
    char writeData[256];
    sprintf(writeData, "%s%s\n", g_prefix, msg);

    trace("%sWriting to 0x%I64x: '%s'...",
        g_prefix, (long long)conout, msg);
    DWORD actual = 0;
    BOOL ret = WriteConsoleA(conout, writeData, strlen(writeData), &actual, NULL);
    trace("%sWriting to 0x%I64x: '%s'... %s",
        g_prefix, (long long)conout, msg,
        successOrFail(ret && actual == strlen(writeData)));
}

static HANDLE startChildInSameConsole(const wchar_t *args, BOOL
                                    bInheritHandles=FALSE) {
    wchar_t program[1024];
    wchar_t cmdline[1024];
    GetModuleFileNameW(NULL, program, 1024);
    swprintf(cmdline, L"\"%ls\" %ls", program, args);

    STARTUPINFOW sui;
    PROCESS_INFORMATION pi;
    memset(&sui, 0, sizeof(sui));
    memset(&pi, 0, sizeof(pi));
    sui.cb = sizeof(sui);

    CreateProcessW(program, cmdline,
                   NULL, NULL,
                   /*bInheritHandles=*/bInheritHandles,
                   /*dwCreationFlags=*/0,
                   NULL, NULL,
                   &sui, &pi);

    return pi.hProcess;
}

static HANDLE dup(HANDLE h, HANDLE targetProcess) {
    HANDLE h2 = INVALID_HANDLE_VALUE;
    BOOL ret = DuplicateHandle(
        GetCurrentProcess(), h,
        targetProcess, &h2,
        0, TRUE, DUPLICATE_SAME_ACCESS);
    trace("dup(0x%I64x) to process 0x%I64x... %s, 0x%I64x",
        (long long)h,
        (long long)targetProcess,
        successOrFail(ret),
        (long long)h2);
    return h2;
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        startChildProcess(L"parent");
        return 0;
    }

    if (!strcmp(argv[1], "parent")) {
        g_prefix = "parent: ";
        dumpHandles();
        HANDLE hChild = startChildInSameConsole(L"child");

        // Windows 10.
        HANDLE orig1 = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE new1 = createBuffer();

        Sleep(2000);
        setConsoleActiveScreenBuffer(new1);

        // Handle duplication results to child process in same console:
        //  - Windows XP:                                       fails
        //  - Windows 7 Ultimate SP1 32-bit:                    fails
        //  - Windows Server 2008 R2 Datacenter SP1 64-bit:     fails
        //  - Windows 8 Enterprise 32-bit:                      succeeds
        //  - Windows 10:                                       succeeds
        HANDLE orig2 = dup(orig1, GetCurrentProcess());
        HANDLE new2 = dup(new1, GetCurrentProcess());

        dup(orig1, hChild);
        dup(new1, hChild);

        // The writes to orig1/orig2 are invisible.  The writes to new1/new2
        // are visible.
        writeTest(orig1, "write to orig1");
        writeTest(orig2, "write to orig2");
        writeTest(new1, "write to new1");
        writeTest(new2, "write to new2");

        Sleep(120000);
        return 0;
    }

    if (!strcmp(argv[1], "child")) {
        g_prefix = "child: ";
        dumpHandles();
        Sleep(4000);
        for (unsigned int i = 0x1; i <= 0xB0; ++i) {
            char msg[256];
            sprintf(msg, "Write to handle 0x%x", i);
            HANDLE h = reinterpret_cast<HANDLE>(i);
            writeTest(h, msg);
        }
        Sleep(120000);
        return 0;
    }

    return 0;
}
