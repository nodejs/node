/*
  zip_utf-8.c -- UTF-8 support functions for libzip
  Copyright (C) 2011-2021 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <info@libzip.org>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
  3. The names of the authors may not be used to endorse or promote
     products derived from this software without specific prior
     written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "zipint.h"

#include <stdlib.h>


static const zip_uint16_t _cp437_to_unicode[256] = {
    /* 0x00 - 0x0F */
    0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, 0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,

    /* 0x10 - 0x1F */
    0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8, 0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC,

    /* 0x20 - 0x2F */
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,

    /* 0x30 - 0x3F */
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,

    /* 0x40 - 0x4F */
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,

    /* 0x50 - 0x5F */
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,

    /* 0x60 - 0x6F */
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,

    /* 0x70 - 0x7F */
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x2302,

    /* 0x80 - 0x8F */
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,

    /* 0x90 - 0x9F */
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,

    /* 0xA0 - 0xAF */
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,

    /* 0xB0 - 0xBF */
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,

    /* 0xC0 - 0xCF */
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,

    /* 0xD0 - 0xDF */
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,

    /* 0xE0 - 0xEF */
    0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,

    /* 0xF0 - 0xFF */
    0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0};

#define UTF_8_LEN_2_MASK 0xe0
#define UTF_8_LEN_2_MATCH 0xc0
#define UTF_8_LEN_3_MASK 0xf0
#define UTF_8_LEN_3_MATCH 0xe0
#define UTF_8_LEN_4_MASK 0xf8
#define UTF_8_LEN_4_MATCH 0xf0
#define UTF_8_CONTINUE_MASK 0xc0
#define UTF_8_CONTINUE_MATCH 0x80


zip_encoding_type_t
_zip_guess_encoding(zip_string_t *str, zip_encoding_type_t expected_encoding) {
    zip_encoding_type_t enc;
    const zip_uint8_t *name;
    zip_uint32_t i, j, ulen;

    if (str == NULL)
        return ZIP_ENCODING_ASCII;

    name = str->raw;

    if (str->encoding != ZIP_ENCODING_UNKNOWN)
        enc = str->encoding;
    else {
        enc = ZIP_ENCODING_ASCII;
        for (i = 0; i < str->length; i++) {
            if ((name[i] > 31 && name[i] < 128) || name[i] == '\r' || name[i] == '\n' || name[i] == '\t')
                continue;

            enc = ZIP_ENCODING_UTF8_GUESSED;
            if ((name[i] & UTF_8_LEN_2_MASK) == UTF_8_LEN_2_MATCH)
                ulen = 1;
            else if ((name[i] & UTF_8_LEN_3_MASK) == UTF_8_LEN_3_MATCH)
                ulen = 2;
            else if ((name[i] & UTF_8_LEN_4_MASK) == UTF_8_LEN_4_MATCH)
                ulen = 3;
            else {
                enc = ZIP_ENCODING_CP437;
                break;
            }

            if (i + ulen >= str->length) {
                enc = ZIP_ENCODING_CP437;
                break;
            }

            for (j = 1; j <= ulen; j++) {
                if ((name[i + j] & UTF_8_CONTINUE_MASK) != UTF_8_CONTINUE_MATCH) {
                    enc = ZIP_ENCODING_CP437;
                    goto done;
                }
            }
            i += ulen;
        }
    }

done:
    str->encoding = enc;

    if (expected_encoding != ZIP_ENCODING_UNKNOWN) {
        if (expected_encoding == ZIP_ENCODING_UTF8_KNOWN && enc == ZIP_ENCODING_UTF8_GUESSED)
            str->encoding = enc = ZIP_ENCODING_UTF8_KNOWN;

        if (expected_encoding != enc && enc != ZIP_ENCODING_ASCII)
            return ZIP_ENCODING_ERROR;
    }

    return enc;
}


static zip_uint32_t
_zip_unicode_to_utf8_len(zip_uint32_t codepoint) {
    if (codepoint < 0x0080)
        return 1;
    if (codepoint < 0x0800)
        return 2;
    if (codepoint < 0x10000)
        return 3;
    return 4;
}


static zip_uint32_t
_zip_unicode_to_utf8(zip_uint32_t codepoint, zip_uint8_t *buf) {
    if (codepoint < 0x0080) {
        buf[0] = codepoint & 0xff;
        return 1;
    }
    if (codepoint < 0x0800) {
        buf[0] = (zip_uint8_t)(UTF_8_LEN_2_MATCH | ((codepoint >> 6) & 0x1f));
        buf[1] = (zip_uint8_t)(UTF_8_CONTINUE_MATCH | (codepoint & 0x3f));
        return 2;
    }
    if (codepoint < 0x10000) {
        buf[0] = (zip_uint8_t)(UTF_8_LEN_3_MATCH | ((codepoint >> 12) & 0x0f));
        buf[1] = (zip_uint8_t)(UTF_8_CONTINUE_MATCH | ((codepoint >> 6) & 0x3f));
        buf[2] = (zip_uint8_t)(UTF_8_CONTINUE_MATCH | (codepoint & 0x3f));
        return 3;
    }
    buf[0] = (zip_uint8_t)(UTF_8_LEN_4_MATCH | ((codepoint >> 18) & 0x07));
    buf[1] = (zip_uint8_t)(UTF_8_CONTINUE_MATCH | ((codepoint >> 12) & 0x3f));
    buf[2] = (zip_uint8_t)(UTF_8_CONTINUE_MATCH | ((codepoint >> 6) & 0x3f));
    buf[3] = (zip_uint8_t)(UTF_8_CONTINUE_MATCH | (codepoint & 0x3f));
    return 4;
}


zip_uint8_t *
_zip_cp437_to_utf8(const zip_uint8_t *const _cp437buf, zip_uint32_t len, zip_uint32_t *utf8_lenp, zip_error_t *error) {
    zip_uint8_t *cp437buf = (zip_uint8_t *)_cp437buf;
    zip_uint8_t *utf8buf;
    zip_uint32_t buflen, i, offset;

    if (len == 0) {
        if (utf8_lenp)
            *utf8_lenp = 0;
        return NULL;
    }

    buflen = 1;
    for (i = 0; i < len; i++)
        buflen += _zip_unicode_to_utf8_len(_cp437_to_unicode[cp437buf[i]]);

    if ((utf8buf = (zip_uint8_t *)malloc(buflen)) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    offset = 0;
    for (i = 0; i < len; i++)
        offset += _zip_unicode_to_utf8(_cp437_to_unicode[cp437buf[i]], utf8buf + offset);

    utf8buf[buflen - 1] = 0;
    if (utf8_lenp)
        *utf8_lenp = buflen - 1;
    return utf8buf;
}
