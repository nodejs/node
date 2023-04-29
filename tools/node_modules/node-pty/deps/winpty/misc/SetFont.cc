#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "TestUtil.cc"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

// See https://en.wikipedia.org/wiki/List_of_CJK_fonts
const wchar_t kMSGothic[] = { 0xff2d, 0xff33, 0x0020, 0x30b4, 0x30b7, 0x30c3, 0x30af, 0 }; // Japanese
const wchar_t kNSimSun[] = { 0x65b0, 0x5b8b, 0x4f53, 0 }; // Simplified Chinese
const wchar_t kMingLight[] = { 0x7d30, 0x660e, 0x9ad4, 0 }; // Traditional Chinese
const wchar_t kGulimChe[] = { 0xad74, 0xb9bc, 0xccb4, 0 }; // Korean

int main() {
    setlocale(LC_ALL, "");
    wchar_t *cmdline = GetCommandLineW();
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(cmdline, &argc);
    const HANDLE conout = openConout();

    if (argc == 1) {
        cprintf(L"Usage:\n");
        cprintf(L"  SetFont <index>\n");
        cprintf(L"  SetFont options\n");
        cprintf(L"\n");
        cprintf(L"Options for SetCurrentConsoleFontEx:\n");
        cprintf(L"  -idx INDEX\n");
        cprintf(L"  -w WIDTH\n");
        cprintf(L"  -h HEIGHT\n");
        cprintf(L"  -family (0xNN|NN)\n");
        cprintf(L"  -weight (normal|bold|NNN)\n");
        cprintf(L"  -face FACENAME\n");
        cprintf(L"  -face-{gothic|simsun|minglight|gulimche) [JP,CN-sim,CN-tra,KR]\n");
        cprintf(L"  -tt\n");
        cprintf(L"  -vec\n");
        cprintf(L"  -vp\n");
        cprintf(L"  -dev\n");
        cprintf(L"  -roman\n");
        cprintf(L"  -swiss\n");
        cprintf(L"  -modern\n");
        cprintf(L"  -script\n");
        cprintf(L"  -decorative\n");
        return 0;
    }

    if (isdigit(argv[1][0])) {
        int index = _wtoi(argv[1]);
        HMODULE kernel32 = LoadLibraryW(L"kernel32.dll");
        FARPROC proc = GetProcAddress(kernel32, "SetConsoleFont");
        if (proc == NULL) {
            cprintf(L"Couldn't get address of SetConsoleFont\n");
        } else {
            BOOL ret = reinterpret_cast<BOOL WINAPI(*)(HANDLE, DWORD)>(proc)(
                    conout, index);
            cprintf(L"SetFont returned %d\n", ret);
        }
        return 0;
    }

    CONSOLE_FONT_INFOEX fontex = {0};
    fontex.cbSize = sizeof(fontex);

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (i + 1 < argc) {
            std::wstring next = argv[i + 1];
            if (arg == L"-idx") {
                fontex.nFont = _wtoi(next.c_str());
                ++i; continue;
            } else if (arg == L"-w") {
                fontex.dwFontSize.X = _wtoi(next.c_str());
                ++i; continue;
            } else if (arg == L"-h") {
                fontex.dwFontSize.Y = _wtoi(next.c_str());
                ++i; continue;
            } else if (arg == L"-weight") {
                if (next == L"normal") {
                    fontex.FontWeight = 400;
                } else if (next == L"bold") {
                    fontex.FontWeight = 700;
                } else {
                    fontex.FontWeight = _wtoi(next.c_str());
                }
                ++i; continue;
            } else if (arg == L"-face") {
                wcsncpy(fontex.FaceName, next.c_str(), COUNT_OF(fontex.FaceName));
                ++i; continue;
            } else if (arg == L"-family") {
                fontex.FontFamily = strtol(narrowString(next).c_str(), nullptr, 0);
                ++i; continue;
            }
        }
        if (arg == L"-tt") {
            fontex.FontFamily |= TMPF_TRUETYPE;
        } else if (arg == L"-vec") {
            fontex.FontFamily |= TMPF_VECTOR;
        } else if (arg == L"-vp") {
            // Setting the TMPF_FIXED_PITCH bit actually indicates variable
            // pitch.
            fontex.FontFamily |= TMPF_FIXED_PITCH;
        } else if (arg == L"-dev") {
            fontex.FontFamily |= TMPF_DEVICE;
        } else if (arg == L"-roman") {
            fontex.FontFamily = (fontex.FontFamily & ~0xF0) | FF_ROMAN;
        } else if (arg == L"-swiss") {
            fontex.FontFamily = (fontex.FontFamily & ~0xF0) | FF_SWISS;
        } else if (arg == L"-modern") {
            fontex.FontFamily = (fontex.FontFamily & ~0xF0) | FF_MODERN;
        } else if (arg == L"-script") {
            fontex.FontFamily = (fontex.FontFamily & ~0xF0) | FF_SCRIPT;
        } else if (arg == L"-decorative") {
            fontex.FontFamily = (fontex.FontFamily & ~0xF0) | FF_DECORATIVE;
        } else if (arg == L"-face-gothic") {
            wcsncpy(fontex.FaceName, kMSGothic, COUNT_OF(fontex.FaceName));
        } else if (arg == L"-face-simsun") {
            wcsncpy(fontex.FaceName, kNSimSun, COUNT_OF(fontex.FaceName));
        } else if (arg == L"-face-minglight") {
            wcsncpy(fontex.FaceName, kMingLight, COUNT_OF(fontex.FaceName));
        } else if (arg == L"-face-gulimche") {
            wcsncpy(fontex.FaceName, kGulimChe, COUNT_OF(fontex.FaceName));
        } else {
            cprintf(L"Unrecognized argument: %ls\n", arg.c_str());
            exit(1);
        }
    }

    cprintf(L"Setting to: nFont=%u dwFontSize=(%d,%d) "
        L"FontFamily=0x%x FontWeight=%u "
        L"FaceName=\"%ls\"\n",
        static_cast<unsigned>(fontex.nFont),
        fontex.dwFontSize.X, fontex.dwFontSize.Y,
        fontex.FontFamily, fontex.FontWeight,
        fontex.FaceName);

    BOOL ret = SetCurrentConsoleFontEx(
        conout,
        FALSE,
        &fontex);
    cprintf(L"SetCurrentConsoleFontEx returned %d\n", ret);

    return 0;
}
