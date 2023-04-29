// Copyright (c) 2015 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "ConsoleFont.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "../shared/DebugClient.h"
#include "../shared/OsModule.h"
#include "../shared/StringUtil.h"
#include "../shared/WindowsVersion.h"
#include "../shared/WinptyAssert.h"
#include "../shared/winpty_snprintf.h"

namespace {

#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))

// See https://en.wikipedia.org/wiki/List_of_CJK_fonts
const wchar_t kLucidaConsole[] = L"Lucida Console";
const wchar_t kMSGothic[] = { 0xff2d, 0xff33, 0x0020, 0x30b4, 0x30b7, 0x30c3, 0x30af, 0 }; // 932, Japanese
const wchar_t kNSimSun[] = { 0x65b0, 0x5b8b, 0x4f53, 0 }; // 936, Chinese Simplified
const wchar_t kGulimChe[] = { 0xad74, 0xb9bc, 0xccb4, 0 }; // 949, Korean
const wchar_t kMingLight[] = { 0x7d30, 0x660e, 0x9ad4, 0 }; // 950, Chinese Traditional

struct FontSize {
    short size;
    int width;
};

struct Font {
    const wchar_t *faceName;
    unsigned int family;
    short size;
};

// Ideographs in East Asian languages take two columns rather than one.
// In the console screen buffer, a "full-width" character will occupy two
// cells of the buffer, the first with attribute 0x100 and the second with
// attribute 0x200.
//
// Windows does not correctly identify code points as double-width in all
// configurations.  It depends heavily on the code page, the font facename,
// and (somehow) even the font size.  In the 437 code page (MS-DOS), for
// example, no codepoints are interpreted as double-width.  When the console
// is in an East Asian code page (932, 936, 949, or 950), then sometimes
// selecting a "Western" facename like "Lucida Console" or "Consolas" doesn't
// register, or if the font *can* be chosen, then the console doesn't handle
// double-width correctly.  I tested the double-width handling by writing
// several code points with WriteConsole and checking whether one or two cells
// were filled.
//
// In the Japanese code page (932), Microsoft's default font is MS Gothic.
// MS Gothic double-width handling seems to be broken with console versions
// prior to Windows 10 (including Windows 10's legacy mode), and it's
// especially broken in Windows 8 and 8.1.
//
// Test by running: misc/Utf16Echo A2 A3 2014 3044 30FC 4000
//
// The first three codepoints are always rendered as half-width with the
// Windows Japanese fonts.  (Of these, the first two must be half-width,
// but U+2014 could be either.)  The last three are rendered as full-width,
// and they are East_Asian_Width=Wide.
//
// Windows 7 fails by modeling all codepoints as full-width with font
// sizes 22 and above.
//
// Windows 8 gets U+00A2, U+00A3, U+2014, U+30FC, and U+4000 wrong, but
// using a point size not listed in the console properties dialog
// (e.g. "9") is less wrong:
//
//             |        code point               |
//  font       | 00A2 00A3 2014 3044 30FC 4000   | cell size
// ------------+---------------------------------+----------
//  8          |  F    F    F    F    H    H     |   4x8
//  9          |  F    F    F    F    F    F     |   5x9
//  16         |  F    F    F    F    H    H     |   8x16
// raster 6x13 |  H    H    H    F    F    H(*)  |   6x13
//
// (*) The Raster Font renders U+4000 as a white box (i.e. an unsupported
// character).
//

// See:
//  - misc/Font-Report-June2016 directory for per-size details
//  - misc/font-notes.txt
//  - misc/Utf16Echo.cc, misc/FontSurvey.cc, misc/SetFont.cc, misc/GetFont.cc

const FontSize kLucidaFontSizes[] = {
    { 5, 3 },
    { 6, 4 },
    { 8, 5 },
    { 10, 6 },
    { 12, 7 },
    { 14, 8 },
    { 16, 10 },
    { 18, 11 },
    { 20, 12 },
    { 36, 22 },
    { 48, 29 },
    { 60, 36 },
    { 72, 43 },
};

// Japanese.  Used on Vista and Windows 7.
const FontSize k932GothicVista[] = {
    { 6, 3 },
    { 8, 4 },
    { 10, 5 },
    { 12, 6 },
    { 13, 7 },
    { 15, 8 },
    { 17, 9 },
    { 19, 10 },
    { 21, 11 },
    // All larger fonts are more broken w.r.t. full-size East Asian characters.
};

// Japanese.  Used on Windows 8, 8.1, and the legacy 10 console.
const FontSize k932GothicWin8[] = {
    // All of these characters are broken w.r.t. full-size East Asian
    // characters, but they're equally broken.
    { 5, 3 },
    { 7, 4 },
    { 9, 5 },
    { 11, 6 },
    { 13, 7 },
    { 15, 8 },
    { 17, 9 },
    { 20, 10 },
    { 22, 11 },
    { 24, 12 },
    // include extra-large fonts for small terminals
    { 36, 18 },
    { 48, 24 },
    { 60, 30 },
    { 72, 36 },
};

// Japanese.  Used on the new Windows 10 console.
const FontSize k932GothicWin10[] = {
    { 6, 3 },
    { 8, 4 },
    { 10, 5 },
    { 12, 6 },
    { 14, 7 },
    { 16, 8 },
    { 18, 9 },
    { 20, 10 },
    { 22, 11 },
    { 24, 12 },
    // include extra-large fonts for small terminals
    { 36, 18 },
    { 48, 24 },
    { 60, 30 },
    { 72, 36 },
};

// Chinese Simplified.
const FontSize k936SimSun[] = {
    { 6, 3 },
    { 8, 4 },
    { 10, 5 },
    { 12, 6 },
    { 14, 7 },
    { 16, 8 },
    { 18, 9 },
    { 20, 10 },
    { 22, 11 },
    { 24, 12 },
    // include extra-large fonts for small terminals
    { 36, 18 },
    { 48, 24 },
    { 60, 30 },
    { 72, 36 },
};

// Korean.
const FontSize k949GulimChe[] = {
    { 6, 3 },
    { 8, 4 },
    { 10, 5 },
    { 12, 6 },
    { 14, 7 },
    { 16, 8 },
    { 18, 9 },
    { 20, 10 },
    { 22, 11 },
    { 24, 12 },
    // include extra-large fonts for small terminals
    { 36, 18 },
    { 48, 24 },
    { 60, 30 },
    { 72, 36 },
};

// Chinese Traditional.
const FontSize k950MingLight[] = {
    { 6, 3 },
    { 8, 4 },
    { 10, 5 },
    { 12, 6 },
    { 14, 7 },
    { 16, 8 },
    { 18, 9 },
    { 20, 10 },
    { 22, 11 },
    { 24, 12 },
    // include extra-large fonts for small terminals
    { 36, 18 },
    { 48, 24 },
    { 60, 30 },
    { 72, 36 },
};

// Some of these types and functions are missing from the MinGW headers.
// Others are undocumented.

struct AGENT_CONSOLE_FONT_INFO {
    DWORD nFont;
    COORD dwFontSize;
};

struct AGENT_CONSOLE_FONT_INFOEX {
    ULONG cbSize;
    DWORD nFont;
    COORD dwFontSize;
    UINT FontFamily;
    UINT FontWeight;
    WCHAR FaceName[LF_FACESIZE];
};

// undocumented XP API
typedef BOOL WINAPI SetConsoleFont_t(
            HANDLE hOutput,
            DWORD dwFontIndex);

// undocumented XP API
typedef DWORD WINAPI GetNumberOfConsoleFonts_t();

// XP and up
typedef BOOL WINAPI GetCurrentConsoleFont_t(
            HANDLE hOutput,
            BOOL bMaximumWindow,
            AGENT_CONSOLE_FONT_INFO *lpConsoleCurrentFont);

// XP and up
typedef COORD WINAPI GetConsoleFontSize_t(
            HANDLE hConsoleOutput,
            DWORD nFont);

// Vista and up
typedef BOOL WINAPI GetCurrentConsoleFontEx_t(
            HANDLE hConsoleOutput,
            BOOL bMaximumWindow,
            AGENT_CONSOLE_FONT_INFOEX *lpConsoleCurrentFontEx);

// Vista and up
typedef BOOL WINAPI SetCurrentConsoleFontEx_t(
            HANDLE hConsoleOutput,
            BOOL bMaximumWindow,
            AGENT_CONSOLE_FONT_INFOEX *lpConsoleCurrentFontEx);

#define GET_MODULE_PROC(mod, funcName) \
    m_##funcName = reinterpret_cast<funcName##_t*>((mod).proc(#funcName)); \

#define DEFINE_ACCESSOR(funcName) \
    funcName##_t &funcName() const { \
        ASSERT(valid()); \
        return *m_##funcName; \
    }

class XPFontAPI {
public:
    XPFontAPI() : m_kernel32(L"kernel32.dll") {
        GET_MODULE_PROC(m_kernel32, GetCurrentConsoleFont);
        GET_MODULE_PROC(m_kernel32, GetConsoleFontSize);
    }

    bool valid() const {
        return m_GetCurrentConsoleFont != NULL &&
            m_GetConsoleFontSize != NULL;
    }

    DEFINE_ACCESSOR(GetCurrentConsoleFont)
    DEFINE_ACCESSOR(GetConsoleFontSize)

private:
    OsModule m_kernel32;
    GetCurrentConsoleFont_t *m_GetCurrentConsoleFont;
    GetConsoleFontSize_t *m_GetConsoleFontSize;
};

class VistaFontAPI : public XPFontAPI {
public:
    VistaFontAPI() : m_kernel32(L"kernel32.dll") {
        GET_MODULE_PROC(m_kernel32, GetCurrentConsoleFontEx);
        GET_MODULE_PROC(m_kernel32, SetCurrentConsoleFontEx);
    }

    bool valid() const {
        return this->XPFontAPI::valid() &&
            m_GetCurrentConsoleFontEx != NULL &&
            m_SetCurrentConsoleFontEx != NULL;
    }

    DEFINE_ACCESSOR(GetCurrentConsoleFontEx)
    DEFINE_ACCESSOR(SetCurrentConsoleFontEx)

private:
    OsModule m_kernel32;
    GetCurrentConsoleFontEx_t *m_GetCurrentConsoleFontEx;
    SetCurrentConsoleFontEx_t *m_SetCurrentConsoleFontEx;
};

static std::vector<std::pair<DWORD, COORD> > readFontTable(
        XPFontAPI &api, HANDLE conout, DWORD maxCount) {
    std::vector<std::pair<DWORD, COORD> > ret;
    for (DWORD i = 0; i < maxCount; ++i) {
        COORD size = api.GetConsoleFontSize()(conout, i);
        if (size.X == 0 && size.Y == 0) {
            break;
        }
        ret.push_back(std::make_pair(i, size));
    }
    return ret;
}

static void dumpFontTable(HANDLE conout, const char *prefix) {
    const int kMaxCount = 1000;
    if (!isTracingEnabled()) {
        return;
    }
    XPFontAPI api;
    if (!api.valid()) {
        trace("dumpFontTable: cannot dump font table -- missing APIs");
        return;
    }
    std::vector<std::pair<DWORD, COORD> > table =
        readFontTable(api, conout, kMaxCount);
    std::string line;
    char tmp[128];
    size_t first = 0;
    while (first < table.size()) {
        size_t last = std::min(table.size() - 1, first + 10 - 1);
        winpty_snprintf(tmp, "%sfonts %02u-%02u:",
            prefix, static_cast<unsigned>(first), static_cast<unsigned>(last));
        line = tmp;
        for (size_t i = first; i <= last; ++i) {
            if (i % 10 == 5) {
                line += "  - ";
            }
            winpty_snprintf(tmp, " %2dx%-2d",
                table[i].second.X, table[i].second.Y);
            line += tmp;
        }
        trace("%s", line.c_str());
        first = last + 1;
    }
    if (table.size() == kMaxCount) {
        trace("%sfonts: ... stopped reading at %d fonts ...",
            prefix, kMaxCount);
    }
}

static std::string stringToCodePoints(const std::wstring &str) {
    std::string ret = "(";
    for (size_t i = 0; i < str.size(); ++i) {
        char tmp[32];
        winpty_snprintf(tmp, "%X", str[i]);
        if (ret.size() > 1) {
            ret.push_back(' ');
        }
        ret += tmp;
    }
    ret.push_back(')');
    return ret;
}

static void dumpFontInfoEx(
        const AGENT_CONSOLE_FONT_INFOEX &infoex,
        const char *prefix) {
    if (!isTracingEnabled()) {
        return;
    }
    std::wstring faceName(infoex.FaceName,
        winpty_wcsnlen(infoex.FaceName, COUNT_OF(infoex.FaceName)));
    trace("%snFont=%u dwFontSize=(%d,%d) "
        "FontFamily=0x%x FontWeight=%u FaceName=%s %s",
        prefix,
        static_cast<unsigned>(infoex.nFont),
        infoex.dwFontSize.X, infoex.dwFontSize.Y,
        infoex.FontFamily, infoex.FontWeight, utf8FromWide(faceName).c_str(),
        stringToCodePoints(faceName).c_str());
}

static void dumpVistaFont(VistaFontAPI &api, HANDLE conout, const char *prefix) {
    if (!isTracingEnabled()) {
        return;
    }
    AGENT_CONSOLE_FONT_INFOEX infoex = {0};
    infoex.cbSize = sizeof(infoex);
    if (!api.GetCurrentConsoleFontEx()(conout, FALSE, &infoex)) {
        trace("GetCurrentConsoleFontEx call failed");
        return;
    }
    dumpFontInfoEx(infoex, prefix);
}

static void dumpXPFont(XPFontAPI &api, HANDLE conout, const char *prefix) {
    if (!isTracingEnabled()) {
        return;
    }
    AGENT_CONSOLE_FONT_INFO info = {0};
    if (!api.GetCurrentConsoleFont()(conout, FALSE, &info)) {
        trace("GetCurrentConsoleFont call failed");
        return;
    }
    trace("%snFont=%u dwFontSize=(%d,%d)",
        prefix,
        static_cast<unsigned>(info.nFont),
        info.dwFontSize.X, info.dwFontSize.Y);
}

static bool setFontVista(
        VistaFontAPI &api,
        HANDLE conout,
        const Font &font) {
    AGENT_CONSOLE_FONT_INFOEX infoex = {};
    infoex.cbSize = sizeof(AGENT_CONSOLE_FONT_INFOEX);
    infoex.dwFontSize.Y = font.size;
    infoex.FontFamily = font.family;
    infoex.FontWeight = 400;
    winpty_wcsncpy_nul(infoex.FaceName, font.faceName);
    dumpFontInfoEx(infoex, "setFontVista: setting font to: ");
    if (!api.SetCurrentConsoleFontEx()(conout, FALSE, &infoex)) {
        trace("setFontVista: SetCurrentConsoleFontEx call failed");
        return false;
    }
    memset(&infoex, 0, sizeof(infoex));
    infoex.cbSize = sizeof(infoex);
    if (!api.GetCurrentConsoleFontEx()(conout, FALSE, &infoex)) {
        trace("setFontVista: GetCurrentConsoleFontEx call failed");
        return false;
    }
    if (wcsncmp(infoex.FaceName, font.faceName,
            COUNT_OF(infoex.FaceName)) != 0) {
        trace("setFontVista: face name was not set");
        dumpFontInfoEx(infoex, "setFontVista: post-call font: ");
        return false;
    }
    // We'd like to verify that the new font size is correct, but we can't
    // predict what it will be, even though we just set it to `pxSize` through
    // an apprently symmetric interface.  For the Chinese and Korean fonts, the
    // new `infoex.dwFontSize.Y` value can be slightly larger than the height
    // we specified.
    return true;
}

static Font selectSmallFont(int codePage, int columns, bool isNewW10) {
    // Iterate over a set of font sizes according to the code page, and select
    // one.

    const wchar_t *faceName = nullptr;
    unsigned int fontFamily = 0;
    const FontSize *table = nullptr;
    size_t tableSize = 0;

    switch (codePage) {
        case 932: // Japanese
            faceName = kMSGothic;
            fontFamily = 0x36;
            if (isNewW10) {
                table = k932GothicWin10;
                tableSize = COUNT_OF(k932GothicWin10);
            } else if (isAtLeastWindows8()) {
                table = k932GothicWin8;
                tableSize = COUNT_OF(k932GothicWin8);
            } else {
                table = k932GothicVista;
                tableSize = COUNT_OF(k932GothicVista);
            }
            break;
        case 936: // Chinese Simplified
            faceName = kNSimSun;
            fontFamily = 0x36;
            table = k936SimSun;
            tableSize = COUNT_OF(k936SimSun);
            break;
        case 949: // Korean
            faceName = kGulimChe;
            fontFamily = 0x36;
            table = k949GulimChe;
            tableSize = COUNT_OF(k949GulimChe);
            break;
        case 950: // Chinese Traditional
            faceName = kMingLight;
            fontFamily = 0x36;
            table = k950MingLight;
            tableSize = COUNT_OF(k950MingLight);
            break;
        default:
            faceName = kLucidaConsole;
            fontFamily = 0x36;
            table = kLucidaFontSizes;
            tableSize = COUNT_OF(kLucidaFontSizes);
            break;
    }

    size_t bestIndex = static_cast<size_t>(-1);
    std::tuple<int, int> bestScore = std::make_tuple(-1, -1);

    // We might want to pick the smallest possible font, because we don't know
    // how large the monitor is (and the monitor size can change).  We might
    // want to pick a larger font to accommodate console programs that resize
    // the console on their own, like DOS edit.com, which tends to resize the
    // console to 80 columns.

    for (size_t i = 0; i < tableSize; ++i) {
        const int width = table[i].width * columns;

        // In general, we'd like to pick a font size where cutting the number
        // of columns in half doesn't immediately violate the minimum width
        // constraint.  (e.g. To run DOS edit.com, a user might resize their
        // terminal to ~100 columns so it's big enough to show the 80 columns
        // post-resize.)  To achieve this, give priority to fonts that allow
        // this halving.  We don't want to encourage *very* large fonts,
        // though, so disable the effect as the number of columns scales from
        // 80 to 40.
        const int halfColumns = std::min(columns, std::max(40, columns / 2));
        const int halfWidth = table[i].width * halfColumns;

        std::tuple<int, int> thisScore = std::make_tuple(-1, -1);
        if (width >= 160 && halfWidth >= 160) {
            // Both sizes are good.  Prefer the smaller fonts.
            thisScore = std::make_tuple(2, -width);
        } else if (width >= 160) {
            // Prefer the smaller fonts.
            thisScore = std::make_tuple(1, -width);
        } else {
            // Otherwise, prefer the largest font in our table.
            thisScore = std::make_tuple(0, width);
        }
        if (thisScore > bestScore) {
            bestIndex = i;
            bestScore = thisScore;
        }
    }

    ASSERT(bestIndex != static_cast<size_t>(-1));
    return Font { faceName, fontFamily, table[bestIndex].size };
}

static void setSmallFontVista(VistaFontAPI &api, HANDLE conout,
                              int columns, bool isNewW10) {
    int codePage = GetConsoleOutputCP();
    const auto font = selectSmallFont(codePage, columns, isNewW10);
    if (setFontVista(api, conout, font)) {
        trace("setSmallFontVista: success");
        return;
    }
    if (codePage == 932 || codePage == 936 ||
            codePage == 949 || codePage == 950) {
        trace("setSmallFontVista: falling back to default codepage font instead");
        const auto fontFB = selectSmallFont(0, columns, isNewW10);
        if (setFontVista(api, conout, fontFB)) {
            trace("setSmallFontVista: fallback was successful");
            return;
        }
    }
    trace("setSmallFontVista: failure");
}

struct FontSizeComparator {
    bool operator()(const std::pair<DWORD, COORD> &obj1,
                    const std::pair<DWORD, COORD> &obj2) const {
        int score1 = obj1.second.X + obj1.second.Y;
        int score2 = obj2.second.X + obj2.second.Y;
        return score1 < score2;
    }
};

} // anonymous namespace

// A Windows console window can never be larger than the desktop window.  To
// maximize the possible size of the console in rows*cols, try to configure
// the console with a small font.  Unfortunately, we cannot make the font *too*
// small, because there is also a minimum window size in pixels.
void setSmallFont(HANDLE conout, int columns, bool isNewW10) {
    trace("setSmallFont: attempting to set a small font for %d columns "
        "(CP=%u OutputCP=%u)",
        columns,
        static_cast<unsigned>(GetConsoleCP()),
        static_cast<unsigned>(GetConsoleOutputCP()));
    VistaFontAPI vista;
    if (vista.valid()) {
        dumpVistaFont(vista, conout, "previous font: ");
        dumpFontTable(conout, "previous font table: ");
        setSmallFontVista(vista, conout, columns, isNewW10);
        dumpVistaFont(vista, conout, "new font: ");
        dumpFontTable(conout, "new font table: ");
        return;
    }
    trace("setSmallFont: neither Vista nor XP APIs detected -- giving up");
    dumpFontTable(conout, "font table: ");
}
