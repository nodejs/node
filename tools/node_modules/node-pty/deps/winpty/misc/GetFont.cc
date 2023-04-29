#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>

#include "../src/shared/OsModule.h"
#include "../src/shared/StringUtil.h"

#include "TestUtil.cc"
#include "../src/shared/StringUtil.cc"

#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))

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

class UndocumentedXPFontAPI : public XPFontAPI {
public:
    UndocumentedXPFontAPI() : m_kernel32(L"kernel32.dll") {
        GET_MODULE_PROC(m_kernel32, SetConsoleFont);
        GET_MODULE_PROC(m_kernel32, GetNumberOfConsoleFonts);
    }

    bool valid() const {
        return this->XPFontAPI::valid() &&
            m_SetConsoleFont != NULL &&
            m_GetNumberOfConsoleFonts != NULL;
    }

    DEFINE_ACCESSOR(SetConsoleFont)
    DEFINE_ACCESSOR(GetNumberOfConsoleFonts)

private:
    OsModule m_kernel32;
    SetConsoleFont_t *m_SetConsoleFont;
    GetNumberOfConsoleFonts_t *m_GetNumberOfConsoleFonts;
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

static void dumpFontTable(HANDLE conout) {
    const int kMaxCount = 1000;
    XPFontAPI api;
    if (!api.valid()) {
        printf("dumpFontTable: cannot dump font table -- missing APIs\n");
        return;
    }
    std::vector<std::pair<DWORD, COORD> > table =
        readFontTable(api, conout, kMaxCount);
    std::string line;
    char tmp[128];
    size_t first = 0;
    while (first < table.size()) {
        size_t last = std::min(table.size() - 1, first + 10 - 1);
        winpty_snprintf(tmp, "%02u-%02u:",
            static_cast<unsigned>(first), static_cast<unsigned>(last));
        line = tmp;
        for (size_t i = first; i <= last; ++i) {
            if (i % 10 == 5) {
                line += "  - ";
            }
            winpty_snprintf(tmp, " %2dx%-2d",
                table[i].second.X, table[i].second.Y);
            line += tmp;
        }
        printf("%s\n", line.c_str());
        first = last + 1;
    }
    if (table.size() == kMaxCount) {
        printf("... stopped reading at %d fonts ...\n", kMaxCount);
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
        const AGENT_CONSOLE_FONT_INFOEX &infoex) {
    std::wstring faceName(infoex.FaceName,
        winpty_wcsnlen(infoex.FaceName, COUNT_OF(infoex.FaceName)));
    cprintf(L"nFont=%u dwFontSize=(%d,%d) "
        "FontFamily=0x%x FontWeight=%u FaceName=%ls %hs\n",
        static_cast<unsigned>(infoex.nFont),
        infoex.dwFontSize.X, infoex.dwFontSize.Y,
        infoex.FontFamily, infoex.FontWeight, faceName.c_str(),
        stringToCodePoints(faceName).c_str());
}

static void dumpVistaFont(VistaFontAPI &api, HANDLE conout, BOOL maxWindow) {
    AGENT_CONSOLE_FONT_INFOEX infoex = {0};
    infoex.cbSize = sizeof(infoex);
    if (!api.GetCurrentConsoleFontEx()(conout, maxWindow, &infoex)) {
        printf("GetCurrentConsoleFontEx call failed\n");
        return;
    }
    dumpFontInfoEx(infoex);
}

static void dumpXPFont(XPFontAPI &api, HANDLE conout, BOOL maxWindow) {
    AGENT_CONSOLE_FONT_INFO info = {0};
    if (!api.GetCurrentConsoleFont()(conout, maxWindow, &info)) {
        printf("GetCurrentConsoleFont call failed\n");
        return;
    }
    printf("nFont=%u dwFontSize=(%d,%d)\n",
        static_cast<unsigned>(info.nFont),
        info.dwFontSize.X, info.dwFontSize.Y);
}

static void dumpFontAndTable(HANDLE conout) {
    VistaFontAPI vista;
    if (vista.valid()) {
        printf("maxWnd=0: "); dumpVistaFont(vista, conout, FALSE);
        printf("maxWnd=1: "); dumpVistaFont(vista, conout, TRUE);
        dumpFontTable(conout);
        return;
    }
    UndocumentedXPFontAPI xp;
    if (xp.valid()) {
        printf("maxWnd=0: "); dumpXPFont(xp, conout, FALSE);
        printf("maxWnd=1: "); dumpXPFont(xp, conout, TRUE);
        dumpFontTable(conout);
        return;
    }
    printf("setSmallFont: neither Vista nor XP APIs detected -- giving up\n");
    dumpFontTable(conout);
}

int main() {
    const HANDLE conout = openConout();
    const COORD largest = GetLargestConsoleWindowSize(conout);
    printf("largestConsoleWindowSize=(%d,%d)\n", largest.X, largest.Y);
    dumpFontAndTable(conout);
    UndocumentedXPFontAPI xp;
    if (xp.valid()) {
        printf("GetNumberOfConsoleFonts returned %u\n", xp.GetNumberOfConsoleFonts()());
    } else {
        printf("The GetNumberOfConsoleFonts API was missing\n");
    }
    printf("CP=%u OutputCP=%u\n", GetConsoleCP(), GetConsoleOutputCP());
    return 0;
}
