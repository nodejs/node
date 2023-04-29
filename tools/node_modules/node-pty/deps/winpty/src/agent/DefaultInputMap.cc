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

#include "DefaultInputMap.h"

#include <windows.h>
#include <string.h>

#include <algorithm>

#include "../shared/StringBuilder.h"
#include "../shared/WinptyAssert.h"
#include "InputMap.h"

#define ESC "\x1B"
#define DIM(x) (sizeof(x) / sizeof((x)[0]))

namespace {

struct EscapeEncoding {
    bool alt_prefix_allowed;
    char prefix;
    char id;
    int modifiers;
    InputMap::Key key;
};

// Modifiers.  A "modifier" is an integer from 2 to 8 that conveys the status
// of Shift(1), Alt(2), and Ctrl(4).  The value is constructed by OR'ing the
// appropriate value for each active modifier, then adding 1.
//
// Details:
//  - kBare:    expands to: ESC <prefix> <suffix>
//  - kSemiMod: expands to: ESC <prefix> <numid> ; <mod> <suffix>
//  - kBareMod: expands to: ESC <prefix> <mod> <suffix>
const int kBare        = 0x01;
const int kSemiMod     = 0x02;
const int kBareMod     = 0x04;

// Numeric escape sequences suffixes:
//  - with no flag: accept: ~
//  - kSuffixCtrl:  accept: ~ ^
//  - kSuffixShift: accept: ~ $
//  - kSuffixBoth:  accept: ~ ^ $ @
const int kSuffixCtrl  = 0x08;
const int kSuffixShift = 0x10;
const int kSuffixBoth  = kSuffixCtrl | kSuffixShift;

static const EscapeEncoding escapeLetterEncodings[] = {
    // Conventional arrow keys
    // kBareMod: Ubuntu /etc/inputrc and IntelliJ/JediTerm use escapes like: ESC [ n ABCD
    { true,  '[', 'A', kBare | kBareMod | kSemiMod, { VK_UP,      '\0', 0             } },
    { true,  '[', 'B', kBare | kBareMod | kSemiMod, { VK_DOWN,    '\0', 0             } },
    { true,  '[', 'C', kBare | kBareMod | kSemiMod, { VK_RIGHT,   '\0', 0             } },
    { true,  '[', 'D', kBare | kBareMod | kSemiMod, { VK_LEFT,    '\0', 0             } },

    // putty.  putty uses this sequence for Ctrl-Arrow, Shift-Arrow, and
    // Ctrl-Shift-Arrow, but I can only decode to one choice, so I'm just
    // leaving the modifier off altogether.
    { true,  'O', 'A', kBare,                       { VK_UP,      '\0', 0             } },
    { true,  'O', 'B', kBare,                       { VK_DOWN,    '\0', 0             } },
    { true,  'O', 'C', kBare,                       { VK_RIGHT,   '\0', 0             } },
    { true,  'O', 'D', kBare,                       { VK_LEFT,    '\0', 0             } },

    // rxvt, rxvt-unicode
    // Shift-Ctrl-Arrow can't be identified.  It's the same as Shift-Arrow.
    { true,  '[', 'a', kBare,                       { VK_UP,      '\0', SHIFT_PRESSED       } },
    { true,  '[', 'b', kBare,                       { VK_DOWN,    '\0', SHIFT_PRESSED       } },
    { true,  '[', 'c', kBare,                       { VK_RIGHT,   '\0', SHIFT_PRESSED       } },
    { true,  '[', 'd', kBare,                       { VK_LEFT,    '\0', SHIFT_PRESSED       } },
    { true,  'O', 'a', kBare,                       { VK_UP,      '\0', LEFT_CTRL_PRESSED   } },
    { true,  'O', 'b', kBare,                       { VK_DOWN,    '\0', LEFT_CTRL_PRESSED   } },
    { true,  'O', 'c', kBare,                       { VK_RIGHT,   '\0', LEFT_CTRL_PRESSED   } },
    { true,  'O', 'd', kBare,                       { VK_LEFT,    '\0', LEFT_CTRL_PRESSED   } },

    // Numpad 5 with NumLock off
    //  * xterm, mintty, and gnome-terminal use `ESC [ E`.
    //  * putty, TERM=cygwin, TERM=linux all use `ESC [ G` for 5
    //  * putty uses `ESC O G` for Ctrl-5 and Shift-5.  Omit the modifier
    //    as with putty's arrow keys.
    //  * I never saw modifiers inserted into these escapes, but I think
    //    it should be completely OK with the CSI escapes.
    { true,  '[', 'E', kBare | kSemiMod,            { VK_CLEAR,   '\0', 0             } },
    { true,  '[', 'G', kBare | kSemiMod,            { VK_CLEAR,   '\0', 0             } },
    { true,  'O', 'G', kBare,                       { VK_CLEAR,   '\0', 0             } },

    // Home/End, letter version
    //  * gnome-terminal uses `ESC O [HF]`.  I never saw it modified.
    // kBareMod: IntelliJ/JediTerm uses escapes like: ESC [ n HF
    { true,  '[', 'H', kBare | kBareMod | kSemiMod, { VK_HOME,    '\0', 0             } },
    { true,  '[', 'F', kBare | kBareMod | kSemiMod, { VK_END,     '\0', 0             } },
    { true,  'O', 'H', kBare,                       { VK_HOME,    '\0', 0             } },
    { true,  'O', 'F', kBare,                       { VK_END,     '\0', 0             } },

    // F1-F4, letter version (xterm, VTE, konsole)
    { true,  '[', 'P', kSemiMod,                    { VK_F1,      '\0', 0             } },
    { true,  '[', 'Q', kSemiMod,                    { VK_F2,      '\0', 0             } },
    { true,  '[', 'R', kSemiMod,                    { VK_F3,      '\0', 0             } },
    { true,  '[', 'S', kSemiMod,                    { VK_F4,      '\0', 0             } },

    // GNOME VTE and Konsole have special encodings for modified F1-F4:
    //  * [VTE]     ESC O 1 ; n [PQRS]
    //  * [Konsole] ESC O     n [PQRS]
    { false, 'O', 'P', kBare | kBareMod | kSemiMod, { VK_F1,      '\0', 0             } },
    { false, 'O', 'Q', kBare | kBareMod | kSemiMod, { VK_F2,      '\0', 0             } },
    { false, 'O', 'R', kBare | kBareMod | kSemiMod, { VK_F3,      '\0', 0             } },
    { false, 'O', 'S', kBare | kBareMod | kSemiMod, { VK_F4,      '\0', 0             } },

    // Handle the "application numpad" escape sequences.
    //
    // Terminals output these codes under various circumstances:
    //  * rxvt-unicode: numpad, hold down SHIFT
    //  * rxvt: numpad, by default
    //  * xterm: numpad, after enabling app-mode using DECPAM (`ESC =`).  xterm
    //    generates `ESC O <mod> <letter>` for modified numpad presses,
    //    necessitating kBareMod.
    //  * mintty: by combining Ctrl with various keys such as '1' or ','.
    //    Handling those keys is difficult, because mintty is generating the
    //    same sequence for Ctrl-1 and Ctrl-NumPadEnd -- should the virtualKey
    //    be '1' or VK_HOME?

    { true,  'O', 'M', kBare | kBareMod,            { VK_RETURN,   '\r',   0 } },
    { true,  'O', 'j', kBare | kBareMod,            { VK_MULTIPLY, '*',    0 } },
    { true,  'O', 'k', kBare | kBareMod,            { VK_ADD,      '+',    0 } },
    { true,  'O', 'm', kBare | kBareMod,            { VK_SUBTRACT, '-',    0 } },
    { true,  'O', 'n', kBare | kBareMod,            { VK_DELETE,   '\0',   0 } },
    { true,  'O', 'o', kBare | kBareMod,            { VK_DIVIDE,   '/',    0 } },
    { true,  'O', 'p', kBare | kBareMod,            { VK_INSERT,   '\0',   0 } },
    { true,  'O', 'q', kBare | kBareMod,            { VK_END,      '\0',   0 } },
    { true,  'O', 'r', kBare | kBareMod,            { VK_DOWN,     '\0',   0 } },
    { true,  'O', 's', kBare | kBareMod,            { VK_NEXT,     '\0',   0 } },
    { true,  'O', 't', kBare | kBareMod,            { VK_LEFT,     '\0',   0 } },
    { true,  'O', 'u', kBare | kBareMod,            { VK_CLEAR,    '\0',   0 } },
    { true,  'O', 'v', kBare | kBareMod,            { VK_RIGHT,    '\0',   0 } },
    { true,  'O', 'w', kBare | kBareMod,            { VK_HOME,     '\0',   0 } },
    { true,  'O', 'x', kBare | kBareMod,            { VK_UP,       '\0',   0 } },
    { true,  'O', 'y', kBare | kBareMod,            { VK_PRIOR,    '\0',   0 } },

    { true,  '[', 'M', kBare | kSemiMod,            { VK_RETURN,   '\r',   0 } },
    { true,  '[', 'j', kBare | kSemiMod,            { VK_MULTIPLY, '*',    0 } },
    { true,  '[', 'k', kBare | kSemiMod,            { VK_ADD,      '+',    0 } },
    { true,  '[', 'm', kBare | kSemiMod,            { VK_SUBTRACT, '-',    0 } },
    { true,  '[', 'n', kBare | kSemiMod,            { VK_DELETE,   '\0',   0 } },
    { true,  '[', 'o', kBare | kSemiMod,            { VK_DIVIDE,   '/',    0 } },
    { true,  '[', 'p', kBare | kSemiMod,            { VK_INSERT,   '\0',   0 } },
    { true,  '[', 'q', kBare | kSemiMod,            { VK_END,      '\0',   0 } },
    { true,  '[', 'r', kBare | kSemiMod,            { VK_DOWN,     '\0',   0 } },
    { true,  '[', 's', kBare | kSemiMod,            { VK_NEXT,     '\0',   0 } },
    { true,  '[', 't', kBare | kSemiMod,            { VK_LEFT,     '\0',   0 } },
    { true,  '[', 'u', kBare | kSemiMod,            { VK_CLEAR,    '\0',   0 } },
    { true,  '[', 'v', kBare | kSemiMod,            { VK_RIGHT,    '\0',   0 } },
    { true,  '[', 'w', kBare | kSemiMod,            { VK_HOME,     '\0',   0 } },
    { true,  '[', 'x', kBare | kSemiMod,            { VK_UP,       '\0',   0 } },
    { true,  '[', 'y', kBare | kSemiMod,            { VK_PRIOR,    '\0',   0 } },

    { false, '[', 'Z', kBare,                       { VK_TAB,     '\t', SHIFT_PRESSED } },
};

static const EscapeEncoding escapeNumericEncodings[] = {
    { true,  '[',  1,  kBare | kSemiMod | kSuffixBoth,      { VK_HOME,    '\0', 0             } },
    { true,  '[',  2,  kBare | kSemiMod | kSuffixBoth,      { VK_INSERT,  '\0', 0             } },
    { true,  '[',  3,  kBare | kSemiMod | kSuffixBoth,      { VK_DELETE,  '\0', 0             } },
    { true,  '[',  4,  kBare | kSemiMod | kSuffixBoth,      { VK_END,     '\0', 0             } },
    { true,  '[',  5,  kBare | kSemiMod | kSuffixBoth,      { VK_PRIOR,   '\0', 0             } },
    { true,  '[',  6,  kBare | kSemiMod | kSuffixBoth,      { VK_NEXT,    '\0', 0             } },
    { true,  '[',  7,  kBare | kSemiMod | kSuffixBoth,      { VK_HOME,    '\0', 0             } },
    { true,  '[',  8,  kBare | kSemiMod | kSuffixBoth,      { VK_END,     '\0', 0             } },
    { true,  '[', 11,  kBare | kSemiMod | kSuffixBoth,      { VK_F1,      '\0', 0             } },
    { true,  '[', 12,  kBare | kSemiMod | kSuffixBoth,      { VK_F2,      '\0', 0             } },
    { true,  '[', 13,  kBare | kSemiMod | kSuffixBoth,      { VK_F3,      '\0', 0             } },
    { true,  '[', 14,  kBare | kSemiMod | kSuffixBoth,      { VK_F4,      '\0', 0             } },
    { true,  '[', 15,  kBare | kSemiMod | kSuffixBoth,      { VK_F5,      '\0', 0             } },
    { true,  '[', 17,  kBare | kSemiMod | kSuffixBoth,      { VK_F6,      '\0', 0             } },
    { true,  '[', 18,  kBare | kSemiMod | kSuffixBoth,      { VK_F7,      '\0', 0             } },
    { true,  '[', 19,  kBare | kSemiMod | kSuffixBoth,      { VK_F8,      '\0', 0             } },
    { true,  '[', 20,  kBare | kSemiMod | kSuffixBoth,      { VK_F9,      '\0', 0             } },
    { true,  '[', 21,  kBare | kSemiMod | kSuffixBoth,      { VK_F10,     '\0', 0             } },
    { true,  '[', 23,  kBare | kSemiMod | kSuffixBoth,      { VK_F11,     '\0', 0             } },
    { true,  '[', 24,  kBare | kSemiMod | kSuffixBoth,      { VK_F12,     '\0', 0             } },
    { true,  '[', 25,  kBare | kSemiMod | kSuffixBoth,      { VK_F3,      '\0', SHIFT_PRESSED } },
    { true,  '[', 26,  kBare | kSemiMod | kSuffixBoth,      { VK_F4,      '\0', SHIFT_PRESSED } },
    { true,  '[', 28,  kBare | kSemiMod | kSuffixBoth,      { VK_F5,      '\0', SHIFT_PRESSED } },
    { true,  '[', 29,  kBare | kSemiMod | kSuffixBoth,      { VK_F6,      '\0', SHIFT_PRESSED } },
    { true,  '[', 31,  kBare | kSemiMod | kSuffixBoth,      { VK_F7,      '\0', SHIFT_PRESSED } },
    { true,  '[', 32,  kBare | kSemiMod | kSuffixBoth,      { VK_F8,      '\0', SHIFT_PRESSED } },
    { true,  '[', 33,  kBare | kSemiMod | kSuffixBoth,      { VK_F9,      '\0', SHIFT_PRESSED } },
    { true,  '[', 34,  kBare | kSemiMod | kSuffixBoth,      { VK_F10,     '\0', SHIFT_PRESSED } },
};

const int kCsiShiftModifier = 1;
const int kCsiAltModifier   = 2;
const int kCsiCtrlModifier  = 4;

static inline bool useEnhancedForVirtualKey(uint16_t vk) {
    switch (vk) {
        case VK_UP:
        case VK_DOWN:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
            return true;
        default:
            return false;
    }
}

static void addSimpleEntries(InputMap &inputMap) {
    struct SimpleEncoding {
        const char *encoding;
        InputMap::Key key;
    };

    static const SimpleEncoding simpleEncodings[] = {
        // Ctrl-<letter/digit> seems to be handled OK by the default code path.

        {   "\x7F",         { VK_BACK,  '\x08', 0,                                  } },
        {   ESC "\x7F",     { VK_BACK,  '\x08', LEFT_ALT_PRESSED,                   } },
        {   "\x03",         { 'C',      '\x03', LEFT_CTRL_PRESSED,                  } },

        // Handle special F1-F5 for TERM=linux and TERM=cygwin.
        {   ESC "[[A",      { VK_F1,    '\0',   0                                   } },
        {   ESC "[[B",      { VK_F2,    '\0',   0                                   } },
        {   ESC "[[C",      { VK_F3,    '\0',   0                                   } },
        {   ESC "[[D",      { VK_F4,    '\0',   0                                   } },
        {   ESC "[[E",      { VK_F5,    '\0',   0                                   } },

        {   ESC ESC "[[A",  { VK_F1,    '\0',   LEFT_ALT_PRESSED                    } },
        {   ESC ESC "[[B",  { VK_F2,    '\0',   LEFT_ALT_PRESSED                    } },
        {   ESC ESC "[[C",  { VK_F3,    '\0',   LEFT_ALT_PRESSED                    } },
        {   ESC ESC "[[D",  { VK_F4,    '\0',   LEFT_ALT_PRESSED                    } },
        {   ESC ESC "[[E",  { VK_F5,    '\0',   LEFT_ALT_PRESSED                    } },
    };

    for (size_t i = 0; i < DIM(simpleEncodings); ++i) {
        auto k = simpleEncodings[i].key;
        if (useEnhancedForVirtualKey(k.virtualKey)) {
            k.keyState |= ENHANCED_KEY;
        }
        inputMap.set(simpleEncodings[i].encoding,
                     strlen(simpleEncodings[i].encoding),
                     k);
    }
}

struct ExpandContext {
    InputMap &inputMap;
    const EscapeEncoding &e;
    char *buffer;
    char *bufferEnd;
};

static inline void setEncoding(const ExpandContext &ctx, char *end,
                               uint16_t extraKeyState) {
    InputMap::Key k = ctx.e.key;
    k.keyState |= extraKeyState;
    if (k.keyState & LEFT_CTRL_PRESSED) {
        switch (k.virtualKey) {
            case VK_ADD:
            case VK_DIVIDE:
            case VK_MULTIPLY:
            case VK_SUBTRACT:
                k.unicodeChar = '\0';
                break;
            case VK_RETURN:
                k.unicodeChar = '\n';
                break;
        }
    }
    if (useEnhancedForVirtualKey(k.virtualKey)) {
        k.keyState |= ENHANCED_KEY;
    }
    ctx.inputMap.set(ctx.buffer, end - ctx.buffer, k);
}

static inline uint16_t keyStateForMod(int mod) {
    int ret = 0;
    if ((mod - 1) & kCsiShiftModifier)  ret |= SHIFT_PRESSED;
    if ((mod - 1) & kCsiAltModifier)    ret |= LEFT_ALT_PRESSED;
    if ((mod - 1) & kCsiCtrlModifier)   ret |= LEFT_CTRL_PRESSED;
    return ret;
}

static void expandNumericEncodingSuffix(const ExpandContext &ctx, char *p,
        uint16_t extraKeyState) {
    ASSERT(p <= ctx.bufferEnd - 1);
    {
        char *q = p;
        *q++ = '~';
        setEncoding(ctx, q, extraKeyState);
    }
    if (ctx.e.modifiers & kSuffixShift) {
        char *q = p;
        *q++ = '$';
        setEncoding(ctx, q, extraKeyState | SHIFT_PRESSED);
    }
    if (ctx.e.modifiers & kSuffixCtrl) {
        char *q = p;
        *q++ = '^';
        setEncoding(ctx, q, extraKeyState | LEFT_CTRL_PRESSED);
    }
    if (ctx.e.modifiers & (kSuffixCtrl | kSuffixShift)) {
        char *q = p;
        *q++ = '@';
        setEncoding(ctx, q, extraKeyState | SHIFT_PRESSED | LEFT_CTRL_PRESSED);
    }
}

template <bool is_numeric>
static inline void expandEncodingAfterAltPrefix(
        const ExpandContext &ctx, char *p, uint16_t extraKeyState) {
    auto appendId = [&](char *&ptr) {
        const auto idstr = decOfInt(ctx.e.id);
        ASSERT(ptr <= ctx.bufferEnd - idstr.size());
        std::copy(idstr.data(), idstr.data() + idstr.size(), ptr);
        ptr += idstr.size();
    };
    ASSERT(p <= ctx.bufferEnd - 2);
    *p++ = '\x1b';
    *p++ = ctx.e.prefix;
    if (ctx.e.modifiers & kBare) {
        char *q = p;
        if (is_numeric) {
            appendId(q);
            expandNumericEncodingSuffix(ctx, q, extraKeyState);
        } else {
            ASSERT(q <= ctx.bufferEnd - 1);
            *q++ = ctx.e.id;
            setEncoding(ctx, q, extraKeyState);
        }
    }
    if (ctx.e.modifiers & kBareMod) {
        ASSERT(!is_numeric && "kBareMod is invalid with numeric sequences");
        for (int mod = 2; mod <= 8; ++mod) {
            char *q = p;
            ASSERT(q <= ctx.bufferEnd - 2);
            *q++ = '0' + mod;
            *q++ = ctx.e.id;
            setEncoding(ctx, q, extraKeyState | keyStateForMod(mod));
        }
    }
    if (ctx.e.modifiers & kSemiMod) {
        for (int mod = 2; mod <= 8; ++mod) {
            char *q = p;
            if (is_numeric) {
                appendId(q);
                ASSERT(q <= ctx.bufferEnd - 2);
                *q++ = ';';
                *q++ = '0' + mod;
                expandNumericEncodingSuffix(
                    ctx, q, extraKeyState | keyStateForMod(mod));
            } else {
                ASSERT(q <= ctx.bufferEnd - 4);
                *q++ = '1';
                *q++ = ';';
                *q++ = '0' + mod;
                *q++ = ctx.e.id;
                setEncoding(ctx, q, extraKeyState | keyStateForMod(mod));
            }
        }
    }
}

template <bool is_numeric>
static inline void expandEncoding(const ExpandContext &ctx) {
    if (ctx.e.alt_prefix_allowed) {
        // For better or for worse, this code expands all of:
        //  *     ESC [       <key>     -- <key>
        //  * ESC ESC [       <key>     -- Alt-<key>
        //  *     ESC [ 1 ; 3 <key>     -- Alt-<key>
        //  * ESC ESC [ 1 ; 3 <key>     -- Alt-<key> specified twice
        // I suspect no terminal actually emits the last one (i.e. specifying
        // the Alt modifier using both methods), but I have seen a terminal
        // that emitted a prefix ESC for Alt and a non-Alt modifier.
        char *p = ctx.buffer;
        ASSERT(p <= ctx.bufferEnd - 1);
        *p++ = '\x1b';
        expandEncodingAfterAltPrefix<is_numeric>(ctx, p, LEFT_ALT_PRESSED);
    }
    expandEncodingAfterAltPrefix<is_numeric>(ctx, ctx.buffer, 0);
}

template <bool is_numeric, size_t N>
static void addEscapes(InputMap &inputMap, const EscapeEncoding (&encodings)[N]) {
    char buffer[32];
    for (size_t i = 0; i < DIM(encodings); ++i) {
        ExpandContext ctx = {
            inputMap, encodings[i],
            buffer, buffer + sizeof(buffer)
        };
        expandEncoding<is_numeric>(ctx);
    }
}

} // anonymous namespace

void addDefaultEntriesToInputMap(InputMap &inputMap) {
    addEscapes<false>(inputMap, escapeLetterEncodings);
    addEscapes<true>(inputMap, escapeNumericEncodings);
    addSimpleEntries(inputMap);
}
