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

#include "Agent.h"

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <utility>
#include <vector>

#include "../include/winpty_constants.h"

#include "../shared/AgentMsg.h"
#include "../shared/Buffer.h"
#include "../shared/DebugClient.h"
#include "../shared/GenRandom.h"
#include "../shared/StringBuilder.h"
#include "../shared/StringUtil.h"
#include "../shared/WindowsVersion.h"
#include "../shared/WinptyAssert.h"

#include "ConsoleFont.h"
#include "ConsoleInput.h"
#include "NamedPipe.h"
#include "Scraper.h"
#include "Terminal.h"
#include "Win32ConsoleBuffer.h"

namespace {

static BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType)
{
    if (dwCtrlType == CTRL_C_EVENT) {
        // Do nothing and claim to have handled the event.
        return TRUE;
    }
    return FALSE;
}

// We can detect the new Windows 10 console by observing the effect of the
// Mark command.  In older consoles, Mark temporarily moves the cursor to the
// top-left of the console window.  In the new console, the cursor isn't
// initially moved.
//
// We might like to use Mark to freeze the console, but we can't, because when
// the Mark command ends, the console moves the cursor back to its starting
// point, even if the console application has moved it in the meantime.
static void detectNewWindows10Console(
        Win32Console &console, Win32ConsoleBuffer &buffer)
{
    if (!isAtLeastWindows8()) {
        return;
    }

    ConsoleScreenBufferInfo info = buffer.bufferInfo();

    // Make sure the window isn't 1x1.  AFAIK, this should never happen
    // accidentally.  It is difficult to make it happen deliberately.
    if (info.srWindow.Left == info.srWindow.Right &&
            info.srWindow.Top == info.srWindow.Bottom) {
        trace("detectNewWindows10Console: Initial console window was 1x1 -- "
              "expanding for test");
        setSmallFont(buffer.conout(), 400, false);
        buffer.moveWindow(SmallRect(0, 0, 1, 1));
        buffer.resizeBuffer(Coord(400, 1));
        buffer.moveWindow(SmallRect(0, 0, 2, 1));
        // This use of GetLargestConsoleWindowSize ought to be unnecessary
        // given the behavior I've seen from moveWindow(0, 0, 1, 1), but
        // I'd like to be especially sure, considering that this code will
        // rarely be tested.
        const auto largest = GetLargestConsoleWindowSize(buffer.conout());
        buffer.moveWindow(
            SmallRect(0, 0, std::min(largest.X, buffer.bufferSize().X), 1));
        info = buffer.bufferInfo();
        ASSERT(info.srWindow.Right > info.srWindow.Left &&
            "Could not expand console window from 1x1");
    }

    // Test whether MARK moves the cursor.
    const Coord initialPosition(info.srWindow.Right, info.srWindow.Bottom);
    buffer.setCursorPosition(initialPosition);
    ASSERT(!console.frozen());
    console.setFreezeUsesMark(true);
    console.setFrozen(true);
    const bool isNewW10 = (buffer.cursorPosition() == initialPosition);
    console.setFrozen(false);
    buffer.setCursorPosition(Coord(0, 0));

    trace("Attempting to detect new Windows 10 console using MARK: %s",
        isNewW10 ? "detected" : "not detected");
    console.setFreezeUsesMark(false);
    console.setNewW10(isNewW10);
}

static inline WriteBuffer newPacket() {
    WriteBuffer packet;
    packet.putRawValue<uint64_t>(0); // Reserve space for size.
    return packet;
}

static HANDLE duplicateHandle(HANDLE h) {
    HANDLE ret = nullptr;
    if (!DuplicateHandle(
            GetCurrentProcess(), h,
            GetCurrentProcess(), &ret,
            0, FALSE, DUPLICATE_SAME_ACCESS)) {
        ASSERT(false && "DuplicateHandle failed!");
    }
    return ret;
}

// It's safe to truncate a handle from 64-bits to 32-bits, or to sign-extend it
// back to 64-bits.  See the MSDN article, "Interprocess Communication Between
// 32-bit and 64-bit Applications".
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa384203.aspx
static int64_t int64FromHandle(HANDLE h) {
    return static_cast<int64_t>(reinterpret_cast<intptr_t>(h));
}

} // anonymous namespace

Agent::Agent(LPCWSTR controlPipeName,
             uint64_t agentFlags,
             int mouseMode,
             int initialCols,
             int initialRows) :
    m_useConerr((agentFlags & WINPTY_FLAG_CONERR) != 0),
    m_plainMode((agentFlags & WINPTY_FLAG_PLAIN_OUTPUT) != 0),
    m_mouseMode(mouseMode)
{
    trace("Agent::Agent entered");

    ASSERT(initialCols >= 1 && initialRows >= 1);
    initialCols = std::min(initialCols, MAX_CONSOLE_WIDTH);
    initialRows = std::min(initialRows, MAX_CONSOLE_HEIGHT);

    const bool outputColor =
        !m_plainMode || (agentFlags & WINPTY_FLAG_COLOR_ESCAPES);
    const Coord initialSize(initialCols, initialRows);

    auto primaryBuffer = openPrimaryBuffer();
    if (m_useConerr) {
        m_errorBuffer = Win32ConsoleBuffer::createErrorBuffer();
    }

    detectNewWindows10Console(m_console, *primaryBuffer);

    m_controlPipe = &connectToControlPipe(controlPipeName);
    m_coninPipe = &createDataServerPipe(false, L"conin");
    m_conoutPipe = &createDataServerPipe(true, L"conout");
    if (m_useConerr) {
        m_conerrPipe = &createDataServerPipe(true, L"conerr");
    }

    // Send an initial response packet to winpty.dll containing pipe names.
    {
        auto setupPacket = newPacket();
        setupPacket.putWString(m_coninPipe->name());
        setupPacket.putWString(m_conoutPipe->name());
        if (m_useConerr) {
            setupPacket.putWString(m_conerrPipe->name());
        }
        writePacket(setupPacket);
    }

    std::unique_ptr<Terminal> primaryTerminal;
    primaryTerminal.reset(new Terminal(*m_conoutPipe,
                                       m_plainMode,
                                       outputColor));
    m_primaryScraper.reset(new Scraper(m_console,
                                       *primaryBuffer,
                                       std::move(primaryTerminal),
                                       initialSize));
    if (m_useConerr) {
        std::unique_ptr<Terminal> errorTerminal;
        errorTerminal.reset(new Terminal(*m_conerrPipe,
                                         m_plainMode,
                                         outputColor));
        m_errorScraper.reset(new Scraper(m_console,
                                         *m_errorBuffer,
                                         std::move(errorTerminal),
                                         initialSize));
    }

    m_console.setTitle(m_currentTitle);

    const HANDLE conin = GetStdHandle(STD_INPUT_HANDLE);
    m_consoleInput.reset(
        new ConsoleInput(conin, m_mouseMode, *this, m_console));

    // Setup Ctrl-C handling.  First restore default handling of Ctrl-C.  This
    // attribute is inherited by child processes.  Then register a custom
    // Ctrl-C handler that does nothing.  The handler will be called when the
    // agent calls GenerateConsoleCtrlEvent.
    SetConsoleCtrlHandler(NULL, FALSE);
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);

    setPollInterval(25);
}

Agent::~Agent()
{
    trace("Agent::~Agent entered");
    agentShutdown();
    if (m_childProcess != NULL) {
        CloseHandle(m_childProcess);
    }
}

// Write a "Device Status Report" command to the terminal.  The terminal will
// reply with a row+col escape sequence.  Presumably, the DSR reply will not
// split a keypress escape sequence, so it should be safe to assume that the
// bytes before it are complete keypresses.
void Agent::sendDsr()
{
    if (!m_plainMode && !m_conoutPipe->isClosed()) {
        m_conoutPipe->write("\x1B[6n");
    }
}

NamedPipe &Agent::connectToControlPipe(LPCWSTR pipeName)
{
    NamedPipe &pipe = createNamedPipe();
    pipe.connectToServer(pipeName, NamedPipe::OpenMode::Duplex);
    pipe.setReadBufferSize(64 * 1024);
    return pipe;
}

// Returns a new server named pipe.  It has not yet been connected.
NamedPipe &Agent::createDataServerPipe(bool write, const wchar_t *kind)
{
    const auto name =
        (WStringBuilder(128)
            << L"\\\\.\\pipe\\winpty-"
            << kind << L'-'
            << GenRandom().uniqueName()).str_moved();
    NamedPipe &pipe = createNamedPipe();
    pipe.openServerPipe(
        name.c_str(),
        write ? NamedPipe::OpenMode::Writing
              : NamedPipe::OpenMode::Reading,
        write ? 8192 : 0,
        write ? 0 : 256);
    if (!write) {
        pipe.setReadBufferSize(64 * 1024);
    }
    return pipe;
}

void Agent::onPipeIo(NamedPipe &namedPipe)
{
    if (&namedPipe == m_conoutPipe || &namedPipe == m_conerrPipe) {
        autoClosePipesForShutdown();
    } else if (&namedPipe == m_coninPipe) {
        pollConinPipe();
    } else if (&namedPipe == m_controlPipe) {
        pollControlPipe();
    }
}

void Agent::pollControlPipe()
{
    if (m_controlPipe->isClosed()) {
        trace("Agent exiting (control pipe is closed)");
        shutdown();
        return;
    }

    while (true) {
        uint64_t packetSize = 0;
        const auto amt1 =
            m_controlPipe->peek(&packetSize, sizeof(packetSize));
        if (amt1 < sizeof(packetSize)) {
            break;
        }
        ASSERT(packetSize >= sizeof(packetSize) && packetSize <= SIZE_MAX);
        if (m_controlPipe->bytesAvailable() < packetSize) {
            if (m_controlPipe->readBufferSize() < packetSize) {
                m_controlPipe->setReadBufferSize(packetSize);
            }
            break;
        }
        std::vector<char> packetData;
        packetData.resize(packetSize);
        const auto amt2 = m_controlPipe->read(packetData.data(), packetSize);
        ASSERT(amt2 == packetSize);
        try {
            ReadBuffer buffer(std::move(packetData));
            buffer.getRawValue<uint64_t>(); // Discard the size.
            handlePacket(buffer);
        } catch (const ReadBuffer::DecodeError&) {
            ASSERT(false && "Decode error");
        }
    }
}

void Agent::handlePacket(ReadBuffer &packet)
{
    const int type = packet.getInt32();
    switch (type) {
    case AgentMsg::StartProcess:
        handleStartProcessPacket(packet);
        break;
    case AgentMsg::SetSize:
        // TODO: I think it might make sense to collapse consecutive SetSize
        // messages.  i.e. The terminal process can probably generate SetSize
        // messages faster than they can be processed, and some GUIs might
        // generate a flood of them, so if we can read multiple SetSize packets
        // at once, we can ignore the early ones.
        handleSetSizePacket(packet);
        break;
    case AgentMsg::GetConsoleProcessList:
        handleGetConsoleProcessListPacket(packet);
        break;
    default:
        trace("Unrecognized message, id:%d", type);
    }
}

void Agent::writePacket(WriteBuffer &packet)
{
    const auto &bytes = packet.buf();
    packet.replaceRawValue<uint64_t>(0, bytes.size());
    m_controlPipe->write(bytes.data(), bytes.size());
}

void Agent::handleStartProcessPacket(ReadBuffer &packet)
{
    ASSERT(m_childProcess == nullptr);
    ASSERT(!m_closingOutputPipes);

    const uint64_t spawnFlags = packet.getInt64();
    const bool wantProcessHandle = packet.getInt32() != 0;
    const bool wantThreadHandle = packet.getInt32() != 0;
    const auto program = packet.getWString();
    const auto cmdline = packet.getWString();
    const auto cwd = packet.getWString();
    const auto env = packet.getWString();
    const auto desktop = packet.getWString();
    packet.assertEof();

    auto cmdlineV = vectorWithNulFromString(cmdline);
    auto desktopV = vectorWithNulFromString(desktop);
    auto envV = vectorFromString(env);

    LPCWSTR programArg = program.empty() ? nullptr : program.c_str();
    LPWSTR cmdlineArg = cmdline.empty() ? nullptr : cmdlineV.data();
    LPCWSTR cwdArg = cwd.empty() ? nullptr : cwd.c_str();
    LPWSTR envArg = env.empty() ? nullptr : envV.data();

    STARTUPINFOW sui = {};
    PROCESS_INFORMATION pi = {};
    sui.cb = sizeof(sui);
    sui.lpDesktop = desktop.empty() ? nullptr : desktopV.data();
    BOOL inheritHandles = FALSE;
    if (m_useConerr) {
        inheritHandles = TRUE;
        sui.dwFlags |= STARTF_USESTDHANDLES;
        sui.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        sui.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        sui.hStdError = m_errorBuffer->conout();
    }

    const BOOL success =
        CreateProcessW(programArg, cmdlineArg, nullptr, nullptr,
                       /*bInheritHandles=*/inheritHandles,
                       /*dwCreationFlags=*/CREATE_UNICODE_ENVIRONMENT,
                       envArg, cwdArg, &sui, &pi);
    const int lastError = success ? 0 : GetLastError();

    trace("CreateProcess: %s %u",
          (success ? "success" : "fail"),
          static_cast<unsigned int>(pi.dwProcessId));

    auto reply = newPacket();
    if (success) {
        int64_t replyProcess = 0;
        int64_t replyThread = 0;
        if (wantProcessHandle) {
            replyProcess = int64FromHandle(duplicateHandle(pi.hProcess));
        }
        if (wantThreadHandle) {
            replyThread = int64FromHandle(duplicateHandle(pi.hThread));
        }
        CloseHandle(pi.hThread);
        m_childProcess = pi.hProcess;
        m_autoShutdown = (spawnFlags & WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN) != 0;
        m_exitAfterShutdown = (spawnFlags & WINPTY_SPAWN_FLAG_EXIT_AFTER_SHUTDOWN) != 0;
        reply.putInt32(static_cast<int32_t>(StartProcessResult::ProcessCreated));
        reply.putInt64(replyProcess);
        reply.putInt64(replyThread);
    } else {
        reply.putInt32(static_cast<int32_t>(StartProcessResult::CreateProcessFailed));
        reply.putInt32(lastError);
    }
    writePacket(reply);
}

void Agent::handleSetSizePacket(ReadBuffer &packet)
{
    const int cols = packet.getInt32();
    const int rows = packet.getInt32();
    packet.assertEof();
    resizeWindow(cols, rows);
    auto reply = newPacket();
    writePacket(reply);
}

void Agent::handleGetConsoleProcessListPacket(ReadBuffer &packet)
{
    packet.assertEof();

    auto processList = std::vector<DWORD>(64);
    auto processCount = GetConsoleProcessList(&processList[0], processList.size());
    if (processList.size() < processCount) {
        processList.resize(processCount);
        processCount = GetConsoleProcessList(&processList[0], processList.size());
    }

    if (processCount == 0) {
        trace("GetConsoleProcessList failed");
    }

    auto reply = newPacket();
    reply.putInt32(processCount);
    for (DWORD i = 0; i < processCount; i++) {
        reply.putInt32(processList[i]);
    }
    writePacket(reply);
}

void Agent::pollConinPipe()
{
    const std::string newData = m_coninPipe->readAllToString();
    if (hasDebugFlag("input_separated_bytes")) {
        // This debug flag is intended to help with testing incomplete escape
        // sequences and multibyte UTF-8 encodings.  (I wonder if the normal
        // code path ought to advance a state machine one byte at a time.)
        for (size_t i = 0; i < newData.size(); ++i) {
            m_consoleInput->writeInput(newData.substr(i, 1));
        }
    } else {
        m_consoleInput->writeInput(newData);
    }
}

void Agent::onPollTimeout()
{
    m_consoleInput->updateInputFlags();
    const bool enableMouseMode = m_consoleInput->shouldActivateTerminalMouse();

    // Give the ConsoleInput object a chance to flush input from an incomplete
    // escape sequence (e.g. pressing ESC).
    m_consoleInput->flushIncompleteEscapeCode();

    const bool shouldScrapeContent = !m_closingOutputPipes;

    // Check if the child process has exited.
    if (m_autoShutdown &&
            m_childProcess != nullptr &&
            WaitForSingleObject(m_childProcess, 0) == WAIT_OBJECT_0) {
        CloseHandle(m_childProcess);
        m_childProcess = nullptr;

        // Close the data socket to signal to the client that the child
        // process has exited.  If there's any data left to send, send it
        // before closing the socket.
        m_closingOutputPipes = true;
    }

    // Scrape for output *after* the above exit-check to ensure that we collect
    // the child process's final output.
    if (shouldScrapeContent) {
        syncConsoleTitle();
        scrapeBuffers();
    }

    // We must ensure that we disable mouse mode before closing the CONOUT
    // pipe, so update the mouse mode here.
    m_primaryScraper->terminal().enableMouseMode(
        enableMouseMode && !m_closingOutputPipes);

    autoClosePipesForShutdown();
}

void Agent::autoClosePipesForShutdown()
{
    if (m_closingOutputPipes) {
        // We don't want to close a pipe before it's connected!  If we do, the
        // libwinpty client may try to connect to a non-existent pipe.  This
        // case is important for short-lived programs.
        if (m_conoutPipe->isConnected() &&
                m_conoutPipe->bytesToSend() == 0) {
            trace("Closing CONOUT pipe (auto-shutdown)");
            m_conoutPipe->closePipe();
        }
        if (m_conerrPipe != nullptr &&
                m_conerrPipe->isConnected() &&
                m_conerrPipe->bytesToSend() == 0) {
            trace("Closing CONERR pipe (auto-shutdown)");
            m_conerrPipe->closePipe();
        }
        if (m_exitAfterShutdown &&
                m_conoutPipe->isClosed() &&
                (m_conerrPipe == nullptr || m_conerrPipe->isClosed())) {
            trace("Agent exiting (exit-after-shutdown)");
            shutdown();
        }
    }
}

std::unique_ptr<Win32ConsoleBuffer> Agent::openPrimaryBuffer()
{
    // If we're using a separate buffer for stderr, and a program were to
    // activate the stderr buffer, then we could accidentally scrape the same
    // buffer twice.  That probably shouldn't happen in ordinary use, but it
    // can be avoided anyway by using the original console screen buffer in
    // that mode.
    if (!m_useConerr) {
        return Win32ConsoleBuffer::openConout();
    } else {
        return Win32ConsoleBuffer::openStdout();
    }
}

void Agent::resizeWindow(int cols, int rows)
{
    ASSERT(cols >= 1 && rows >= 1);
    cols = std::min(cols, MAX_CONSOLE_WIDTH);
    rows = std::min(rows, MAX_CONSOLE_HEIGHT);

    Win32Console::FreezeGuard guard(m_console, m_console.frozen());
    const Coord newSize(cols, rows);
    ConsoleScreenBufferInfo info;
    auto primaryBuffer = openPrimaryBuffer();
    m_primaryScraper->resizeWindow(*primaryBuffer, newSize, info);
    m_consoleInput->setMouseWindowRect(info.windowRect());
    if (m_errorScraper) {
        m_errorScraper->resizeWindow(*m_errorBuffer, newSize, info);
    }

    // Synthesize a WINDOW_BUFFER_SIZE_EVENT event.  Normally, Windows
    // generates this event only when the buffer size changes, not when the
    // window size changes.  This behavior is undesirable in two ways:
    //  - When winpty expands the window horizontally, it must expand the
    //    buffer first, then the window.  At least some programs (e.g. the WSL
    //    bash.exe wrapper) use the window width rather than the buffer width,
    //    so there is a short timespan during which they can read the wrong
    //    value.
    //  - If the window's vertical size is changed, no event is generated,
    //    even though a typical well-behaved console program cares about the
    //    *window* height, not the *buffer* height.
    // This synthesization works around a design flaw in the console.  It's probably
    // harmless.  See https://github.com/rprichard/winpty/issues/110.
    INPUT_RECORD sizeEvent {};
    sizeEvent.EventType = WINDOW_BUFFER_SIZE_EVENT;
    sizeEvent.Event.WindowBufferSizeEvent.dwSize = primaryBuffer->bufferSize();
    DWORD actual {};
    WriteConsoleInputW(GetStdHandle(STD_INPUT_HANDLE), &sizeEvent, 1, &actual);
}

void Agent::scrapeBuffers()
{
    Win32Console::FreezeGuard guard(m_console, m_console.frozen());
    ConsoleScreenBufferInfo info;
    m_primaryScraper->scrapeBuffer(*openPrimaryBuffer(), info);
    m_consoleInput->setMouseWindowRect(info.windowRect());
    if (m_errorScraper) {
        m_errorScraper->scrapeBuffer(*m_errorBuffer, info);
    }
}

void Agent::syncConsoleTitle()
{
    std::wstring newTitle = m_console.title();
    if (newTitle != m_currentTitle) {
        std::string command = std::string("\x1b]0;") +
                utf8FromWide(newTitle) + "\x07";
        m_conoutPipe->write(command.c_str());
        m_currentTitle = newTitle;
    }
}
