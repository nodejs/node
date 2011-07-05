// Copyright 2010 the V8 project authors. All rights reserved.
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

#ifndef V8_V8_TEST_H_
#define V8_V8_TEST_H_

#include "v8.h"

#ifdef _WIN32
// Setup for Windows DLL export/import. See v8.h in this directory for
// information on how to build/use V8 as a DLL.
#if defined(BUILDING_V8_SHARED) && defined(USING_V8_SHARED)
#error both BUILDING_V8_SHARED and USING_V8_SHARED are set - please check the\
  build configuration to ensure that at most one of these is set
#endif

#ifdef BUILDING_V8_SHARED
#define V8EXPORT __declspec(dllexport)
#elif USING_V8_SHARED
#define V8EXPORT __declspec(dllimport)
#else
#define V8EXPORT
#endif

#else  // _WIN32

// Setup for Linux shared library export. See v8.h in this directory for
// information on how to build/use V8 as shared library.
#if defined(__GNUC__) && (__GNUC__ >= 4) && defined(V8_SHARED)
#define V8EXPORT __attribute__ ((visibility("default")))
#else  // defined(__GNUC__) && (__GNUC__ >= 4)
#define V8EXPORT
#endif  // defined(__GNUC__) && (__GNUC__ >= 4)

#endif  // _WIN32


/**
 * Testing support for the V8 JavaScript engine.
 */
namespace v8 {

class V8EXPORT Testing {
 public:
  enum StressType {
    kStressTypeOpt,
    kStressTypeDeopt
  };

  /**
   * Set the type of stressing to do. The default if not set is kStressTypeOpt.
   */
  static void SetStressRunType(StressType type);

  /**
   * Get the number of runs of a given test that is required to get the full
   * stress coverage.
   */
  static int GetStressRuns();

  /**
   * Indicate the number of the run which is about to start. The value of run
   * should be between 0 and one less than the result from GetStressRuns()
   */
  static void PrepareStressRun(int run);
};


}  // namespace v8


#undef V8EXPORT


#endif  // V8_V8_TEST_H_
