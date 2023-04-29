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

#ifndef UNICODE_ENCODING_H
#define UNICODE_ENCODING_H

#include <stdint.h>

// Encode the Unicode codepoint with UTF-8.  The buffer must be at least 4
// bytes in size.
static inline int encodeUtf8(char *out, uint32_t code) {
    if (code < 0x80) {
        out[0] = code;
        return 1;
    } else if (code < 0x800) {
        out[0] = ((code >> 6) & 0x1F) | 0xC0;
        out[1] = ((code >> 0) & 0x3F) | 0x80;
        return 2;
    } else if (code < 0x10000) {
        if (code >= 0xD800 && code <= 0xDFFF) {
            // The code points 0xD800 to 0xDFFF are reserved for UTF-16
            // surrogate pairs and do not have an encoding in UTF-8.
            return 0;
        }
        out[0] = ((code >> 12) & 0x0F) | 0xE0;
        out[1] = ((code >>  6) & 0x3F) | 0x80;
        out[2] = ((code >>  0) & 0x3F) | 0x80;
        return 3;
    } else if (code < 0x110000) {
        out[0] = ((code >> 18) & 0x07) | 0xF0;
        out[1] = ((code >> 12) & 0x3F) | 0x80;
        out[2] = ((code >>  6) & 0x3F) | 0x80;
        out[3] = ((code >>  0) & 0x3F) | 0x80;
        return 4;
    } else {
        // Encoding error
        return 0;
    }
}

// Encode the Unicode codepoint with UTF-16.  The buffer must be large enough
// to hold the output -- either 1 or 2 elements.
static inline int encodeUtf16(wchar_t *out, uint32_t code) {
    if (code < 0x10000) {
        if (code >= 0xD800 && code <= 0xDFFF) {
            // The code points 0xD800 to 0xDFFF are reserved for UTF-16
            // surrogate pairs and do not have an encoding in UTF-16.
            return 0;
        }
        out[0] = code;
        return 1;
    } else if (code < 0x110000) {
        code -= 0x10000;
        out[0] = 0xD800 | (code >> 10);
        out[1] = 0xDC00 | (code & 0x3FF);
        return 2;
    } else {
        // Encoding error
        return 0;
    }
}

// Return the byte size of a UTF-8 character using the value of the first
// byte.
static inline int utf8CharLength(char firstByte) {
    // This code would probably be faster if it used __builtin_clz.
    if ((firstByte & 0x80) == 0) {
        return 1;
    } else if ((firstByte & 0xE0) == 0xC0) {
        return 2;
    } else if ((firstByte & 0xF0) == 0xE0) {
        return 3;
    } else if ((firstByte & 0xF8) == 0xF0) {
        return 4;
    } else {
        // Malformed UTF-8.
        return 0;
    }
}

// The pointer must point to 1-4 bytes, as indicated by the first byte.
// Returns -1 on decoding error.
static inline uint32_t decodeUtf8(const char *in) {
    const uint32_t kInvalid = static_cast<uint32_t>(-1);
    switch (utf8CharLength(in[0])) {
        case 1: {
            return in[0];
        }
        case 2: {
            if ((in[1] & 0xC0) != 0x80) {
                return kInvalid;
            }
            uint32_t tmp = 0;
            tmp = (in[0] & 0x1F) << 6;
            tmp |= (in[1] & 0x3F);
            return tmp <= 0x7F ? kInvalid : tmp;
        }
        case 3: {
            if ((in[1] & 0xC0) != 0x80 ||
                    (in[2] & 0xC0) != 0x80) {
                return kInvalid;
            }
            uint32_t tmp = 0;
            tmp = (in[0] & 0x0F) << 12;
            tmp |= (in[1] & 0x3F) << 6;
            tmp |= (in[2] & 0x3F);
            if (tmp <= 0x07FF || (tmp >= 0xD800 && tmp <= 0xDFFF)) {
                return kInvalid;
            } else {
                return tmp;
            }
        }
        case 4: {
            if ((in[1] & 0xC0) != 0x80 ||
                    (in[2] & 0xC0) != 0x80 ||
                    (in[3] & 0xC0) != 0x80) {
                return kInvalid;
            }
            uint32_t tmp = 0;
            tmp = (in[0] & 0x07) << 18;
            tmp |= (in[1] & 0x3F) << 12;
            tmp |= (in[2] & 0x3F) << 6;
            tmp |= (in[3] & 0x3F);
            if (tmp <= 0xFFFF || tmp > 0x10FFFF) {
                return kInvalid;
            } else {
                return tmp;
            }
        }
        default: {
            return kInvalid;
        }
    }
}

static inline uint32_t decodeSurrogatePair(wchar_t ch1, wchar_t ch2) {
    return ((ch1 - 0xD800) << 10) + (ch2 - 0xDC00) + 0x10000;
}

#endif // UNICODE_ENCODING_H
