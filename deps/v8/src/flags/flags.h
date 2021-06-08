// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FLAGS_FLAGS_H_
#define V8_FLAGS_FLAGS_H_

#include <vector>

#include "src/common/globals.h"

namespace v8 {
namespace internal {

// Declare all of our flags.
#define FLAG_MODE_DECLARE
#include "src/flags/flag-definitions.h"  // NOLINT

// The global list of all flags.
class V8_EXPORT_PRIVATE FlagList {
 public:
  // The list of all flags with a value different from the default
  // and their values. The format of the list is like the format of the
  // argv array passed to the main function, e.g.
  // ("--prof", "--log-file", "v8.prof", "--nolazy").
  //
  // The caller is responsible for disposing the list, as well
  // as every element of it.
  static std::vector<const char*>* argv();

  class HelpOptions {
   public:
    enum ExitBehavior : bool { kExit = true, kDontExit = false };

    explicit HelpOptions(ExitBehavior exit_behavior = kExit,
                         const char* usage = nullptr)
        : exit_behavior_(exit_behavior), usage_(usage) {}

    bool ShouldExit() { return exit_behavior_ == kExit; }
    bool HasUsage() { return usage_ != nullptr; }
    const char* usage() { return usage_; }

   private:
    ExitBehavior exit_behavior_;
    const char* usage_;
  };

  // Set the flag values by parsing the command line. If remove_flags is
  // set, the recognized flags and associated values are removed from (argc,
  // argv) and only unknown arguments remain. Returns 0 if no error occurred.
  // Otherwise, returns the argv index > 0 for the argument where an error
  // occurred. In that case, (argc, argv) will remain unchanged independent of
  // the remove_flags value, and no assumptions about flag settings should be
  // made. If exit_behavior is set to Exit and --help has been specified on the
  // command line, then the usage string will be printed, if it was specified,
  // followed by the help flag and then the process will exit. Otherwise the
  // flag help will be displayed but execution will continue.
  //
  // The following syntax for flags is accepted (both '-' and '--' are ok):
  //
  //   --flag        (bool flags only)
  //   --no-flag     (bool flags only)
  //   --flag=value  (non-bool flags only, no spaces around '=')
  //   --flag value  (non-bool flags only)
  //   --            (capture all remaining args in JavaScript)
  static int SetFlagsFromCommandLine(
      int* argc, char** argv, bool remove_flags,
      FlagList::HelpOptions help_options = FlagList::HelpOptions());

  // Set the flag values by parsing the string str. Splits string into argc
  // substrings argv[], each of which consisting of non-white-space chars,
  // and then calls SetFlagsFromCommandLine() and returns its result.
  static int SetFlagsFromString(const char* str, size_t len);

  // Reset all flags to their default value.
  static void ResetAllFlags();

  // Print help to stdout with flags, types, and default values.
  static void PrintHelp();

  // Set flags as consequence of being implied by another flag.
  static void EnforceFlagImplications();

  // Hash of flags (to quickly determine mismatching flag expectations).
  // This hash is calculated during V8::Initialize and cached.
  static uint32_t Hash();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_FLAGS_FLAGS_H_
