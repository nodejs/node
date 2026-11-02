// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_BASE_SUBPROCESS_H_
#define V8_EXPERIMENTAL_REDLINE_BASE_SUBPROCESS_H_

#include <cstdint>
#include <string>
#include <vector>

namespace redline {

// Result of running a child process to completion.
struct SubprocessResult {
  // True if the child was started at all.
  bool started = false;
  // True if the child was killed because it exceeded the timeout.
  bool timed_out = false;
  // Exit code if the child exited normally, otherwise -1.
  int exit_code = -1;
  // Terminating signal if the child was killed by one, otherwise 0.
  int signal = 0;
  std::string stdout_data;
  std::string stderr_data;
  uint64_t elapsed_us = 0;
  // Why `started` is false.
  std::string error;

  bool Succeeded() const { return started && !timed_out && exit_code == 0; }
};

// Runs `argv` (argv[0] is the executable path) with stdin from /dev/null,
// capturing stdout and stderr (each truncated at `max_output_bytes`). Kills
// the process group after `timeout_ms` milliseconds. Core dumps are disabled.
// The child's environment is inherited; `extra_env` (KEY=VALUE) is appended.
SubprocessResult RunSubprocess(const std::vector<std::string>& argv,
                               int64_t timeout_ms,
                               const std::vector<std::string>& extra_env = {},
                               size_t max_output_bytes = 1 << 20);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_BASE_SUBPROCESS_H_
