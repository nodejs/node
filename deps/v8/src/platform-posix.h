// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_PLATFORM_POSIX_H_
#define V8_PLATFORM_POSIX_H_

#if !defined(ANDROID)
#include <cxxabi.h>
#endif
#include <stdio.h>

#include "platform.h"

namespace v8 {
namespace internal {

// Used by platform implementation files during OS::PostSetUp().
void POSIXPostSetUp();

// Used by platform implementation files during OS::DumpBacktrace()
// and OS::StackWalk().
template<int (*backtrace)(void**, int),
         char** (*backtrace_symbols)(void* const*, int)>
struct POSIXBacktraceHelper {
  static void DumpBacktrace() {
    void* trace[100];
    int size = backtrace(trace, ARRAY_SIZE(trace));
    char** symbols = backtrace_symbols(trace, size);
    fprintf(stderr, "\n==== C stack trace ===============================\n\n");
    if (size == 0) {
      fprintf(stderr, "(empty)\n");
    } else if (symbols == NULL) {
      fprintf(stderr, "(no symbols)\n");
    } else {
      for (int i = 1; i < size; ++i) {
        fprintf(stderr, "%2d: ", i);
        char mangled[201];
        if (sscanf(symbols[i], "%*[^(]%*[(]%200[^)+]", mangled) == 1) {// NOLINT
          char* demangled = NULL;
#if !defined(ANDROID)
          int status;
          size_t length;
          demangled = abi::__cxa_demangle(mangled, NULL, &length, &status);
#endif
          fprintf(stderr, "%s\n", demangled != NULL ? demangled : mangled);
          free(demangled);
        } else {
          fprintf(stderr, "??\n");
        }
      }
    }
    fflush(stderr);
    free(symbols);
  }

  static int StackWalk(Vector<OS::StackFrame> frames) {
    int frames_size = frames.length();
    ScopedVector<void*> addresses(frames_size);

    int frames_count = backtrace(addresses.start(), frames_size);

    char** symbols = backtrace_symbols(addresses.start(), frames_count);
    if (symbols == NULL) {
      return OS::kStackWalkError;
    }

    for (int i = 0; i < frames_count; i++) {
      frames[i].address = addresses[i];
      // Format a text representation of the frame based on the information
      // available.
      OS::SNPrintF(MutableCStrVector(frames[i].text, OS::kStackWalkMaxTextLen),
                   "%s", symbols[i]);
      // Make sure line termination is in place.
      frames[i].text[OS::kStackWalkMaxTextLen - 1] = '\0';
    }

    free(symbols);

    return frames_count;
  }
};

} }  // namespace v8::internal

#endif  // V8_PLATFORM_POSIX_H_
