// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// redline: a coverage-guided JavaScript fuzzer for d8.
//
//   redline <command> [--flags] [args]
//
// Run "redline help" for the list of commands and "redline help <command>"
// for the flags of one command.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/commands/command.h"
#include "include/v8-version-string.h"

namespace redline {

namespace {

constexpr const char* kLog = "redline";

const char kGlobalFlagsHelp[] =
    "global flags:\n"
    "  --log-level=verbose|info|warning|error   (default info)\n"
    "  --verbose                                 same as --log-level=verbose\n"
    "\n"
    "target flags (commands that run d8):\n"
    "  --d8=PATH                   the instrumented d8 (or $REDLINE_D8)\n"
    "  --d8-args=\"--a --b\"         replace the default d8 flags\n"
    "  --d8-extra-args=\"--c\"       append to the d8 flags\n"
    "  --d8-env=K=V,K2=V2          extra environment variables for d8\n"
    "  --timeout-ms=N              per-execution timeout (default 1000)\n"
    "  --max-execs-before-respawn=N  respawn d8 after N executions (1000)\n"
    "  --no-capture-output         do not capture d8 stdout/stderr\n";

int RunVersion(const Flags&);
int RunHelp(const Flags& flags);

const Command kVersionCommand = {
    "version", "Print the V8 version this binary was built from",
    "usage: redline version\n", RunVersion};

const Command kHelpCommand = {"help", "Show help for a command",
                              "usage: redline help [command]\n", RunHelp};

const Command* const kCommands[] = {
    &kFuzzCommand,     &kShellCommand,     &kMutateCommand,   &kAuditCommand,
    &kMinimizeCommand, &kAmplifyCommand,   &kExecCommand,     &kReplayCommand,
    &kParseCommand,    &kRoundtripCommand, &kSelftestCommand, &kVersionCommand,
    &kHelpCommand,
};

const Command* FindCommand(const std::string& name) {
  for (const Command* command : kCommands) {
    if (name == command->name) {
      return command;
    }
  }
  return nullptr;
}

void PrintCommandList() {
  printf("usage: redline <command> [flags] [args]\n\ncommands:\n");
  for (const Command* command : kCommands) {
    printf("  %-10s %s\n", command->name, command->summary);
  }
  printf("\n%s", kGlobalFlagsHelp);
}

int RunVersion(const Flags&) {
  printf("redline (V8 %s)\n", V8_VERSION_STRING);
  return kExitOk;
}

int RunHelp(const Flags& flags) {
  if (flags.positional().empty()) {
    PrintCommandList();
    return kExitOk;
  }
  const Command* command = FindCommand(flags.positional()[0]);
  if (command == nullptr) {
    fprintf(stderr, "Unknown command '%s'\n", flags.positional()[0].c_str());
    PrintCommandList();
    return kExitUsage;
  }
  printf("%s\n%s", command->usage, kGlobalFlagsHelp);
  return kExitOk;
}

bool ApplyGlobalFlags(const Flags& flags) {
  std::string level = flags.GetString("log-level", "info");
  if (flags.GetBool("verbose", false)) {
    level = "verbose";
  }
  if (level == "verbose") {
    SetLogLevel(LogLevel::kVerbose);
  } else if (level == "info") {
    SetLogLevel(LogLevel::kInfo);
  } else if (level == "warning") {
    SetLogLevel(LogLevel::kWarning);
  } else if (level == "error") {
    SetLogLevel(LogLevel::kError);
  } else {
    fprintf(stderr, "Unknown --log-level=%s\n", level.c_str());
    return false;
  }
  return true;
}

int Main(int argc, const char* const* argv) {
  if (argc < 2 || strcmp(argv[1], "--help") == 0 ||
      strcmp(argv[1], "-h") == 0) {
    PrintCommandList();
    return argc < 2 ? kExitUsage : kExitOk;
  }
  const Command* command = FindCommand(argv[1]);
  if (command == nullptr) {
    fprintf(stderr, "Unknown command '%s'\n\n", argv[1]);
    PrintCommandList();
    return kExitUsage;
  }
  Flags flags = Flags::Parse(argc, argv, 2);
  if (flags.GetBool("help", false)) {
    printf("%s\n%s", command->usage, kGlobalFlagsHelp);
    return kExitOk;
  }
  if (!ApplyGlobalFlags(flags)) {
    return kExitUsage;
  }
  int result = command->run(flags);
  // A command that stopped early on an error has not read its remaining
  // flags; reporting those as typos would only bury the real error.
  if (result == kExitOk) {
    for (const std::string& unused : flags.UnusedFlags()) {
      REDLINE_WARN(kLog, "Flag --%s is not used by '%s' (typo?)",
                   unused.c_str(), command->name);
    }
  }
  return result;
}

}  // namespace

}  // namespace redline

int main(int argc, char* argv[]) {
  return redline::Main(argc, const_cast<const char* const*>(argv));
}
