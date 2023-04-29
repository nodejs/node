// Copyright (c) 2011-2015 Ryan Prichard
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

#include "ConsoleInput.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>

#include "../include/winpty_constants.h"

#include "../shared/DebugClient.h"
#include "../shared/StringBuilder.h"
#include "../shared/UnixCtrlChars.h"

#include "ConsoleInputReencoding.h"
#include "DebugShowInput.h"
#include "DefaultInputMap.h"
#include "DsrSender.h"
#include "UnicodeEncoding.h"
#include "Win32Console.h"

// MAPVK_VK_TO_VSC isn't defined by the old MinGW.
#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif

namespace {

struct MouseRecord {
    bool release;
    int flags;
    COORD coord;

    std::string toString() const;
};

std::string MouseRecord::toString() const {
    StringBuilder sb(40);
    sb << "pos=" << coord.X << ',' << coord.Y
       << " flags=0x" << hexOfInt(flags);
    if (release) {
        sb << " release";
    }
    return sb.str_moved();
}

const unsigned int kIncompleteEscapeTimeoutMs = 1000u;

#define CHECK(cond)                                 \
        do {                                        \
            if (!(cond)) { return 0; }              \
        } while(0)

#define ADVANCE()                                   \
        do {                                        \
            pch++;                                  \
            if (pch == stop) { return -1; }         \
        } while(0)

#define SCAN_INT(out, maxLen)                       \
        do {                                        \
            (out) = 0;                              \
            CHECK(isdigit(*pch));                   \
            const char *begin = pch;                \
            do {                                    \
                CHECK(pch - begin + 1 < maxLen);    \
                (out) = (out) * 10 + *pch - '0';    \
                ADVANCE();                          \
            } while (isdigit(*pch));                \
        } while(0)

#define SCAN_SIGNED_INT(out, maxLen)                \
        do {                                        \
            bool negative = false;                  \
            if (*pch == '-') {                      \
                negative = true;                    \
                ADVANCE();                          \
            }                                       \
            SCAN_INT(out, maxLen);                  \
            if (negative) {                         \
                (out) = -(out);                     \
            }                                       \
        } while(0)

// Match the Device Status Report console input:  ESC [ nn ; mm R
// Returns:
// 0   no match
// >0  match, returns length of match
// -1  incomplete match
static int matchDsr(const char *input, int inputSize)
{
    int32_t dummy = 0;
    const char *pch = input;
    const char *stop = input + inputSize;
    CHECK(*pch == '\x1B');  ADVANCE();
    CHECK(*pch == '[');     ADVANCE();
    SCAN_INT(dummy, 8);
    CHECK(*pch == ';');     ADVANCE();
    SCAN_INT(dummy, 8);
    CHECK(*pch == 'R');
    return pch - input + 1;
}

static int matchMouseDefault(const char *input, int inputSize,
                             MouseRecord &out)
{
    const char *pch = input;
    const char *stop = input + inputSize;
    CHECK(*pch == '\x1B');              ADVANCE();
    CHECK(*pch == '[');                 ADVANCE();
    CHECK(*pch == 'M');                 ADVANCE();
    out.flags = (*pch - 32) & 0xFF;     ADVANCE();
    out.coord.X = (*pch - '!') & 0xFF;
    ADVANCE();
    out.coord.Y = (*pch - '!') & 0xFF;
    out.release = false;
    return pch - input + 1;
}

static int matchMouse1006(const char *input, int inputSize, MouseRecord &out)
{
    const char *pch = input;
    const char *stop = input + inputSize;
    int32_t temp;
    CHECK(*pch == '\x1B');      ADVANCE();
    CHECK(*pch == '[');         ADVANCE();
    CHECK(*pch == '<');         ADVANCE();
    SCAN_INT(out.flags, 8);
    CHECK(*pch == ';');         ADVANCE();
    SCAN_SIGNED_INT(temp, 8); out.coord.X = temp - 1;
    CHECK(*pch == ';');         ADVANCE();
    SCAN_SIGNED_INT(temp, 8); out.coord.Y = temp - 1;
    CHECK(*pch == 'M' || *pch == 'm');
    out.release = (*pch == 'm');
    return pch - input + 1;
}

static int matchMouse1015(const char *input, int inputSize, MouseRecord &out)
{
    const char *pch = input;
    const char *stop = input + inputSize;
    int32_t temp;
    CHECK(*pch == '\x1B');      ADVANCE();
    CHECK(*pch == '[');         ADVANCE();
    SCAN_INT(out.flags, 8); out.flags -= 32;
    CHECK(*pch == ';');         ADVANCE();
    SCAN_SIGNED_INT(temp, 8); out.coord.X = temp - 1;
    CHECK(*pch == ';');         ADVANCE();
    SCAN_SIGNED_INT(temp, 8); out.coord.Y = temp - 1;
    CHECK(*pch == 'M');
    out.release = false;
    return pch - input + 1;
}

// Match a mouse input escape sequence of any kind.
// 0   no match
// >0  match, returns length of match
// -1  incomplete match
static int matchMouseRecord(const char *input, int inputSize, MouseRecord &out)
{
    memset(&out, 0, sizeof(out));
    int ret;
    if ((ret = matchMouse1006(input, inputSize, out)) != 0) { return ret; }
    if ((ret = matchMouse1015(input, inputSize, out)) != 0) { return ret; }
    if ((ret = matchMouseDefault(input, inputSize, out)) != 0) { return ret; }
    return 0;
}

#undef CHECK
#undef ADVANCE
#undef SCAN_INT

} // anonymous namespace

ConsoleInput::ConsoleInput(HANDLE conin, int mouseMode, DsrSender &dsrSender,
                           Win32Console &console) :
    m_console(console),
    m_conin(conin),
    m_mouseMode(mouseMode),
    m_dsrSender(dsrSender)
{
    addDefaultEntriesToInputMap(m_inputMap);
    if (hasDebugFlag("dump_input_map")) {
        m_inputMap.dumpInputMap();
    }

    // Configure Quick Edit mode according to the mouse mode.  Enable
    // InsertMode for two reasons:
    //  - If it's OFF, it's difficult for the user to turn it ON.  The
    //    properties dialog is inaccesible.  winpty still faithfully handles
    //    the Insert key, which toggles between the insertion and overwrite
    //    modes.
    //  - When we modify the QuickEdit setting, if ExtendedFlags is OFF,
    //    then we must choose the InsertMode setting.  I don't *think* this
    //    case happens, though, because a new console always has ExtendedFlags
    //    ON.
    // See misc/EnableExtendedFlags.txt.
    DWORD mode = 0;
    if (!GetConsoleMode(conin, &mode)) {
        trace("Agent startup: GetConsoleMode failed");
    } else {
        mode |= ENABLE_EXTENDED_FLAGS;
        mode |= ENABLE_INSERT_MODE;
        if (m_mouseMode == WINPTY_MOUSE_MODE_AUTO) {
            mode |= ENABLE_QUICK_EDIT_MODE;
        } else {
            mode &= ~ENABLE_QUICK_EDIT_MODE;
        }
        if (!SetConsoleMode(conin, mode)) {
            trace("Agent startup: SetConsoleMode failed");
        }
    }

    updateInputFlags(true);
}

void ConsoleInput::writeInput(const std::string &input)
{
    if (input.size() == 0) {
        return;
    }

    if (isTracingEnabled()) {
        static bool debugInput = hasDebugFlag("input");
        if (debugInput) {
            std::string dumpString;
            for (size_t i = 0; i < input.size(); ++i) {
                const char ch = input[i];
                const char ctrl = decodeUnixCtrlChar(ch);
                if (ctrl != '\0') {
                    dumpString += '^';
                    dumpString += ctrl;
                } else {
                    dumpString += ch;
                }
            }
            dumpString += " (";
            for (size_t i = 0; i < input.size(); ++i) {
                if (i > 0) {
                    dumpString += ' ';
                }
                const unsigned char uch = input[i];
                char buf[32];
                winpty_snprintf(buf, "%02X", uch);
                dumpString += buf;
            }
            dumpString += ')';
            trace("input chars: %s", dumpString.c_str());
        }
    }

    m_byteQueue.append(input);
    doWrite(false);
    if (!m_byteQueue.empty() && !m_dsrSent) {
        trace("send DSR");
        m_dsrSender.sendDsr();
        m_dsrSent = true;
    }
    m_lastWriteTick = GetTickCount();
}

void ConsoleInput::flushIncompleteEscapeCode()
{
    if (!m_byteQueue.empty() &&
            (GetTickCount() - m_lastWriteTick) > kIncompleteEscapeTimeoutMs) {
        doWrite(true);
        m_byteQueue.clear();
    }
}

void ConsoleInput::updateInputFlags(bool forceTrace)
{
    const DWORD mode = inputConsoleMode();
    const bool newFlagEE = (mode & ENABLE_EXTENDED_FLAGS) != 0;
    const bool newFlagMI = (mode & ENABLE_MOUSE_INPUT) != 0;
    const bool newFlagQE = (mode & ENABLE_QUICK_EDIT_MODE) != 0;
    const bool newFlagEI = (mode & 0x200) != 0;
    if (forceTrace ||
            newFlagEE != m_enableExtendedEnabled ||
            newFlagMI != m_mouseInputEnabled ||
            newFlagQE != m_quickEditEnabled ||
            newFlagEI != m_escapeInputEnabled) {
        trace("CONIN modes: Extended=%s, MouseInput=%s QuickEdit=%s EscapeInput=%s",
            newFlagEE ? "on" : "off",
            newFlagMI ? "on" : "off",
            newFlagQE ? "on" : "off",
            newFlagEI ? "on" : "off");
    }
    m_enableExtendedEnabled = newFlagEE;
    m_mouseInputEnabled = newFlagMI;
    m_quickEditEnabled = newFlagQE;
    m_escapeInputEnabled = newFlagEI;
}

bool ConsoleInput::shouldActivateTerminalMouse()
{
    // Return whether the agent should activate the terminal's mouse mode.
    if (m_mouseMode == WINPTY_MOUSE_MODE_AUTO) {
        // Some programs (e.g. Cygwin command-line programs like bash.exe and
        // python2.7.exe) turn off ENABLE_EXTENDED_FLAGS and turn on
        // ENABLE_MOUSE_INPUT, but do not turn off QuickEdit mode and do not
        // actually care about mouse input.  Only enable the terminal mouse
        // mode if ENABLE_EXTENDED_FLAGS is on.  See
        // misc/EnableExtendedFlags.txt.
        return m_mouseInputEnabled && !m_quickEditEnabled &&
                m_enableExtendedEnabled;
    } else if (m_mouseMode == WINPTY_MOUSE_MODE_FORCE) {
        return true;
    } else {
        return false;
    }
}

void ConsoleInput::doWrite(bool isEof)
{
    const char *data = m_byteQueue.c_str();
    std::vector<INPUT_RECORD> records;
    size_t idx = 0;
    while (idx < m_byteQueue.size()) {
        int charSize = scanInput(records, &data[idx], m_byteQueue.size() - idx, isEof);
        if (charSize == -1)
            break;
        idx += charSize;
    }
    m_byteQueue.erase(0, idx);
    flushInputRecords(records);
}

void ConsoleInput::flushInputRecords(std::vector<INPUT_RECORD> &records)
{
    if (records.size() == 0) {
        return;
    }
    DWORD actual = 0;
    if (!WriteConsoleInputW(m_conin, records.data(), records.size(), &actual)) {
        trace("WriteConsoleInputW failed");
    }
    records.clear();
}

// This behavior isn't strictly correct, because the keypresses (probably?)
// adopt the keyboard state (e.g. Ctrl/Alt/Shift modifiers) of the current
// window station's keyboard, which has no necessary relationship to the winpty
// instance.  It's unlikely to be an issue in practice, but it's conceivable.
// (Imagine a foreground SSH server, where the local user holds down Ctrl,
// while the remote user tries to use WSL navigation keys.)  I suspect using
// the BackgroundDesktop mechanism in winpty would fix the problem.
//
// https://github.com/rprichard/winpty/issues/116
static void sendKeyMessage(HWND hwnd, bool isKeyDown, uint16_t virtualKey)
{
    uint32_t scanCode = MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC);
    if (scanCode > 255) {
        scanCode = 0;
    }
    SendMessage(hwnd, isKeyDown ? WM_KEYDOWN : WM_KEYUP, virtualKey,
        (scanCode << 16) | 1u | (isKeyDown ? 0u : 0xc0000000u));
}

int ConsoleInput::scanInput(std::vector<INPUT_RECORD> &records,
                            const char *input,
                            int inputSize,
                            bool isEof)
{
    ASSERT(inputSize >= 1);

    // Ctrl-C.
    //
    // In processed mode, use GenerateConsoleCtrlEvent so that Ctrl-C handlers
    // are called.  GenerateConsoleCtrlEvent unfortunately doesn't interrupt
    // ReadConsole calls[1].  Using WM_KEYDOWN/UP fixes the ReadConsole
    // problem, but breaks in background window stations/desktops.
    //
    // In unprocessed mode, there's an entry for Ctrl-C in the SimpleEncoding
    // table in DefaultInputMap.
    //
    // [1] https://github.com/rprichard/winpty/issues/116
    if (input[0] == '\x03' && (inputConsoleMode() & ENABLE_PROCESSED_INPUT)) {
        flushInputRecords(records);
        trace("Ctrl-C");
        const BOOL ret = GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
        trace("GenerateConsoleCtrlEvent: %d", ret);
        return 1;
    }

    if (input[0] == '\x1B') {
        // Attempt to match the Device Status Report (DSR) reply.
        int dsrLen = matchDsr(input, inputSize);
        if (dsrLen > 0) {
            trace("Received a DSR reply");
            m_dsrSent = false;
            return dsrLen;
        } else if (!isEof && dsrLen == -1) {
            // Incomplete DSR match.
            trace("Incomplete DSR match");
            return -1;
        }

        int mouseLen = scanMouseInput(records, input, inputSize);
        if (mouseLen > 0 || (!isEof && mouseLen == -1)) {
            return mouseLen;
        }
    }

    // Search the input map.
    InputMap::Key match;
    bool incomplete;
    int matchLen = m_inputMap.lookupKey(input, inputSize, match, incomplete);
    if (!isEof && incomplete) {
        // Incomplete match -- need more characters (or wait for a
        // timeout to signify flushed input).
        trace("Incomplete escape sequence");
        return -1;
    } else if (matchLen > 0) {
        uint32_t winCodePointDn = match.unicodeChar;
        if ((match.keyState & LEFT_CTRL_PRESSED) && (match.keyState & LEFT_ALT_PRESSED)) {
            winCodePointDn = '\0';
        }
        uint32_t winCodePointUp = winCodePointDn;
        if (match.keyState & LEFT_ALT_PRESSED) {
            winCodePointUp = '\0';
        }
        appendKeyPress(records, match.virtualKey,
                       winCodePointDn, winCodePointUp, match.keyState,
                       match.unicodeChar, match.keyState);
        return matchLen;
    }

    // Recognize Alt-<character>.
    //
    // This code doesn't match Alt-ESC, which is encoded as `ESC ESC`, but
    // maybe it should.  I was concerned that pressing ESC rapidly enough could
    // accidentally trigger Alt-ESC.  (e.g. The user would have to be faster
    // than the DSR flushing mechanism or use a decrepit terminal.  The user
    // might be on a slow network connection.)
    if (input[0] == '\x1B' && inputSize >= 2 && input[1] != '\x1B') {
        const int len = utf8CharLength(input[1]);
        if (len > 0) {
            if (1 + len > inputSize) {
                // Incomplete character.
                trace("Incomplete UTF-8 character in Alt-<Char>");
                return -1;
            }
            appendUtf8Char(records, &input[1], len, true);
            return 1 + len;
        }
    }

    // A UTF-8 character.
    const int len = utf8CharLength(input[0]);
    if (len == 0) {
        static bool debugInput = isTracingEnabled() && hasDebugFlag("input");
        if (debugInput) {
            trace("Discarding invalid input byte: %02X",
                static_cast<unsigned char>(input[0]));
        }
        return 1;
    }
    if (len > inputSize) {
        // Incomplete character.
        trace("Incomplete UTF-8 character");
        return -1;
    }
    appendUtf8Char(records, &input[0], len, false);
    return len;
}

int ConsoleInput::scanMouseInput(std::vector<INPUT_RECORD> &records,
                                 const char *input,
                                 int inputSize)
{
    MouseRecord record;
    const int len = matchMouseRecord(input, inputSize, record);
    if (len <= 0) {
        return len;
    }

    if (isTracingEnabled()) {
        static bool debugInput = hasDebugFlag("input");
        if (debugInput) {
            trace("mouse input: %s", record.toString().c_str());
        }
    }

    const int button = record.flags & 0x03;
    INPUT_RECORD newRecord = {0};
    newRecord.EventType = MOUSE_EVENT;
    MOUSE_EVENT_RECORD &mer = newRecord.Event.MouseEvent;

    mer.dwMousePosition.X =
        m_mouseWindowRect.Left +
            std::max(0, std::min<int>(record.coord.X,
                                      m_mouseWindowRect.width() - 1));

    mer.dwMousePosition.Y =
        m_mouseWindowRect.Top +
            std::max(0, std::min<int>(record.coord.Y,
                                      m_mouseWindowRect.height() - 1));

    // The modifier state is neatly independent of everything else.
    if (record.flags & 0x04) { mer.dwControlKeyState |= SHIFT_PRESSED;     }
    if (record.flags & 0x08) { mer.dwControlKeyState |= LEFT_ALT_PRESSED;  }
    if (record.flags & 0x10) { mer.dwControlKeyState |= LEFT_CTRL_PRESSED; }

    if (record.flags & 0x40) {
        // Mouse wheel
        mer.dwEventFlags |= MOUSE_WHEELED;
        if (button == 0) {
            // up
            mer.dwButtonState |= 0x00780000;
        } else if (button == 1) {
            // down
            mer.dwButtonState |= 0xff880000;
        } else {
            // Invalid -- do nothing
            return len;
        }
    } else {
        // Ordinary mouse event
        if (record.flags & 0x20) { mer.dwEventFlags |= MOUSE_MOVED; }
        if (button == 3) {
            m_mouseButtonState = 0;
            // Potentially advance double-click detection.
            m_doubleClick.released = true;
        } else {
            const DWORD relevantFlag =
                (button == 0) ? FROM_LEFT_1ST_BUTTON_PRESSED :
                (button == 1) ? FROM_LEFT_2ND_BUTTON_PRESSED :
                (button == 2) ? RIGHTMOST_BUTTON_PRESSED :
                0;
            ASSERT(relevantFlag != 0);
            if (record.release) {
                m_mouseButtonState &= ~relevantFlag;
                if (relevantFlag == m_doubleClick.button) {
                    // Potentially advance double-click detection.
                    m_doubleClick.released = true;
                } else {
                    // End double-click detection.
                    m_doubleClick = DoubleClickDetection();
                }
            } else if ((m_mouseButtonState & relevantFlag) == 0) {
                // The button has been newly pressed.
                m_mouseButtonState |= relevantFlag;
                // Detect a double-click.  This code looks for an exact
                // coordinate match, which is stricter than what Windows does,
                // but Windows has pixel coordinates, and we only have terminal
                // coordinates.
                if (m_doubleClick.button == relevantFlag &&
                        m_doubleClick.pos == record.coord &&
                        (GetTickCount() - m_doubleClick.tick <
                            GetDoubleClickTime())) {
                    // Record a double-click and end double-click detection.
                    mer.dwEventFlags |= DOUBLE_CLICK;
                    m_doubleClick = DoubleClickDetection();
                } else {
                    // Begin double-click detection.
                    m_doubleClick.button = relevantFlag;
                    m_doubleClick.pos = record.coord;
                    m_doubleClick.tick = GetTickCount();
                }
            }
        }
    }

    mer.dwButtonState |= m_mouseButtonState;

    if (m_mouseInputEnabled && !m_quickEditEnabled) {
        if (isTracingEnabled()) {
            static bool debugInput = hasDebugFlag("input");
            if (debugInput) {
                trace("mouse event: %s", mouseEventToString(mer).c_str());
            }
        }

        records.push_back(newRecord);
    }

    return len;
}

void ConsoleInput::appendUtf8Char(std::vector<INPUT_RECORD> &records,
                                  const char *charBuffer,
                                  const int charLen,
                                  const bool terminalAltEscape)
{
    const uint32_t codePoint = decodeUtf8(charBuffer);
    if (codePoint == static_cast<uint32_t>(-1)) {
        static bool debugInput = isTracingEnabled() && hasDebugFlag("input");
        if (debugInput) {
            StringBuilder error(64);
            error << "Discarding invalid UTF-8 sequence:";
            for (int i = 0; i < charLen; ++i) {
                error << ' ';
                error << hexOfInt<true, uint8_t>(charBuffer[i]);
            }
            trace("%s", error.c_str());
        }
        return;
    }

    const short charScan = codePoint > 0xFFFF ? -1 : VkKeyScan(codePoint);
    uint16_t virtualKey = 0;
    uint16_t winKeyState = 0;
    uint32_t winCodePointDn = codePoint;
    uint32_t winCodePointUp = codePoint;
    uint16_t vtKeyState = 0;

    if (charScan != -1) {
        virtualKey = charScan & 0xFF;
        if (charScan & 0x100) {
            winKeyState |= SHIFT_PRESSED;
        }
        if (charScan & 0x200) {
            winKeyState |= LEFT_CTRL_PRESSED;
        }
        if (charScan & 0x400) {
            winKeyState |= RIGHT_ALT_PRESSED;
        }
        if (terminalAltEscape && (winKeyState & LEFT_CTRL_PRESSED)) {
            // If the terminal escapes a Ctrl-<Key> with Alt, then set the
            // codepoint to 0.  On the other hand, if a character requires
            // AltGr (like U+00B2 on a German layout), then VkKeyScan will
            // report both Ctrl and Alt pressed, and we should keep the
            // codepoint.  See https://github.com/rprichard/winpty/issues/109.
            winCodePointDn = 0;
            winCodePointUp = 0;
        }
    }
    if (terminalAltEscape) {
        winCodePointUp = 0;
        winKeyState |= LEFT_ALT_PRESSED;
        vtKeyState |= LEFT_ALT_PRESSED;
    }

    appendKeyPress(records, virtualKey,
                   winCodePointDn, winCodePointUp, winKeyState,
                   codePoint, vtKeyState);
}

void ConsoleInput::appendKeyPress(std::vector<INPUT_RECORD> &records,
                                  const uint16_t virtualKey,
                                  const uint32_t winCodePointDn,
                                  const uint32_t winCodePointUp,
                                  const uint16_t winKeyState,
                                  const uint32_t vtCodePoint,
                                  const uint16_t vtKeyState)
{
    const bool ctrl = (winKeyState & LEFT_CTRL_PRESSED) != 0;
    const bool leftAlt = (winKeyState & LEFT_ALT_PRESSED) != 0;
    const bool rightAlt = (winKeyState & RIGHT_ALT_PRESSED) != 0;
    const bool shift = (winKeyState & SHIFT_PRESSED) != 0;
    const bool enhanced = (winKeyState & ENHANCED_KEY) != 0;
    bool hasDebugInput = false;

    if (isTracingEnabled()) {
        static bool debugInput = hasDebugFlag("input");
        if (debugInput) {
            hasDebugInput = true;
            InputMap::Key key = { virtualKey, winCodePointDn, winKeyState };
            trace("keypress: %s", key.toString().c_str());
        }
    }

    if (m_escapeInputEnabled &&
            (virtualKey == VK_UP ||
                virtualKey == VK_DOWN ||
                virtualKey == VK_LEFT ||
                virtualKey == VK_RIGHT ||
                virtualKey == VK_HOME ||
                virtualKey == VK_END) &&
            !ctrl && !leftAlt && !rightAlt && !shift) {
        flushInputRecords(records);
        if (hasDebugInput) {
            trace("sending keypress to console HWND");
        }
        sendKeyMessage(m_console.hwnd(), true, virtualKey);
        sendKeyMessage(m_console.hwnd(), false, virtualKey);
        return;
    }

    uint16_t stepKeyState = 0;
    if (ctrl) {
        stepKeyState |= LEFT_CTRL_PRESSED;
        appendInputRecord(records, TRUE, VK_CONTROL, 0, stepKeyState);
    }
    if (leftAlt) {
        stepKeyState |= LEFT_ALT_PRESSED;
        appendInputRecord(records, TRUE, VK_MENU, 0, stepKeyState);
    }
    if (rightAlt) {
        stepKeyState |= RIGHT_ALT_PRESSED;
        appendInputRecord(records, TRUE, VK_MENU, 0, stepKeyState | ENHANCED_KEY);
    }
    if (shift) {
        stepKeyState |= SHIFT_PRESSED;
        appendInputRecord(records, TRUE, VK_SHIFT, 0, stepKeyState);
    }
    if (enhanced) {
        stepKeyState |= ENHANCED_KEY;
    }
    if (m_escapeInputEnabled) {
        reencodeEscapedKeyPress(records, virtualKey, vtCodePoint, vtKeyState);
    } else {
        appendCPInputRecords(records, TRUE, virtualKey, winCodePointDn, stepKeyState);
    }
    appendCPInputRecords(records, FALSE, virtualKey, winCodePointUp, stepKeyState);
    if (enhanced) {
        stepKeyState &= ~ENHANCED_KEY;
    }
    if (shift) {
        stepKeyState &= ~SHIFT_PRESSED;
        appendInputRecord(records, FALSE, VK_SHIFT, 0, stepKeyState);
    }
    if (rightAlt) {
        stepKeyState &= ~RIGHT_ALT_PRESSED;
        appendInputRecord(records, FALSE, VK_MENU, 0, stepKeyState | ENHANCED_KEY);
    }
    if (leftAlt) {
        stepKeyState &= ~LEFT_ALT_PRESSED;
        appendInputRecord(records, FALSE, VK_MENU, 0, stepKeyState);
    }
    if (ctrl) {
        stepKeyState &= ~LEFT_CTRL_PRESSED;
        appendInputRecord(records, FALSE, VK_CONTROL, 0, stepKeyState);
    }
}

void ConsoleInput::appendCPInputRecords(std::vector<INPUT_RECORD> &records,
                                        BOOL keyDown,
                                        uint16_t virtualKey,
                                        uint32_t codePoint,
                                        uint16_t keyState)
{
    // This behavior really doesn't match that of the Windows console (in
    // normal, non-escape-mode).  Judging by the copy-and-paste behavior,
    // Windows apparently handles everything outside of the keyboard layout by
    // first sending a sequence of Alt+KeyPad events, then finally a key-up
    // event whose UnicodeChar has the appropriate value.  For U+00A2 (CENT
    // SIGN):
    //
    //      key: dn rpt=1 scn=56 LAlt-MENU ch=0
    //      key: dn rpt=1 scn=79 LAlt-NUMPAD1 ch=0
    //      key: up rpt=1 scn=79 LAlt-NUMPAD1 ch=0
    //      key: dn rpt=1 scn=76 LAlt-NUMPAD5 ch=0
    //      key: up rpt=1 scn=76 LAlt-NUMPAD5 ch=0
    //      key: dn rpt=1 scn=76 LAlt-NUMPAD5 ch=0
    //      key: up rpt=1 scn=76 LAlt-NUMPAD5 ch=0
    //      key: up rpt=1 scn=56 MENU ch=0xa2
    //
    // The Alt+155 value matches the encoding of U+00A2 in CP-437.  Curiously,
    // if I use "chcp 1252" to change the encoding, then copy-and-pasting
    // produces Alt+162 instead.  (U+00A2 is 162 in CP-1252.)  However, typing
    // Alt+155 or Alt+162 produce the same characters regardless of console
    // code page.  (That is, they use CP-437 and yield U+00A2 and U+00F3.)
    //
    // For characters outside the BMP, Windows repeats the process for both
    // UTF-16 code units, e.g, for U+1F300 (CYCLONE):
    //
    //      key: dn rpt=1 scn=56 LAlt-MENU ch=0
    //      key: dn rpt=1 scn=77 LAlt-NUMPAD6 ch=0
    //      key: up rpt=1 scn=77 LAlt-NUMPAD6 ch=0
    //      key: dn rpt=1 scn=81 LAlt-NUMPAD3 ch=0
    //      key: up rpt=1 scn=81 LAlt-NUMPAD3 ch=0
    //      key: up rpt=1 scn=56 MENU ch=0xd83c
    //      key: dn rpt=1 scn=56 LAlt-MENU ch=0
    //      key: dn rpt=1 scn=77 LAlt-NUMPAD6 ch=0
    //      key: up rpt=1 scn=77 LAlt-NUMPAD6 ch=0
    //      key: dn rpt=1 scn=81 LAlt-NUMPAD3 ch=0
    //      key: up rpt=1 scn=81 LAlt-NUMPAD3 ch=0
    //      key: up rpt=1 scn=56 MENU ch=0xdf00
    //
    // In this case, it sends Alt+63 twice, which signifies '?'.  Apparently
    // CMD and Cygwin bash are both able to decode this.
    //
    // Also note that typing Alt+NNN still works if NumLock is off, e.g.:
    //
    //      key: dn rpt=1 scn=56 LAlt-MENU ch=0
    //      key: dn rpt=1 scn=79 LAlt-END ch=0
    //      key: up rpt=1 scn=79 LAlt-END ch=0
    //      key: dn rpt=1 scn=76 LAlt-CLEAR ch=0
    //      key: up rpt=1 scn=76 LAlt-CLEAR ch=0
    //      key: dn rpt=1 scn=76 LAlt-CLEAR ch=0
    //      key: up rpt=1 scn=76 LAlt-CLEAR ch=0
    //      key: up rpt=1 scn=56 MENU ch=0xa2
    //
    // Evidently, the Alt+NNN key events are not intended to be decoded to a
    // character.  Maybe programs are looking for a key-up ALT/MENU event with
    // a non-zero character?

    wchar_t ws[2];
    const int wslen = encodeUtf16(ws, codePoint);

    if (wslen == 1) {
        appendInputRecord(records, keyDown, virtualKey, ws[0], keyState);
    } else if (wslen == 2) {
        appendInputRecord(records, keyDown, virtualKey, ws[0], keyState);
        appendInputRecord(records, keyDown, virtualKey, ws[1], keyState);
    } else {
        // This situation isn't that bad, but it should never happen,
        // because invalid codepoints shouldn't reach this point.
        trace("INTERNAL ERROR: appendInputRecordCP: invalid codePoint: "
              "U+%04X", codePoint);
    }
}

void ConsoleInput::appendInputRecord(std::vector<INPUT_RECORD> &records,
                                     BOOL keyDown,
                                     uint16_t virtualKey,
                                     wchar_t utf16Char,
                                     uint16_t keyState)
{
    INPUT_RECORD ir = {};
    ir.EventType = KEY_EVENT;
    ir.Event.KeyEvent.bKeyDown = keyDown;
    ir.Event.KeyEvent.wRepeatCount = 1;
    ir.Event.KeyEvent.wVirtualKeyCode = virtualKey;
    ir.Event.KeyEvent.wVirtualScanCode =
            MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC);
    ir.Event.KeyEvent.uChar.UnicodeChar = utf16Char;
    ir.Event.KeyEvent.dwControlKeyState = keyState;
    records.push_back(ir);
}

DWORD ConsoleInput::inputConsoleMode()
{
    DWORD mode = 0;
    if (!GetConsoleMode(m_conin, &mode)) {
        trace("GetConsoleMode failed");
        return 0;
    }
    return mode;
}
