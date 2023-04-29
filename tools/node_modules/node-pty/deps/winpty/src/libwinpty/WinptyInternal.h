// Copyright (c) 2016 Ryan Prichard
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

#ifndef LIBWINPTY_WINPTY_INTERNAL_H
#define LIBWINPTY_WINPTY_INTERNAL_H

#include <memory>
#include <string>

#include "../include/winpty.h"

#include "../shared/Mutex.h"
#include "../shared/OwnedHandle.h"

// The structures in this header are not intended to be accessed directly by
// client programs.

struct winpty_error_s {
    winpty_result_t code;
    const wchar_t *msgStatic;
    // Use a pointer to a std::shared_ptr so that the struct remains simple
    // enough to statically initialize, for the benefit of static error
    // objects like kOutOfMemory.
    std::shared_ptr<std::wstring> *msgDynamic;
};

struct winpty_config_s {
    uint64_t flags = 0;
    int cols = 80;
    int rows = 25;
    int mouseMode = WINPTY_MOUSE_MODE_AUTO;
    DWORD timeoutMs = 30000;
};

struct winpty_s {
    Mutex mutex;
    OwnedHandle agentProcess;
    OwnedHandle controlPipe;
    DWORD agentTimeoutMs = 0;
    OwnedHandle ioEvent;
    std::wstring spawnDesktopName;
    std::wstring coninPipeName;
    std::wstring conoutPipeName;
    std::wstring conerrPipeName;
};

struct winpty_spawn_config_s {
    uint64_t winptyFlags = 0;
    std::wstring appname;
    std::wstring cmdline;
    std::wstring cwd;
    std::wstring env;
};

#endif // LIBWINPTY_WINPTY_INTERNAL_H
