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

#include "DebugShowInput.h"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "../shared/StringBuilder.h"
#include "InputMap.h"

namespace {

struct Flag {
    DWORD value;
    const char *text;
};

static const Flag kButtonStates[] = {
    { FROM_LEFT_1ST_BUTTON_PRESSED, "1" },
    { FROM_LEFT_2ND_BUTTON_PRESSED, "2" },
    { FROM_LEFT_3RD_BUTTON_PRESSED, "3" },
    { FROM_LEFT_4TH_BUTTON_PRESSED, "4" },
    { RIGHTMOST_BUTTON_PRESSED,     "R" },
};

static const Flag kControlKeyStates[] = {
    { CAPSLOCK_ON,          "CapsLock"      },
    { ENHANCED_KEY,         "Enhanced"      },
    { LEFT_ALT_PRESSED,     "LAlt"          },
    { LEFT_CTRL_PRESSED,    "LCtrl"         },
    { NUMLOCK_ON,           "NumLock"       },
    { RIGHT_ALT_PRESSED,    "RAlt"          },
    { RIGHT_CTRL_PRESSED,   "RCtrl"         },
    { SCROLLLOCK_ON,        "ScrollLock"    },
    { SHIFT_PRESSED,        "Shift"         },
};

static const Flag kMouseEventFlags[] = {
    { DOUBLE_CLICK,         "Double"        },
    { 8/*MOUSE_HWHEELED*/,  "HWheel"        },
    { MOUSE_MOVED,          "Move"          },
    { MOUSE_WHEELED,        "Wheel"         },
};

static void writeFlags(StringBuilder &out, DWORD flags,
                       const char *remainderName,
                       const Flag *table, size_t tableSize,
                       char pre, char sep, char post) {
    DWORD remaining = flags;
    bool wroteSomething = false;
    for (size_t i = 0; i < tableSize; ++i) {
        const Flag &f = table[i];
        if ((f.value & flags) == f.value) {
            if (!wroteSomething && pre != '\0') {
                out << pre;
            } else if (wroteSomething && sep != '\0') {
                out << sep;
            }
            out << f.text;
            wroteSomething = true;
            remaining &= ~f.value;
        }
    }
    if (remaining != 0) {
        if (!wroteSomething && pre != '\0') {
            out << pre;
        } else if (wroteSomething && sep != '\0') {
            out << sep;
        }
        out << remainderName << "(0x" << hexOfInt(remaining) << ')';
        wroteSomething = true;
    }
    if (wroteSomething && post != '\0') {
        out << post;
    }
}

template <size_t n>
static void writeFlags(StringBuilder &out, DWORD flags,
                       const char *remainderName,
                       const Flag (&table)[n],
                       char pre, char sep, char post) {
    writeFlags(out, flags, remainderName, table, n, pre, sep, post);
}

} // anonymous namespace

std::string controlKeyStatePrefix(DWORD controlKeyState) {
    StringBuilder sb;
    writeFlags(sb, controlKeyState,
               "keyState", kControlKeyStates, '\0', '-', '-');
    return sb.str_moved();
}

std::string mouseEventToString(const MOUSE_EVENT_RECORD &mer) {
    const uint16_t buttons = mer.dwButtonState & 0xFFFF;
    const int16_t wheel = mer.dwButtonState >> 16;
    StringBuilder sb;
    sb << "pos=" << mer.dwMousePosition.X << ','
                 << mer.dwMousePosition.Y;
    writeFlags(sb, mer.dwControlKeyState, "keyState", kControlKeyStates, ' ', ' ', '\0');
    writeFlags(sb, mer.dwEventFlags, "flags", kMouseEventFlags, ' ', ' ', '\0');
    writeFlags(sb, buttons, "buttons", kButtonStates, ' ', ' ', '\0');
    if (wheel != 0) {
        sb << " wheel=" << wheel;
    }
    return sb.str_moved();
}

void debugShowInput(bool enableMouse, bool escapeInput) {
    HANDLE conin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD origConsoleMode = 0;
    if (!GetConsoleMode(conin, &origConsoleMode)) {
        fprintf(stderr, "Error: could not read console mode -- "
                        "is STDIN a console handle?\n");
        exit(1);
    }
    DWORD restoreConsoleMode = origConsoleMode;
    if (enableMouse && !(restoreConsoleMode & ENABLE_EXTENDED_FLAGS)) {
        // We need to disable QuickEdit mode, because it blocks mouse events.
        // If ENABLE_EXTENDED_FLAGS wasn't originally in the console mode, then
        // we have no way of knowning whether QuickEdit or InsertMode are
        // currently enabled.  Enable them both (eventually), because they're
        // sensible defaults.  This case shouldn't happen typically.  See
        // misc/EnableExtendedFlags.txt.
        restoreConsoleMode |= ENABLE_EXTENDED_FLAGS;
        restoreConsoleMode |= ENABLE_QUICK_EDIT_MODE;
        restoreConsoleMode |= ENABLE_INSERT_MODE;
    }
    DWORD newConsoleMode = restoreConsoleMode;
    newConsoleMode &= ~ENABLE_PROCESSED_INPUT;
    newConsoleMode &= ~ENABLE_LINE_INPUT;
    newConsoleMode &= ~ENABLE_ECHO_INPUT;
    newConsoleMode |= ENABLE_WINDOW_INPUT;
    if (enableMouse) {
        newConsoleMode |= ENABLE_MOUSE_INPUT;
        newConsoleMode &= ~ENABLE_QUICK_EDIT_MODE;
    } else {
        newConsoleMode &= ~ENABLE_MOUSE_INPUT;
    }
    if (escapeInput) {
        // As of this writing (2016-06-05), Microsoft has shipped two preview
        // builds of Windows 10 (14316 and 14342) that include a new "Windows
        // Subsystem for Linux" that runs Ubuntu in a new subsystem.  Running
        // bash in this subsystem requires the non-legacy console mode, and the
        // console input buffer is put into a special mode where escape
        // sequences are written into the console input buffer.  This mode is
        // enabled with the 0x200 flag, which is as-yet undocumented.
        // See https://github.com/rprichard/winpty/issues/82.
        newConsoleMode |= 0x200;
    }
    if (!SetConsoleMode(conin, newConsoleMode)) {
        fprintf(stderr, "Error: could not set console mode "
            "(0x%x -> 0x%x -> 0x%x)\n",
            static_cast<unsigned int>(origConsoleMode),
            static_cast<unsigned int>(newConsoleMode),
            static_cast<unsigned int>(restoreConsoleMode));
        exit(1);
    }
    printf("\nPress any keys -- Ctrl-D exits\n\n");
    INPUT_RECORD records[32];
    DWORD actual = 0;
    bool finished = false;
    while (!finished &&
            ReadConsoleInputW(conin, records, 32, &actual) && actual >= 1) {
        StringBuilder sb;
        for (DWORD i = 0; i < actual; ++i) {
            const INPUT_RECORD &record = records[i];
            if (record.EventType == KEY_EVENT) {
                const KEY_EVENT_RECORD &ker = record.Event.KeyEvent;
                InputMap::Key key = {
                    ker.wVirtualKeyCode,
                    ker.uChar.UnicodeChar,
                    static_cast<uint16_t>(ker.dwControlKeyState),
                };
                sb << "key: " << (ker.bKeyDown ? "dn" : "up")
                   << " rpt=" << ker.wRepeatCount
                   << " scn=" << (ker.wVirtualScanCode ? "0x" : "") << hexOfInt(ker.wVirtualScanCode)
                   << ' ' << key.toString() << '\n';
                if ((ker.dwControlKeyState &
                        (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) &&
                        ker.wVirtualKeyCode == 'D') {
                    finished = true;
                    break;
                } else if (ker.wVirtualKeyCode == 0 &&
                        ker.wVirtualScanCode == 0 &&
                        ker.uChar.UnicodeChar == 4) {
                    // Also look for a zeroed-out Ctrl-D record generated for
                    // ENABLE_VIRTUAL_TERMINAL_INPUT.
                    finished = true;
                    break;
                }
            } else if (record.EventType == MOUSE_EVENT) {
                const MOUSE_EVENT_RECORD &mer = record.Event.MouseEvent;
                sb << "mouse: " << mouseEventToString(mer) << '\n';
            } else if (record.EventType == WINDOW_BUFFER_SIZE_EVENT) {
                const WINDOW_BUFFER_SIZE_RECORD &wbsr =
                    record.Event.WindowBufferSizeEvent;
                sb << "buffer-resized: dwSize=("
                   << wbsr.dwSize.X << ','
                   << wbsr.dwSize.Y << ")\n";
            } else if (record.EventType == MENU_EVENT) {
                const MENU_EVENT_RECORD &mer = record.Event.MenuEvent;
                sb << "menu-event: commandId=0x"
                   << hexOfInt(mer.dwCommandId) << '\n';
            } else if (record.EventType == FOCUS_EVENT) {
                const FOCUS_EVENT_RECORD &fer = record.Event.FocusEvent;
                sb << "focus: " << (fer.bSetFocus ? "gained" : "lost") << '\n';
            }
        }

        const auto str = sb.str_moved();
        fwrite(str.data(), 1, str.size(), stdout);
        fflush(stdout);
    }
    SetConsoleMode(conin, restoreConsoleMode);
}
