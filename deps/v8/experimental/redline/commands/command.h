// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_COMMANDS_COMMAND_H_
#define V8_EXPERIMENTAL_REDLINE_COMMANDS_COMMAND_H_

#include <string>
#include <vector>

namespace redline {

class Flags;

// A redline subcommand: "redline <name> [flags] [positional args]".
struct Command {
  const char* name;
  const char* summary;
  // Full usage text, printed by "redline help <name>".
  const char* usage;
  int (*run)(const Flags& flags);
};

// Exit codes shared by all commands.
constexpr int kExitOk = 0;
constexpr int kExitFailure = 1;
constexpr int kExitUsage = 2;

extern const Command kAmplifyCommand;
extern const Command kAuditCommand;
extern const Command kExecCommand;
extern const Command kFuzzCommand;
extern const Command kMinimizeCommand;
extern const Command kMutateCommand;
extern const Command kParseCommand;
extern const Command kReplayCommand;
extern const Command kRoundtripCommand;
extern const Command kSelftestCommand;
extern const Command kShellCommand;

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_COMMANDS_COMMAND_H_
