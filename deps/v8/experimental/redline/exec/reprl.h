// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_EXEC_REPRL_H_
#define V8_EXPERIMENTAL_REDLINE_EXEC_REPRL_H_

#include <sys/types.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "experimental/redline/exec/execution.h"

namespace redline {

class DataChannel;

// Drives one target process over the REPRL ("read-eval-print-reset-loop")
// protocol implemented by d8 when built with v8_fuzzilli=true. This is a C++
// port of Fuzzilli's libreprl and speaks exactly the same protocol, so any
// d8 that works with Fuzzilli works here:
//
//  - The child is started with four well-known file descriptors:
//      100: control pipe, child reads commands ("exec" + 8 byte script size)
//      101: control pipe, child writes a 4 byte status word per execution
//      102: data channel, child reads the script source
//      103: data channel, child writes fuzzilli('FUZZILLI_PRINT') output
//    plus an "SHM_ID" environment variable naming the coverage bitmap.
//  - After startup the child writes "HELO" on 101 and expects "HELO" on 100.
//  - Per execution the parent writes the script into the 102 channel, sends
//    "exec" and the size, waits for the status word (or a timeout, or EOF
//    which means the child died). The status word is `exit_code << 8`;
//    children that die report their wait status (signal in the low byte).
//  - The same child is reused for many executions; it is killed on timeouts,
//    crashes, and after max_execs_before_respawn executions.
//
// Instances are not thread-safe: use one per worker thread.
class ReprlChild {
 public:
  struct Options {
    std::string executable;
    std::vector<std::string> args;  // without argv[0]
    // Complete environment of the child (this class adds nothing itself).
    std::vector<std::pair<std::string, std::string>> env;
    bool capture_stdout = true;
    bool capture_stderr = true;
    // Kill and respawn the child after this many executions to bound the
    // amount of state that accumulates in the target. 0 disables this.
    int max_execs_before_respawn = 1000;
    // How long to wait for the HELO handshake after spawning.
    uint64_t startup_timeout_us = static_cast<uint64_t>(60) * 1000 * 1000;
  };

  static std::unique_ptr<ReprlChild> Create(Options options,
                                            std::string* error);
  ~ReprlChild();
  ReprlChild(const ReprlChild&) = delete;
  ReprlChild& operator=(const ReprlChild&) = delete;

  // Executes `script`, spawning a child first if none is running. Returns
  // true if the execution happened (whatever its outcome). Returns false
  // on an infrastructure failure (cannot spawn, protocol violation) with a
  // description in *error; the child, if any, has been killed in that case
  // and the next call will spawn a fresh one.
  bool Execute(std::string_view script, uint64_t timeout_us, Execution* result,
               std::string* error);

  // Terminates the current child, if any.
  void Kill();

  bool is_running() const { return pid_ != 0; }
  pid_t pid() const { return pid_; }
  int execs_since_spawn() const { return execs_since_spawn_; }
  uint64_t total_spawns() const { return total_spawns_; }
  uint64_t total_executions() const { return total_executions_; }
  const Options& options() const { return options_; }

 private:
  explicit ReprlChild(Options options);
  bool Initialize(std::string* error);
  bool Spawn(std::string* error);
  void OnChildTerminated();
  void CollectOutput(Execution* result);

  Options options_;
  std::vector<std::string> argv_storage_;
  std::vector<std::string> env_storage_;
  std::vector<char*> argv_;
  std::vector<char*> envp_;

  std::unique_ptr<DataChannel> script_channel_;   // parent -> child (fd 102)
  std::unique_ptr<DataChannel> fuzzout_channel_;  // child -> parent (fd 103)
  std::unique_ptr<DataChannel> stdout_channel_;
  std::unique_ptr<DataChannel> stderr_channel_;

  pid_t pid_ = 0;
  int ctrl_in_ = -1;   // we read status words here
  int ctrl_out_ = -1;  // we write commands here
  int execs_since_spawn_ = 0;
  uint64_t total_spawns_ = 0;
  uint64_t total_executions_ = 0;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_EXEC_REPRL_H_
