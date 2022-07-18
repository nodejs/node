// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FLAGS_FLAGS_H_
#define V8_FLAGS_FLAGS_H_

#include "src/base/optional.h"
#include "src/common/globals.h"

namespace v8::internal {

// The value of a single flag (this is the type of all FLAG_* globals).
template <typename T>
class FlagValue {
 public:
  constexpr FlagValue(T value) : value_(value) {}

  // Implicitly convert to a {T}. Not marked {constexpr} so we do not compiler
  // warnings about dead code (when checking readonly flags).
  operator T() const { return value_; }

  // Explicitly convert to a {T} via {value()}. This is {constexpr} so we can
  // use it for computing other constants.
  constexpr T value() const { return value_; }

  // Assign a new value (defined below).
  inline FlagValue<T>& operator=(T new_value);

 private:
  T value_;
};

// Declare all of our flags.
#define FLAG_MODE_DECLARE
#include "src/flags/flag-definitions.h"

// The global list of all flags.
class V8_EXPORT_PRIVATE FlagList {
 public:
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

  // Freeze the current flag values (disallow changes via the API).
  // TODO(12887): Actually write-protect the flags.
  static void FreezeFlags();

  // Returns true if the flags are currently frozen.
  static bool IsFrozen();

  // Free dynamically allocated memory of strings. This is called during
  // teardown; flag values cannot be used afterwards any more.
  static void ReleaseDynamicAllocations();

  // Print help to stdout with flags, types, and default values.
  static void PrintHelp();

  static void PrintValues();

  // Set flags as consequence of being implied by another flag.
  static void EnforceFlagImplications();

  // Hash of flags (to quickly determine mismatching flag expectations).
  // This hash is calculated during V8::Initialize and cached.
  static uint32_t Hash();
};

template <typename T>
FlagValue<T>& FlagValue<T>::operator=(T new_value) {
  CHECK(!FlagList::IsFrozen());
  value_ = new_value;
  return *this;
}

}  // namespace v8::internal

#endif  // V8_FLAGS_FLAGS_H_
