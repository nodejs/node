//
// Windows versions tested
//
// Vista Enterprise SP2 32-bit
//  - ver reports [Version 6.0.6002]
//  - kernel32.dll product/file versions are 6.0.6002.19381
//
// Windows 7 Ultimate SP1 32-bit
//  - ver reports [Version 6.1.7601]
//  - conhost.exe product/file versions are 6.1.7601.18847
//  - kernel32.dll product/file versions are 6.1.7601.18847
//
// Windows Server 2008 R2 Datacenter SP1 64-bit
//  - ver reports [Version 6.1.7601]
//  - conhost.exe product/file versions are 6.1.7601.23153
//  - kernel32.dll product/file versions are 6.1.7601.23153
//
// Windows 8 Enterprise 32-bit
//  - ver reports [Version 6.2.9200]
//  - conhost.exe product/file versions are 6.2.9200.16578
//  - kernel32.dll product/file versions are 6.2.9200.16859
//

//
// Specific version details on working Server 2008 R2:
//
//      dwMajorVersion      = 6
//      dwMinorVersion      = 1
//      dwBuildNumber       = 7601
//      dwPlatformId        = 2
//      szCSDVersion        = Service Pack 1
//      wServicePackMajor   = 1
//      wServicePackMinor   = 0
//      wSuiteMask          = 0x190
//      wProductType        = 0x3
//
// Specific version details on broken Win7:
//
//      dwMajorVersion      = 6
//      dwMinorVersion      = 1
//      dwBuildNumber       = 7601
//      dwPlatformId        = 2
//      szCSDVersion        = Service Pack 1
//      wServicePackMajor   = 1
//      wServicePackMinor   = 0
//      wSuiteMask          = 0x100
//      wProductType        = 0x1
//

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "TestUtil.cc"

const char *g_prefix = "";

static void dumpHandles() {
    trace("%sSTDIN=0x%I64x STDOUT=0x%I64x STDERR=0x%I64x",
        g_prefix,
        (long long)GetStdHandle(STD_INPUT_HANDLE),
        (long long)GetStdHandle(STD_OUTPUT_HANDLE),
        (long long)GetStdHandle(STD_ERROR_HANDLE));
}

static const char *successOrFail(BOOL ret) {
    return ret ? "ok" : "FAILED";
}

static void startChildInSameConsole(const wchar_t *args, BOOL
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
}

static void closeHandle(HANDLE h) {
    trace("%sClosing handle 0x%I64x...", g_prefix, (long long)h);
    trace("%sClosing handle 0x%I64x... %s", g_prefix, (long long)h, successOrFail(CloseHandle(h)));
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

static HANDLE openConout() {

    // If sa isn't provided, the handle defaults to not-inheritable.
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    trace("%sOpening CONOUT...", g_prefix);
    HANDLE conout = CreateFileW(L"CONOUT$",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                &sa,
                OPEN_EXISTING, 0, NULL);
    trace("%sOpening CONOUT... 0x%I64x", g_prefix, (long long)conout);
    return conout;
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

static void writeTest(const char *msg) {
    writeTest(GetStdHandle(STD_OUTPUT_HANDLE), msg);
}



///////////////////////////////////////////////////////////////////////////////
// TEST 1 -- create new buffer, activate it, and close the handle.  The console
// automatically switches the screen buffer back to the original.
//
// This test passes everywhere.
//

static void test1(int argc, char *argv[]) {
    if (!strcmp(argv[1], "1")) {
        startChildProcess(L"1:child");
        return;
    }

    HANDLE origBuffer = GetStdHandle(STD_OUTPUT_HANDLE);
    writeTest(origBuffer, "<-- origBuffer -->");

    HANDLE newBuffer = createBuffer();
    writeTest(newBuffer, "<-- newBuffer -->");
    setConsoleActiveScreenBuffer(newBuffer);
    Sleep(2000);

    writeTest(origBuffer, "TEST PASSED!");

    // Closing the handle w/o switching the active screen buffer automatically
    // switches the console back to the original buffer.
    closeHandle(newBuffer);

    while (true) {
        Sleep(1000);
    }
}



///////////////////////////////////////////////////////////////////////////////
// TEST 2 -- Test program that creates and activates newBuffer, starts a child
// process, then closes its newBuffer handle.  newBuffer remains activated,
// because the child keeps it active.  (Also see TEST D.)
//

static void test2(int argc, char *argv[]) {
    if (!strcmp(argv[1], "2")) {
        startChildProcess(L"2:parent");
        return;
    }

    if (!strcmp(argv[1], "2:parent")) {
        g_prefix = "parent: ";
        dumpHandles();
        HANDLE origBuffer = GetStdHandle(STD_OUTPUT_HANDLE);
        writeTest(origBuffer, "<-- origBuffer -->");

        HANDLE newBuffer = createBuffer();
        writeTest(newBuffer, "<-- newBuffer -->");
        setConsoleActiveScreenBuffer(newBuffer);

        Sleep(1000);
        writeTest(newBuffer, "bInheritHandles=FALSE:");
        startChildInSameConsole(L"2:child", FALSE);
        Sleep(1000);
        writeTest(newBuffer, "bInheritHandles=TRUE:");
        startChildInSameConsole(L"2:child", TRUE);

        Sleep(1000);
        trace("parent:----");

        // Close the new buffer.  The active screen buffer doesn't automatically
        // switch back to origBuffer, because the child process has a handle open
        // to the original buffer.
        closeHandle(newBuffer);

        Sleep(600 * 1000);
        return;
    }

    if (!strcmp(argv[1], "2:child")) {
        g_prefix = "child: ";
        dumpHandles();
        // The child's output isn't visible, because it's still writing to
        // origBuffer.
        trace("child:----");
        writeTest("writing to STDOUT");

        // Handle inheritability is curious.  The console handles this program
        // creates are inheritable, but CreateProcess is called with both
        // bInheritHandles=TRUE and bInheritHandles=FALSE.
        //
        // Vista and Windows 7: bInheritHandles has no effect.  The child and
        // parent processes have the same STDIN/STDOUT/STDERR handles:
        // 0x3, 0x7, and 0xB.  The parent has a 0xF handle for newBuffer.
        // The child can only write to 0x7, 0xB, and 0xF.  Only the writes to
        // 0xF are visible (i.e. they touch newBuffer).
        //
        // Windows 8 or Windows 10 (legacy or non-legacy): the lowest 2 bits of
        // the HANDLE to WriteConsole seem to be ignored.  The new process'
        // console handles always refer to the buffer that was active when they
        // started, but the values of the handles depend upon bInheritHandles.
        // With bInheritHandles=TRUE, the child has the same
        // STDIN/STDOUT/STDERR/newBuffer handles as the parent, and the three
        // output handles all work, though their output is all visible.  With
        // bInheritHandles=FALSE, the child has different STDIN/STDOUT/STDERR
        // handles, and only the new STDOUT/STDERR handles work.
        //
        for (unsigned int i = 0x1; i <= 0xB0; ++i) {
            char msg[256];
            sprintf(msg, "Write to handle 0x%x", i);
            HANDLE h = reinterpret_cast<HANDLE>(i);
            writeTest(h, msg);
        }

        Sleep(600 * 1000);
        return;
    }
}



///////////////////////////////////////////////////////////////////////////////
// TEST A -- demonstrate an apparent Windows bug with screen buffers
//
// Steps:
//  - The parent starts a child process.
//  - The child process creates and activates newBuffer
//  - The parent opens CONOUT$ and writes to it.
//  - The parent closes CONOUT$.
//     - At this point, broken Windows reactivates origBuffer.
//  - The child writes to newBuffer again.
//  - The child activates origBuffer again, then closes newBuffer.
//
// Test passes if the message "TEST PASSED!" is visible.
// Test commonly fails if conhost.exe crashes.
//
// Results:
//  - Windows 7 Ultimate SP1 32-bit: conhost.exe crashes
//  - Windows Server 2008 R2 Datacenter SP1 64-bit: PASS
//  - Windows 8 Enterprise 32-bit: PASS
//  - Windows 10 64-bit (legacy and non-legacy): PASS
//

static void testA_parentWork() {
    // Open an extra CONOUT$ handle so that the HANDLE values in parent and
    // child don't collide.  I think it's OK if they collide, but since we're
    // trying to track down a Windows bug, it's best to avoid unnecessary
    // complication.
    HANDLE dummy = openConout();

    Sleep(3000);

    // Step 2: Open CONOUT$ in the parent.  This opens the active buffer, which
    // was just created in the child.  It's handle 0x13.  Write to it.

    HANDLE newBuffer = openConout();
    writeTest(newBuffer, "step2: writing to newBuffer");

    Sleep(3000);

    // Step 3: Close handle 0x13.  With Windows 7, the console switches back to
    // origBuffer, and (unless I'm missing something) it shouldn't.

    closeHandle(newBuffer);
}

static void testA_childWork() {
    HANDLE origBuffer = GetStdHandle(STD_OUTPUT_HANDLE);

    //
    // Step 1: Create the new screen buffer in the child process and make it
    // active.  (Typically, it's handle 0x0F.)
    //

    HANDLE newBuffer = createBuffer();

    setConsoleActiveScreenBuffer(newBuffer);
    writeTest(newBuffer, "<-- newBuffer -->");

    Sleep(9000);
    trace("child:----");

    // Step 4: write to the newBuffer again.
    writeTest(newBuffer, "TEST PASSED!");

    //
    // Step 5: Switch back to the original screen buffer and close the new
    // buffer.  The switch call succeeds, but the CloseHandle call freezes for
    // several seconds, because conhost.exe crashes.
    //
    Sleep(3000);

    setConsoleActiveScreenBuffer(origBuffer);
    writeTest(origBuffer, "writing to origBuffer");

    closeHandle(newBuffer);

    // The console HWND is NULL.
    trace("child: console HWND=0x%I64x", (long long)GetConsoleWindow());

    // At this point, the console window has closed, but the parent/child
    // processes are still running.  Calling AllocConsole would fail, but
    // calling FreeConsole followed by AllocConsole would both succeed, and a
    // new console would appear.
}

static void testA(int argc, char *argv[]) {

    if (!strcmp(argv[1], "A")) {
        startChildProcess(L"A:parent");
        return;
    }

    if (!strcmp(argv[1], "A:parent")) {
        g_prefix = "parent: ";
        trace("parent:----");
        dumpHandles();
        writeTest("<-- origBuffer -->");
        startChildInSameConsole(L"A:child");
        testA_parentWork();
        Sleep(120000);
        return;
    }

    if (!strcmp(argv[1], "A:child")) {
        g_prefix = "child: ";
        dumpHandles();
        testA_childWork();
        Sleep(120000);
        return;
    }
}



///////////////////////////////////////////////////////////////////////////////
// TEST B -- invert TEST A -- also crashes conhost on Windows 7
//
// Test passes if the message "TEST PASSED!" is visible.
// Test commonly fails if conhost.exe crashes.
//
// Results:
//  - Windows 7 Ultimate SP1 32-bit: conhost.exe crashes
//  - Windows Server 2008 R2 Datacenter SP1 64-bit: PASS
//  - Windows 8 Enterprise 32-bit: PASS
//  - Windows 10 64-bit (legacy and non-legacy): PASS
//

static void testB(int argc, char *argv[]) {
    if (!strcmp(argv[1], "B")) {
        startChildProcess(L"B:parent");
        return;
    }

    if (!strcmp(argv[1], "B:parent")) {
        g_prefix = "parent: ";
        startChildInSameConsole(L"B:child");
        writeTest("<-- origBuffer -->");
        HANDLE origBuffer = GetStdHandle(STD_OUTPUT_HANDLE);

        //
        // Step 1: Create the new buffer and make it active.
        //
        trace("%s----", g_prefix);
        HANDLE newBuffer = createBuffer();
        setConsoleActiveScreenBuffer(newBuffer);
        writeTest(newBuffer, "<-- newBuffer -->");

        //
        // Step 4: Attempt to write again to the new buffer.
        //
        Sleep(9000);
        trace("%s----", g_prefix);
        writeTest(newBuffer, "TEST PASSED!");

        //
        // Step 5: Switch back to the original buffer.
        //
        Sleep(3000);
        trace("%s----", g_prefix);
        setConsoleActiveScreenBuffer(origBuffer);
        closeHandle(newBuffer);
        writeTest(origBuffer, "writing to the initial buffer");

        Sleep(60000);
        return;
    }

    if (!strcmp(argv[1], "B:child")) {
        g_prefix = "child: ";
        Sleep(3000);
        trace("%s----", g_prefix);

        //
        // Step 2: Open the newly active buffer and write to it.
        //
        HANDLE newBuffer = openConout();
        writeTest(newBuffer, "writing to newBuffer");

        //
        // Step 3: Close the newly active buffer.
        //
        Sleep(3000);
        closeHandle(newBuffer);

        Sleep(60000);
        return;
    }
}



///////////////////////////////////////////////////////////////////////////////
// TEST C -- Interleaving open/close of console handles also seems to break on
// Windows 7.
//
// Test:
//  - child creates and activates newBuf1
//  - parent opens newBuf1
//  - child creates and activates newBuf2
//  - parent opens newBuf2, then closes newBuf1
//  - child switches back to newBuf1
//     * At this point, the console starts malfunctioning.
//  - parent and child close newBuf2
//  - child closes newBuf1
//
// Test passes if the message "TEST PASSED!" is visible.
// Test commonly fails if conhost.exe crashes.
//
// Results:
//  - Windows 7 Ultimate SP1 32-bit: conhost.exe crashes
//  - Windows Server 2008 R2 Datacenter SP1 64-bit: PASS
//  - Windows 8 Enterprise 32-bit: PASS
//  - Windows 10 64-bit (legacy and non-legacy): PASS
//

static void testC(int argc, char *argv[]) {
    if (!strcmp(argv[1], "C")) {
        startChildProcess(L"C:parent");
        return;
    }

    if (!strcmp(argv[1], "C:parent")) {
        startChildInSameConsole(L"C:child");
        writeTest("<-- origBuffer -->");
        g_prefix = "parent: ";

        // At time=4, open newBuffer1.
        Sleep(4000);
        trace("%s---- t=4", g_prefix);
        const HANDLE newBuffer1 = openConout();

        // At time=8, open newBuffer2, and close newBuffer1.
        Sleep(4000);
        trace("%s---- t=8", g_prefix);
        const HANDLE newBuffer2 = openConout();
        closeHandle(newBuffer1);

        // At time=25, cleanup of newBuffer2.
        Sleep(17000);
        trace("%s---- t=25", g_prefix);
        closeHandle(newBuffer2);

        Sleep(240000);
        return;
    }

    if (!strcmp(argv[1], "C:child")) {
        g_prefix = "child: ";

        // At time=2, create newBuffer1 and activate it.
        Sleep(2000);
        trace("%s---- t=2", g_prefix);
        const HANDLE newBuffer1 = createBuffer();
        setConsoleActiveScreenBuffer(newBuffer1);
        writeTest(newBuffer1, "<-- newBuffer1 -->");

        // At time=6, create newBuffer2 and activate it.
        Sleep(4000);
        trace("%s---- t=6", g_prefix);
        const HANDLE newBuffer2 = createBuffer();
        setConsoleActiveScreenBuffer(newBuffer2);
        writeTest(newBuffer2, "<-- newBuffer2 -->");

        // At time=10, attempt to switch back to newBuffer1.  The parent process
        // has opened and closed its handle to newBuffer1, so does it still exist?
        Sleep(4000);
        trace("%s---- t=10", g_prefix);
        setConsoleActiveScreenBuffer(newBuffer1);
        writeTest(newBuffer1, "write to newBuffer1: TEST PASSED!");

        // At time=25, cleanup of newBuffer2.
        Sleep(15000);
        trace("%s---- t=25", g_prefix);
        closeHandle(newBuffer2);

        // At time=35, cleanup of newBuffer1.  The console should switch to the
        // initial buffer again.
        Sleep(10000);
        trace("%s---- t=35", g_prefix);
        closeHandle(newBuffer1);

        Sleep(240000);
        return;
    }
}



///////////////////////////////////////////////////////////////////////////////
// TEST D -- parent creates a new buffer, child launches, writes,
// closes it output handle, then parent writes again.  (Also see TEST 2.)
//
// On success, this will appear:
//
//    parent: <-- newBuffer -->
//    child: writing to newBuffer
//    parent: TEST PASSED!
//
// If this appears, it indicates that the child's closing its output handle did
// not destroy newBuffer.
//
// Results:
//  - Windows 7 Ultimate SP1 32-bit: PASS
//  - Windows 8 Enterprise 32-bit: PASS
//  - Windows 10 64-bit (legacy and non-legacy): PASS
//

static void testD(int argc, char *argv[]) {
    if (!strcmp(argv[1], "D")) {
        startChildProcess(L"D:parent");
        return;
    }

    if (!strcmp(argv[1], "D:parent")) {
        g_prefix = "parent: ";
        HANDLE origBuffer = GetStdHandle(STD_OUTPUT_HANDLE);
        writeTest(origBuffer, "<-- origBuffer -->");

        HANDLE newBuffer = createBuffer();
        writeTest(newBuffer, "<-- newBuffer -->");
        setConsoleActiveScreenBuffer(newBuffer);

        // At t=2, start a child process, explicitly forcing it to use
        // newBuffer for its standard handles.  These calls are apparently
        // redundant on Windows 8 and up.
        Sleep(2000);
        trace("parent:----");
        trace("parent: starting child process");
        SetStdHandle(STD_OUTPUT_HANDLE, newBuffer);
        SetStdHandle(STD_ERROR_HANDLE, newBuffer);
        startChildInSameConsole(L"D:child");
        SetStdHandle(STD_OUTPUT_HANDLE, origBuffer);
        SetStdHandle(STD_ERROR_HANDLE, origBuffer);

        // At t=6, write again to newBuffer.
        Sleep(4000);
        trace("parent:----");
        writeTest(newBuffer, "TEST PASSED!");

        // At t=8, close the newBuffer.  In earlier versions of windows
        // (including Server 2008 R2), the console then switches back to
        // origBuffer.  As of Windows 8, it doesn't, because somehow the child
        // process is keeping the console on newBuffer, even though the child
        // process closed its STDIN/STDOUT/STDERR handles.  Killing the child
        // process by hand after the test finishes *does* force the console
        // back to origBuffer.
        Sleep(2000);
        closeHandle(newBuffer);

        Sleep(120000);
        return;
    }

    if (!strcmp(argv[1], "D:child")) {
        g_prefix = "child: ";
        // At t=2, the child starts.
        trace("child:----");
        dumpHandles();
        writeTest("writing to newBuffer");

        // At t=4, the child explicitly closes its handle.
        Sleep(2000);
        trace("child:----");
        if (GetStdHandle(STD_ERROR_HANDLE) != GetStdHandle(STD_OUTPUT_HANDLE)) {
            closeHandle(GetStdHandle(STD_ERROR_HANDLE));
        }
        closeHandle(GetStdHandle(STD_OUTPUT_HANDLE));
        closeHandle(GetStdHandle(STD_INPUT_HANDLE));

        Sleep(120000);
        return;
    }
}



int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("USAGE: %s testnum\n", argv[0]);
        return 0;
    }

    if (argv[1][0] == '1') {
        test1(argc, argv);
    } else if (argv[1][0] == '2') {
        test2(argc, argv);
    } else if (argv[1][0] == 'A') {
        testA(argc, argv);
    } else if (argv[1][0] == 'B') {
        testB(argc, argv);
    } else if (argv[1][0] == 'C') {
        testC(argc, argv);
    } else if (argv[1][0] == 'D') {
        testD(argc, argv);
    }
    return 0;
}
