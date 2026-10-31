// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_EXEC_TARGET_OPTIONS_H_
#define V8_EXPERIMENTAL_REDLINE_EXEC_TARGET_OPTIONS_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "experimental/redline/exec/reprl.h"

namespace redline {

class Flags;

// Everything needed to start and talk to the target: the d8 binary, its
// flags and environment, and the execution timeout. All commands that run
// scripts share these flags:
//
//   --d8=PATH                 the instrumented d8 (or $REDLINE_D8)
//   --d8-args="--a --b"       replaces the default d8 flags
//   --d8-extra-args="--c"     appended to the (default or given) d8 flags
//   --d8-env=K=V,K2=V2        extra environment variables for d8
//   --timeout-ms=N            per-execution timeout (default 1000)
//   --max-execs-before-respawn=N  respawn d8 after N executions (1000)
//   --capture-output          capture d8 stdout/stderr (default true)
struct TargetOptions {
  std::string executable;
  std::vector<std::string> args;
  std::vector<std::pair<std::string, std::string>> extra_env;
  uint64_t timeout_us = static_cast<uint64_t>(1000) * 1000;
  int max_execs_before_respawn = 1000;
  bool capture_output = true;

  // The flags Fuzzilli's V8 profile passes to d8 (without the randomized
  // part), so that coverage numbers are comparable.
  static std::vector<std::string> DefaultArgs();

  // Parses the shared target flags. Returns false with a message if the
  // target is not usable.
  static bool FromFlags(const Flags& flags, TargetOptions* out,
                        std::string* error);

  // Builds the REPRL options for a child whose coverage bitmap is named
  // `shm_name`. The environment is deliberately minimal: a small allowlist
  // inherited from this process, SHM_ID, and extra_env.
  ReprlChild::Options ToReprlOptions(const std::string& shm_name) const;

  // The environment of a plain (non-REPRL) d8 run: the same allowlist as
  // ToReprlOptions plus extra_env, without SHM_ID.
  std::vector<std::pair<std::string, std::string>> PlainEnvironment() const;

  // The variables a target inherits from this process (allowlist).
  static std::vector<std::pair<std::string, std::string>>
  InheritedEnvironment();

  std::string CommandLine() const;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_EXEC_TARGET_OPTIONS_H_
