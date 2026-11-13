// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// `redline flags`: prints random `d8` flag sets for a preset or a profile.

#include "experimental/redline/commands/flags_command.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/commands/command.h"
#include "experimental/redline/core/random.h"
#include "experimental/redline/engine/config.h"
#include "experimental/redline/engine/fuzzer.h"

namespace redline {

namespace {

constexpr char kLog[] = "flags";

const char kUsage[] =
    "usage: redline flags [options]\n"
    "\n"
    "Prints N random d8 flag sets on stdout, one draw per line.\n"
    "\n"
    "options:\n"
    "  --preset=NAME|FILE       preset configuration (e.g. maglev, turbolev)\n"
    "  --config=FILE|'{json}'   JSON configuration\n"
    "  --randomize-d8-flags     enable random d8 flags without a preset\n"
    "  --d8-flag-profile=NAME   maglev (default) or turbolev\n"
    "  --n=N                    number of draws to print (default 10)\n"
    "  --seed=S                 random seed (default 1)\n"
    "  --tunable=NAME=V,...     override tunables\n";

int Run(const Flags& flags) {
  std::vector<std::string> lines;
  std::string error;
  if (!DrawD8FlagLines(flags, &lines, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitUsage;
  }
  std::vector<std::string> unused = flags.UnusedFlags();
  for (const std::string& flag : unused) {
    if (flag == "log-level" || flag == "verbose") {
      continue;
    }
    REDLINE_WARN(kLog, "unknown flag --%s (ignored)", flag.c_str());
  }
  for (const std::string& line : lines) {
    printf("%s\n", line.c_str());
  }
  return kExitOk;
}

}  // namespace

bool DrawD8FlagLines(const Flags& flags, std::vector<std::string>* lines,
                     std::string* error) {
  if (!flags.positional().empty()) {
    *error = "unexpected positional argument '" + flags.positional()[0] + "'";
    return false;
  }
  const int64_t n = flags.GetInt("n", 10);
  if (n < 1) {
    *error = "--n must be at least 1";
    return false;
  }
  const bool has_seed = flags.Has("seed");
  FuzzConfig config;
  if (!FuzzConfig::Load(flags, &config, error, /*for_run=*/false)) {
    return false;
  }
  if (!config.randomize_d8_flags) {
    *error =
        "randomize_d8_flags is false (use a preset with random d8 flags or "
        "pass --randomize-d8-flags)";
    return false;
  }
  const uint64_t seed = has_seed || config.seed != 0 ? config.seed : 1;
  Fuzzer fuzzer(std::move(config));
  Random rng(seed);
  lines->clear();
  lines->reserve(static_cast<size_t>(n));
  for (int64_t i = 0; i < n; i++) {
    std::vector<std::string> drawn = fuzzer.DrawD8Flags(rng);
    std::string line;
    for (size_t j = 0; j < drawn.size(); j++) {
      if (j > 0) {
        line += ' ';
      }
      line += drawn[j];
    }
    lines->push_back(std::move(line));
  }
  return true;
}

const Command kFlagsCommand = {
    "flags",
    "Print random d8 flag draws for a preset or profile",
    kUsage,
    Run,
};

}  // namespace redline
