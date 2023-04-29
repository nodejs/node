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

#ifndef UNIX_CTRL_CHARS_H
#define UNIX_CTRL_CHARS_H

inline char decodeUnixCtrlChar(char ch) {
    const char ctrlKeys[] = {
        /* 0x00 */ '@', /* 0x01 */ 'A', /* 0x02 */ 'B', /* 0x03 */ 'C',
        /* 0x04 */ 'D', /* 0x05 */ 'E', /* 0x06 */ 'F', /* 0x07 */ 'G',
        /* 0x08 */ 'H', /* 0x09 */ 'I', /* 0x0A */ 'J', /* 0x0B */ 'K',
        /* 0x0C */ 'L', /* 0x0D */ 'M', /* 0x0E */ 'N', /* 0x0F */ 'O',
        /* 0x10 */ 'P', /* 0x11 */ 'Q', /* 0x12 */ 'R', /* 0x13 */ 'S',
        /* 0x14 */ 'T', /* 0x15 */ 'U', /* 0x16 */ 'V', /* 0x17 */ 'W',
        /* 0x18 */ 'X', /* 0x19 */ 'Y', /* 0x1A */ 'Z', /* 0x1B */ '[',
        /* 0x1C */ '\\', /* 0x1D */ ']', /* 0x1E */ '^', /* 0x1F */ '_',
    };
    unsigned char uch = ch;
    if (uch < 32) {
        return ctrlKeys[uch];
    } else if (uch == 127) {
        return '?';
    } else {
        return '\0';
    }
}

#endif // UNIX_CTRL_CHARS_H
