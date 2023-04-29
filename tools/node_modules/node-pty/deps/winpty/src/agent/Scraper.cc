// Copyright (c) 2011-2016 Ryan Prichard
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

#include "Scraper.h"

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <utility>

#include "../shared/WinptyAssert.h"
#include "../shared/winpty_snprintf.h"

#include "ConsoleFont.h"
#include "Win32Console.h"
#include "Win32ConsoleBuffer.h"

namespace {

template <typename T>
T constrained(T min, T val, T max) {
    ASSERT(min <= max);
    return std::min(std::max(min, val), max);
}

} // anonymous namespace

Scraper::Scraper(
        Win32Console &console,
        Win32ConsoleBuffer &buffer,
        std::unique_ptr<Terminal> terminal,
        Coord initialSize) :
    m_console(console),
    m_terminal(std::move(terminal)),
    m_ptySize(initialSize)
{
    m_consoleBuffer = &buffer;

    resetConsoleTracking(Terminal::OmitClear, buffer.windowRect().top());

    m_bufferData.resize(BUFFER_LINE_COUNT);

    // Setup the initial screen buffer and window size.
    //
    // Use SetConsoleWindowInfo to shrink the console window as much as
    // possible -- to a 1x1 cell at the top-left.  This call always succeeds.
    // Prior to the new Windows 10 console, it also actually resizes the GUI
    // window to 1x1 cell.  Nevertheless, even though the GUI window can
    // therefore be narrower than its minimum, calling
    // SetConsoleScreenBufferSize with a 1x1 size still fails.
    //
    // While the small font intends to support large buffers, a user could
    // still hit a limit imposed by their monitor width, so cap the new window
    // size to GetLargestConsoleWindowSize().
    setSmallFont(buffer.conout(), initialSize.X, m_console.isNewW10());
    buffer.moveWindow(SmallRect(0, 0, 1, 1));
    buffer.resizeBufferRange(Coord(initialSize.X, BUFFER_LINE_COUNT));
    const auto largest = GetLargestConsoleWindowSize(buffer.conout());
    buffer.moveWindow(SmallRect(
        0, 0,
        std::min(initialSize.X, largest.X),
        std::min(initialSize.Y, largest.Y)));
    buffer.setCursorPosition(Coord(0, 0));

    // For the sake of the color translation heuristic, set the console color
    // to LtGray-on-Black.
    buffer.setTextAttribute(Win32ConsoleBuffer::kDefaultAttributes);
    buffer.clearAllLines(m_consoleBuffer->bufferInfo());

    m_consoleBuffer = nullptr;
}

Scraper::~Scraper()
{
}

// Whether or not the agent is frozen on entry, it will be frozen on exit.
void Scraper::resizeWindow(Win32ConsoleBuffer &buffer,
                           Coord newSize,
                           ConsoleScreenBufferInfo &finalInfoOut)
{
    m_consoleBuffer = &buffer;
    m_ptySize = newSize;
    syncConsoleContentAndSize(true, finalInfoOut);
    m_consoleBuffer = nullptr;
}

// This function may freeze the agent, but it will not unfreeze it.
void Scraper::scrapeBuffer(Win32ConsoleBuffer &buffer,
                           ConsoleScreenBufferInfo &finalInfoOut)
{
    m_consoleBuffer = &buffer;
    syncConsoleContentAndSize(false, finalInfoOut);
    m_consoleBuffer = nullptr;
}

void Scraper::resetConsoleTracking(
    Terminal::SendClearFlag sendClear, int64_t scrapedLineCount)
{
    for (ConsoleLine &line : m_bufferData) {
        line.reset();
    }
    m_syncRow = -1;
    m_scrapedLineCount = scrapedLineCount;
    m_scrolledCount = 0;
    m_maxBufferedLine = -1;
    m_dirtyWindowTop = -1;
    m_dirtyLineCount = 0;
    m_terminal->reset(sendClear, m_scrapedLineCount);
}

// Detect window movement.  If the window moves down (presumably as a
// result of scrolling), then assume that all screen buffer lines down to
// the bottom of the window are dirty.
void Scraper::markEntireWindowDirty(const SmallRect &windowRect)
{
    m_dirtyLineCount = std::max(m_dirtyLineCount,
                                windowRect.top() + windowRect.height());
}

// Scan the screen buffer and advance the dirty line count when we find
// non-empty lines.
void Scraper::scanForDirtyLines(const SmallRect &windowRect)
{
    const int w = m_readBuffer.rect().width();
    ASSERT(m_dirtyLineCount >= 1);
    const CHAR_INFO *const prevLine =
        m_readBuffer.lineData(m_dirtyLineCount - 1);
    WORD prevLineAttr = prevLine[w - 1].Attributes;
    const int stopLine = windowRect.top() + windowRect.height();

    for (int line = m_dirtyLineCount; line < stopLine; ++line) {
        const CHAR_INFO *lineData = m_readBuffer.lineData(line);
        for (int col = 0; col < w; ++col) {
            const WORD colAttr = lineData[col].Attributes;
            if (lineData[col].Char.UnicodeChar != L' ' ||
                    colAttr != prevLineAttr) {
                m_dirtyLineCount = line + 1;
                break;
            }
        }
        prevLineAttr = lineData[w - 1].Attributes;
    }
}

// Clear lines in the line buffer.  The `firstRow` parameter is in
// screen-buffer coordinates.
void Scraper::clearBufferLines(
        const int firstRow,
        const int count)
{
    ASSERT(!m_directMode);
    for (int row = firstRow; row < firstRow + count; ++row) {
        const int64_t bufLine = row + m_scrolledCount;
        m_maxBufferedLine = std::max(m_maxBufferedLine, bufLine);
        m_bufferData[bufLine % BUFFER_LINE_COUNT].blank(
            Win32ConsoleBuffer::kDefaultAttributes);
    }
}

static bool cursorInWindow(const ConsoleScreenBufferInfo &info)
{
    return info.dwCursorPosition.Y >= info.srWindow.Top &&
           info.dwCursorPosition.Y <= info.srWindow.Bottom;
}

void Scraper::resizeImpl(const ConsoleScreenBufferInfo &origInfo)
{
    ASSERT(m_console.frozen());
    const int cols = m_ptySize.X;
    const int rows = m_ptySize.Y;
    Coord finalBufferSize;

    {
        //
        // To accommodate Windows 10, erase all lines up to the top of the
        // visible window.  It's hard to tell whether this is strictly
        // necessary.  It ensures that the sync marker won't move downward,
        // and it ensures that we won't repeat lines that have already scrolled
        // up into the scrollback.
        //
        // It *is* possible for these blank lines to reappear in the visible
        // window (e.g. if the window is made taller), but because we blanked
        // the lines in the line buffer, we still don't output them again.
        //
        const Coord origBufferSize = origInfo.bufferSize();
        const SmallRect origWindowRect = origInfo.windowRect();

        if (m_directMode) {
            for (ConsoleLine &line : m_bufferData) {
                line.reset();
            }
        } else {
            m_consoleBuffer->clearLines(0, origWindowRect.Top, origInfo);
            clearBufferLines(0, origWindowRect.Top);
            if (m_syncRow != -1) {
                createSyncMarker(std::min(
                    m_syncRow,
                    BUFFER_LINE_COUNT - rows
                                      - SYNC_MARKER_LEN
                                      - SYNC_MARKER_MARGIN));
            }
        }

        finalBufferSize = Coord(
            cols,
            // If there was previously no scrollback (e.g. a full-screen app
            // in direct mode) and we're reducing the window height, then
            // reduce the console buffer's height too.
            (origWindowRect.height() == origBufferSize.Y)
                ? rows
                : std::max<int>(rows, origBufferSize.Y));

        // Reset the console font size.  We need to do this before shrinking
        // the window, because we might need to make the font bigger to permit
        // a smaller window width.  Making the font smaller could expand the
        // screen buffer, which would hang the conhost process in the
        // Windows 10 (10240 build) if the console selection is in progress, so
        // unfreeze it first.
        m_console.setFrozen(false);
        setSmallFont(m_consoleBuffer->conout(), cols, m_console.isNewW10());
    }

    // We try to make the font small enough so that the entire screen buffer
    // fits on the monitor, but it can't be guaranteed.
    const auto largest =
        GetLargestConsoleWindowSize(m_consoleBuffer->conout());
    const short visibleCols = std::min<short>(cols, largest.X);
    const short visibleRows = std::min<short>(rows, largest.Y);

    {
        // Make the window small enough.  We want the console frozen during
        // this step so we don't accidentally move the window above the cursor.
        m_console.setFrozen(true);
        const auto info = m_consoleBuffer->bufferInfo();
        const auto &bufferSize = info.dwSize;
        const int tmpWindowWidth = std::min(bufferSize.X, visibleCols);
        const int tmpWindowHeight = std::min(bufferSize.Y, visibleRows);
        SmallRect tmpWindowRect(
            0,
            std::min<int>(bufferSize.Y - tmpWindowHeight,
                          info.windowRect().Top),
            tmpWindowWidth,
            tmpWindowHeight);
        if (cursorInWindow(info)) {
            tmpWindowRect = tmpWindowRect.ensureLineIncluded(
                info.cursorPosition().Y);
        }
        m_consoleBuffer->moveWindow(tmpWindowRect);
    }

    {
        // Resize the buffer to the final desired size.
        m_console.setFrozen(false);
        m_consoleBuffer->resizeBufferRange(finalBufferSize);
    }

    {
        // Expand the window to its full size.
        m_console.setFrozen(true);
        const ConsoleScreenBufferInfo info = m_consoleBuffer->bufferInfo();

        SmallRect finalWindowRect(
            0,
            std::min<int>(info.bufferSize().Y - visibleRows,
                          info.windowRect().Top),
            visibleCols,
            visibleRows);

        //
        // Once a line in the screen buffer is "dirty", it should stay visible
        // in the console window, so that we continue to update its content in
        // the terminal.  This code is particularly (only?) necessary on
        // Windows 10, where making the buffer wider can rewrap lines and move
        // the console window upward.
        //
        if (!m_directMode && m_dirtyLineCount > finalWindowRect.Bottom + 1) {
            // In theory, we avoid ensureLineIncluded, because, a massive
            // amount of output could have occurred while the console was
            // unfrozen, so that the *top* of the window is now below the
            // dirtiest tracked line.
            finalWindowRect = SmallRect(
                0, m_dirtyLineCount - visibleRows,
                visibleCols, visibleRows);
        }

        // Highest priority constraint: ensure that the cursor remains visible.
        if (cursorInWindow(info)) {
            finalWindowRect = finalWindowRect.ensureLineIncluded(
                info.cursorPosition().Y);
        }

        m_consoleBuffer->moveWindow(finalWindowRect);
        m_dirtyWindowTop = finalWindowRect.Top;
    }

    ASSERT(m_console.frozen());
}

void Scraper::syncConsoleContentAndSize(
    bool forceResize,
    ConsoleScreenBufferInfo &finalInfoOut)
{
    // We'll try to avoid freezing the console by reading large chunks (or
    // all!) of the screen buffer without otherwise attempting to synchronize
    // with the console application.  We can only do this on Windows 10 and up
    // because:
    //  - Prior to Windows 8, the size of a ReadConsoleOutputW call was limited
    //    by the ~32KB RPC buffer.
    //  - Prior to Windows 10, an out-of-range read region crashes the caller.
    //    (See misc/WindowsBugCrashReader.cc.)
    //
    if (!m_console.isNewW10() || forceResize) {
        m_console.setFrozen(true);
    }

    const ConsoleScreenBufferInfo info = m_consoleBuffer->bufferInfo();
    bool cursorVisible = true;
    CONSOLE_CURSOR_INFO cursorInfo = {};
    if (!GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo)) {
        trace("GetConsoleCursorInfo failed");
    } else {
        cursorVisible = cursorInfo.bVisible != 0;
    }

    // If an app resizes the buffer height, then we enter "direct mode", where
    // we stop trying to track incremental console changes.
    const bool newDirectMode = (info.bufferSize().Y != BUFFER_LINE_COUNT);
    if (newDirectMode != m_directMode) {
        trace("Entering %s mode", newDirectMode ? "direct" : "scrolling");
        resetConsoleTracking(Terminal::SendClear,
                             newDirectMode ? 0 : info.windowRect().top());
        m_directMode = newDirectMode;

        // When we switch from direct->scrolling mode, make sure the console is
        // the right size.
        if (!m_directMode) {
            m_console.setFrozen(true);
            forceResize = true;
        }
    }

    if (m_directMode) {
        // In direct-mode, resizing the console redraws the terminal, so do it
        // before scraping.
        if (forceResize) {
            resizeImpl(info);
        }
        directScrapeOutput(info, cursorVisible);
    } else {
        if (!m_console.frozen()) {
            if (!scrollingScrapeOutput(info, cursorVisible, true)) {
                m_console.setFrozen(true);
            }
        }
        if (m_console.frozen()) {
            scrollingScrapeOutput(info, cursorVisible, false);
        }
        // In scrolling mode, we want to scrape before resizing, because we'll
        // erase everything in the console buffer up to the top of the console
        // window.
        if (forceResize) {
            resizeImpl(info);
        }
    }

    finalInfoOut = forceResize ? m_consoleBuffer->bufferInfo() : info;
}

// Try to match Windows' behavior w.r.t. to the LVB attribute flags.  In some
// situations, Windows ignores the LVB flags on a character cell because of
// backwards compatibility -- apparently some programs set the flags without
// intending to enable reverse-video or underscores.
//
// [rprichard 2017-01-15] I haven't actually noticed any old programs that need
// this treatment -- the motivation for this function comes from the MSDN
// documentation for SetConsoleMode and ENABLE_LVB_GRID_WORLDWIDE.
WORD Scraper::attributesMask()
{
    const auto WINPTY_ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x4u;
    const auto WINPTY_ENABLE_LVB_GRID_WORLDWIDE          = 0x10u;
    const auto WINPTY_COMMON_LVB_REVERSE_VIDEO           = 0x4000u;
    const auto WINPTY_COMMON_LVB_UNDERSCORE              = 0x8000u;

    const auto cp = GetConsoleOutputCP();
    const auto isCjk = (cp == 932 || cp == 936 || cp == 949 || cp == 950);

    const DWORD outputMode = [this]{
        ASSERT(this->m_consoleBuffer != nullptr);
        DWORD mode = 0;
        if (!GetConsoleMode(this->m_consoleBuffer->conout(), &mode)) {
            mode = 0;
        }
        return mode;
    }();
    const bool hasEnableLvbGridWorldwide =
        (outputMode & WINPTY_ENABLE_LVB_GRID_WORLDWIDE) != 0;
    const bool hasEnableVtProcessing =
        (outputMode & WINPTY_ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;

    // The new Windows 10 console (as of 14393) seems to respect
    // COMMON_LVB_REVERSE_VIDEO even in CP437 w/o the other enabling modes, so
    // try to match that behavior.
    const auto isReverseSupported =
        isCjk || hasEnableLvbGridWorldwide || hasEnableVtProcessing || m_console.isNewW10();
    const auto isUnderscoreSupported =
        isCjk || hasEnableLvbGridWorldwide || hasEnableVtProcessing;

    WORD mask = ~0;
    if (!isReverseSupported)    { mask &= ~WINPTY_COMMON_LVB_REVERSE_VIDEO; }
    if (!isUnderscoreSupported) { mask &= ~WINPTY_COMMON_LVB_UNDERSCORE; }
    return mask;
}

void Scraper::directScrapeOutput(const ConsoleScreenBufferInfo &info,
                                 bool consoleCursorVisible)
{
    const SmallRect windowRect = info.windowRect();

    const SmallRect scrapeRect(
        windowRect.left(), windowRect.top(),
        std::min<SHORT>(std::min(windowRect.width(), m_ptySize.X),
                        MAX_CONSOLE_WIDTH),
        std::min<SHORT>(std::min(windowRect.height(), m_ptySize.Y),
                        BUFFER_LINE_COUNT));
    const int w = scrapeRect.width();
    const int h = scrapeRect.height();

    const Coord cursor = info.cursorPosition();
    const bool showTerminalCursor =
        consoleCursorVisible && scrapeRect.contains(cursor);
    const int cursorColumn = !showTerminalCursor ? -1 : cursor.X - scrapeRect.Left;
    const int cursorLine = !showTerminalCursor ? -1 : cursor.Y - scrapeRect.Top;

    if (!showTerminalCursor) {
        m_terminal->hideTerminalCursor();
    }

    largeConsoleRead(m_readBuffer, *m_consoleBuffer, scrapeRect, attributesMask());

    for (int line = 0; line < h; ++line) {
        const CHAR_INFO *const curLine =
            m_readBuffer.lineData(scrapeRect.top() + line);
        ConsoleLine &bufLine = m_bufferData[line];
        if (bufLine.detectChangeAndSetLine(curLine, w)) {
            const int lineCursorColumn =
                line == cursorLine ? cursorColumn : -1;
            m_terminal->sendLine(line, curLine, w, lineCursorColumn);
        }
    }

    if (showTerminalCursor) {
        m_terminal->showTerminalCursor(cursorColumn, cursorLine);
    }
}

bool Scraper::scrollingScrapeOutput(const ConsoleScreenBufferInfo &info,
                                    bool consoleCursorVisible,
                                    bool tentative)
{
    const Coord cursor = info.cursorPosition();
    const SmallRect windowRect = info.windowRect();

    if (m_syncRow != -1) {
        // If a synchronizing marker was placed into the history, look for it
        // and adjust the scroll count.
        const int markerRow = findSyncMarker();
        if (markerRow == -1) {
            if (tentative) {
                // I *think* it's possible to keep going, but it's simple to
                // bail out.
                return false;
            }
            // Something has happened.  Reset the terminal.
            trace("Sync marker has disappeared -- resetting the terminal"
                  " (m_syncCounter=%u)",
                  m_syncCounter);
            resetConsoleTracking(Terminal::SendClear, windowRect.top());
        } else if (markerRow != m_syncRow) {
            ASSERT(markerRow < m_syncRow);
            m_scrolledCount += (m_syncRow - markerRow);
            m_syncRow = markerRow;
            // If the buffer has scrolled, then the entire window is dirty.
            markEntireWindowDirty(windowRect);
        }
    }

    // Creating a new sync row requires clearing part of the console buffer, so
    // avoid doing it if there's already a sync row that's good enough.
    const int newSyncRow =
        static_cast<int>(windowRect.top()) - SYNC_MARKER_LEN - SYNC_MARKER_MARGIN;
    const bool shouldCreateSyncRow =
        newSyncRow >= m_syncRow + SYNC_MARKER_LEN + SYNC_MARKER_MARGIN;
    if (tentative && shouldCreateSyncRow) {
        // It's difficult even in principle to put down a new marker if the
        // console can scroll an arbitrarily amount while we're writing.
        return false;
    }

    // Update the dirty line count:
    //  - If the window has moved, the entire window is dirty.
    //  - Everything up to the cursor is dirty.
    //  - All lines above the window are dirty.
    //  - Any non-blank lines are dirty.
    if (m_dirtyWindowTop != -1) {
        if (windowRect.top() > m_dirtyWindowTop) {
            // The window has moved down, presumably as a result of scrolling.
            markEntireWindowDirty(windowRect);
        } else if (windowRect.top() < m_dirtyWindowTop) {
            if (tentative) {
                // I *think* it's possible to keep going, but it's simple to
                // bail out.
                return false;
            }
            // The window has moved upward.  This is generally not expected to
            // happen, but the CMD/PowerShell CLS command will move the window
            // to the top as part of clearing everything else in the console.
            trace("Window moved upward -- resetting the terminal"
                  " (m_syncCounter=%u)",
                  m_syncCounter);
            resetConsoleTracking(Terminal::SendClear, windowRect.top());
        }
    }
    m_dirtyWindowTop = windowRect.top();
    m_dirtyLineCount = std::max(m_dirtyLineCount, cursor.Y + 1);
    m_dirtyLineCount = std::max(m_dirtyLineCount, (int)windowRect.top());

    // There will be at least one dirty line, because there is a cursor.
    ASSERT(m_dirtyLineCount >= 1);

    // The first line to scrape, in virtual line coordinates.
    const int64_t firstVirtLine = std::min(m_scrapedLineCount,
                                           windowRect.top() + m_scrolledCount);

    // Read all the data we will need from the console.  Start reading with the
    // first line to scrape, but adjust the the read area upward to account for
    // scanForDirtyLines' need to read the previous attribute.  Read to the
    // bottom of the window.  (It's not clear to me whether the
    // m_dirtyLineCount adjustment here is strictly necessary.  It isn't
    // necessary so long as the cursor is inside the current window.)
    const int firstReadLine = std::min<int>(firstVirtLine - m_scrolledCount,
                                            m_dirtyLineCount - 1);
    const int stopReadLine = std::max(windowRect.top() + windowRect.height(),
                                      m_dirtyLineCount);
    ASSERT(firstReadLine >= 0 && stopReadLine > firstReadLine);
    largeConsoleRead(m_readBuffer,
                     *m_consoleBuffer,
                     SmallRect(0, firstReadLine,
                               std::min<SHORT>(info.bufferSize().X,
                                               MAX_CONSOLE_WIDTH),
                               stopReadLine - firstReadLine),
                     attributesMask());

    // If we're scraping the buffer without freezing it, we have to query the
    // buffer position data separately from the buffer content, so the two
    // could easily be out-of-sync.  If they *are* out-of-sync, abort the
    // scrape operation and restart it frozen.  (We may have updated the
    // dirty-line high-water-mark, but that should be OK.)
    if (tentative) {
        const auto infoCheck = m_consoleBuffer->bufferInfo();
        if (info.bufferSize() != infoCheck.bufferSize() ||
                info.windowRect() != infoCheck.windowRect() ||
                info.cursorPosition() != infoCheck.cursorPosition()) {
            return false;
        }
        if (m_syncRow != -1 && m_syncRow != findSyncMarker()) {
            return false;
        }
    }

    if (shouldCreateSyncRow) {
        ASSERT(!tentative);
        createSyncMarker(newSyncRow);
    }

    // At this point, we're finished interacting (reading or writing) the
    // console, and we just need to convert our collected data into terminal
    // output.

    scanForDirtyLines(windowRect);

    // Note that it's possible for all the lines on the current window to
    // be non-dirty.

    // The line to stop scraping at, in virtual line coordinates.
    const int64_t stopVirtLine =
        std::min(m_dirtyLineCount, windowRect.top() + windowRect.height()) +
            m_scrolledCount;

    const bool showTerminalCursor =
        consoleCursorVisible && windowRect.contains(cursor);
    const int64_t cursorLine = !showTerminalCursor ? -1 : cursor.Y + m_scrolledCount;
    const int cursorColumn = !showTerminalCursor ? -1 : cursor.X;

    if (!showTerminalCursor) {
        m_terminal->hideTerminalCursor();
    }

    bool sawModifiedLine = false;

    const int w = m_readBuffer.rect().width();
    for (int64_t line = firstVirtLine; line < stopVirtLine; ++line) {
        const CHAR_INFO *curLine =
            m_readBuffer.lineData(line - m_scrolledCount);
        ConsoleLine &bufLine = m_bufferData[line % BUFFER_LINE_COUNT];
        if (line > m_maxBufferedLine) {
            m_maxBufferedLine = line;
            sawModifiedLine = true;
        }
        if (sawModifiedLine) {
            bufLine.setLine(curLine, w);
        } else {
            sawModifiedLine = bufLine.detectChangeAndSetLine(curLine, w);
        }
        if (sawModifiedLine) {
            const int lineCursorColumn =
                line == cursorLine ? cursorColumn : -1;
            m_terminal->sendLine(line, curLine, w, lineCursorColumn);
        }
    }

    m_scrapedLineCount = windowRect.top() + m_scrolledCount;

    if (showTerminalCursor) {
        m_terminal->showTerminalCursor(cursorColumn, cursorLine);
    }

    return true;
}

void Scraper::syncMarkerText(CHAR_INFO (&output)[SYNC_MARKER_LEN])
{
    // XXX: The marker text generated here could easily collide with ordinary
    // console output.  Does it make sense to try to avoid the collision?
    char str[SYNC_MARKER_LEN + 1];
    winpty_snprintf(str, "S*Y*N*C*%08x", m_syncCounter);
    for (int i = 0; i < SYNC_MARKER_LEN; ++i) {
        output[i].Char.UnicodeChar = str[i];
        output[i].Attributes = 7;
    }
}

int Scraper::findSyncMarker()
{
    ASSERT(m_syncRow >= 0);
    CHAR_INFO marker[SYNC_MARKER_LEN];
    CHAR_INFO column[BUFFER_LINE_COUNT];
    syncMarkerText(marker);
    SmallRect rect(0, 0, 1, m_syncRow + SYNC_MARKER_LEN);
    m_consoleBuffer->read(rect, column);
    int i;
    for (i = m_syncRow; i >= 0; --i) {
        int j;
        for (j = 0; j < SYNC_MARKER_LEN; ++j) {
            if (column[i + j].Char.UnicodeChar != marker[j].Char.UnicodeChar)
                break;
        }
        if (j == SYNC_MARKER_LEN)
            return i;
    }
    return -1;
}

void Scraper::createSyncMarker(int row)
{
    ASSERT(row >= 1);

    // Clear the lines around the marker to ensure that Windows 10's rewrapping
    // does not affect the marker.
    m_consoleBuffer->clearLines(row - 1, SYNC_MARKER_LEN + 1,
                                m_consoleBuffer->bufferInfo());

    // Write a new marker.
    m_syncCounter++;
    CHAR_INFO marker[SYNC_MARKER_LEN];
    syncMarkerText(marker);
    m_syncRow = row;
    SmallRect markerRect(0, m_syncRow, 1, SYNC_MARKER_LEN);
    m_consoleBuffer->write(markerRect, marker);
}
