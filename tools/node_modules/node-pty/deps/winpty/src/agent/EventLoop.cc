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

#include "EventLoop.h"

#include <algorithm>

#include "NamedPipe.h"
#include "../shared/DebugClient.h"
#include "../shared/WinptyAssert.h"

EventLoop::~EventLoop() {
    for (NamedPipe *pipe : m_pipes) {
        delete pipe;
    }
    m_pipes.clear();
}

// Enter the event loop.  Runs until the I/O or timeout handler calls exit().
void EventLoop::run()
{
    std::vector<HANDLE> waitHandles;
    DWORD lastTime = GetTickCount();
    while (!m_exiting) {
        bool didSomething = false;

        // Attempt to make progress with the pipes.
        waitHandles.clear();
        for (size_t i = 0; i < m_pipes.size(); ++i) {
            if (m_pipes[i]->serviceIo(&waitHandles)) {
                onPipeIo(*m_pipes[i]);
                didSomething = true;
            }
        }

        // Call the timeout if enough time has elapsed.
        if (m_pollInterval > 0) {
            int elapsed = GetTickCount() - lastTime;
            if (elapsed >= m_pollInterval) {
                onPollTimeout();
                lastTime = GetTickCount();
                didSomething = true;
            }
        }

        if (didSomething)
            continue;

        // If there's nothing to do, wait.
        DWORD timeout = INFINITE;
        if (m_pollInterval > 0)
            timeout = std::max(0, (int)(lastTime + m_pollInterval - GetTickCount()));
        if (waitHandles.size() == 0) {
            ASSERT(timeout != INFINITE);
            if (timeout > 0)
                Sleep(timeout);
        } else {
            DWORD result = WaitForMultipleObjects(waitHandles.size(),
                                                  waitHandles.data(),
                                                  FALSE,
                                                  timeout);
            ASSERT(result != WAIT_FAILED);
        }
    }
}

NamedPipe &EventLoop::createNamedPipe()
{
    NamedPipe *ret = new NamedPipe();
    m_pipes.push_back(ret);
    return *ret;
}

void EventLoop::setPollInterval(int ms)
{
    m_pollInterval = ms;
}

void EventLoop::shutdown()
{
    m_exiting = true;
}
