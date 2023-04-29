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

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <limits>
#include <string>
#include <vector>

#include "../include/winpty.h"

#include "../shared/AgentMsg.h"
#include "../shared/BackgroundDesktop.h"
#include "../shared/Buffer.h"
#include "../shared/DebugClient.h"
#include "../shared/GenRandom.h"
#include "../shared/OwnedHandle.h"
#include "../shared/StringBuilder.h"
#include "../shared/StringUtil.h"
#include "../shared/WindowsSecurity.h"
#include "../shared/WindowsVersion.h"
#include "../shared/WinptyAssert.h"
#include "../shared/WinptyException.h"
#include "../shared/WinptyVersion.h"

#include "AgentLocation.h"
#include "LibWinptyException.h"
#include "WinptyInternal.h"



/*****************************************************************************
 * Error handling -- translate C++ exceptions to an optional error object
 * output and log the result. */

static const winpty_error_s kOutOfMemory = {
    WINPTY_ERROR_OUT_OF_MEMORY,
    L"Out of memory",
    nullptr
};

static const winpty_error_s kBadRpcPacket = {
    WINPTY_ERROR_UNSPECIFIED,
    L"Bad RPC packet",
    nullptr
};

static const winpty_error_s kUncaughtException = {
    WINPTY_ERROR_UNSPECIFIED,
    L"Uncaught C++ exception",
    nullptr
};

/* Gets the error code from the error object. */
WINPTY_API winpty_result_t winpty_error_code(winpty_error_ptr_t err) {
    return err != nullptr ? err->code : WINPTY_ERROR_SUCCESS;
}

/* Returns a textual representation of the error.  The string is freed when
 * the error is freed. */
WINPTY_API LPCWSTR winpty_error_msg(winpty_error_ptr_t err) {
    if (err != nullptr) {
        if (err->msgStatic != nullptr) {
            return err->msgStatic;
        } else {
            ASSERT(err->msgDynamic != nullptr);
            std::wstring *msgPtr = err->msgDynamic->get();
            ASSERT(msgPtr != nullptr);
            return msgPtr->c_str();
        }
    } else {
        return L"Success";
    }
}

/* Free the error object.  Every error returned from the winpty API must be
 * freed. */
WINPTY_API void winpty_error_free(winpty_error_ptr_t err) {
    if (err != nullptr && err->msgDynamic != nullptr) {
        delete err->msgDynamic;
        delete err;
    }
}

static void translateException(winpty_error_ptr_t *&err) {
    winpty_error_ptr_t ret = nullptr;
    try {
        try {
            throw;
        } catch (const ReadBuffer::DecodeError&) {
            ret = const_cast<winpty_error_ptr_t>(&kBadRpcPacket);
        } catch (const LibWinptyException &e) {
            std::unique_ptr<winpty_error_t> obj(new winpty_error_t);
            obj->code = e.code();
            obj->msgStatic = nullptr;
            obj->msgDynamic =
                new std::shared_ptr<std::wstring>(e.whatSharedStr());
            ret = obj.release();
        } catch (const WinptyException &e) {
            std::unique_ptr<winpty_error_t> obj(new winpty_error_t);
            std::shared_ptr<std::wstring> msg(new std::wstring(e.what()));
            obj->code = WINPTY_ERROR_UNSPECIFIED;
            obj->msgStatic = nullptr;
            obj->msgDynamic = new std::shared_ptr<std::wstring>(msg);
            ret = obj.release();
        }
    } catch (const std::bad_alloc&) {
        ret = const_cast<winpty_error_ptr_t>(&kOutOfMemory);
    } catch (...) {
        ret = const_cast<winpty_error_ptr_t>(&kUncaughtException);
    }
    trace("libwinpty error: code=%u msg='%s'",
        static_cast<unsigned>(ret->code),
        utf8FromWide(winpty_error_msg(ret)).c_str());
    if (err != nullptr) {
        *err = ret;
    } else {
        winpty_error_free(ret);
    }
}

#define API_TRY \
    if (err != nullptr) { *err = nullptr; } \
    try

#define API_CATCH(ret) \
    catch (...) { translateException(err); return (ret); }



/*****************************************************************************
 * Configuration of a new agent. */

WINPTY_API winpty_config_t *
winpty_config_new(UINT64 flags, winpty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        ASSERT((flags & WINPTY_FLAG_MASK) == flags);
        std::unique_ptr<winpty_config_t> ret(new winpty_config_t);
        ret->flags = flags;
        return ret.release();
    } API_CATCH(nullptr)
}

WINPTY_API void winpty_config_free(winpty_config_t *cfg) {
    delete cfg;
}

WINPTY_API void
winpty_config_set_initial_size(winpty_config_t *cfg, int cols, int rows) {
    ASSERT(cfg != nullptr && cols > 0 && rows > 0);
    cfg->cols = cols;
    cfg->rows = rows;
}

WINPTY_API void
winpty_config_set_mouse_mode(winpty_config_t *cfg, int mouseMode) {
    ASSERT(cfg != nullptr &&
        mouseMode >= WINPTY_MOUSE_MODE_NONE &&
        mouseMode <= WINPTY_MOUSE_MODE_FORCE);
    cfg->mouseMode = mouseMode;
}

WINPTY_API void
winpty_config_set_agent_timeout(winpty_config_t *cfg, DWORD timeoutMs) {
    ASSERT(cfg != nullptr && timeoutMs > 0);
    cfg->timeoutMs = timeoutMs;
}



/*****************************************************************************
 * Agent I/O. */

namespace {

// Once an I/O operation fails with ERROR_IO_PENDING, the caller *must* wait
// for it to complete, even after calling CancelIo on it!  See
// https://blogs.msdn.microsoft.com/oldnewthing/20110202-00/?p=11613.  This
// class enforces that requirement.
class PendingIo {
    HANDLE m_file;
    OVERLAPPED &m_over;
    bool m_finished;
public:
    // The file handle and OVERLAPPED object must live as long as the PendingIo
    // object.
    PendingIo(HANDLE file, OVERLAPPED &over) :
        m_file(file), m_over(over), m_finished(false) {}
    ~PendingIo() {
        if (!m_finished) {
            // We're not usually that interested in CancelIo's return value.
            // In any case, we must not throw an exception in this dtor.
            CancelIo(m_file);
            waitForCompletion();
        }
    }
    std::tuple<BOOL, DWORD> waitForCompletion(DWORD &actual) WINPTY_NOEXCEPT {
        m_finished = true;
        const BOOL success =
            GetOverlappedResult(m_file, &m_over, &actual, TRUE);
        return std::make_tuple(success, GetLastError());
    }
    std::tuple<BOOL, DWORD> waitForCompletion() WINPTY_NOEXCEPT {
        DWORD actual = 0;
        return waitForCompletion(actual);
    }
};

} // anonymous namespace

static void handlePendingIo(winpty_t &wp, OVERLAPPED &over, BOOL &success,
                            DWORD &lastError, DWORD &actual) {
    if (!success && lastError == ERROR_IO_PENDING) {
        PendingIo io(wp.controlPipe.get(), over);
        const HANDLE waitHandles[2] = { wp.ioEvent.get(),
                                        wp.agentProcess.get() };
        DWORD waitRet = WaitForMultipleObjects(
            2, waitHandles, FALSE, wp.agentTimeoutMs);
        if (waitRet != WAIT_OBJECT_0) {
            // The I/O is still pending.  Cancel it, close the I/O event, and
            // throw an exception.
            if (waitRet == WAIT_OBJECT_0 + 1) {
                throw LibWinptyException(WINPTY_ERROR_AGENT_DIED, L"agent died");
            } else if (waitRet == WAIT_TIMEOUT) {
                throw LibWinptyException(WINPTY_ERROR_AGENT_TIMEOUT,
                                      L"agent timed out");
            } else if (waitRet == WAIT_FAILED) {
                throwWindowsError(L"WaitForMultipleObjects failed");
            } else {
                ASSERT(false &&
                    "unexpected WaitForMultipleObjects return value");
            }
        }
        std::tie(success, lastError) = io.waitForCompletion(actual);
    }
}

static void handlePendingIo(winpty_t &wp, OVERLAPPED &over, BOOL &success,
                            DWORD &lastError) {
    DWORD actual = 0;
    handlePendingIo(wp, over, success, lastError, actual);
}

static void handleReadWriteErrors(winpty_t &wp, BOOL success, DWORD lastError,
                                  const wchar_t *genericErrMsg) {
    if (!success) {
        // If the pipe connection is broken after it's been connected, then
        // later I/O operations fail with ERROR_BROKEN_PIPE (reads) or
        // ERROR_NO_DATA (writes).  With Wine, they may also fail with
        // ERROR_PIPE_NOT_CONNECTED.  See this gist[1].
        //
        // [1] https://gist.github.com/rprichard/8dd8ca134b39534b7da2733994aa07ba
        if (lastError == ERROR_BROKEN_PIPE || lastError == ERROR_NO_DATA ||
                lastError == ERROR_PIPE_NOT_CONNECTED) {
            throw LibWinptyException(WINPTY_ERROR_LOST_CONNECTION,
                L"lost connection to agent");
        } else {
            throwWindowsError(genericErrMsg, lastError);
        }
    }
}

// Calls ConnectNamedPipe to wait until the agent connects to the control pipe.
static void
connectControlPipe(winpty_t &wp) {
    OVERLAPPED over = {};
    over.hEvent = wp.ioEvent.get();
    BOOL success = ConnectNamedPipe(wp.controlPipe.get(), &over);
    DWORD lastError = GetLastError();
    handlePendingIo(wp, over, success, lastError);
    if (!success && lastError == ERROR_PIPE_CONNECTED) {
        success = TRUE;
    }
    if (!success) {
        throwWindowsError(L"ConnectNamedPipe failed", lastError);
    }
}

static void writeData(winpty_t &wp, const void *data, size_t amount) {
    // Perform a single pipe write.
    DWORD actual = 0;
    OVERLAPPED over = {};
    over.hEvent = wp.ioEvent.get();
    BOOL success = WriteFile(wp.controlPipe.get(), data, amount,
                             &actual, &over);
    DWORD lastError = GetLastError();
    if (!success) {
        handlePendingIo(wp, over, success, lastError, actual);
        handleReadWriteErrors(wp, success, lastError, L"WriteFile failed");
        ASSERT(success);
    }
    // TODO: Can a partial write actually happen somehow?
    ASSERT(actual == amount && "WriteFile wrote fewer bytes than requested");
}

static inline WriteBuffer newPacket() {
    WriteBuffer packet;
    packet.putRawValue<uint64_t>(0); // Reserve space for size.
    return packet;
}

static void writePacket(winpty_t &wp, WriteBuffer &packet) {
    const auto &buf = packet.buf();
    packet.replaceRawValue<uint64_t>(0, buf.size());
    writeData(wp, buf.data(), buf.size());
}

static size_t readData(winpty_t &wp, void *data, size_t amount) {
    DWORD actual = 0;
    OVERLAPPED over = {};
    over.hEvent = wp.ioEvent.get();
    BOOL success = ReadFile(wp.controlPipe.get(), data, amount,
                            &actual, &over);
    DWORD lastError = GetLastError();
    if (!success) {
        handlePendingIo(wp, over, success, lastError, actual);
        handleReadWriteErrors(wp, success, lastError, L"ReadFile failed");
    }
    return actual;
}

static void readAll(winpty_t &wp, void *data, size_t amount) {
    while (amount > 0) {
        const size_t chunk = readData(wp, data, amount);
        ASSERT(chunk <= amount && "readData result is larger than amount");
        data = reinterpret_cast<char*>(data) + chunk;
        amount -= chunk;
    }
}

static uint64_t readUInt64(winpty_t &wp) {
    uint64_t ret = 0;
    readAll(wp, &ret, sizeof(ret));
    return ret;
}

// Returns a reply packet's payload.
static ReadBuffer readPacket(winpty_t &wp) {
    const uint64_t packetSize = readUInt64(wp);
    if (packetSize < sizeof(packetSize) || packetSize > SIZE_MAX) {
        throwWinptyException(L"Agent RPC error: invalid packet size");
    }
    const size_t payloadSize = packetSize - sizeof(packetSize);
    std::vector<char> bytes(payloadSize);
    readAll(wp, bytes.data(), bytes.size());
    return ReadBuffer(std::move(bytes));
}

static OwnedHandle createControlPipe(const std::wstring &name) {
    const auto sd = createPipeSecurityDescriptorOwnerFullControl();
    if (!sd) {
        throwWinptyException(
            L"could not create the control pipe's SECURITY_DESCRIPTOR");
    }
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = sd.get();
    HANDLE ret = CreateNamedPipeW(name.c_str(),
        /*dwOpenMode=*/
        PIPE_ACCESS_DUPLEX |
            FILE_FLAG_FIRST_PIPE_INSTANCE |
            FILE_FLAG_OVERLAPPED,
        /*dwPipeMode=*/rejectRemoteClientsPipeFlag(),
        /*nMaxInstances=*/1,
        /*nOutBufferSize=*/8192,
        /*nInBufferSize=*/256,
        /*nDefaultTimeOut=*/30000,
        &sa);
    if (ret == INVALID_HANDLE_VALUE) {
        throwWindowsError(L"CreateNamedPipeW failed");
    }
    return OwnedHandle(ret);
}



/*****************************************************************************
 * Start the agent. */

static OwnedHandle createEvent() {
    // manual reset, initially unset
    HANDLE h = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (h == nullptr) {
        throwWindowsError(L"CreateEventW failed");
    }
    return OwnedHandle(h);
}

// For debugging purposes, provide a way to keep the console on the main window
// station, visible.
static bool shouldShowConsoleWindow() {
    char buf[32];
    return GetEnvironmentVariableA("WINPTY_SHOW_CONSOLE", buf, sizeof(buf)) > 0;
}

static bool shouldCreateBackgroundDesktop(bool &createUsingAgent) {
    // Prior to Windows 7, winpty's repeated selection-deselection loop
    // prevented the user from interacting with their *visible* console
    // windows, unless we placed the console onto a background desktop.
    // The SetProcessWindowStation call interferes with the clipboard and
    // isn't thread-safe, though[1].  The call should perhaps occur in a
    // special agent subprocess.  Spawning a process in a background desktop
    // also breaks ConEmu, but marking the process SW_HIDE seems to correct
    // that[2].
    //
    // Windows 7 moved a lot of console handling out of csrss.exe and into
    // a per-console conhost.exe process, which may explain why it isn't
    // affected.
    //
    // This is a somewhat risky change, so there are low-level flags to
    // assist in debugging if there are issues.
    //
    // [1] https://github.com/rprichard/winpty/issues/58
    // [2] https://github.com/rprichard/winpty/issues/70
    bool ret = !shouldShowConsoleWindow() && !isAtLeastWindows7();
    const bool force = hasDebugFlag("force_desktop");
    const bool force_spawn = hasDebugFlag("force_desktop_spawn");
    const bool force_curproc = hasDebugFlag("force_desktop_curproc");
    const bool suppress = hasDebugFlag("no_desktop");
    if (force + force_spawn + force_curproc + suppress > 1) {
        trace("error: Only one of force_desktop, force_desktop_spawn, "
              "force_desktop_curproc, and no_desktop may be set");
    } else if (force) {
        ret = true;
    } else if (force_spawn) {
        ret = true;
        createUsingAgent = true;
    } else if (force_curproc) {
        ret = true;
        createUsingAgent = false;
    } else if (suppress) {
        ret = false;
    }
    return ret;
}

static bool shouldSpecifyHideFlag() {
    const bool force = hasDebugFlag("force_sw_hide");
    const bool suppress = hasDebugFlag("no_sw_hide");
    bool ret = !shouldShowConsoleWindow();
    if (force && suppress) {
        trace("error: Both the force_sw_hide and no_sw_hide flags are set");
    } else if (force) {
        ret = true;
    } else if (suppress) {
        ret = false;
    }
    return ret;
}

static OwnedHandle startAgentProcess(
        const std::wstring &desktop,
        const std::wstring &controlPipeName,
        const std::wstring &params,
        DWORD creationFlags,
        DWORD &agentPid) {
    const std::wstring exePath = findAgentProgram();
    const std::wstring cmdline =
        (WStringBuilder(256)
            << L"\"" << exePath << L"\" "
            << controlPipeName << L' '
            << params).str_moved();

    auto cmdlineV = vectorWithNulFromString(cmdline);
    auto desktopV = vectorWithNulFromString(desktop);

    // Start the agent.
    STARTUPINFOW sui = {};
    sui.cb = sizeof(sui);
    sui.lpDesktop = desktop.empty() ? nullptr : desktopV.data();

    if (shouldSpecifyHideFlag()) {
        sui.dwFlags |= STARTF_USESHOWWINDOW;
        sui.wShowWindow = SW_HIDE;
    }
    PROCESS_INFORMATION pi = {};
    const BOOL success =
        CreateProcessW(exePath.c_str(),
                       cmdlineV.data(),
                       nullptr, nullptr,
                       /*bInheritHandles=*/FALSE,
                       /*dwCreationFlags=*/creationFlags,
                       nullptr, nullptr,
                       &sui, &pi);
    if (!success) {
        const DWORD lastError = GetLastError();
        const auto errStr =
            (WStringBuilder(256)
                << L"winpty-agent CreateProcess failed: cmdline='" << cmdline
                << L"' err=0x" << whexOfInt(lastError)).str_moved();
        throw LibWinptyException(
            WINPTY_ERROR_AGENT_CREATION_FAILED, errStr.c_str());
    }
    CloseHandle(pi.hThread);
    TRACE("Created agent successfully, pid=%u, cmdline=%s",
          static_cast<unsigned int>(pi.dwProcessId),
          utf8FromWide(cmdline).c_str());
    agentPid = pi.dwProcessId;
    return OwnedHandle(pi.hProcess);
}

static void verifyPipeClientPid(HANDLE serverPipe, DWORD agentPid) {
    const auto client = getNamedPipeClientProcessId(serverPipe);
    const auto success = std::get<0>(client);
    const auto lastError = std::get<2>(client);
    if (success == GetNamedPipeClientProcessId_Result::Success) {
        const auto clientPid = std::get<1>(client);
        if (clientPid != agentPid) {
            WStringBuilder errMsg;
            errMsg << L"Security check failed: pipe client pid (" << clientPid
                   << L") does not match agent pid (" << agentPid << L")";
            throwWinptyException(errMsg.c_str());
        }
    } else if (success == GetNamedPipeClientProcessId_Result::UnsupportedOs) {
        trace("Pipe client PID security check skipped: "
            "GetNamedPipeClientProcessId unsupported on this OS version");
    } else {
        throwWindowsError(L"GetNamedPipeClientProcessId failed", lastError);
    }
}

static std::unique_ptr<winpty_t>
createAgentSession(const winpty_config_t *cfg,
                   const std::wstring &desktop,
                   const std::wstring &params,
                   DWORD creationFlags) {
    std::unique_ptr<winpty_t> wp(new winpty_t);
    wp->agentTimeoutMs = cfg->timeoutMs;
    wp->ioEvent = createEvent();

    // Create control server pipe.
    const auto pipeName =
        L"\\\\.\\pipe\\winpty-control-" + GenRandom().uniqueName();
    wp->controlPipe = createControlPipe(pipeName);

    DWORD agentPid = 0;
    wp->agentProcess = startAgentProcess(
        desktop, pipeName, params, creationFlags, agentPid);
    connectControlPipe(*wp.get());
    verifyPipeClientPid(wp->controlPipe.get(), agentPid);

    return std::move(wp);
}

namespace {

class AgentDesktop {
public:
    virtual std::wstring name() = 0;
    virtual ~AgentDesktop() {}
};

class AgentDesktopDirect : public AgentDesktop {
public:
    AgentDesktopDirect(BackgroundDesktop &&desktop) :
        m_desktop(std::move(desktop))
    {
    }
    std::wstring name() override { return m_desktop.desktopName(); }
private:
    BackgroundDesktop m_desktop;
};

class AgentDesktopIndirect : public AgentDesktop {
public:
    AgentDesktopIndirect(std::unique_ptr<winpty_t> &&wp,
                         std::wstring &&desktopName) :
        m_wp(std::move(wp)),
        m_desktopName(std::move(desktopName))
    {
    }
    std::wstring name() override { return m_desktopName; }
private:
    std::unique_ptr<winpty_t> m_wp;
    std::wstring m_desktopName;
};

} // anonymous namespace

std::unique_ptr<AgentDesktop>
setupBackgroundDesktop(const winpty_config_t *cfg) {
    bool useDesktopAgent =
        !(cfg->flags & WINPTY_FLAG_ALLOW_CURPROC_DESKTOP_CREATION);
    const bool useDesktop = shouldCreateBackgroundDesktop(useDesktopAgent);

    if (!useDesktop) {
        return std::unique_ptr<AgentDesktop>();
    }

    if (useDesktopAgent) {
        auto wp = createAgentSession(
            cfg, std::wstring(), L"--create-desktop", DETACHED_PROCESS);

        // Read the desktop name.
        auto packet = readPacket(*wp.get());
        auto desktopName = packet.getWString();
        packet.assertEof();

        if (desktopName.empty()) {
            return std::unique_ptr<AgentDesktop>();
        } else {
            return std::unique_ptr<AgentDesktop>(
                new AgentDesktopIndirect(std::move(wp),
                                         std::move(desktopName)));
        }
    } else {
        try {
            BackgroundDesktop desktop;
            return std::unique_ptr<AgentDesktop>(new AgentDesktopDirect(
                std::move(desktop)));
        } catch (const WinptyException &e) {
            trace("Error: failed to create background desktop, "
                  "using original desktop instead: %s",
                  utf8FromWide(e.what()).c_str());
            return std::unique_ptr<AgentDesktop>();
        }
    }
}

WINPTY_API winpty_t *
winpty_open(const winpty_config_t *cfg,
            winpty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        ASSERT(cfg != nullptr);
        dumpWindowsVersion();
        dumpVersionToTrace();

        // Setup a background desktop for the agent.
        auto desktop = setupBackgroundDesktop(cfg);
        const auto desktopName = desktop ? desktop->name() : std::wstring();

        // Start the primary agent session.
        const auto params =
            (WStringBuilder(128)
                << cfg->flags << L' '
                << cfg->mouseMode << L' '
                << cfg->cols << L' '
                << cfg->rows).str_moved();
        auto wp = createAgentSession(cfg, desktopName, params,
                                     CREATE_NEW_CONSOLE);

        // Close handles to the background desktop and restore the original
        // window station.  This must wait until we know the agent is running
        // -- if we close these handles too soon, then the desktop and
        // windowstation will be destroyed before the agent can connect with
        // them.
        //
        // If we used a separate agent process to create the desktop, we
        // disconnect from that process here, allowing it to exit.
        desktop.reset();

        // If we ran the agent process on a background desktop, then when we
        // spawn a child process from the agent, it will need to be explicitly
        // placed back onto the original desktop.
        if (!desktopName.empty()) {
            wp->spawnDesktopName = getCurrentDesktopName();
        }

        // Get the CONIN/CONOUT pipe names.
        auto packet = readPacket(*wp.get());
        wp->coninPipeName = packet.getWString();
        wp->conoutPipeName = packet.getWString();
        if (cfg->flags & WINPTY_FLAG_CONERR) {
            wp->conerrPipeName = packet.getWString();
        }
        packet.assertEof();

        return wp.release();
    } API_CATCH(nullptr)
}

WINPTY_API HANDLE winpty_agent_process(winpty_t *wp) {
    ASSERT(wp != nullptr);
    return wp->agentProcess.get();
}



/*****************************************************************************
 * I/O pipes. */

static const wchar_t *cstrFromWStringOrNull(const std::wstring &str) {
    try {
        return str.c_str();
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

WINPTY_API LPCWSTR winpty_conin_name(winpty_t *wp) {
    ASSERT(wp != nullptr);
    return cstrFromWStringOrNull(wp->coninPipeName);
}

WINPTY_API LPCWSTR winpty_conout_name(winpty_t *wp) {
    ASSERT(wp != nullptr);
    return cstrFromWStringOrNull(wp->conoutPipeName);
}

WINPTY_API LPCWSTR winpty_conerr_name(winpty_t *wp) {
    ASSERT(wp != nullptr);
    if (wp->conerrPipeName.empty()) {
        return nullptr;
    } else {
        return cstrFromWStringOrNull(wp->conerrPipeName);
    }
}



/*****************************************************************************
 * winpty agent RPC calls. */

namespace {

// Close the control pipe if something goes wrong with the pipe communication,
// which could leave the control pipe in an inconsistent state.
class RpcOperation {
public:
    RpcOperation(winpty_t &wp) : m_wp(wp) {
        if (m_wp.controlPipe.get() == nullptr) {
            throwWinptyException(L"Agent shutdown due to RPC failure");
        }
    }
    ~RpcOperation() {
        if (!m_success) {
            trace("~RpcOperation: Closing control pipe");
            m_wp.controlPipe.dispose(true);
        }
    }
    void success() { m_success = true; }
private:
    winpty_t &m_wp;
    bool m_success = false;
};

} // anonymous namespace



/*****************************************************************************
 * winpty agent RPC call: process creation. */

// Return a std::wstring containing every character of the environment block.
// Typically, the block is non-empty, so the std::wstring returned ends with
// two NUL terminators.  (These two terminators are counted in size(), so
// calling c_str() produces a triply-terminated string.)
static std::wstring wstringFromEnvBlock(const wchar_t *env) {
    std::wstring envStr;
    if (env != NULL) {
        const wchar_t *p = env;
        while (*p != L'\0') {
            p += wcslen(p) + 1;
        }
        p++;
        envStr.assign(env, p);

        // Assuming the environment was non-empty, envStr now ends with two NUL
        // terminators.
        //
        // If the environment were empty, though, then envStr would only be
        // singly terminated, but the MSDN documentation thinks an env block is
        // always doubly-terminated, so add an extra NUL just in case it
        // matters.
        const auto envStrSz = envStr.size();
        if (envStrSz == 1) {
            ASSERT(envStr[0] == L'\0');
            envStr.push_back(L'\0');
        } else {
            ASSERT(envStrSz >= 3);
            ASSERT(envStr[envStrSz - 3] != L'\0');
            ASSERT(envStr[envStrSz - 2] == L'\0');
            ASSERT(envStr[envStrSz - 1] == L'\0');
        }
    }
    return envStr;
}

WINPTY_API winpty_spawn_config_t *
winpty_spawn_config_new(UINT64 winptyFlags,
                        LPCWSTR appname /*OPTIONAL*/,
                        LPCWSTR cmdline /*OPTIONAL*/,
                        LPCWSTR cwd /*OPTIONAL*/,
                        LPCWSTR env /*OPTIONAL*/,
                        winpty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        ASSERT((winptyFlags & WINPTY_SPAWN_FLAG_MASK) == winptyFlags);
        std::unique_ptr<winpty_spawn_config_t> cfg(new winpty_spawn_config_t);
        cfg->winptyFlags = winptyFlags;
        if (appname != nullptr) { cfg->appname = appname; }
        if (cmdline != nullptr) { cfg->cmdline = cmdline; }
        if (cwd != nullptr) { cfg->cwd = cwd; }
        if (env != nullptr) { cfg->env = wstringFromEnvBlock(env); }
        return cfg.release();
    } API_CATCH(nullptr)
}

WINPTY_API void winpty_spawn_config_free(winpty_spawn_config_t *cfg) {
    delete cfg;
}

// It's safe to truncate a handle from 64-bits to 32-bits, or to sign-extend it
// back to 64-bits.  See the MSDN article, "Interprocess Communication Between
// 32-bit and 64-bit Applications".
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa384203.aspx
static inline HANDLE handleFromInt64(int64_t i) {
    return reinterpret_cast<HANDLE>(static_cast<intptr_t>(i));
}

// Given a process and a handle in that process, duplicate the handle into the
// current process and close it in the originating process.
static inline OwnedHandle stealHandle(HANDLE process, HANDLE handle) {
    HANDLE result = nullptr;
    if (!DuplicateHandle(process, handle,
            GetCurrentProcess(),
            &result, 0, FALSE,
            DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
        throwWindowsError(L"DuplicateHandle of process handle");
    }
    return OwnedHandle(result);
}

WINPTY_API BOOL
winpty_spawn(winpty_t *wp,
             const winpty_spawn_config_t *cfg,
             HANDLE *process_handle /*OPTIONAL*/,
             HANDLE *thread_handle /*OPTIONAL*/,
             DWORD *create_process_error /*OPTIONAL*/,
             winpty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        ASSERT(wp != nullptr && cfg != nullptr);

        if (process_handle != nullptr) { *process_handle = nullptr; }
        if (thread_handle != nullptr) { *thread_handle = nullptr; }
        if (create_process_error != nullptr) { *create_process_error = 0; }

        LockGuard<Mutex> lock(wp->mutex);
        RpcOperation rpc(*wp);

        // Send spawn request.
        auto packet = newPacket();
        packet.putInt32(AgentMsg::StartProcess);
        packet.putInt64(cfg->winptyFlags);
        packet.putInt32(process_handle != nullptr);
        packet.putInt32(thread_handle != nullptr);
        packet.putWString(cfg->appname);
        packet.putWString(cfg->cmdline);
        packet.putWString(cfg->cwd);
        packet.putWString(cfg->env);
        packet.putWString(wp->spawnDesktopName);
        writePacket(*wp, packet);

        // Receive reply.
        auto reply = readPacket(*wp);
        const auto result = static_cast<StartProcessResult>(reply.getInt32());
        if (result == StartProcessResult::CreateProcessFailed) {
            const DWORD lastError = reply.getInt32();
            reply.assertEof();
            if (create_process_error != nullptr) {
                *create_process_error = lastError;
            }
            rpc.success();
            throw LibWinptyException(WINPTY_ERROR_SPAWN_CREATE_PROCESS_FAILED,
                L"CreateProcess failed");
        } else if (result == StartProcessResult::ProcessCreated) {
            const HANDLE remoteProcess = handleFromInt64(reply.getInt64());
            const HANDLE remoteThread = handleFromInt64(reply.getInt64());
            reply.assertEof();
            OwnedHandle localProcess;
            OwnedHandle localThread;
            if (remoteProcess != nullptr) {
                localProcess =
                    stealHandle(wp->agentProcess.get(), remoteProcess);
            }
            if (remoteThread != nullptr) {
                localThread =
                    stealHandle(wp->agentProcess.get(), remoteThread);
            }
            if (process_handle != nullptr) {
                *process_handle = localProcess.release();
            }
            if (thread_handle != nullptr) {
                *thread_handle = localThread.release();
            }
            rpc.success();
        } else {
            throwWinptyException(
                L"Agent RPC error: invalid StartProcessResult");
        }
        return TRUE;
    } API_CATCH(FALSE)
}



/*****************************************************************************
 * winpty agent RPC calls: everything else */

WINPTY_API BOOL
winpty_set_size(winpty_t *wp, int cols, int rows,
                winpty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        ASSERT(wp != nullptr && cols > 0 && rows > 0);
        LockGuard<Mutex> lock(wp->mutex);
        RpcOperation rpc(*wp);
        auto packet = newPacket();
        packet.putInt32(AgentMsg::SetSize);
        packet.putInt32(cols);
        packet.putInt32(rows);
        writePacket(*wp, packet);
        readPacket(*wp).assertEof();
        rpc.success();
        return TRUE;
    } API_CATCH(FALSE)
}

WINPTY_API int
winpty_get_console_process_list(winpty_t *wp, int *processList, const int processCount,
                                winpty_error_ptr_t *err /*OPTIONAL*/) {
    API_TRY {
        ASSERT(wp != nullptr);
        ASSERT(processList != nullptr);
        LockGuard<Mutex> lock(wp->mutex);
        RpcOperation rpc(*wp);
        auto packet = newPacket();
        packet.putInt32(AgentMsg::GetConsoleProcessList);
        writePacket(*wp, packet);
        auto reply = readPacket(*wp);

        auto actualProcessCount = reply.getInt32();

        if (actualProcessCount <= processCount) {
            for (auto i = 0; i < actualProcessCount; i++) {
                processList[i] = reply.getInt32();
            }
        }

        reply.assertEof();
        rpc.success();
        return actualProcessCount;
    } API_CATCH(0)
}

WINPTY_API void winpty_free(winpty_t *wp) {
    // At least in principle, CloseHandle can fail, so this deletion can
    // fail.  It won't throw an exception, but maybe there's an error that
    // should be propagated?
    delete wp;
}
