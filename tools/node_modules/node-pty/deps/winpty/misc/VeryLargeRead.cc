//
// 2015-09-25
// I measured these limits on the size of a single ReadConsoleOutputW call.
// The limit seems to more-or-less disppear with Windows 8, which is the first
// OS to stop using ALPCs for console I/O.  My guess is that the new I/O
// method does not use the 64KiB shared memory buffer that the ALPC method
// uses.
//
// I'm guessing the remaining difference between Windows 8/8.1 and Windows 10
// might be related to the 32-vs-64-bitness.
//
// Client OSs
//
// Windows XP 32-bit VM ==> up to 13304 characters
//  - 13304x1 works, but 13305x1 fails instantly
// Windows 7 32-bit VM ==> between 16-17 thousand characters
//  - 16000x1 works, 17000x1 fails instantly
//  - 163x100 *crashes* conhost.exe but leaves VeryLargeRead.exe running
// Windows 8 32-bit VM ==> between 240-250 million characters
//  - 10000x24000 works, but 10000x25000 does not
// Windows 8.1 32-bit VM ==> between 240-250 million characters
//  - 10000x24000 works, but 10000x25000 does not
// Windows 10 64-bit VM ==> no limit (tested to 576 million characters)
//  - 24000x24000 works
//  - `ver` reports [Version 10.0.10240], conhost.exe and ConhostV1.dll are
//    10.0.10240.16384 for file and product version.  ConhostV2.dll is
//    10.0.10240.16391 for file and product version.
//
// Server OSs
//
// Windows Server 2008 64-bit VM ==> 14300-14400 characters
//  - 14300x1 works, 14400x1 fails instantly
//  - This OS does not have conhost.exe.
//  - `ver` reports [Version 6.0.6002]
// Windows Server 2008 R2 64-bit VM ==> 15600-15700 characters
//  - 15600x1 works, 15700x1 fails instantly
//  - This OS has conhost.exe, and procexp.exe reveals console ALPC ports in
//    use in conhost.exe.
//  - `ver` reports [Version 6.1.7601], conhost.exe is 6.1.7601.23153 for file
//    and product version.
// Windows Server 2012 64-bit VM ==> at least 100 million characters
//  - 10000x10000 works (VM had only 1GiB of RAM, so I skipped larger tests)
//  - This OS has Windows 8's task manager and procexp.exe reveals the same
//    lack of ALPC ports and the same \Device\ConDrv\* files as Windows 8.
//  - `ver` reports [Version 6.2.9200], conhost.exe is 6.2.9200.16579 for file
//    and product version.
//
// To summarize:
//
// client-OS    server-OS               notes
// ---------------------------------------------------------------------------
// XP           Server 2008             CSRSS, small reads
// 7            Server 2008 R2          ALPC-to-conhost, small reads
// 8, 8.1       Server 2012             new I/O interface, large reads allowed
// 10           <no server OS yet>      enhanced console w/rewrapping
//
// (Presumably, Win2K, Vista, and Win2K3 behave the same as XP.  conhost.exe
// was announced as a Win7 feature.)
//

#include <windows.h>
#include <assert.h>
#include <vector>

#include "TestUtil.cc"

int main(int argc, char *argv[]) {
    long long width = 9000;
    long long height = 9000;

    assert(argc >= 1);
    if (argc == 4) {
        width = atoi(argv[2]);
        height = atoi(argv[3]);
    } else {
        if (argc == 3) {
            width = atoi(argv[1]);
            height = atoi(argv[2]);
        }
        wchar_t args[1024];
        swprintf(args, 1024, L"CHILD %lld %lld", width, height);
        startChildProcess(args);
        return 0;
    }

    const HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);

    setWindowPos(0, 0, 1, 1);
    setBufferSize(width, height);
    setWindowPos(0, 0, std::min(80LL, width), std::min(50LL, height));

    setCursorPos(0, 0);
    printf("A");
    fflush(stdout);
    setCursorPos(width - 2, height - 1);
    printf("B");
    fflush(stdout);

    trace("sizeof(CHAR_INFO) = %d", (int)sizeof(CHAR_INFO));

    trace("Allocating buffer...");
    CHAR_INFO *buffer = new CHAR_INFO[width * height];
    assert(buffer != NULL);
    memset(&buffer[0], 0, sizeof(CHAR_INFO));
    memset(&buffer[width * height - 2], 0, sizeof(CHAR_INFO));

    COORD bufSize = { width, height };
    COORD bufCoord = { 0, 0 };
    SMALL_RECT readRegion = { 0, 0, width - 1, height - 1 };
    trace("ReadConsoleOutputW: calling...");
    BOOL success = ReadConsoleOutputW(conout, buffer, bufSize, bufCoord, &readRegion);
    trace("ReadConsoleOutputW: success=%d", success);

    assert(buffer[0].Char.UnicodeChar == L'A');
    assert(buffer[width * height - 2].Char.UnicodeChar == L'B');
    trace("Top-left and bottom-right characters read successfully!");

    Sleep(30000);

    delete [] buffer;
    return 0;
}
