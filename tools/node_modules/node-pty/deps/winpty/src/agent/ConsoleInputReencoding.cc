// Copyright (c) 2016 Ryan Prichard
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

#include "ConsoleInputReencoding.h"

#include "ConsoleInput.h"

namespace {

static void outch(std::vector<INPUT_RECORD> &out, wchar_t ch) {
    ConsoleInput::appendInputRecord(out, TRUE, 0, ch, 0);
}

} // anonymous namespace

void reencodeEscapedKeyPress(
        std::vector<INPUT_RECORD> &out,
        uint16_t virtualKey,
        uint32_t codePoint,
        uint16_t keyState) {

    struct EscapedKey {
        enum { None, Numeric, Letter } kind;
        wchar_t content[2];
    };

    EscapedKey escapeCode = {};
    switch (virtualKey) {
        case VK_UP:     escapeCode = { EscapedKey::Letter, {'A'} }; break;
        case VK_DOWN:   escapeCode = { EscapedKey::Letter, {'B'} }; break;
        case VK_RIGHT:  escapeCode = { EscapedKey::Letter, {'C'} }; break;
        case VK_LEFT:   escapeCode = { EscapedKey::Letter, {'D'} }; break;
        case VK_CLEAR:  escapeCode = { EscapedKey::Letter, {'E'} }; break;
        case VK_F1:     escapeCode = { EscapedKey::Numeric, {'1', '1'} }; break;
        case VK_F2:     escapeCode = { EscapedKey::Numeric, {'1', '2'} }; break;
        case VK_F3:     escapeCode = { EscapedKey::Numeric, {'1', '3'} }; break;
        case VK_F4:     escapeCode = { EscapedKey::Numeric, {'1', '4'} }; break;
        case VK_F5:     escapeCode = { EscapedKey::Numeric, {'1', '5'} }; break;
        case VK_F6:     escapeCode = { EscapedKey::Numeric, {'1', '7'} }; break;
        case VK_F7:     escapeCode = { EscapedKey::Numeric, {'1', '8'} }; break;
        case VK_F8:     escapeCode = { EscapedKey::Numeric, {'1', '9'} }; break;
        case VK_F9:     escapeCode = { EscapedKey::Numeric, {'2', '0'} }; break;
        case VK_F10:    escapeCode = { EscapedKey::Numeric, {'2', '1'} }; break;
        case VK_F11:    escapeCode = { EscapedKey::Numeric, {'2', '3'} }; break;
        case VK_F12:    escapeCode = { EscapedKey::Numeric, {'2', '4'} }; break;
        case VK_HOME:   escapeCode = { EscapedKey::Letter, {'H'} }; break;
        case VK_INSERT: escapeCode = { EscapedKey::Numeric, {'2'} }; break;
        case VK_DELETE: escapeCode = { EscapedKey::Numeric, {'3'} }; break;
        case VK_END:    escapeCode = { EscapedKey::Letter, {'F'} }; break;
        case VK_PRIOR:  escapeCode = { EscapedKey::Numeric, {'5'} }; break;
        case VK_NEXT:   escapeCode = { EscapedKey::Numeric, {'6'} }; break;
    }
    if (escapeCode.kind != EscapedKey::None) {
        int flags = 0;
        if (keyState & SHIFT_PRESSED)       { flags |= 0x1; }
        if (keyState & LEFT_ALT_PRESSED)    { flags |= 0x2; }
        if (keyState & LEFT_CTRL_PRESSED)   { flags |= 0x4; }
        outch(out, L'\x1b');
        outch(out, L'[');
        if (escapeCode.kind == EscapedKey::Numeric) {
            for (wchar_t ch : escapeCode.content) {
                if (ch != L'\0') {
                    outch(out, ch);
                }
            }
        } else if (flags != 0) {
            outch(out, L'1');
        }
        if (flags != 0) {
            outch(out, L';');
            outch(out, L'1' + flags);
        }
        if (escapeCode.kind == EscapedKey::Numeric) {
            outch(out, L'~');
        } else {
            outch(out, escapeCode.content[0]);
        }
        return;
    }

    switch (virtualKey) {
        case VK_BACK:
            if (keyState & LEFT_ALT_PRESSED) {
                outch(out, L'\x1b');
            }
            outch(out, L'\x7f');
            return;
        case VK_TAB:
            if (keyState & SHIFT_PRESSED) {
                outch(out, L'\x1b');
                outch(out, L'[');
                outch(out, L'Z');
                return;
            }
            break;
    }

    if (codePoint != 0) {
        if (keyState & LEFT_ALT_PRESSED) {
            outch(out, L'\x1b');
        }
        ConsoleInput::appendCPInputRecords(out, TRUE, 0, codePoint, 0);
    }
}
