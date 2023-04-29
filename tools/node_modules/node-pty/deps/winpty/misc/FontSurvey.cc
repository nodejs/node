#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "TestUtil.cc"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

// See https://en.wikipedia.org/wiki/List_of_CJK_fonts
const wchar_t kMSGothic[] = { 0xff2d, 0xff33, 0x0020, 0x30b4, 0x30b7, 0x30c3, 0x30af, 0 }; // Japanese
const wchar_t kNSimSun[] = { 0x65b0, 0x5b8b, 0x4f53, 0 }; // Simplified Chinese
const wchar_t kMingLight[] = { 0x7d30, 0x660e, 0x9ad4, 0 }; // Traditional Chinese
const wchar_t kGulimChe[] = { 0xad74, 0xb9bc, 0xccb4, 0 }; // Korean

std::vector<bool> condense(const std::vector<CHAR_INFO> &buf) {
    std::vector<bool> ret;
    size_t i = 0;
    while (i < buf.size()) {
        if (buf[i].Char.UnicodeChar == L' ' &&
                ((buf[i].Attributes & 0x300) == 0)) {
            // end of line
            break;
        } else if (i + 1 < buf.size() &&
                ((buf[i].Attributes & 0x300) == 0x100) &&
                ((buf[i + 1].Attributes & 0x300) == 0x200) &&
                buf[i].Char.UnicodeChar != L' ' &&
                buf[i].Char.UnicodeChar == buf[i + 1].Char.UnicodeChar) {
            // double-width
            ret.push_back(true);
            i += 2;
        } else if ((buf[i].Attributes & 0x300) == 0) {
            // single-width
            ret.push_back(false);
            i++;
        } else {
            ASSERT(false && "unexpected output");
        }
    }
    return ret;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s \"arguments for SetFont.exe\"\n", argv[0]);
        return 1;
    }

    const char *setFontArgs = argv[1];

    const wchar_t testLine[] = { 0xA2, 0xA3, 0x2014, 0x3044, 0x30FC, 0x4000, 0 };
    const HANDLE conout = openConout();

    char setFontCmd[1024];
    for (int h = 1; h <= 100; ++h) {
        sprintf(setFontCmd, ".\\SetFont.exe %s -h %d && cls", setFontArgs, h);
        system(setFontCmd);

        CONSOLE_FONT_INFOEX infoex = {};
        infoex.cbSize = sizeof(infoex);
        BOOL success = GetCurrentConsoleFontEx(conout, FALSE, &infoex);
        ASSERT(success && "GetCurrentConsoleFontEx failed");

        DWORD actual = 0;
        success = WriteConsoleW(conout, testLine, wcslen(testLine), &actual, nullptr);
        ASSERT(success && actual == wcslen(testLine));

        std::vector<CHAR_INFO> readBuf(14);
        const SMALL_RECT readRegion = {0, 0, static_cast<short>(readBuf.size() - 1), 0};
        SMALL_RECT readRegion2 = readRegion;
        success = ReadConsoleOutputW(
            conout, readBuf.data(), 
            {static_cast<short>(readBuf.size()), 1}, 
            {0, 0},
            &readRegion2);
        ASSERT(success && !memcmp(&readRegion, &readRegion2, sizeof(readRegion)));

        const auto widths = condense(readBuf);
        std::string widthsStr;
        for (bool width : widths) {
            widthsStr.append(width ? "F" : "H");
        }
        char size[16];
        sprintf(size, "%d,%d", infoex.dwFontSize.X, infoex.dwFontSize.Y);
        const char *status = "";
        if (widthsStr == "HHFFFF") {
            status = "GOOD";
        } else if (widthsStr == "HHHFFF") {
            status = "OK";
        } else {
            status = "BAD";
        }
        trace("Size %3d: %-7s %-4s (%s)", h, size, status, widthsStr.c_str());
    }
    sprintf(setFontCmd, ".\\SetFont.exe %s -h 14", setFontArgs);
    system(setFontCmd);
}
