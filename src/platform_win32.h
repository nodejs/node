// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef NODE_PLATFORM_WIN32_H_
#define NODE_PLATFORM_WIN32_H_

// Require at least Windows XP SP1
// (GetProcessId requires it)
#ifndef _WIN32_WINNT
# define _WIN32_WINNT 0x0501
#endif

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN // implies NOCRYPT and NOGDI.
#endif

#ifndef NOMINMAX
# define NOMINMAX
#endif

#ifndef NOKERNEL
# define NOKERNEL
#endif

#ifndef NOUSER
# define NOUSER
#endif

#ifndef NOSERVICE
# define NOSERVICE
#endif

#ifndef NOSOUND
# define NOSOUND
#endif

#ifndef NOMCX
# define NOMCX
#endif

#include <windows.h>
#include <winsock2.h>

#if defined(_MSC_VER)
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

namespace node {

#define NO_IMPL_MSG(...) \
    fprintf(stderr, "Not implemented: %s\n", #__VA_ARGS__);

const char *winapi_strerror(const int errorno);
void winapi_perror(const char* prefix);

}

#endif  // NODE_PLATFORM_WIN32_H_

