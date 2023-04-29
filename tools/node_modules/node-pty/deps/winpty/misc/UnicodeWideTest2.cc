//
// Test half-width vs full-width characters.
//

#include <windows.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "TestUtil.cc"

static void writeChars(const wchar_t *text) {
    wcslen(text);
    const int len = wcslen(text);
    DWORD actual = 0;
    BOOL ret = WriteConsoleW(
        GetStdHandle(STD_OUTPUT_HANDLE),
        text, len, &actual, NULL);
    trace("writeChars: ret=%d, actual=%lld", ret, (long long)actual);
}

static void dumpChars(int x, int y, int w, int h) {
    BOOL ret;
    const COORD bufSize = {w, h};
    const COORD bufCoord = {0, 0};
    const SMALL_RECT topLeft = {x, y, x + w - 1, y + h - 1};
    CHAR_INFO mbcsData[w * h];
    CHAR_INFO unicodeData[w * h];
    SMALL_RECT readRegion;
    readRegion = topLeft;
    ret = ReadConsoleOutputW(GetStdHandle(STD_OUTPUT_HANDLE), unicodeData,
                             bufSize, bufCoord, &readRegion);
    assert(ret);
    readRegion = topLeft;
    ret = ReadConsoleOutputA(GetStdHandle(STD_OUTPUT_HANDLE), mbcsData,
                             bufSize, bufCoord, &readRegion);
    assert(ret);

    printf("\n");
    for (int i = 0; i < w * h; ++i) {
        printf("(%02d,%02d) CHAR: %04x %4x -- %02x %4x\n",
            x + i % w, y + i / w,
            (unsigned short)unicodeData[i].Char.UnicodeChar,
            (unsigned short)unicodeData[i].Attributes,
            (unsigned char)mbcsData[i].Char.AsciiChar,
            (unsigned short)mbcsData[i].Attributes);
    }
}

int main(int argc, char *argv[]) {
    system("cls");
    setWindowPos(0, 0, 1, 1);
    setBufferSize(80, 38);
    setWindowPos(0, 0, 80, 38);

    // Write text.
    const wchar_t text1[] = {
        0x3044, // U+3044 (HIRAGANA LETTER I)
        0x2014, // U+2014 (EM DASH)
        0x3044, // U+3044 (HIRAGANA LETTER I)
        0xFF2D, // U+FF2D (FULLWIDTH LATIN CAPITAL LETTER M)
        0x30FC, // U+30FC (KATAKANA-HIRAGANA PROLONGED SOUND MARK)
        0x0031, // U+3031 (DIGIT ONE)
        0x2014, // U+2014 (EM DASH)
        0x0032, // U+0032 (DIGIT TWO)
        0x005C, // U+005C (REVERSE SOLIDUS)
        0x3044, // U+3044 (HIRAGANA LETTER I)
        0
    };
    setCursorPos(0, 0);
    writeChars(text1);

    setCursorPos(78, 1);
    writeChars(L"<>");

    const wchar_t text2[] = {
        0x0032, // U+3032 (DIGIT TWO)
        0x3044, // U+3044 (HIRAGANA LETTER I)
        0,
    };
    setCursorPos(78, 1);
    writeChars(text2);

    system("pause");

    dumpChars(0, 0, 17, 1);
    dumpChars(2, 0, 2, 1);
    dumpChars(2, 0, 1, 1);
    dumpChars(3, 0, 1, 1);
    dumpChars(78, 1, 2, 1);
    dumpChars(0, 2, 2, 1);

    system("pause");
    system("cls");

    const wchar_t text3[] = {
        0x30FC, 0x30FC, 0x30FC, 0xFF2D, // 1
        0x30FC, 0x30FC, 0x30FC, 0xFF2D, // 2
        0x30FC, 0x30FC, 0x30FC, 0xFF2D, // 3
        0x30FC, 0x30FC, 0x30FC, 0xFF2D, // 4
        0x30FC, 0x30FC, 0x30FC, 0xFF2D, // 5
        0x30FC, 0x30FC, 0x30FC, 0xFF2D, // 6
        0x30FC, 0x30FC, 0x30FC, 0xFF2D, // 7
        0x30FC, 0x30FC, 0x30FC, 0xFF2D, // 8
        0x30FC, 0x30FC, 0x30FC, 0xFF2D, // 9
        0x30FC, 0x30FC, 0x30FC, 0xFF2D, // 10
        0x30FC, 0x30FC, 0x30FC, 0xFF2D, // 11
        0x30FC, 0x30FC, 0x30FC, 0xFF2D, // 12
        L'\r', '\n',
        L'\r', '\n',
        0
    };
    writeChars(text3);
    system("pause");
    {
        const COORD bufSize = {80, 2};
        const COORD bufCoord = {0, 0};
        SMALL_RECT readRegion = {0, 0, 79, 1};
        CHAR_INFO unicodeData[160];
        BOOL ret = ReadConsoleOutputW(GetStdHandle(STD_OUTPUT_HANDLE), unicodeData,
                                 bufSize, bufCoord, &readRegion);
        assert(ret);
        for (int i = 0; i < 96; ++i) {
            printf("%04x ", unicodeData[i].Char.UnicodeChar);
        }
        printf("\n");
    }

    return 0;
}
