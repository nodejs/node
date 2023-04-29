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

#include "WakeupFd.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void setFdNonBlock(int fd) {
    int status = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, status | O_NONBLOCK);
}

WakeupFd::WakeupFd() {
    int pipeFd[2];
    if (pipe(pipeFd) != 0) {
        perror("Could not create internal wakeup pipe");
        abort();
    }
    m_pipeReadFd = pipeFd[0];
    m_pipeWriteFd = pipeFd[1];
    setFdNonBlock(m_pipeReadFd);
    setFdNonBlock(m_pipeWriteFd);
}

WakeupFd::~WakeupFd() {
    close(m_pipeReadFd);
    close(m_pipeWriteFd);
}

void WakeupFd::set() {
    char dummy = 0;
    int ret;
    do {
        ret = write(m_pipeWriteFd, &dummy, 1);
    } while (ret < 0 && errno == EINTR);
}

void WakeupFd::reset() {
    char tmpBuf[256];
    while (true) {
        int amount = read(m_pipeReadFd, tmpBuf, sizeof(tmpBuf));
        if (amount < 0 && errno == EAGAIN) {
            break;
        } else if (amount <= 0) {
            perror("error reading from internal wakeup pipe");
            abort();
        }
    }
}
