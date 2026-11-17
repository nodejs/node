// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_EXEC_EXECUTION_H_
#define V8_EXPERIMENTAL_REDLINE_EXEC_EXECUTION_H_

#include <cstdint>
#include <string>

namespace redline {

// How a script execution in the target ended. The classification follows
// Fuzzilli's so that numbers are directly comparable:
//  - kSucceeded: the target exited the REPRL iteration with status 0.
//  - kFailed:    the script terminated with an uncaught exception (or the
//                target reported a non-zero exit code for the iteration).
//  - kCrashed:   the target process died from a signal.
//  - kTimedOut:  the execution exceeded the timeout and the target was
//                killed.
enum class Outcome : uint8_t { kSucceeded, kFailed, kCrashed, kTimedOut };

const char* OutcomeName(Outcome outcome);

struct Execution {
  Outcome outcome = Outcome::kSucceeded;
  // Exit code for kSucceeded/kFailed, terminating signal for kCrashed.
  int exit_code = 0;
  int signal = 0;
  // Wall time spent waiting for the target, in microseconds.
  uint64_t exec_time_us = 0;
  // Output produced by the target during this execution. The "fuzzout"
  // channel is what fuzzilli('FUZZILLI_PRINT', ...) writes to; stdout and
  // stderr are only captured when enabled on the ReprlChild.
  std::string fuzzout;
  std::string stdout_output;
  std::string stderr_output;

  bool succeeded() const { return outcome == Outcome::kSucceeded; }
  bool crashed() const { return outcome == Outcome::kCrashed; }
  bool timed_out() const { return outcome == Outcome::kTimedOut; }

  // For failed executions: the uncaught exception as d8 reports it, e.g.
  // "TypeError: Cannot read properties of undefined (reading 'length')"
  // (the first "<Name>Error: <message>" line of the output, without the
  // "file:line:" prefix). Empty if no such line was captured.
  std::string ExceptionSignature() const;

  // True if the target rejected the script itself as invalid JavaScript:
  // an uncaught SyntaxError reported without any stack frame. (A
  // SyntaxError thrown while running, by JSON.parse, new RegExp or eval,
  // comes with "    at ..." frames.) Needs the output to be captured.
  bool IsParseError() const;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_EXEC_EXECUTION_H_
