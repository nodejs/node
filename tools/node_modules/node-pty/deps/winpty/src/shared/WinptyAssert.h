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

#ifndef WINPTY_ASSERT_H
#define WINPTY_ASSERT_H

#ifdef WINPTY_AGENT_ASSERT

void agentShutdown();
void agentAssertFail(const char *file, int line, const char *cond);

// Calling the standard assert() function does not work in the agent because
// the error message would be printed to the console, and the only way the
// user can see the console is via a working agent!  Moreover, the console may
// be frozen, so attempting to write to it would block forever.  This custom
// assert function instead sends the message to the DebugServer, then attempts
// to close the console, then quietly exits.
#define ASSERT(cond) \
    do {                                                    \
        if (!(cond)) {                                      \
            agentAssertFail(__FILE__, __LINE__, #cond);     \
        }                                                   \
    } while(0)

#else

void assertTrace(const char *file, int line, const char *cond);

// In the other targets, log the assert failure to the debugserver, then fail
// using the ordinary assert mechanism.  In case assert is compiled out, fail
// using abort.  The amount of code inlined is unfortunate, but asserts aren't
// used much outside the agent.
#include <assert.h>
#include <stdlib.h>
#define ASSERT_CONDITION(cond) (false && (cond))
#define ASSERT(cond) \
    do {                                            \
        if (!(cond)) {                              \
            assertTrace(__FILE__, __LINE__, #cond); \
            assert(ASSERT_CONDITION(#cond));        \
            abort();                                \
        }                                           \
    } while(0)

#endif

#endif // WINPTY_ASSERT_H
