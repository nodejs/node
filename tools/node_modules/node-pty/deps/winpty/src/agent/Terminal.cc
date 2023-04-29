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

#include "Terminal.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "NamedPipe.h"
#include "UnicodeEncoding.h"
#include "../shared/DebugClient.h"
#include "../shared/WinptyAssert.h"
#include "../shared/winpty_snprintf.h"

#define CSI "\x1b["

// Work around the old MinGW, which lacks COMMON_LVB_LEADING_BYTE and
// COMMON_LVB_TRAILING_BYTE.
const int WINPTY_COMMON_LVB_LEADING_BYTE  = 0x100;
const int WINPTY_COMMON_LVB_TRAILING_BYTE = 0x200;
const int WINPTY_COMMON_LVB_REVERSE_VIDEO = 0x4000;
const int WINPTY_COMMON_LVB_UNDERSCORE    = 0x8000;

const int COLOR_ATTRIBUTE_MASK =
        FOREGROUND_BLUE |
        FOREGROUND_GREEN |
        FOREGROUND_RED |
        FOREGROUND_INTENSITY |
        BACKGROUND_BLUE |
        BACKGROUND_GREEN |
        BACKGROUND_RED |
        BACKGROUND_INTENSITY |
        WINPTY_COMMON_LVB_REVERSE_VIDEO |
        WINPTY_COMMON_LVB_UNDERSCORE;

const int FLAG_RED    = 1;
const int FLAG_GREEN  = 2;
const int FLAG_BLUE   = 4;
const int FLAG_BRIGHT = 8;

const int BLACK  = 0;
const int DKGRAY = BLACK | FLAG_BRIGHT;
const int LTGRAY = FLAG_RED | FLAG_GREEN | FLAG_BLUE;
const int WHITE  = LTGRAY | FLAG_BRIGHT;

// SGR parameters (Select Graphic Rendition)
const int SGR_FORE = 30;
const int SGR_FORE_HI = 90;
const int SGR_BACK = 40;
const int SGR_BACK_HI = 100;

namespace {

static void outUInt(std::string &out, unsigned int n)
{
    char buf[32];
    char *pbuf = &buf[32];
    *(--pbuf) = '\0';
    do {
        *(--pbuf) = '0' + n % 10;
        n /= 10;
    } while (n != 0);
    out.append(pbuf);
}

static void outputSetColorSgrParams(std::string &out, bool isFore, int color)
{
    out.push_back(';');
    const int sgrBase = isFore ? SGR_FORE : SGR_BACK;
    if (color & FLAG_BRIGHT) {
        // Some terminals don't support the 9X/10X "intensive" color parameters
        // (e.g. the Eclipse TM terminal as of this writing).  Those terminals
        // will quietly ignore a 9X/10X code, and the other terminals will
        // ignore a 3X/4X code if it's followed by a 9X/10X code.  Therefore,
        // output a 3X/4X code as a fallback, then override it.
        const int colorBase = color & ~FLAG_BRIGHT;
        outUInt(out, sgrBase + colorBase);
        out.push_back(';');
        outUInt(out, sgrBase + (SGR_FORE_HI - SGR_FORE) + colorBase);
    } else {
        outUInt(out, sgrBase + color);
    }
}

static void outputSetColor(std::string &out, int color)
{
    int fore = 0;
    int back = 0;
    if (color & FOREGROUND_RED)       fore |= FLAG_RED;
    if (color & FOREGROUND_GREEN)     fore |= FLAG_GREEN;
    if (color & FOREGROUND_BLUE)      fore |= FLAG_BLUE;
    if (color & FOREGROUND_INTENSITY) fore |= FLAG_BRIGHT;
    if (color & BACKGROUND_RED)       back |= FLAG_RED;
    if (color & BACKGROUND_GREEN)     back |= FLAG_GREEN;
    if (color & BACKGROUND_BLUE)      back |= FLAG_BLUE;
    if (color & BACKGROUND_INTENSITY) back |= FLAG_BRIGHT;

    if (color & WINPTY_COMMON_LVB_REVERSE_VIDEO) {
        // n.b.: The COMMON_LVB_REVERSE_VIDEO flag also swaps
        // FOREGROUND_INTENSITY and BACKGROUND_INTENSITY.  Tested on
        // Windows 10 v14393.
        std::swap(fore, back);
    }

    // Translate the fore/back colors into terminal escape codes using
    // a heuristic that works OK with common white-on-black or
    // black-on-white color schemes.  We don't know which color scheme
    // the terminal is using.  It is ugly to force white-on-black text
    // on a black-on-white terminal, and it's even ugly to force the
    // matching scheme.  It's probably relevant that the default
    // fore/back terminal colors frequently do not match any of the 16
    // palette colors.

    // Typical default terminal color schemes (according to palette,
    // when possible):
    //  - mintty:               LtGray-on-Black(A)
    //  - putty:                LtGray-on-Black(A)
    //  - xterm:                LtGray-on-Black(A)
    //  - Konsole:              LtGray-on-Black(A)
    //  - JediTerm/JetBrains:   Black-on-White(B)
    //  - rxvt:                 Black-on-White(B)

    // If the background is the default color (black), then it will
    // map to Black(A) or White(B).  If we translate White to White,
    // then a Black background and a White background in the console
    // are both White with (B).  Therefore, we should translate White
    // using SGR 7 (Invert).  The typical finished mapping table for
    // background grayscale colors is:
    //
    //  (A) White => LtGray(fore)
    //  (A) Black => Black(back)
    //  (A) LtGray => LtGray
    //  (A) DkGray => DkGray
    //
    //  (B) White => Black(fore)
    //  (B) Black => White(back)
    //  (B) LtGray => LtGray
    //  (B) DkGray => DkGray
    //

    out.append(CSI "0");
    if (back == BLACK) {
        if (fore == LTGRAY) {
            // The "default" foreground color.  Use the terminal's
            // default colors.
        } else if (fore == WHITE) {
            // Sending the literal color white would behave poorly if
            // the terminal were black-on-white.  Sending Bold is not
            // guaranteed to alter the color, but it will make the text
            // visually distinct, so do that instead.
            out.append(";1");
        } else if (fore == DKGRAY) {
            // Set the foreground color to DkGray(90) with a fallback
            // of LtGray(37) for terminals that don't handle the 9X SGR
            // parameters (e.g. Eclipse's TM Terminal as of this
            // writing).
            out.append(";37;90");
        } else {
            outputSetColorSgrParams(out, true, fore);
        }
    } else if (back == WHITE) {
        // Set the background color using Invert on the default
        // foreground color, and set the foreground color by setting a
        // background color.

        // Use the terminal's inverted colors.
        out.append(";7");
        if (fore == LTGRAY || fore == BLACK) {
            // We're likely mapping Console White to terminal LtGray or
            // Black.  If they are the Console foreground color, then
            // don't set a terminal foreground color to avoid creating
            // invisible text.
        } else {
            outputSetColorSgrParams(out, false, fore);
        }
    } else {
        // Set the foreground and background to match exactly that in
        // the Windows console.
        outputSetColorSgrParams(out, true, fore);
        outputSetColorSgrParams(out, false, back);
    }
    if (fore == back) {
        // The foreground and background colors are exactly equal, so
        // attempt to hide the text using the Conceal SGR parameter,
        // which some terminals support.
        out.append(";8");
    }
    if (color & WINPTY_COMMON_LVB_UNDERSCORE) {
        out.append(";4");
    }
    out.push_back('m');
}

static inline unsigned int fixSpecialCharacters(unsigned int ch)
{
    if (ch <= 0x1b) {
        switch (ch) {
            // The Windows Console has a popup window (e.g. that appears with
            // F7) that is sometimes bordered with box-drawing characters.
            // With the Japanese and Korean system locales (CP932 and CP949),
            // the UnicodeChar values for the box-drawing characters are 1
            // through 6.  Detect this and map the values to the correct
            // Unicode values.
            //
            // N.B. In the English locale, the UnicodeChar values are correct,
            // and they identify single-line characters rather than
            // double-line.  In the Chinese Simplified and Traditional locales,
            // the popups use ASCII characters instead.
            case 1: return 0x2554; // BOX DRAWINGS DOUBLE DOWN AND RIGHT
            case 2: return 0x2557; // BOX DRAWINGS DOUBLE DOWN AND LEFT
            case 3: return 0x255A; // BOX DRAWINGS DOUBLE UP AND RIGHT
            case 4: return 0x255D; // BOX DRAWINGS DOUBLE UP AND LEFT
            case 5: return 0x2551; // BOX DRAWINGS DOUBLE VERTICAL
            case 6: return 0x2550; // BOX DRAWINGS DOUBLE HORIZONTAL

            // Convert an escape character to some other character.  This
            // conversion only applies to console cells containing an escape
            // character.  In newer versions of Windows 10 (e.g. 10.0.10586),
            // the non-legacy console recognizes escape sequences in
            // WriteConsole and interprets them without writing them to the
            // cells of the screen buffer.  In that case, the conversion here
            // does not apply.
            case 0x1b: return '?';
        }
    }
    return ch;
}

static inline bool isFullWidthCharacter(const CHAR_INFO *data, int width)
{
    if (width < 2) {
        return false;
    }
    return
        (data[0].Attributes & WINPTY_COMMON_LVB_LEADING_BYTE) &&
        (data[1].Attributes & WINPTY_COMMON_LVB_TRAILING_BYTE) &&
        data[0].Char.UnicodeChar == data[1].Char.UnicodeChar;
}

// Scan to find a single Unicode Scalar Value.  Full-width characters occupy
// two console cells, and this code also tries to handle UTF-16 surrogate
// pairs.
//
// Windows expands at least some wide characters outside the Basic
// Multilingual Plane into four cells, such as U+20000:
//   1. 0xD840, attr=0x107
//   2. 0xD840, attr=0x207
//   3. 0xDC00, attr=0x107
//   4. 0xDC00, attr=0x207
// Even in the Traditional Chinese locale on Windows 10, this text is rendered
// as two boxes, but if those boxes are copied-and-pasted, the character is
// copied correctly.
static inline void scanUnicodeScalarValue(
    const CHAR_INFO *data, int width,
    int &outCellCount, unsigned int &outCharValue)
{
    ASSERT(width >= 1);

    const int w1 = isFullWidthCharacter(data, width) ? 2 : 1;
    const wchar_t c1 = data[0].Char.UnicodeChar;

    if ((c1 & 0xF800) == 0xD800) {
        // The first cell is either a leading or trailing surrogate pair.
        if ((c1 & 0xFC00) != 0xD800 ||
                width <= w1 ||
                ((data[w1].Char.UnicodeChar & 0xFC00) != 0xDC00)) {
            // Invalid surrogate pair
            outCellCount = w1;
            outCharValue = '?';
        } else {
            // Valid surrogate pair
            outCellCount = w1 + (isFullWidthCharacter(&data[w1], width - w1) ? 2 : 1);
            outCharValue = decodeSurrogatePair(c1, data[w1].Char.UnicodeChar);
        }
    } else {
        outCellCount = w1;
        outCharValue = c1;
    }
}

} // anonymous namespace

void Terminal::reset(SendClearFlag sendClearFirst, int64_t newLine)
{
    if (sendClearFirst == SendClear && !m_plainMode) {
        // 0m   ==> reset SGR parameters
        // 1;1H ==> move cursor to top-left position
        // 2J   ==> clear the entire screen
        m_output.write(CSI "0m" CSI "1;1H" CSI "2J");
    }
    m_remoteLine = newLine;
    m_remoteColumn = 0;
    m_lineData.clear();
    m_cursorHidden = false;
    m_remoteColor = -1;
}

void Terminal::sendLine(int64_t line, const CHAR_INFO *lineData, int width,
                        int cursorColumn)
{
    ASSERT(width >= 1);

    moveTerminalToLine(line);

    // If possible, see if we can append to what we've already output for this
    // line.
    if (m_lineDataValid) {
        ASSERT(m_lineData.size() == static_cast<size_t>(m_remoteColumn));
        if (m_remoteColumn > 0) {
            // In normal mode, if m_lineData.size() equals `width`, then we
            // will have trouble outputing the "erase rest of line" command,
            // which must be output before reaching the end of the line.  In
            // plain mode, we don't output that command, so we're OK with a
            // full line.
            bool okWidth = false;
            if (m_plainMode) {
                okWidth = static_cast<size_t>(width) >= m_lineData.size();
            } else {
                okWidth = static_cast<size_t>(width) > m_lineData.size();
            }
            if (!okWidth ||
                    memcmp(m_lineData.data(), lineData,
                           sizeof(CHAR_INFO) * m_lineData.size()) != 0) {
                m_lineDataValid = false;
            }
        }
    }
    if (!m_lineDataValid) {
        // We can't reuse, so we must reset this line.
        hideTerminalCursor();
        if (m_plainMode) {
            // We can't backtrack, so repeat this line.
            m_output.write("\r\n");
        } else {
            m_output.write("\r");
        }
        m_lineDataValid = true;
        m_lineData.clear();
        m_remoteColumn = 0;
    }

    std::string &termLine = m_termLineWorkingBuffer;
    termLine.clear();
    size_t trimmedLineLength = 0;
    int trimmedCellCount = m_lineData.size();
    bool alreadyErasedLine = false;

    int cellCount = 1;
    for (int i = m_lineData.size(); i < width; i += cellCount) {
        if (m_outputColor) {
            int color = lineData[i].Attributes & COLOR_ATTRIBUTE_MASK;
            if (color != m_remoteColor) {
                outputSetColor(termLine, color);
                trimmedLineLength = termLine.size();
                m_remoteColor = color;

                // All the cells just up to this color change will be output.
                trimmedCellCount = i;
            }
        }
        unsigned int ch;
        scanUnicodeScalarValue(&lineData[i], width - i, cellCount, ch);
        if (ch == ' ') {
            // Tentatively add this space character.  We'll only output it if
            // we see something interesting after it.
            termLine.push_back(' ');
        } else {
            if (i + cellCount == width) {
                // We'd like to erase the line after outputting all non-blank
                // characters, but this doesn't work if the last cell in the
                // line is non-blank.  At the point, the cursor is positioned
                // just past the end of the line, but in many terminals,
                // issuing a CSI 0K at that point also erases the last cell in
                // the line.  Work around this behavior by issuing the erase
                // one character early in that case.
                if (!m_plainMode) {
                    termLine.append(CSI "0K"); // Erase from cursor to EOL
                }
                alreadyErasedLine = true;
            }
            ch = fixSpecialCharacters(ch);
            char enc[4];
            int enclen = encodeUtf8(enc, ch);
            if (enclen == 0) {
                enc[0] = '?';
                enclen = 1;
            }
            termLine.append(enc, enclen);
            trimmedLineLength = termLine.size();

            // All the cells up to and including this cell will be output.
            trimmedCellCount = i + cellCount;
        }
    }

    if (cursorColumn != -1 && trimmedCellCount > cursorColumn) {
        // The line content would run past the cursor, so hide it before we
        // output.
        hideTerminalCursor();
    }

    m_output.write(termLine.data(), trimmedLineLength);
    if (!alreadyErasedLine && !m_plainMode) {
        m_output.write(CSI "0K"); // Erase from cursor to EOL
    }

    ASSERT(trimmedCellCount <= width);
    m_lineData.insert(m_lineData.end(),
                      &lineData[m_lineData.size()],
                      &lineData[trimmedCellCount]);
    m_remoteColumn = trimmedCellCount;
}

void Terminal::showTerminalCursor(int column, int64_t line)
{
    moveTerminalToLine(line);
    if (!m_plainMode) {
        if (m_remoteColumn != column) {
            char buffer[32];
            winpty_snprintf(buffer, CSI "%dG", column + 1);
            m_output.write(buffer);
            m_lineDataValid = (column == 0);
            m_lineData.clear();
            m_remoteColumn = column;
        }
        if (m_cursorHidden) {
            m_output.write(CSI "?25h");
            m_cursorHidden = false;
        }
    }
}

void Terminal::hideTerminalCursor()
{
    if (!m_plainMode) {
        if (m_cursorHidden) {
            return;
        }
        m_output.write(CSI "?25l");
        m_cursorHidden = true;
    }
}

void Terminal::moveTerminalToLine(int64_t line)
{
    if (line == m_remoteLine) {
        return;
    }

    // Do not use CPL or CNL.  Konsole 2.5.4 does not support Cursor Previous
    // Line (CPL) -- there are "Undecodable sequence" errors.  gnome-terminal
    // 2.32.0 does handle it.  Cursor Next Line (CNL) does nothing if the
    // cursor is on the last line already.

    hideTerminalCursor();

    if (line < m_remoteLine) {
        if (m_plainMode) {
            // We can't backtrack, so instead repeat the lines again.
            m_output.write("\r\n");
            m_remoteLine = line;
        } else {
            // Backtrack and overwrite previous lines.
            // CUrsor Up (CUU)
            char buffer[32];
            winpty_snprintf(buffer, "\r" CSI "%uA",
                static_cast<unsigned int>(m_remoteLine - line));
            m_output.write(buffer);
            m_remoteLine = line;
        }
    } else if (line > m_remoteLine) {
        while (line > m_remoteLine) {
            m_output.write("\r\n");
            m_remoteLine++;
        }
    }

    m_lineDataValid = true;
    m_lineData.clear();
    m_remoteColumn = 0;
}

void Terminal::enableMouseMode(bool enabled)
{
    if (m_mouseModeEnabled == enabled || m_plainMode) {
        return;
    }
    m_mouseModeEnabled = enabled;
    if (enabled) {
        // Start by disabling UTF-8 coordinate mode (1005), just in case we
        // have a terminal that does not support 1006/1015 modes, and 1005
        // happens to be enabled.  The UTF-8 coordinates can't be unambiguously
        // decoded.
        //
        // Enable basic mouse support first (1000), then try to switch to
        // button-move mode (1002), then try full mouse-move mode (1003).
        // Terminals that don't support a mode will be stuck at the highest
        // mode they do support.
        //
        // Enable encoding mode 1015 first, then try to switch to 1006.  On
        // some terminals, both modes will be enabled, but 1006 will have
        // priority.  On other terminals, 1006 wins because it's listed last.
        //
        // See misc/MouseInputNotes.txt for details.
        m_output.write(
            CSI "?1005l"
            CSI "?1000h" CSI "?1002h" CSI "?1003h" CSI "?1015h" CSI "?1006h");
    } else {
        // Resetting both encoding modes (1006 and 1015) is necessary, but
        // apparently we only need to use reset on one of the 100[023] modes.
        // Doing both doesn't hurt.
        m_output.write(
            CSI "?1006l" CSI "?1015l" CSI "?1003l" CSI "?1002l" CSI "?1000l");
    }
}
