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

#ifndef AGENT_SCRAPER_H
#define AGENT_SCRAPER_H

#include <windows.h>

#include <stdint.h>

#include <memory>
#include <vector>

#include "ConsoleLine.h"
#include "Coord.h"
#include "LargeConsoleRead.h"
#include "SmallRect.h"
#include "Terminal.h"

class ConsoleScreenBufferInfo;
class Win32Console;
class Win32ConsoleBuffer;

// We must be able to issue a single ReadConsoleOutputW call of
// MAX_CONSOLE_WIDTH characters, and a single read of approximately several
// hundred fewer characters than BUFFER_LINE_COUNT.
const int BUFFER_LINE_COUNT = 3000;
const int MAX_CONSOLE_WIDTH = 2500;
const int MAX_CONSOLE_HEIGHT = 2000;
const int SYNC_MARKER_LEN = 16;
const int SYNC_MARKER_MARGIN = 200;

class Scraper {
public:
    Scraper(
        Win32Console &console,
        Win32ConsoleBuffer &buffer,
        std::unique_ptr<Terminal> terminal,
        Coord initialSize);
    ~Scraper();
    void resizeWindow(Win32ConsoleBuffer &buffer,
                      Coord newSize,
                      ConsoleScreenBufferInfo &finalInfoOut);
    void scrapeBuffer(Win32ConsoleBuffer &buffer,
                      ConsoleScreenBufferInfo &finalInfoOut);
    Terminal &terminal() { return *m_terminal; }

private:
    void resetConsoleTracking(
        Terminal::SendClearFlag sendClear, int64_t scrapedLineCount);
    void markEntireWindowDirty(const SmallRect &windowRect);
    void scanForDirtyLines(const SmallRect &windowRect);
    void clearBufferLines(int firstRow, int count);
    void resizeImpl(const ConsoleScreenBufferInfo &origInfo);
    void syncConsoleContentAndSize(bool forceResize,
                                   ConsoleScreenBufferInfo &finalInfoOut);
    WORD attributesMask();
    void directScrapeOutput(const ConsoleScreenBufferInfo &info,
                            bool consoleCursorVisible);
    bool scrollingScrapeOutput(const ConsoleScreenBufferInfo &info,
                               bool consoleCursorVisible,
                               bool tentative);
    void syncMarkerText(CHAR_INFO (&output)[SYNC_MARKER_LEN]);
    int findSyncMarker();
    void createSyncMarker(int row);

private:
    Win32Console &m_console;
    Win32ConsoleBuffer *m_consoleBuffer = nullptr;
    std::unique_ptr<Terminal> m_terminal;

    int m_syncRow = -1;
    unsigned int m_syncCounter = 0;

    bool m_directMode = false;
    Coord m_ptySize;
    int64_t m_scrapedLineCount = 0;
    int64_t m_scrolledCount = 0;
    int64_t m_maxBufferedLine = -1;
    LargeConsoleReadBuffer m_readBuffer;
    std::vector<ConsoleLine> m_bufferData;
    int m_dirtyWindowTop = -1;
    int m_dirtyLineCount = 0;
};

#endif // AGENT_SCRAPER_H
