// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
#ifndef V8_FLAGS_H_
#define V8_FLAGS_H_

namespace v8 {
namespace internal {

// Declare all of our flags.
#define FLAG_MODE_DECLARE
#include "flag-definitions.h"

// The global list of all flags.
class FlagList {
 public:
  // The list of all flags with a value different from the default
  // and their values. The format of the list is like the format of the
  // argv array passed to the main function, e.g.
  // ("--prof", "--log-file", "v8.prof", "--nolazy").
  //
  // The caller is responsible for disposing the list, as well
  // as every element of it.
  static List<const char*>* argv();

  // Set the flag values by parsing the command line. If remove_flags is
  // set, the flags and associated values are removed from (argc,
  // argv). Returns 0 if no error occurred. Otherwise, returns the argv
  // index > 0 for the argument where an error occurred. In that case,
  // (argc, argv) will remain unchanged independent of the remove_flags
  // value, and no assumptions about flag settings should be made.
  //
  // The following syntax for flags is accepted (both '-' and '--' are ok):
  //
  //   --flag        (bool flags only)
  //   --noflag      (bool flags only)
  //   --flag=value  (non-bool flags only, no spaces around '=')
  //   --flag value  (non-bool flags only)
  //   --            (equivalent to --js_arguments, captures all remaining args)
  static int SetFlagsFromCommandLine(int* argc, char** argv, bool remove_flags);

  // Set the flag values by parsing the string str. Splits string into argc
  // substrings argv[], each of which consisting of non-white-space chars,
  // and then calls SetFlagsFromCommandLine() and returns its result.
  static int SetFlagsFromString(const char* str, int len);

  // Reset all flags to their default value.
  static void ResetAllFlags();

  // Print help to stdout with flags, types, and default values.
  static void PrintHelp();

  // Set flags as consequence of being implied by another flag.
  static void EnforceFlagImplications();
};

} }  // namespace v8::internal

#endif  // V8_FLAGS_H_
