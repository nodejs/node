// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// "redline amplify": runs the amplify step (DESIGN.md 5.8) on one program and
// prints the d8 flags that reproduce its crash, if any. The same code runs in
// the fuzzer's crash triage.

#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/json_parser.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/commands/command.h"
#include "experimental/redline/engine/amplifier.h"
#include "experimental/redline/engine/prelude.h"
#include "experimental/redline/exec/process_run.h"

namespace redline {

namespace {

constexpr const char* kLog = "amplify";

// The value of a "key: value" line of a crash report header, or "".
std::string ReportField(const std::string& report, const std::string& key) {
  const std::string prefix = key + ": ";
  size_t pos = 0;
  while (pos < report.size()) {
    size_t end = report.find('\n', pos);
    if (end == std::string::npos) {
      end = report.size();
    }
    if (report.compare(pos, 4, "--- ") == 0) {
      break;
    }
    if (report.compare(pos, prefix.size(), prefix) == 0) {
      return report.substr(pos + prefix.size(), end - pos - prefix.size());
    }
    pos = end + 1;
  }
  return "";
}

// The text between "--- stderr ---" and "--- stdout ---" of a crash report.
std::string ReportStderr(const std::string& report) {
  const std::string open = "--- stderr ---\n";
  size_t from = report.find(open);
  if (from == std::string::npos) {
    return "";
  }
  from += open.size();
  size_t to = report.find("\n--- stdout ---", from);
  return report.substr(from,
                       to == std::string::npos ? std::string::npos : to - from);
}

int RunAmplify(const Flags& flags) {
  TargetOptions target;
  std::string error;
  if (!TargetOptions::FromFlags(flags, &target, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitUsage;
  }
  if (flags.positional().size() != 1) {
    REDLINE_ERROR(kLog, "Give exactly one JavaScript file");
    return kExitUsage;
  }
  std::string program;
  if (!ReadFileToString(flags.positional()[0], &program, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitFailure;
  }
  // The prelude: "default" (Redline's), "none", or a file.
  std::string prelude;
  const std::string prelude_flag = flags.GetString("prelude", "default");
  if (prelude_flag == "default") {
    prelude = std::string(DefaultPrelude());
  } else if (prelude_flag != "none" &&
             !ReadFileToString(prelude_flag, &prelude, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitFailure;
  }
  // The strategy: {"strategy": NAME, "params": {...}}, default flags.
  JsonValue amplify;
  if (flags.Has("amplify") &&
      !JsonValue::Parse(flags.GetString("amplify"), &amplify, &error)) {
    REDLINE_ERROR(kLog, "--amplify: %s", error.c_str());
    return kExitUsage;
  }
  std::unique_ptr<AmplifyStrategy> strategy = CreateAmplifyStrategy(
      amplify.GetString("strategy", "flags"), amplify.Get("params"), &error);
  if (strategy == nullptr) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitUsage;
  }

  AmplifyRequest request;
  request.target = target;
  request.program = program;
  request.prelude = prelude;
  bool remove_script_on_exit = false;
  if (flags.Has("work-file")) {
    request.script_path = flags.GetString("work-file");
  } else {
    char tmpl[] = "/tmp/redline_amplify_XXXXXX.js";
    int fd = mkstemps(tmpl, 3);
    if (fd < 0) {
      REDLINE_ERROR(kLog, "mkstemps failed: %s", strerror(errno));
      return kExitFailure;
    }
    close(fd);
    request.script_path = tmpl;
    remove_script_on_exit = true;
  }
  struct ScriptFileGuard {
    std::string path;
    bool active = false;
    ~ScriptFileGuard() {
      if (active && !path.empty()) {
        std::remove(path.c_str());
      }
    }
  } script_guard{request.script_path, remove_script_on_exit};
  if (!WriteStringToFile(request.script_path,
                         prelude.empty() ? program : prelude + "\n" + program,
                         &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitFailure;
  }
  // One plain run without extra flags: its time sets --stress-runs, and
  // without --crash its crash is the one to amplify.
  std::vector<std::string> args = target.args;
  args.push_back(request.script_path);
  const ProcessResult first =
      RunProcess(target.executable, args, target.PlainEnvironment(),
                 static_cast<uint64_t>(60) * 1000 * 1000);
  if (!first.started) {
    REDLINE_ERROR(kLog, "Cannot start d8: %s", first.error.c_str());
    return kExitFailure;
  }
  request.exec_time_us = first.wall_us;
  if (flags.Has("crash")) {
    std::string report;
    if (!ReadFileToString(flags.GetString("crash"), &report, &error)) {
      REDLINE_ERROR(kLog, "%s", error.c_str());
      return kExitFailure;
    }
    request.stderr_text = ReportStderr(report);
    request.signature = CrashSignature::FromOutput(
        static_cast<int>(
            strtol(ReportField(report, "signal").c_str(), nullptr, 10)),
        static_cast<int>(
            strtol(ReportField(report, "exit_code").c_str(), nullptr, 10)),
        request.stderr_text);
    const std::string recorded = ReportField(report, "signature");
    if (!recorded.empty() &&
        request.signature.text.rfind(recorded.substr(0, 40), 0) != 0) {
      REDLINE_WARN(kLog,
                   "signature from the report's stderr (%s) differs from its "
                   "header (%s)",
                   request.signature.text.c_str(), recorded.c_str());
    }
  } else {
    if (first.signal == 0 && first.exit_code == 0) {
      REDLINE_ERROR(kLog,
                    "The program does not crash in one plain run: pass "
                    "--crash=REPORT.txt");
      return kExitFailure;
    }
    request.signature = CrashSignature::FromOutput(
        first.signal, first.exit_code, first.stderr_text);
    request.stderr_text = first.stderr_text;
  }
  printf("signature: %s\nfirst plain run: %.0f ms\n",
         request.signature.text.c_str(),
         static_cast<double>(first.wall_us) / 1000.0);

  Random rng(static_cast<uint64_t>(
      flags.GetInt("seed", static_cast<int64_t>(WallClockNowMs()))));
  const AmplifyResult result = strategy->Amplify(request, rng);
  printf(
      "amplified: %s\n%s: %s\namplified_rate: %d/%d\namplify_processes: "
      "%d\namplify_time_sec: %.0f\n"
      "program_wrapped: %s\ntried: %s\n",
      result.amplified ? "yes" : "no",
      result.amplified ? "amplified_args" : "best_candidate",
      result.args.c_str(), result.hits, result.processes,
      result.total_processes, result.time_sec,
      result.program.empty() ? "no" : "yes", result.note.c_str());
  // The changed program, which the amplified flags reproduce.
  if (!result.program.empty() && flags.Has("out")) {
    if (!WriteStringToFile(flags.GetString("out"), result.program, &error)) {
      REDLINE_ERROR(kLog, "%s", error.c_str());
      return kExitFailure;
    }
    printf("wrapped program: %s\n", flags.GetString("out").c_str());
  }
  return result.amplified ? kExitOk : kExitFailure;
}

}  // namespace

const Command kAmplifyCommand = {
    "amplify",
    "Search d8 flags that reproduce a flaky crash",
    "usage: redline amplify --d8=PATH [options] FILE.js\n"
    "\n"
    "Runs the amplify step of crash triage on one program: candidate d8 flags\n"
    "(from stack rules, then the plain loop) each run in fresh plain d8\n"
    "processes with --stress-runs. Prints the flags that reproduce the crash.\n"
    "Exit code 0 if amplified, 1 otherwise.\n"
    "\n"
    "options:\n"
    "  --crash=REPORT.txt   the crash to reproduce (a Redline crash report);\n"
    "                       default: the crash of one plain run\n"
    "  --prelude=FILE       the prelude to put before the program, "
    "\"default\"\n"
    "                       (Redline's) or \"none\" (default: default)\n"
    "  --amplify=JSON       {\"strategy\": \"flags\", \"params\": {...}}\n"
    "  --seed=N             random seed (the --random-seed values of d8)\n"
    "  --work-file=PATH     where to write the script for d8\n"
    "  --out=FILE           write the wrapped program here, if the step "
    "wrapped\n"
    "                       it in try/catch (the amplified flags reproduce "
    "it)\n",
    RunAmplify,
};

}  // namespace redline
