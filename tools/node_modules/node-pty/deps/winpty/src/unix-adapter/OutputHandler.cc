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

#include "OutputHandler.h"

#include <assert.h>
#include <errno.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include "../shared/DebugClient.h"
#include "Util.h"
#include "WakeupFd.h"

OutputHandler::OutputHandler(
        HANDLE conout, int outputfd, WakeupFd &completionWakeup) :
    m_conout(conout),
    m_outputfd(outputfd),
    m_completionWakeup(completionWakeup),
    m_threadHasBeenJoined(false),
    m_threadCompleted(0)
{
    pthread_create(&m_thread, NULL, OutputHandler::threadProcS, this);
}

void OutputHandler::shutdown() {
    if (!m_threadHasBeenJoined) {
        int ret = pthread_join(m_thread, NULL);
        assert(ret == 0 && "pthread_join failed");
        m_threadHasBeenJoined = true;
    }
}

void OutputHandler::threadProc() {
    std::vector<char> buffer(4096);
    while (true) {
        DWORD numRead = 0;
        BOOL ret = ReadFile(m_conout,
                            &buffer[0], buffer.size(),
                            &numRead, NULL);
        if (!ret || numRead == 0) {
            if (!ret && GetLastError() == ERROR_BROKEN_PIPE) {
                trace("OutputHandler: pipe closed: numRead=%u",
                    static_cast<unsigned int>(numRead));
            } else {
                trace("OutputHandler: read failed: "
                    "ret=%d lastError=0x%x numRead=%u",
                    ret,
                    static_cast<unsigned int>(GetLastError()),
                    static_cast<unsigned int>(numRead));
            }
            break;
        }
        if (!writeAll(m_outputfd, &buffer[0], numRead)) {
            break;
        }
    }
    m_threadCompleted = 1;
    m_completionWakeup.set();
}
