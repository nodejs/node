// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/exec/target_options.h"

#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/log.h"

namespace redline {

namespace {

std::vector<std::string> SplitWhitespace(const std::string& text) {
  std::vector<std::string> result;
  std::istringstream stream(text);
  std::string piece;
  while (stream >> piece) {
    result.push_back(piece);
  }
  return result;
}

}  // namespace

// static
std::vector<std::string> TargetOptions::DefaultArgs() {
  // Keep in sync with v8ProcessArgs() in Fuzzilli's V8CommonProfile.swift.
  return {
      "--expose-gc",
      "--expose-externalize-string",
      "--omit-quit",
      "--allow-natives-syntax",
      "--fuzzing",
      "--harmony",
      "--experimental-fuzzing",
      "--js-staging",
      "--wasm-staging",
      "--wasm-fast-api",
      "--expose-fast-api",
      "--wasm-test-streaming",
  };
}

// static
bool TargetOptions::FromFlags(const Flags& flags, TargetOptions* out,
                              std::string* error) {
  out->executable = flags.GetString("d8");
  if (out->executable.empty()) {
    const char* from_env = getenv("REDLINE_D8");
    if (from_env != nullptr) {
      out->executable = from_env;
    }
  }
  if (out->executable.empty()) {
    *error = "No target given: pass --d8=PATH or set $REDLINE_D8";
    return false;
  }
  if (access(out->executable.c_str(), X_OK) != 0) {
    *error = StringPrintf("--d8=%s is not an executable file: %s",
                          out->executable.c_str(), strerror(errno));
    return false;
  }

  if (flags.Has("d8-args")) {
    out->args = SplitWhitespace(flags.GetString("d8-args"));
  } else {
    out->args = DefaultArgs();
  }
  for (const std::string& arg :
       SplitWhitespace(flags.GetString("d8-extra-args"))) {
    out->args.push_back(arg);
  }

  for (const std::string& entry : flags.GetList("d8-env")) {
    size_t eq = entry.find('=');
    if (eq == std::string::npos) {
      *error = StringPrintf("--d8-env entry '%s' is not of the form KEY=VALUE",
                            entry.c_str());
      return false;
    }
    out->extra_env.emplace_back(entry.substr(0, eq), entry.substr(eq + 1));
  }

  int64_t timeout_ms = flags.GetInt("timeout-ms", 1000);
  if (timeout_ms <= 0) {
    *error = "--timeout-ms must be positive";
    return false;
  }
  out->timeout_us = static_cast<uint64_t>(timeout_ms) * 1000;
  out->max_execs_before_respawn =
      static_cast<int>(flags.GetInt("max-execs-before-respawn", 1000));
  out->capture_output = flags.GetBool("capture-output", true);
  return true;
}

ReprlChild::Options TargetOptions::ToReprlOptions(
    const std::string& shm_name) const {
  ReprlChild::Options options;
  options.executable = executable;
  options.args = args;
  options.capture_stdout = capture_output;
  options.capture_stderr = capture_output;
  options.max_execs_before_respawn = max_execs_before_respawn;
  options.env = InheritedEnvironment();
  options.env.emplace_back("SHM_ID", shm_name);
  for (const auto& kv : extra_env) {
    options.env.push_back(kv);
  }
  return options;
}

// static
std::vector<std::pair<std::string, std::string>>
TargetOptions::InheritedEnvironment() {
  static const char* const kInheritedVariables[] = {
      "PATH",          "HOME",
      "TMPDIR",        "LANG",
      "ASAN_OPTIONS",  "MSAN_OPTIONS",
      "UBSAN_OPTIONS", "TSAN_OPTIONS",
      "LSAN_OPTIONS",  "LD_LIBRARY_PATH",
  };
  std::vector<std::pair<std::string, std::string>> env;
  for (const char* name : kInheritedVariables) {
    const char* value = getenv(name);
    if (value != nullptr) {
      env.emplace_back(name, value);
    }
  }
  return env;
}

std::vector<std::pair<std::string, std::string>>
TargetOptions::PlainEnvironment() const {
  std::vector<std::pair<std::string, std::string>> env = InheritedEnvironment();
  for (const auto& kv : extra_env) {
    env.push_back(kv);
  }
  return env;
}

std::string TargetOptions::CommandLine() const {
  std::string result = executable;
  for (const std::string& arg : args) {
    result += ' ';
    result += arg;
  }
  return result;
}

}  // namespace redline
