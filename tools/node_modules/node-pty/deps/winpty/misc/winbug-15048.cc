/*

Test program demonstrating a problem in Windows 15048's ReadConsoleOutput API.

To compile:

    cl /nologo /EHsc winbug-15048.cc shell32.lib

Example of regressed input:

Case 1:

    > chcp 932
    > winbug-15048 -face-gothic 3044

    Correct output:

        1**34      (nb: U+3044 replaced with '**' to avoid MSVC encoding warning)
        5678

        ReadConsoleOutputW (both rows, 3 cols)
        row 0: U+0031(0007) U+3044(0107) U+3044(0207) U+0033(0007)
        row 1: U+0035(0007) U+0036(0007) U+0037(0007) U+0038(0007)

        ReadConsoleOutputW (both rows, 4 cols)
        row 0: U+0031(0007) U+3044(0107) U+3044(0207) U+0033(0007) U+0034(0007)
        row 1: U+0035(0007) U+0036(0007) U+0037(0007) U+0038(0007) U+0020(0007)

        ReadConsoleOutputW (second row)
        row 1: U+0035(0007) U+0036(0007) U+0037(0007) U+0038(0007) U+0020(0007)

        ...

    Win10 15048 bad output:

        1**34
        5678

        ReadConsoleOutputW (both rows, 3 cols)
        row 0: U+0031(0007) U+3044(0007) U+0033(0007) U+0035(0007)
        row 1: U+0036(0007) U+0037(0007) U+0038(0007) U+0000(0000)

        ReadConsoleOutputW (both rows, 4 cols)
        row 0: U+0031(0007) U+3044(0007) U+0033(0007) U+0034(0007) U+0035(0007)
        row 1: U+0036(0007) U+0037(0007) U+0038(0007) U+0020(0007) U+0000(0000)

        ReadConsoleOutputW (second row)
        row 1: U+0035(0007) U+0036(0007) U+0037(0007) U+0038(0007) U+0020(0007)

        ...

    The U+3044 character (HIRAGANA LETTER I) occupies two columns, but it only
    fills one record in the ReadConsoleOutput output buffer, which has the
    effect of shifting the first cell of the second row into the last cell of
    the first row.  Ordinarily, the first and second cells would also have the
    COMMON_LVB_LEADING_BYTE and COMMON_LVB_TRAILING_BYTE attributes set, which
    allows winpty to detect the double-column character.

Case 2:

    > chcp 437
    > winbug-15048 -face "Lucida Console" -h 4 221A

    The same issue happens with U+221A (SQUARE ROOT), but only in certain
    fonts.  The console seems to think this character occupies two columns
    if the font is sufficiently small.  The Windows console properties dialog
    doesn't allow fonts below 5 pt, but winpty tries to use 2pt and 4pt Lucida
    Console to allow very large console windows.

Case 3:

    > chcp 437
    > winbug-15048 -face "Lucida Console" -h 12 FF12

    The console selection system thinks U+FF12 (FULLWIDTH DIGIT TWO) occupies
    two columns, which happens to be correct, but it's displayed as a single
    column unrecognized character.  It otherwise behaves the same as the other
    cases.

*/

#include <windows.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>

#include <string>

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

// See https://en.wikipedia.org/wiki/List_of_CJK_fonts
const wchar_t kMSGothic[] = { 0xff2d, 0xff33, 0x0020, 0x30b4, 0x30b7, 0x30c3, 0x30af, 0 }; // Japanese
const wchar_t kNSimSun[] = { 0x65b0, 0x5b8b, 0x4f53, 0 }; // Simplified Chinese
const wchar_t kMingLight[] = { 0x7d30, 0x660e, 0x9ad4, 0 }; // Traditional Chinese
const wchar_t kGulimChe[] = { 0xad74, 0xb9bc, 0xccb4, 0 }; // Korean

static void set_font(const wchar_t *name, int size) {
    const HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX fontex {};
    fontex.cbSize = sizeof(fontex);
    fontex.dwFontSize.Y = size;
    fontex.FontWeight = 400;
    fontex.FontFamily = 0x36;
    wcsncpy(fontex.FaceName, name, COUNT_OF(fontex.FaceName));
    assert(SetCurrentConsoleFontEx(conout, FALSE, &fontex));
}

static void usage(const wchar_t *prog) {
    printf("Usage: %ls [options]\n", prog);
    printf("  -h HEIGHT\n");
    printf("  -face FACENAME\n");
    printf("  -face-{gothic|simsun|minglight|gulimche) [JP,CN-sim,CN-tra,KR]\n");
    printf("  hhhh -- print U+hhhh\n");
    exit(1);
}

static void dump_region(SMALL_RECT region, const char *name) {
    const HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);

    CHAR_INFO buf[1000];
    memset(buf, 0xcc, sizeof(buf));

    const int w = region.Right - region.Left + 1;
    const int h = region.Bottom - region.Top + 1;

    assert(ReadConsoleOutputW(
        conout, buf, { (short)w, (short)h }, { 0, 0 },
        &region));

    printf("\n");
    printf("ReadConsoleOutputW (%s)\n", name);
    for (int y = 0; y < h; ++y) {
        printf("row %d: ", region.Top + y);
        for (int i = 0; i < region.Left * 13; ++i) {
            printf(" ");
        }
        for (int x = 0; x < w; ++x) {
            const int i = y * w + x;
            printf("U+%04x(%04x) ", buf[i].Char.UnicodeChar, buf[i].Attributes);
        }
        printf("\n");
    }
}

int main() {
    wchar_t *cmdline = GetCommandLineW();
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(cmdline, &argc);
    const wchar_t *font_name = L"Lucida Console";
    int font_height = 8;
    int test_ch = 0xff12; // U+FF12 FULLWIDTH DIGIT TWO

    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        const std::wstring next = i + 1 < argc ? argv[i + 1] : L"";
        if (arg == L"-face" && i + 1 < argc) {
            font_name = argv[i + 1];
            i++;
        } else if (arg == L"-face-gothic") {
            font_name = kMSGothic;
        } else if (arg == L"-face-simsun") {
            font_name = kNSimSun;
        } else if (arg == L"-face-minglight") {
            font_name = kMingLight;
        } else if (arg == L"-face-gulimche") {
            font_name = kGulimChe;
        } else if (arg == L"-h" && i + 1 < argc) {
            font_height = _wtoi(next.c_str());
            i++;
        } else if (arg.c_str()[0] != '-') {
            test_ch = wcstol(arg.c_str(), NULL, 16);
        } else {
            printf("Unrecognized argument: %ls\n", arg.c_str());
            usage(argv[0]);
        }
    }

    const HANDLE conout = GetStdHandle(STD_OUTPUT_HANDLE);

    set_font(font_name, font_height);

    system("cls");
    DWORD actual = 0;
    wchar_t output[] = L"1234\n5678\n";
    output[1] = test_ch;
    WriteConsoleW(conout, output, 10, &actual, nullptr);

    dump_region({ 0, 0, 3, 1 }, "both rows, 3 cols");
    dump_region({ 0, 0, 4, 1 }, "both rows, 4 cols");
    dump_region({ 0, 1, 4, 1 }, "second row");
    dump_region({ 0, 0, 4, 0 }, "first row");
    dump_region({ 1, 0, 4, 0 }, "first row, skip 1");
    dump_region({ 2, 0, 4, 0 }, "first row, skip 2");
    dump_region({ 3, 0, 4, 0 }, "first row, skip 3");

    set_font(font_name, 14);

    return 0;
}
