// Demonstrates how U+30FC is sometimes handled as a single-width character
// when it should be handled as a double-width character.
//
// It only runs on computers where 932 is a valid code page.  Set the system
// local to "Japanese (Japan)" to ensure this.
//
// The problem seems to happen when U+30FC is printed in a console using the
// Lucida Console font, and only when that font is at certain sizes.
//

#include <windows.h>
#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TestUtil.cc"

#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))

static void setFont(const wchar_t *faceName, int pxSize) {
    CONSOLE_FONT_INFOEX infoex = {0};
    infoex.cbSize = sizeof(infoex);
    infoex.dwFontSize.Y = pxSize;
    wcsncpy(infoex.FaceName, faceName, COUNT_OF(infoex.FaceName));
    BOOL ret = SetCurrentConsoleFontEx(
        GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &infoex);
    assert(ret);
}

static bool performTest(const wchar_t testChar) {
    const HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);

    SetConsoleTextAttribute(conout, 7);

    system("cls");
    DWORD actual = 0;
    BOOL ret = WriteConsoleW(conout, &testChar, 1, &actual, NULL);
    assert(ret && actual == 1);

    CHAR_INFO verify[2];
    COORD bufSize = {2, 1};
    COORD bufCoord = {0, 0};
    const SMALL_RECT readRegion = {0, 0, 1, 0};
    SMALL_RECT actualRegion = readRegion;
    ret = ReadConsoleOutputW(conout, verify, bufSize, bufCoord, &actualRegion);
    assert(ret && !memcmp(&readRegion, &actualRegion, sizeof(readRegion)));
    assert(verify[0].Char.UnicodeChar == testChar);

    if (verify[1].Char.UnicodeChar == testChar) {
        // Typical double-width behavior with a TrueType font.  Pass.
        assert(verify[0].Attributes == 0x107);
        assert(verify[1].Attributes == 0x207);
        return true;
    } else if (verify[1].Char.UnicodeChar == 0) {
        // Typical double-width behavior with a Raster Font.  Pass.
        assert(verify[0].Attributes == 7);
        assert(verify[1].Attributes == 0);
        return true;
    } else if (verify[1].Char.UnicodeChar == L' ') {
        // Single-width behavior.  Fail.
        assert(verify[0].Attributes == 7);
        assert(verify[1].Attributes == 7);
        return false;
    } else {
        // Unexpected output.
        assert(false);
    }
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
    if (argc == 1) {
        startChildProcess(L"CHILD");
        return 0;
    }

    assert(SetConsoleCP(932));
    assert(SetConsoleOutputCP(932));

    const wchar_t testChar = 0x30FC;
    const wchar_t *const faceNames[] = {
        L"Lucida Console",
        L"Consolas",
        L"ＭＳ ゴシック",
    };

    trace("Test started");

    for (auto faceName : faceNames) {
        for (int px = 1; px <= 50; ++px) {
            setFont(faceName, px);
            if (!performTest(testChar)) {
                trace("FAILURE: %s %dpx", narrowString(faceName).c_str(), px);
            }
        }
    }

    trace("Test complete");
    return 0;
}
