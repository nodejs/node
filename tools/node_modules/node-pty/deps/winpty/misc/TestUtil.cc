// This file is included into test programs using #include

#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <vector>
#include <string>

#include "../src/shared/DebugClient.h"
#include "../src/shared/TimeMeasurement.h"

#include "../src/shared/DebugClient.cc"
#include "../src/shared/WinptyAssert.cc"
#include "../src/shared/WinptyException.cc"

// Launch this test program again, in a new console that we will destroy.
static void startChildProcess(const wchar_t *args) {
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
                   /*bInheritHandles=*/FALSE,
                   /*dwCreationFlags=*/CREATE_NEW_CONSOLE,
                   NULL, NULL,
                   &sui, &pi);
}

static void setBufferSize(HANDLE conout, int x, int y) {
    COORD size = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
    BOOL success = SetConsoleScreenBufferSize(conout, size);
    trace("setBufferSize: (%d,%d), result=%d", x, y, success);
}

static void setWindowPos(HANDLE conout, int x, int y, int w, int h) {
    SMALL_RECT r = {
        static_cast<SHORT>(x), static_cast<SHORT>(y),
        static_cast<SHORT>(x + w - 1),
        static_cast<SHORT>(y + h - 1)
    };
    BOOL success = SetConsoleWindowInfo(conout, /*bAbsolute=*/TRUE, &r);
    trace("setWindowPos: (%d,%d,%d,%d), result=%d", x, y, w, h, success);
}

static void setCursorPos(HANDLE conout, int x, int y) {
    COORD coord = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
    SetConsoleCursorPosition(conout, coord);
}

static void setBufferSize(int x, int y) {
    setBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), x, y);
}

static void setWindowPos(int x, int y, int w, int h) {
    setWindowPos(GetStdHandle(STD_OUTPUT_HANDLE), x, y, w, h);
}

static void setCursorPos(int x, int y) {
    setCursorPos(GetStdHandle(STD_OUTPUT_HANDLE), x, y);
}

static void countDown(int sec) {
    for (int i = sec; i > 0; --i) {
        printf("%d.. ", i);
        fflush(stdout);
        Sleep(1000);
    }
    printf("\n");
}

static void writeBox(int x, int y, int w, int h, char ch, int attributes=7) {
    CHAR_INFO info = { 0 };
    info.Char.AsciiChar = ch;
    info.Attributes = attributes;
    std::vector<CHAR_INFO> buf(w * h, info);
    HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD bufSize = { static_cast<SHORT>(w), static_cast<SHORT>(h) };
    COORD bufCoord = { 0, 0 };
    SMALL_RECT writeRegion = {
        static_cast<SHORT>(x),
        static_cast<SHORT>(y),
        static_cast<SHORT>(x + w - 1),
        static_cast<SHORT>(y + h - 1)
    };
    WriteConsoleOutputA(conout, buf.data(), bufSize, bufCoord, &writeRegion);
}

static void setChar(int x, int y, char ch, int attributes=7) {
    writeBox(x, y, 1, 1, ch, attributes);
}

static void fillChar(int x, int y, int repeat, char ch) {
    COORD coord = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
    DWORD actual = 0;
    FillConsoleOutputCharacterA(
        GetStdHandle(STD_OUTPUT_HANDLE),
        ch, repeat, coord, &actual);
}

static void repeatChar(int count, char ch) {
    for (int i = 0; i < count; ++i) {
        putchar(ch);
    }
    fflush(stdout);
}

// I don't know why, but wprintf fails to print this face name,
// "ＭＳ ゴシック" (aka MS Gothic).  It helps to use wprintf instead of printf, and
// it helps to call `setlocale(LC_ALL, "")`, but the Japanese symbols are
// ultimately converted to `?` symbols, even though MS Gothic is able to
// display its own name, and the current code page is 932 (Shift-JIS).
static void cvfprintf(HANDLE conout, const wchar_t *fmt, va_list ap) {
    wchar_t buffer[256];
    vswprintf(buffer, 256 - 1, fmt, ap);
    buffer[255] = L'\0';
    DWORD actual = 0;
    if (!WriteConsoleW(conout, buffer, wcslen(buffer), &actual, NULL)) {
        wprintf(L"WriteConsoleW call failed!\n");
    }
}

static void cfprintf(HANDLE conout, const wchar_t *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    cvfprintf(conout, fmt, ap);
    va_end(ap);
}

static void cprintf(const wchar_t *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    cvfprintf(GetStdHandle(STD_OUTPUT_HANDLE), fmt, ap);
    va_end(ap);
}

static std::string narrowString(const std::wstring &input)
{
    int mblen = WideCharToMultiByte(
        CP_UTF8, 0,
        input.data(), input.size(),
        NULL, 0, NULL, NULL);
    if (mblen <= 0) {
        return std::string();
    }
    std::vector<char> tmp(mblen);
    int mblen2 = WideCharToMultiByte(
        CP_UTF8, 0,
        input.data(), input.size(),
        tmp.data(), tmp.size(),
        NULL, NULL);
    assert(mblen2 == mblen);
    return std::string(tmp.data(), tmp.size());
}

HANDLE openConout() {
    const HANDLE conout = CreateFileW(L"CONOUT$",
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL, OPEN_EXISTING, 0, NULL);
    ASSERT(conout != INVALID_HANDLE_VALUE);
    return conout;
}
