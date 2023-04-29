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

//
// ConsoleLine
//
// This data structure keep tracks of the previous CHAR_INFO content of an
// output line and determines when a line has changed.  Detecting line changes
// is made complicated by terminal resizing.
//

#include "ConsoleLine.h"

#include <algorithm>

#include "../shared/WinptyAssert.h"

static CHAR_INFO blankChar(WORD attributes)
{
    // N.B.: As long as we write to UnicodeChar rather than AsciiChar, there
    // are no padding bytes that could contain uninitialized bytes.  This fact
    // is important for efficient comparison.
    CHAR_INFO ret;
    ret.Attributes = attributes;
    ret.Char.UnicodeChar = L' ';
    return ret;
}

static bool isLineBlank(const CHAR_INFO *line, int length, WORD attributes)
{
    for (int col = 0; col < length; ++col) {
        if (line[col].Attributes != attributes ||
                line[col].Char.UnicodeChar != L' ') {
            return false;
        }
    }
    return true;
}

static inline bool areLinesEqual(
    const CHAR_INFO *line1,
    const CHAR_INFO *line2,
    int length)
{
    return memcmp(line1, line2, sizeof(CHAR_INFO) * length) == 0;
}

ConsoleLine::ConsoleLine() : m_prevLength(0)
{
}

void ConsoleLine::reset()
{
    m_prevLength = 0;
    m_prevData.clear();
}

// Determines whether the given line is sufficiently different from the
// previously seen line as to justify reoutputting the line.  The function
// also sets the `ConsoleLine` to the given line, exactly as if `setLine` had
// been called.
bool ConsoleLine::detectChangeAndSetLine(const CHAR_INFO *const line, const int newLength)
{
    ASSERT(newLength >= 1);
    ASSERT(m_prevLength <= static_cast<int>(m_prevData.size()));

    if (newLength == m_prevLength) {
        bool equalLines = areLinesEqual(m_prevData.data(), line, newLength);
        if (!equalLines) {
            setLine(line, newLength);
        }
        return !equalLines;
    } else {
        if (m_prevLength == 0) {
            setLine(line, newLength);
            return true;
        }

        ASSERT(m_prevLength >= 1);
        const WORD prevBlank = m_prevData[m_prevLength - 1].Attributes;
        const WORD newBlank = line[newLength - 1].Attributes;

        bool equalLines = false;
        if (newLength < m_prevLength) {
            // The line has become shorter.  The lines are equal if the common
            // part is equal, and if the newly truncated characters were blank.
            equalLines =
                areLinesEqual(m_prevData.data(), line, newLength) &&
                isLineBlank(m_prevData.data() + newLength,
                            m_prevLength - newLength,
                            newBlank);
        } else {
            //
            // The line has become longer.  The lines are equal if the common
            // part is equal, and if both the extra characters and any
            // potentially reexposed characters are blank.
            //
            // Two of the most relevant terminals for winpty--mintty and
            // jediterm--don't (currently) erase the obscured content when a
            // line is cleared, so we should anticipate its existence when
            // making a terminal wider and reoutput the line.  See:
            //
            //  * https://github.com/mintty/mintty/issues/480
            //  * https://github.com/JetBrains/jediterm/issues/118
            //
            ASSERT(newLength > m_prevLength);
            equalLines =
                areLinesEqual(m_prevData.data(), line, m_prevLength) &&
                isLineBlank(m_prevData.data() + m_prevLength,
                            std::min<int>(m_prevData.size(), newLength) - m_prevLength,
                            prevBlank) &&
                isLineBlank(line + m_prevLength,
                            newLength - m_prevLength,
                            prevBlank);
        }
        setLine(line, newLength);
        return !equalLines;
    }
}

void ConsoleLine::setLine(const CHAR_INFO *const line, const int newLength)
{
    if (static_cast<int>(m_prevData.size()) < newLength) {
        m_prevData.resize(newLength);
    }
    memcpy(m_prevData.data(), line, sizeof(CHAR_INFO) * newLength);
    m_prevLength = newLength;
}

void ConsoleLine::blank(WORD attributes)
{
    m_prevData.resize(1);
    m_prevData[0] = blankChar(attributes);
    m_prevLength = 1;
}
