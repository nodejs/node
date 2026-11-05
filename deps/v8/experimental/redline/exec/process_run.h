// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runs a program once as a plain process (no REPRL): stdout goes to
// /dev/null, stderr is captured (bounded), and the process is killed after a
// timeout. The crash amplifier uses it to measure a d8 command line exactly as
// ClusterFuzz runs it.

#ifndef V8_EXPERIMENTAL_REDLINE_EXEC_PROCESS_RUN_H_
#define V8_EXPERIMENTAL_REDLINE_EXEC_PROCESS_RUN_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace redline {

struct ProcessResult {
  bool started = false;    // false: the process could not be spawned
  int exit_code = 0;       // valid if signal == 0
  int signal = 0;          // the signal that ended the process, or 0
  bool timed_out = false;  // killed after the timeout
  uint64_t wall_us = 0;
  std::string stderr_text;  // at most max_stderr bytes
  std::string error;        // why it could not be spawned
};

// `args` without argv[0]; `env` is the complete environment of the child.
ProcessResult RunProcess(
    const std::string& executable, const std::vector<std::string>& args,
    const std::vector<std::pair<std::string, std::string>>& env,
    uint64_t timeout_us, size_t max_stderr = static_cast<size_t>(256) * 1024);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_EXEC_PROCESS_RUN_H_
