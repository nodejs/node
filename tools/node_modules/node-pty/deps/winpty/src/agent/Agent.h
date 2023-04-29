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

#ifndef AGENT_H
#define AGENT_H

#include <windows.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "DsrSender.h"
#include "EventLoop.h"
#include "Win32Console.h"

class ConsoleInput;
class NamedPipe;
class ReadBuffer;
class Scraper;
class WriteBuffer;
class Win32ConsoleBuffer;

class Agent : public EventLoop, public DsrSender
{
public:
    Agent(LPCWSTR controlPipeName,
          uint64_t agentFlags,
          int mouseMode,
          int initialCols,
          int initialRows);
    virtual ~Agent();
    void sendDsr() override;

private:
    NamedPipe &connectToControlPipe(LPCWSTR pipeName);
    NamedPipe &createDataServerPipe(bool write, const wchar_t *kind);

private:
    void pollControlPipe();
    void handlePacket(ReadBuffer &packet);
    void writePacket(WriteBuffer &packet);
    void handleStartProcessPacket(ReadBuffer &packet);
    void handleSetSizePacket(ReadBuffer &packet);
    void handleGetConsoleProcessListPacket(ReadBuffer &packet);
    void pollConinPipe();

protected:
    virtual void onPollTimeout() override;
    virtual void onPipeIo(NamedPipe &namedPipe) override;

private:
    void autoClosePipesForShutdown();
    std::unique_ptr<Win32ConsoleBuffer> openPrimaryBuffer();
    void resizeWindow(int cols, int rows);
    void scrapeBuffers();
    void syncConsoleTitle();

private:
    const bool m_useConerr;
    const bool m_plainMode;
    const int m_mouseMode;
    Win32Console m_console;
    std::unique_ptr<Scraper> m_primaryScraper;
    std::unique_ptr<Scraper> m_errorScraper;
    std::unique_ptr<Win32ConsoleBuffer> m_errorBuffer;
    NamedPipe *m_controlPipe = nullptr;
    NamedPipe *m_coninPipe = nullptr;
    NamedPipe *m_conoutPipe = nullptr;
    NamedPipe *m_conerrPipe = nullptr;
    bool m_autoShutdown = false;
    bool m_exitAfterShutdown = false;
    bool m_closingOutputPipes = false;
    std::unique_ptr<ConsoleInput> m_consoleInput;
    HANDLE m_childProcess = nullptr;

    // If the title is initialized to the empty string, then cmd.exe will
    // sometimes print this error:
    //     Not enough storage is available to process this command.
    // It happens on Windows 7 when logged into a Cygwin SSH session, for
    // example.  Using a title of a single space character avoids the problem.
    // See https://github.com/rprichard/winpty/issues/74.
    std::wstring m_currentTitle = L" ";
};

#endif // AGENT_H
