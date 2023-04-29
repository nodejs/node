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

// Encode every code-point using this module and verify that it matches the
// encoding generated using Windows WideCharToMultiByte.

#include "UnicodeEncoding.h"

#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void correctnessByCode()
{
    char mbstr1[4];
    char mbstr2[4];
    wchar_t wch[2];
    for (unsigned int code = 0; code < 0x110000; ++code) {

        // Surrogate pair reserved region.
        const bool isReserved = (code >= 0xD800 && code <= 0xDFFF);

        int mblen1 = encodeUtf8(mbstr1, code);
        if (isReserved ? mblen1 != 0 : mblen1 <= 0) {
            printf("Error: 0x%04X: mblen1=%d\n", code, mblen1);
            continue;
        }

        int wlen = encodeUtf16(wch, code);
        if (isReserved ? wlen != 0 : wlen <= 0) {
            printf("Error: 0x%04X: wlen=%d\n", code, wlen);
            continue;
        }

        if (isReserved) {
            continue;
        }

        if (mblen1 != utf8CharLength(mbstr1[0])) {
            printf("Error: 0x%04X: mblen1=%d, utf8CharLength(mbstr1[0])=%d\n",
                code, mblen1, utf8CharLength(mbstr1[0]));
            continue;
        }

        if (code != decodeUtf8(mbstr1)) {
            printf("Error: 0x%04X: decodeUtf8(mbstr1)=%u\n",
                code, decodeUtf8(mbstr1));
            continue;
        }

        int mblen2 = WideCharToMultiByte(CP_UTF8, 0, wch, wlen, mbstr2, 4, NULL, NULL);
        if (mblen1 != mblen2) {
            printf("Error: 0x%04X: mblen1=%d, mblen2=%d\n", code, mblen1, mblen2);
            continue;
        }

        if (memcmp(mbstr1, mbstr2, mblen1) != 0) {
            printf("Error: 0x%04x: encodings are different\n", code);
            continue;
        }
    }
}

static const char *encodingStr(char (&output)[128], char (&buf)[4])
{
    sprintf(output, "Encoding %02X %02X %02X %02X",
        static_cast<uint8_t>(buf[0]),
        static_cast<uint8_t>(buf[1]),
        static_cast<uint8_t>(buf[2]),
        static_cast<uint8_t>(buf[3]));
    return output;
}

// This test can take a couple of minutes to run.
static void correctnessByUtf8Encoding()
{
    for (uint64_t encoding = 0; encoding <= 0xFFFFFFFF; ++encoding) {

        char mb[4];
        mb[0] = encoding;
        mb[1] = encoding >> 8;
        mb[2] = encoding >> 16;
        mb[3] = encoding >> 24;

        const int mblen = utf8CharLength(mb[0]);
        if (mblen == 0) {
            continue;
        }

        // Test this module.
        const uint32_t code1 = decodeUtf8(mb);
        wchar_t ws1[2] = {};
        const int wslen1 = encodeUtf16(ws1, code1);

        // Test using Windows.  We can't decode a codepoint directly; we have
        // to do UTF8->UTF16, then decode the surrogate pair.
        wchar_t ws2[2] = {};
        const int wslen2 = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, mb, mblen, ws2, 2);
        const uint32_t code2 =
            (wslen2 == 1 ? ws2[0] :
             wslen2 == 2 ? decodeSurrogatePair(ws2[0], ws2[1]) :
             static_cast<uint32_t>(-1));

        // Verify that the two implementations match.
        char prefix[128];
        if (code1 != code2) {
            printf("%s: code1=0x%04x code2=0x%04x\n",
                encodingStr(prefix, mb),
                code1, code2);
            continue;
        }
        if (wslen1 != wslen2) {
            printf("%s: wslen1=%d wslen2=%d\n",
                encodingStr(prefix, mb),
                wslen1, wslen2);
            continue;
        }
        if (memcmp(ws1, ws2, wslen1 * sizeof(wchar_t)) != 0) {
            printf("%s: ws1 != ws2\n", encodingStr(prefix, mb));
            continue;
        }
    }
}

wchar_t g_wch_TEST[] = { 0xD840, 0xDC00 };
char g_ch_TEST[4];
wchar_t *volatile g_pwch = g_wch_TEST;
char *volatile g_pch = g_ch_TEST;
unsigned int volatile g_code = 0xA2000;

static void performance()
{
    {
        clock_t start = clock();
        for (long long i = 0; i < 250000000LL; ++i) {
            int mblen = WideCharToMultiByte(CP_UTF8, 0, g_pwch, 2, g_pch, 4, NULL, NULL);
            assert(mblen == 4);
        }
        clock_t stop = clock();
        printf("%.3fns per char\n", (double)(stop - start) / CLOCKS_PER_SEC * 4.0);
    }

    {
        clock_t start = clock();
        for (long long i = 0; i < 3000000000LL; ++i) {
            int mblen = encodeUtf8(g_pch, g_code);
            assert(mblen == 4);
        }
        clock_t stop = clock();
        printf("%.3fns per char\n", (double)(stop - start) / CLOCKS_PER_SEC / 3.0);
    }
}

int main()
{
    printf("Testing correctnessByCode...\n");
    fflush(stdout);
    correctnessByCode();

    printf("Testing correctnessByUtf8Encoding... (may take a couple minutes)\n");
    fflush(stdout);
    correctnessByUtf8Encoding();

    printf("Testing performance...\n");
    fflush(stdout);
    performance();

    return 0;
}
