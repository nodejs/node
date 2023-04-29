// Copyright (c) 2015 Ryan Prichard
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

#include <cstdio>
#include <cstdlib>

#include <windows.h>

#include "../shared/WindowsSecurity.h"
#include "../shared/WinptyException.h"

const wchar_t *kPipeName = L"\\\\.\\pipe\\DebugServer";

// A message may not be larger than this size.
const int MSG_SIZE = 4096;

static void usage(const char *program, int code) {
    printf("Usage: %s [--everyone]\n"
           "\n"
           "Creates the named pipe %ls and reads messages.  Prints each\n"
           "message to stdout.  By default, only the current user can send messages.\n"
           "Pass --everyone to let anyone send a message.\n"
           "\n"
           "Use the WINPTY_DEBUG environment variable to enable winpty trace output.\n"
           "(e.g. WINPTY_DEBUG=trace for the default trace output.)  Set WINPTYDBG=1\n"
           "to enable trace with older winpty versions.\n",
           program, kPipeName);
    exit(code);
}

int main(int argc, char *argv[]) {
    bool everyone = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--everyone") {
            everyone = true;
        } else if (arg == "-h" || arg == "--help") {
            usage(argv[0], 0);
        } else {
            usage(argv[0], 1);
        }
    }

    SecurityDescriptor sd;
    PSECURITY_ATTRIBUTES psa = nullptr;
    SECURITY_ATTRIBUTES sa = {};
    if (everyone) {
        try {
            sd = createPipeSecurityDescriptorOwnerFullControlEveryoneWrite();
        } catch (const WinptyException &e) {
            fprintf(stderr,
                "error creating security descriptor: %ls\n", e.what());
            exit(1);
        }
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = sd.get();
        psa = &sa;
    }

    HANDLE serverPipe = CreateNamedPipeW(
        kPipeName,
        /*dwOpenMode=*/PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        /*dwPipeMode=*/PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE |
            rejectRemoteClientsPipeFlag(),
        /*nMaxInstances=*/1,
        /*nOutBufferSize=*/MSG_SIZE,
        /*nInBufferSize=*/MSG_SIZE,
        /*nDefaultTimeOut=*/10 * 1000,
        psa);

    if (serverPipe == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "error: could not create %ls pipe: error %u\n",
            kPipeName, static_cast<unsigned>(GetLastError()));
        exit(1);
    }

    char msgBuffer[MSG_SIZE + 1];

    while (true) {
        if (!ConnectNamedPipe(serverPipe, nullptr)) {
            fprintf(stderr, "error: ConnectNamedPipe failed\n");
            fflush(stderr);
            exit(1);
        }
        DWORD bytesRead = 0;
        if (!ReadFile(serverPipe, msgBuffer, MSG_SIZE, &bytesRead, nullptr)) {
            fprintf(stderr, "error: ReadFile on pipe failed\n");
            fflush(stderr);
            DisconnectNamedPipe(serverPipe);
            continue;
        }
        msgBuffer[bytesRead] = '\n';
        fwrite(msgBuffer, 1, bytesRead + 1, stdout);
        fflush(stdout);

        DWORD bytesWritten = 0;
        WriteFile(serverPipe, "OK", 2, &bytesWritten, nullptr);
        DisconnectNamedPipe(serverPipe);
    }
}
