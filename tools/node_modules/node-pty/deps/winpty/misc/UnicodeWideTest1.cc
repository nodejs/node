#include <windows.h>

#include <assert.h>
#include <vector>

#include "TestUtil.cc"

#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))


CHAR_INFO ci(wchar_t ch, WORD attributes) {
    CHAR_INFO ret;
    ret.Char.UnicodeChar = ch;
    ret.Attributes = attributes;
    return ret;
}

CHAR_INFO ci(wchar_t ch) {
    return ci(ch, 7);
}

CHAR_INFO ci() {
    return ci(L' ');
}

bool operator==(SMALL_RECT x, SMALL_RECT y) {
    return !memcmp(&x, &y, sizeof(x));
}

SMALL_RECT sr(COORD pt, COORD size) {
    return {
        pt.X, pt.Y,
        static_cast<SHORT>(pt.X + size.X - 1),
        static_cast<SHORT>(pt.Y + size.Y - 1)
    };
}

static void set(
        const COORD pt,
        const COORD size,
        const std::vector<CHAR_INFO> &data) {
    assert(data.size() == size.X * size.Y);
    SMALL_RECT writeRegion = sr(pt, size);
    BOOL ret = WriteConsoleOutputW(
        GetStdHandle(STD_OUTPUT_HANDLE),
        data.data(), size, {0, 0}, &writeRegion);
    assert(ret && writeRegion == sr(pt, size));
}

static void set(
        const COORD pt,
        const std::vector<CHAR_INFO> &data) {
    set(pt, {static_cast<SHORT>(data.size()), 1}, data);
}

static void writeAttrsAt(
        const COORD pt,
        const std::vector<WORD> &data) {
    DWORD actual = 0;
    BOOL ret = WriteConsoleOutputAttribute(
        GetStdHandle(STD_OUTPUT_HANDLE),
        data.data(), data.size(), pt, &actual);
    assert(ret && actual == data.size());
}

static void writeCharsAt(
        const COORD pt,
        const std::vector<wchar_t> &data) {
    DWORD actual = 0;
    BOOL ret = WriteConsoleOutputCharacterW(
        GetStdHandle(STD_OUTPUT_HANDLE),
        data.data(), data.size(), pt, &actual);
    assert(ret && actual == data.size());
}

static void writeChars(
        const std::vector<wchar_t> &data) {
    DWORD actual = 0;
    BOOL ret = WriteConsoleW(
        GetStdHandle(STD_OUTPUT_HANDLE),
        data.data(), data.size(), &actual, NULL);
    assert(ret && actual == data.size());
}

std::vector<CHAR_INFO> get(
        const COORD pt,
        const COORD size) {
    std::vector<CHAR_INFO> data(size.X * size.Y);
    SMALL_RECT readRegion = sr(pt, size);
    BOOL ret = ReadConsoleOutputW(
        GetStdHandle(STD_OUTPUT_HANDLE),
        data.data(), size, {0, 0}, &readRegion);
    assert(ret && readRegion == sr(pt, size));
    return data;
}

std::vector<wchar_t> readCharsAt(
        const COORD pt,
        int size) {
    std::vector<wchar_t> data(size);
    DWORD actual = 0;
    BOOL ret = ReadConsoleOutputCharacterW(
        GetStdHandle(STD_OUTPUT_HANDLE),
        data.data(), data.size(), pt, &actual);
    assert(ret);
    data.resize(actual); // With double-width chars, we can read fewer than `size`.
    return data;
}

static void dump(const COORD pt, const COORD size) {
    for (CHAR_INFO ci : get(pt, size)) {
        printf("%04X %04X\n", ci.Char.UnicodeChar, ci.Attributes);
    }
}

static void dumpCharsAt(const COORD pt, int size) {
    for (wchar_t ch : readCharsAt(pt, size)) {
        printf("%04X\n", ch);
    }
}

static COORD getCursorPos() {
    CONSOLE_SCREEN_BUFFER_INFO info = { sizeof(info) };
    assert(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info));
    return info.dwCursorPosition;
}

static void test1() {
    // We write "䀀䀀", then write "䀁" in the middle of the two.  The second
    // write turns the first and last cells into spaces.  The LEADING/TRAILING
    // flags retain consistency.
    printf("test1 - overlap full-width char with full-width char\n");
    writeCharsAt({1,0}, {0x4000, 0x4000});
    dump({0,0}, {6,1});
    printf("\n");
    writeCharsAt({2,0}, {0x4001});
    dump({0,0}, {6,1});
    printf("\n");
}

static void test2() {
    // Like `test1`, but use a lower-level API to do the write.  Consistency is
    // preserved here too -- the first and last cells are replaced with spaces.
    printf("test2 - overlap full-width char with full-width char (lowlevel)\n");
    writeCharsAt({1,0}, {0x4000, 0x4000});
    dump({0,0}, {6,1});
    printf("\n");
    set({2,0}, {ci(0x4001,0x107), ci(0x4001,0x207)});
    dump({0,0}, {6,1});
    printf("\n");
}

static void test3() {
    // However, the lower-level API can break the LEADING/TRAILING invariant
    // explicitly:
    printf("test3 - explicitly violate LEADING/TRAILING using lowlevel API\n");
    set({1,0}, {
        ci(0x4000, 0x207),
        ci(0x4001, 0x107),
        ci(0x3044, 7),
        ci(L'X', 0x107),
        ci(L'X', 0x207),
    });
    dump({0,0}, {7,1});
}

static void test4() {
    // It is possible for the two cells of a double-width character to have two
    // colors.
    printf("test4 - use lowlevel to assign two colors to one full-width char\n");
    set({0,0}, {
        ci(0x4000, 0x142),
        ci(0x4000, 0x224),
    });
    dump({0,0}, {2,1});
}

static void test5() {
    // WriteConsoleOutputAttribute doesn't seem to affect the LEADING/TRAILING
    // flags.
    printf("test5 - WriteConsoleOutputAttribute cannot affect LEADING/TRAILING\n");

    // Trying to clear the flags doesn't work...
    writeCharsAt({0,0}, {0x4000});
    dump({0,0}, {2,1});
    writeAttrsAt({0,0}, {0x42, 0x24});
    printf("\n");
    dump({0,0}, {2,1});

    // ... and trying to add them also doesn't work.
    writeCharsAt({0,1}, {'A', ' '});
    writeAttrsAt({0,1}, {0x107, 0x207});
    printf("\n");
    dump({0,1}, {2,1});
}

static void test6() {
    // The cursor position may be on either cell of a double-width character.
    // Visually, the cursor appears under both cells, regardless of which
    // specific one has the cursor.
    printf("test6 - cursor can be either left or right cell of full-width char\n");

    writeCharsAt({2,1}, {0x4000});

    setCursorPos(2, 1);
    auto pos1 = getCursorPos();
    Sleep(1000);

    setCursorPos(3, 1);
    auto pos2 = getCursorPos();
    Sleep(1000);

    setCursorPos(0, 15);
    printf("%d,%d\n", pos1.X, pos1.Y);
    printf("%d,%d\n", pos2.X, pos2.Y);
}

static void runTest(void (&test)()) {
    system("cls");
    setCursorPos(0, 14);
    test();
    system("pause");
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        startChildProcess(L"CHILD");
        return 0;
    }

    setWindowPos(0, 0, 1, 1);
    setBufferSize(80, 40);
    setWindowPos(0, 0, 80, 40);

    auto cp = GetConsoleOutputCP();
    assert(cp == 932 || cp == 936 || cp == 949 || cp == 950);

    runTest(test1);
    runTest(test2);
    runTest(test3);
    runTest(test4);
    runTest(test5);
    runTest(test6);

    return 0;
}
