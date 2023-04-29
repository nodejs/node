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

#ifndef UNIX_ADAPTER_OUTPUT_HANDLER_H
#define UNIX_ADAPTER_OUTPUT_HANDLER_H

#include <windows.h>
#include <pthread.h>
#include <signal.h>

#include "WakeupFd.h"

// Connect winpty CONOUT/CONERR to a Cygwin blocking fd.
class OutputHandler {
public:
    OutputHandler(HANDLE conout, int outputfd, WakeupFd &completionWakeup);
    ~OutputHandler() { shutdown(); }
    bool isComplete() { return m_threadCompleted; }
    void shutdown();

private:
    static void *threadProcS(void *pvthis) {
        reinterpret_cast<OutputHandler*>(pvthis)->threadProc();
        return NULL;
    }
    void threadProc();

    HANDLE m_conout;
    int m_outputfd;
    pthread_t m_thread;
    WakeupFd &m_completionWakeup;
    bool m_threadHasBeenJoined;
    volatile sig_atomic_t m_threadCompleted;
};

#endif // UNIX_ADAPTER_OUTPUT_HANDLER_H
