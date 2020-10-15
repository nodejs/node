// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "libreprl.h"

// Well-known file descriptor numbers for fuzzer <-> fuzzee communication on child process side.
#define CRFD 100
#define CWFD 101
#define DRFD 102
#define DWFD 103

#define CHECK_SUCCESS(cond) if((cond) < 0) { perror(#cond); abort(); }
#define CHECK(cond) if(!(cond)) { fprintf(stderr, "(" #cond ") failed!"); abort(); }

static uint64_t current_millis()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int reprl_spawn_child(char** argv, char** envp, struct reprl_child_process* child)
{
    // We need to make sure that our fds don't end up being 100 - 104.
    if (fcntl(CRFD, F_GETFD) == -1) {
        int devnull = open("/dev/null", O_RDWR);
        dup2(devnull, CRFD);
        dup2(devnull, CWFD);
        dup2(devnull, DRFD);
        dup2(devnull, DWFD);
        close(devnull);
    }

    int crpipe[2] = { 0, 0 };          // control channel child -> fuzzer
    int cwpipe[2] = { 0, 0 };          // control channel fuzzer -> child
    int drpipe[2] = { 0, 0 };          // data channel child -> fuzzer
    int dwpipe[2] = { 0, 0 };          // data channel fuzzer -> child

    int res = 0;
    res |= pipe(crpipe);
    res |= pipe(cwpipe);
    res |= pipe(drpipe);
    res |= pipe(dwpipe);
    if (res != 0) {
        if (crpipe[0] != 0) { close(crpipe[0]); close(crpipe[1]); }
        if (cwpipe[0] != 0) { close(cwpipe[0]); close(cwpipe[1]); }
        if (drpipe[0] != 0) { close(drpipe[0]); close(drpipe[1]); }
        if (dwpipe[0] != 0) { close(dwpipe[0]); close(dwpipe[1]); }
        fprintf(stderr, "[REPRL] Could not setup pipes for communication with child: %s\n", strerror(errno));
        return -1;
    }

    child->crfd = crpipe[0];
    child->cwfd = cwpipe[1];
    child->drfd = drpipe[0];
    child->dwfd = dwpipe[1];

    int flags;
    flags = fcntl(child->drfd, F_GETFL, 0);
    fcntl(child->drfd, F_SETFL, flags | O_NONBLOCK);

    fcntl(child->crfd, F_SETFD, FD_CLOEXEC);
    fcntl(child->cwfd, F_SETFD, FD_CLOEXEC);
    fcntl(child->drfd, F_SETFD, FD_CLOEXEC);
    fcntl(child->dwfd, F_SETFD, FD_CLOEXEC);

    int pid = fork();
    if (pid == 0) {
        dup2(cwpipe[0], CRFD);
        dup2(crpipe[1], CWFD);
        dup2(dwpipe[0], DRFD);
        dup2(drpipe[1], DWFD);
        close(cwpipe[0]);
        close(crpipe[1]);
        close(dwpipe[0]);
        close(drpipe[1]);

        int devnull = open("/dev/null", O_RDWR);
        dup2(devnull, 0);
        dup2(devnull, 1);
        dup2(devnull, 2);
        close(devnull);

        execve(argv[0], argv, envp);
        fprintf(stderr, "[REPRL] Failed to spawn child process\n");
        _exit(-1);
    } else if (pid < 0) {
        fprintf(stderr, "[REPRL] Failed to fork\n");
        return -1;
    }

    close(crpipe[1]);
    close(cwpipe[0]);
    close(drpipe[1]);
    close(dwpipe[0]);

    child->pid = pid;

    int helo;
    if (read(child->crfd, &helo, 4) != 4 || write(child->cwfd, &helo, 4) != 4) {
        fprintf(stderr, "[REPRL] Failed to communicate with child process\n");
        close(child->crfd);
        close(child->cwfd);
        close(child->drfd);
        close(child->dwfd);
        int status;
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        return -1;
    }

    return 0;
}

static char* fetch_output(int fd, size_t* outsize)
{
    ssize_t rv;
    *outsize = 0;
    size_t remaining = 0x1000;
    char* outbuf = malloc(remaining + 1);

    do {
        rv = read(fd, outbuf + *outsize, remaining);
        if (rv == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                fprintf(stderr, "[REPRL] Error while receiving data: %s\n", strerror(errno));
            }
            break;
        }

        *outsize += rv;
        remaining -= rv;

        if (remaining == 0) {
            remaining = *outsize;
            outbuf = realloc(outbuf, *outsize * 2 + 1);
            if (!outbuf) {
                fprintf(stderr, "[REPRL] Could not allocate output buffer");
                _exit(-1);
            }
        }
    } while (rv > 0);

    outbuf[*outsize] = 0;

    return outbuf;
}

// Execute one script, wait for its completion, and return the result.
int reprl_execute_script(int pid, int crfd, int cwfd, int drfd, int dwfd, int timeout, const char* script, int64_t script_length, struct reprl_result* result)
{
    uint64_t start_time = current_millis();

    if (write(cwfd, "exec", 4) != 4 ||
        write(cwfd, &script_length, 8) != 8) {
        fprintf(stderr, "[REPRL] Failed to send command to child process\n");
        return -1;
    }

    int64_t remaining = script_length;
    while (remaining > 0) {
        ssize_t rv = write(dwfd, script, remaining);
        if (rv <= 0) {
            fprintf(stderr, "[REPRL] Failed to send script to child process\n");
            return -1;
        }
        remaining -= rv;
        script += rv;
    }

    struct pollfd fds = {.fd = crfd, .events = POLLIN, .revents = 0};
    if (poll(&fds, 1, timeout) != 1) {
        kill(pid, SIGKILL);
        waitpid(pid, &result->status, 0);
        result->child_died = 1;
    } else {
        result->child_died = 0;
        ssize_t rv = read(crfd, &result->status, 4);
        if (rv != 4) {
            // This should not happen...
            kill(pid, SIGKILL);
            waitpid(pid, &result->status, 0);
            result->child_died = 1;
        }
    }

    result->output = fetch_output(drfd, &result->output_size);
    result->exec_time = current_millis() - start_time;

    return 0;
}
