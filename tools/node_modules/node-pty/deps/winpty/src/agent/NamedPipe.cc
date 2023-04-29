// Copyright (c) 2011-2012 Ryan Prichard
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

#include <string.h>

#include <algorithm>

#include "EventLoop.h"
#include "NamedPipe.h"
#include "../shared/DebugClient.h"
#include "../shared/StringUtil.h"
#include "../shared/WindowsSecurity.h"
#include "../shared/WinptyAssert.h"

// Returns true if anything happens (data received, data sent, pipe error).
bool NamedPipe::serviceIo(std::vector<HANDLE> *waitHandles)
{
    bool justConnected = false;
    const auto kError = ServiceResult::Error;
    const auto kProgress = ServiceResult::Progress;
    const auto kNoProgress = ServiceResult::NoProgress;
    if (m_handle == NULL) {
        return false;
    }
    if (m_connectEvent.get() != nullptr) {
        // We're still connecting this server pipe.  Check whether the pipe is
        // now connected.  If it isn't, add the pipe to the list of handles to
        // wait on.
        DWORD actual = 0;
        BOOL success =
            GetOverlappedResult(m_handle, &m_connectOver, &actual, FALSE);
        if (!success && GetLastError() == ERROR_PIPE_CONNECTED) {
            // I'm not sure this can happen, but it's easy to handle if it
            // does.
            success = TRUE;
        }
        if (!success) {
            ASSERT(GetLastError() == ERROR_IO_INCOMPLETE &&
                "Pended ConnectNamedPipe call failed");
            waitHandles->push_back(m_connectEvent.get());
        } else {
            TRACE("Server pipe [%s] connected",
                utf8FromWide(m_name).c_str());
            m_connectEvent.dispose();
            startPipeWorkers();
            justConnected = true;
        }
    }
    const auto readProgress = m_inputWorker ? m_inputWorker->service() : kNoProgress;
    const auto writeProgress = m_outputWorker ? m_outputWorker->service() : kNoProgress;
    if (readProgress == kError || writeProgress == kError) {
        closePipe();
        return true;
    }
    if (m_inputWorker && m_inputWorker->getWaitEvent() != nullptr) {
        waitHandles->push_back(m_inputWorker->getWaitEvent());
    }
    if (m_outputWorker && m_outputWorker->getWaitEvent() != nullptr) {
        waitHandles->push_back(m_outputWorker->getWaitEvent());
    }
    return justConnected
        || readProgress == kProgress
        || writeProgress == kProgress;
}

// manual reset, initially unset
static OwnedHandle createEvent() {
    HANDLE ret = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    ASSERT(ret != nullptr && "CreateEventW failed");
    return OwnedHandle(ret);
}

NamedPipe::IoWorker::IoWorker(NamedPipe &namedPipe) :
    m_namedPipe(namedPipe),
    m_event(createEvent())
{
}

NamedPipe::ServiceResult NamedPipe::IoWorker::service()
{
    ServiceResult progress = ServiceResult::NoProgress;
    if (m_pending) {
        DWORD actual = 0;
        BOOL ret = GetOverlappedResult(m_namedPipe.m_handle, &m_over, &actual, FALSE);
        if (!ret) {
            if (GetLastError() == ERROR_IO_INCOMPLETE) {
                // There is a pending I/O.
                return progress;
            } else {
                // Pipe error.
                return ServiceResult::Error;
            }
        }
        ResetEvent(m_event.get());
        m_pending = false;
        completeIo(actual);
        m_currentIoSize = 0;
        progress = ServiceResult::Progress;
    }
    DWORD nextSize = 0;
    bool isRead = false;
    while (shouldIssueIo(&nextSize, &isRead)) {
        m_currentIoSize = nextSize;
        DWORD actual = 0;
        memset(&m_over, 0, sizeof(m_over));
        m_over.hEvent = m_event.get();
        BOOL ret = isRead
                ? ReadFile(m_namedPipe.m_handle, m_buffer, nextSize, &actual, &m_over)
                : WriteFile(m_namedPipe.m_handle, m_buffer, nextSize, &actual, &m_over);
        if (!ret) {
            if (GetLastError() == ERROR_IO_PENDING) {
                // There is a pending I/O.
                m_pending = true;
                return progress;
            } else {
                // Pipe error.
                return ServiceResult::Error;
            }
        }
        ResetEvent(m_event.get());
        completeIo(actual);
        m_currentIoSize = 0;
        progress = ServiceResult::Progress;
    }
    return progress;
}

// This function is called after CancelIo has returned.  We need to block until
// the I/O operations have completed, which should happen very quickly.
// https://blogs.msdn.microsoft.com/oldnewthing/20110202-00/?p=11613
void NamedPipe::IoWorker::waitForCanceledIo()
{
    if (m_pending) {
        DWORD actual = 0;
        GetOverlappedResult(m_namedPipe.m_handle, &m_over, &actual, TRUE);
        m_pending = false;
    }
}

HANDLE NamedPipe::IoWorker::getWaitEvent()
{
    return m_pending ? m_event.get() : NULL;
}

void NamedPipe::InputWorker::completeIo(DWORD size)
{
    m_namedPipe.m_inQueue.append(m_buffer, size);
}

bool NamedPipe::InputWorker::shouldIssueIo(DWORD *size, bool *isRead)
{
    *isRead = true;
    ASSERT(!m_namedPipe.isConnecting());
    if (m_namedPipe.isClosed()) {
        return false;
    } else if (m_namedPipe.m_inQueue.size() < m_namedPipe.readBufferSize()) {
        *size = kIoSize;
        return true;
    } else {
        return false;
    }
}

void NamedPipe::OutputWorker::completeIo(DWORD size)
{
    ASSERT(size == m_currentIoSize);
}

bool NamedPipe::OutputWorker::shouldIssueIo(DWORD *size, bool *isRead)
{
    *isRead = false;
    if (!m_namedPipe.m_outQueue.empty()) {
        auto &out = m_namedPipe.m_outQueue;
        const DWORD writeSize = std::min<size_t>(out.size(), kIoSize);
        std::copy(&out[0], &out[writeSize], m_buffer);
        out.erase(0, writeSize);
        *size = writeSize;
        return true;
    } else {
        return false;
    }
}

DWORD NamedPipe::OutputWorker::getPendingIoSize()
{
    return m_pending ? m_currentIoSize : 0;
}

void NamedPipe::openServerPipe(LPCWSTR pipeName, OpenMode::t openMode,
                               int outBufferSize, int inBufferSize) {
    ASSERT(isClosed());
    ASSERT((openMode & OpenMode::Duplex) != 0);
    const DWORD winOpenMode =
              ((openMode & OpenMode::Reading) ? PIPE_ACCESS_INBOUND : 0)
            | ((openMode & OpenMode::Writing) ? PIPE_ACCESS_OUTBOUND : 0)
            | FILE_FLAG_FIRST_PIPE_INSTANCE
            | FILE_FLAG_OVERLAPPED;
    const auto sd = createPipeSecurityDescriptorOwnerFullControl();
    ASSERT(sd && "error creating data pipe SECURITY_DESCRIPTOR");
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = sd.get();
    HANDLE handle = CreateNamedPipeW(
        pipeName,
        /*dwOpenMode=*/winOpenMode,
        /*dwPipeMode=*/rejectRemoteClientsPipeFlag(),
        /*nMaxInstances=*/1,
        /*nOutBufferSize=*/outBufferSize,
        /*nInBufferSize=*/inBufferSize,
        /*nDefaultTimeOut=*/30000,
        &sa);
    TRACE("opened server pipe [%s], handle == %p",
        utf8FromWide(pipeName).c_str(), handle);
    ASSERT(handle != INVALID_HANDLE_VALUE && "Could not open server pipe");
    m_name = pipeName;
    m_handle = handle;
    m_openMode = openMode;

    // Start an asynchronous connection attempt.
    m_connectEvent = createEvent();
    memset(&m_connectOver, 0, sizeof(m_connectOver));
    m_connectOver.hEvent = m_connectEvent.get();
    BOOL success = ConnectNamedPipe(m_handle, &m_connectOver);
    const auto err = GetLastError();
    if (!success && err == ERROR_PIPE_CONNECTED) {
        success = TRUE;
    }
    if (success) {
        TRACE("Server pipe [%s] connected", utf8FromWide(pipeName).c_str());
        m_connectEvent.dispose();
        startPipeWorkers();
    } else if (err != ERROR_IO_PENDING) {
        ASSERT(false && "ConnectNamedPipe call failed");
    }
}

void NamedPipe::connectToServer(LPCWSTR pipeName, OpenMode::t openMode)
{
    ASSERT(isClosed());
    ASSERT((openMode & OpenMode::Duplex) != 0);
    HANDLE handle = CreateFileW(
        pipeName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION | FILE_FLAG_OVERLAPPED,
        NULL);
    TRACE("connected to [%s], handle == %p",
        utf8FromWide(pipeName).c_str(), handle);
    ASSERT(handle != INVALID_HANDLE_VALUE && "Could not connect to pipe");
    m_name = pipeName;
    m_handle = handle;
    m_openMode = openMode;
    startPipeWorkers();
}

void NamedPipe::startPipeWorkers()
{
    if (m_openMode & OpenMode::Reading) {
        m_inputWorker.reset(new InputWorker(*this));
    }
    if (m_openMode & OpenMode::Writing) {
        m_outputWorker.reset(new OutputWorker(*this));
    }
}

size_t NamedPipe::bytesToSend()
{
    ASSERT(m_openMode & OpenMode::Writing);
    auto ret = m_outQueue.size();
    if (m_outputWorker != NULL) {
        ret += m_outputWorker->getPendingIoSize();
    }
    return ret;
}

void NamedPipe::write(const void *data, size_t size)
{
    ASSERT(m_openMode & OpenMode::Writing);
    m_outQueue.append(reinterpret_cast<const char*>(data), size);
}

void NamedPipe::write(const char *text)
{
    write(text, strlen(text));
}

size_t NamedPipe::readBufferSize()
{
    ASSERT(m_openMode & OpenMode::Reading);
    return m_readBufferSize;
}

void NamedPipe::setReadBufferSize(size_t size)
{
    ASSERT(m_openMode & OpenMode::Reading);
    m_readBufferSize = size;
}

size_t NamedPipe::bytesAvailable()
{
    ASSERT(m_openMode & OpenMode::Reading);
    return m_inQueue.size();
}

size_t NamedPipe::peek(void *data, size_t size)
{
    ASSERT(m_openMode & OpenMode::Reading);
    const auto out = reinterpret_cast<char*>(data);
    const size_t ret = std::min(size, m_inQueue.size());
    std::copy(&m_inQueue[0], &m_inQueue[ret], out);
    return ret;
}

size_t NamedPipe::read(void *data, size_t size)
{
    size_t ret = peek(data, size);
    m_inQueue.erase(0, ret);
    return ret;
}

std::string NamedPipe::readToString(size_t size)
{
    ASSERT(m_openMode & OpenMode::Reading);
    size_t retSize = std::min(size, m_inQueue.size());
    std::string ret = m_inQueue.substr(0, retSize);
    m_inQueue.erase(0, retSize);
    return ret;
}

std::string NamedPipe::readAllToString()
{
    ASSERT(m_openMode & OpenMode::Reading);
    std::string ret = m_inQueue;
    m_inQueue.clear();
    return ret;
}

void NamedPipe::closePipe()
{
    if (m_handle == NULL) {
        return;
    }
    CancelIo(m_handle);
    if (m_connectEvent.get() != nullptr) {
        DWORD actual = 0;
        GetOverlappedResult(m_handle, &m_connectOver, &actual, TRUE);
        m_connectEvent.dispose();
    }
    if (m_inputWorker) {
        m_inputWorker->waitForCanceledIo();
        m_inputWorker.reset();
    }
    if (m_outputWorker) {
        m_outputWorker->waitForCanceledIo();
        m_outputWorker.reset();
    }
    CloseHandle(m_handle);
    m_handle = NULL;
}
