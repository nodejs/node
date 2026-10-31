// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// "redline roundtrip": measures how faithfully the FAST frontend handles real
// JavaScript. For every input file it answers, in this order:
//
//   1. Does V8 accept the file?  (in-process v8::ScriptCompiler, no execution)
//   2. Do we accept the file?    (fast::Parser)
//   3. Does printing round-trip? (print -> reparse -> print is a fixpoint)
//   4. Does V8 accept the printed program?
//   5. Does the printed program behave like the original? (both run on d8,
//      exit status + stdout + error lines compared; flaky inputs detected by
//      re-running the original)
//
// The verdict per file is one of the categories below; the summary reports
// counts and the key metrics: the false-reject rate (V8 accepts, we don't)
// and the semantic round-trip rate.

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/json.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/subprocess.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/base/v8_runtime.h"
#include "experimental/redline/commands/command.h"
#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/parser.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/fast/scope.h"
#include "experimental/redline/fast/tree.h"

namespace redline {

namespace {

constexpr const char* kLog = "roundtrip";

const char kUsage[] =
    "usage: redline roundtrip [flags] [--dir=DIR,...] [FILE...]\n"
    "\n"
    "Parses every .js file, prints it back and checks (a) that V8 and the\n"
    "FAST parser agree on what is valid, (b) that printing is a fixpoint,\n"
    "(c) that V8 accepts the printed program and (d) that original and\n"
    "printed program behave identically on d8.\n"
    "\n"
    "input selection:\n"
    "  --dir=DIR,...         directories to scan recursively\n"
    "  --suffix=.js          file suffix to pick (default .js)\n"
    "  --max-files=N         only the first N files (after sorting)\n"
    "  --sample=N --seed=S   a random sample of N files instead\n"
    "\n"
    "checks:\n"
    "  --rename[=all]        alpha-rename all variables (or also functions\n"
    "                        and classes) before printing: tests the scope\n"
    "                        analysis, since behaviour must not change\n"
    "  --no-v8-check         skip the in-process V8 syntax oracle\n"
    "  --no-run              skip the behavioural comparison on d8\n"
    "  --v8-flags=\"...\"      flags for the in-process V8 (default\n"
    "                        --allow-natives-syntax --js-staging "
    "--js-decorators)\n"
    "\n"
    "behavioural comparison:\n"
    "  --d8=PATH             d8 to run (default out/x64.release/d8, "
    "$REDLINE_D8)\n"
    "  --d8-args=\"...\"       d8 flags (default --allow-natives-syntax); the\n"
    "                        '// Flags:' lines of each test are added\n"
    "  --preload=FILE,...    scripts loaded before each test (default:\n"
    "                        test/mjsunit/mjsunit.js when under test/mjsunit)\n"
    "  --files-base=DIR      base directory for '// Files:' lines\n"
    "  --timeout-ms=N        per d8 run (default 10000)\n"
    "  --work-dir=DIR        scratch directory (default "
    "/tmp/redline-roundtrip.PID)\n"
    "\n"
    "output:\n"
    "  --jobs=N              worker threads (default 8)\n"
    "  --events=FILE.jsonl   one JSON record per file\n"
    "  --out-dir=DIR         keep original, printed program and outputs of\n"
    "                        every failing file\n"
    "  --list=N              failing files listed per category (default 20)\n"
    "  --max-failures=N      exit non-zero if more real failures (default 0)\n";

enum class Verdict {
  kOk,                   // everything agrees
  kBothReject,           // V8 and we reject (fine)
  kFalseReject,          // V8 accepts, we reject (the key metric)
  kFalseAccept,          // we accept, V8 rejects
  kReparseError,         // our printed program does not reparse
  kPrintedRejectedByV8,  // V8 rejects our printed program
  kFixpointMismatch,     // print(reparse(print(t))) != print(t)
  kBehaviourMismatch,    // d8 output differs
  kFlaky,                // original is not deterministic itself
  kTimeout,              // original times out; nothing to compare
  kEnvError,             // d8 could not run the original (bad flags, ...)
  kSkipped,              // module / unreadable
  kCount
};

const char* const kVerdictNames[] = {
    "ok",
    "both_reject",
    "false_reject",
    "false_accept",
    "reparse_error",
    "printed_rejected_by_v8",
    "fixpoint_mismatch",
    "behaviour_mismatch",
    "flaky",
    "timeout",
    "env_error",
    "skipped",
};
static_assert(sizeof(kVerdictNames) / sizeof(kVerdictNames[0]) ==
              static_cast<size_t>(Verdict::kCount));

bool IsRealFailure(Verdict v) {
  switch (v) {
    case Verdict::kFalseReject:
    case Verdict::kReparseError:
    case Verdict::kPrintedRejectedByV8:
    case Verdict::kFixpointMismatch:
    case Verdict::kBehaviourMismatch:
      return true;
    default:
      return false;
  }
}

struct Options {
  std::vector<std::string> dirs;
  std::vector<std::string> files;
  std::string suffix = ".js";
  int64_t max_files = 0;
  int64_t sample = 0;
  uint64_t seed = 1;
  bool v8_check = true;
  bool run = true;
  bool rename = false;
  fast::RenameScope rename_scope = fast::RenameScope::kVariables;
  std::string v8_flags;
  std::string d8;
  std::vector<std::string> d8_args;
  std::vector<std::string> preload;
  std::string files_base;
  int64_t timeout_ms = 10000;
  std::string work_dir;
  int jobs = 8;
  std::string events_path;
  std::string out_dir;
  int64_t list = 20;
  int64_t max_failures = 0;
};

struct FileResult {
  Verdict verdict = Verdict::kSkipped;
  std::string detail;
  size_t bytes = 0;
  size_t nodes = 0;
  double parse_ms = 0;
  double print_ms = 0;
  double run_ms = 0;
  bool original_exit_zero = false;
};

// --- test metadata ---------------------------------------------------------

struct TestMeta {
  std::vector<std::string> flags;  // from "// Flags:" lines
  std::vector<std::string> files;  // from "// Files:" lines
  bool is_module = false;
  bool only_strict = false;  // test262 "onlyStrict"
  bool raw = false;          // test262 "raw"
};

std::vector<std::string> SplitWhitespace(std::string_view text) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && isspace(static_cast<unsigned char>(text[i]))) {
      i++;
    }
    size_t start = i;
    while (i < text.size() && !isspace(static_cast<unsigned char>(text[i]))) {
      i++;
    }
    if (i > start) {
      out.emplace_back(text.substr(start, i - start));
    }
  }
  return out;
}

TestMeta ExtractMeta(const std::string& source, const std::string& path) {
  TestMeta meta;
  if (path.size() > 4 && path.ends_with(".mjs")) {
    meta.is_module = true;
  }
  size_t pos = 0;
  while (pos < source.size()) {
    size_t end = source.find('\n', pos);
    if (end == std::string::npos) {
      end = source.size();
    }
    std::string_view line(source.data() + pos, end - pos);
    pos = end + 1;
    size_t first = line.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
      continue;
    }
    line = line.substr(first);
    if (line.starts_with("// Flags:")) {
      for (std::string& flag : SplitWhitespace(line.substr(9))) {
        meta.flags.push_back(std::move(flag));
      }
    } else if (line.starts_with("// Files:")) {
      for (std::string& file : SplitWhitespace(line.substr(9))) {
        meta.files.push_back(std::move(file));
      }
    } else if (line.starts_with("// MODULE")) {
      meta.is_module = true;
    }
  }
  // test262 front matter.
  size_t fm_start = source.find("/*---");
  if (fm_start != std::string::npos) {
    size_t fm_end = source.find("---*/", fm_start);
    if (fm_end != std::string::npos) {
      std::string_view fm(source.data() + fm_start, fm_end - fm_start);
      size_t flags_pos = fm.find("flags:");
      if (flags_pos != std::string_view::npos) {
        size_t line_end = fm.find('\n', flags_pos);
        std::string_view flags_line =
            fm.substr(flags_pos, line_end == std::string_view::npos
                                     ? std::string_view::npos
                                     : line_end - flags_pos);
        if (flags_line.find("module") != std::string_view::npos) {
          meta.is_module = true;
        }
        if (flags_line.find("onlyStrict") != std::string_view::npos) {
          meta.only_strict = true;
        }
        if (flags_line.find("raw") != std::string_view::npos) {
          meta.raw = true;
        }
      }
    }
  }
  return meta;
}

// --- output normalization ----------------------------------------------------

void ReplaceAll(std::string* text, const std::string& from,
                const std::string& to) {
  if (from.empty()) {
    return;
  }
  size_t pos = 0;
  while ((pos = text->find(from, pos)) != std::string::npos) {
    text->replace(pos, from.size(), to);
    pos += to.size();
  }
}

// Replaces "<script>:12:3", "<script>:12" and "<script> 12:3" (the format
// of mjsunit's stack frames) by "<script>:L" / "<script> L" so that line
// numbers, which legitimately differ after printing, do not count.
void NormalizePositions(std::string* text) {
  const std::string marker = "<script>";
  size_t pos = 0;
  while ((pos = text->find(marker, pos)) != std::string::npos) {
    size_t i = pos + marker.size();
    if (i >= text->size() || ((*text)[i] != ':' && (*text)[i] != ' ')) {
      pos = i;
      continue;
    }
    i++;
    size_t digits_start = i;
    while (i < text->size() &&
           isdigit(static_cast<unsigned char>((*text)[i]))) {
      i++;
    }
    if (i == digits_start) {
      pos = i;
      continue;
    }
    size_t j = i;
    if (j < text->size() && (*text)[j] == ':') {
      size_t k = j + 1;
      while (k < text->size() &&
             isdigit(static_cast<unsigned char>((*text)[k]))) {
        k++;
      }
      if (k > j + 1) {
        j = k;
      }
    }
    text->replace(digits_start, j - digits_start, "L");
    pos = digits_start + 1;
  }
}

// d8 reports uncaught exceptions on stdout as
//   <script>:12: TypeError: boom
//   <the source line>
//   <spaces and ^^^ carets>
// The last two lines legitimately differ after printing; drop them.
std::string DropSourceEchoLines(const std::string& text) {
  std::string out;
  size_t pos = 0;
  int skip = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) {
      end = text.size();
    }
    std::string_view line(text.data() + pos, end - pos);
    pos = end + 1;
    if (skip > 0) {
      skip--;
      // Only the caret line is recognizable; be permissive about the echo.
      bool caret_line = !line.empty() &&
                        line.find_first_not_of(" ^") == std::string_view::npos;
      if (skip == 0 && !caret_line) {
        // The second line was not a caret line: keep it.
        out.append(line);
        out.push_back('\n');
      }
      continue;
    }
    out.append(line);
    out.push_back('\n');
    if (line.starts_with("<script>:L: ") &&
        line.find("Error") != std::string_view::npos) {
      skip = 2;
    }
  }
  return out;
}

std::string NormalizeStdout(std::string text, const std::string& path_a,
                            const std::string& path_b) {
  ReplaceAll(&text, path_a, "<script>");
  ReplaceAll(&text, path_b, "<script>");
  NormalizePositions(&text);
  return DropSourceEchoLines(text);
}

// Keeps only the error lines of stderr: uncaught exception reports contain
// a source echo and a caret line that differ after printing.
std::string NormalizeStderr(std::string text, const std::string& path_a,
                            const std::string& path_b) {
  ReplaceAll(&text, path_a, "<script>");
  ReplaceAll(&text, path_b, "<script>");
  NormalizePositions(&text);
  std::string out;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) {
      end = text.size();
    }
    std::string_view line(text.data() + pos, end - pos);
    pos = end + 1;
    if (line.starts_with("    at ")) {
      continue;
    }
    bool error_like = line.find("Error") != std::string_view::npos ||
                      line.find("error") != std::string_view::npos ||
                      line.find("Uncaught") != std::string_view::npos ||
                      line.find("Warning") != std::string_view::npos;
    if (!error_like) {
      continue;
    }
    out.append(line);
    out.push_back('\n');
  }
  return out;
}

struct RunSummary {
  bool timed_out = false;
  int exit_code = -1;
  int signal = 0;
  std::string stdout_norm;
  std::string stderr_norm;
  std::string raw_stdout;
  std::string raw_stderr;

  bool operator==(const RunSummary& other) const {
    return timed_out == other.timed_out && exit_code == other.exit_code &&
           signal == other.signal && stdout_norm == other.stdout_norm &&
           stderr_norm == other.stderr_norm;
  }
  std::string Describe() const {
    if (timed_out) {
      return "timeout";
    }
    if (signal != 0) {
      return StringPrintf("signal %d", signal);
    }
    return StringPrintf("exit %d", exit_code);
  }
};

std::string FirstDifference(const std::string& a, const std::string& b) {
  size_t pos = 0;
  size_t limit = std::min(a.size(), b.size());
  while (pos < limit && a[pos] == b[pos]) {
    pos++;
  }
  auto line_of = [](const std::string& s, size_t p) {
    size_t start = s.rfind('\n', p == 0 ? 0 : p - 1);
    start = start == std::string::npos ? 0 : start + 1;
    size_t end = s.find('\n', p);
    std::string line = s.substr(
        start, end == std::string::npos ? std::string::npos : end - start);
    if (line.size() > 160) {
      line = line.substr(0, 160) + "...";
    }
    return line;
  };
  return "original: " + line_of(a, pos) + " | printed: " + line_of(b, pos);
}

// --- the harness
// ---------------------------------------------------------------

class Harness {
 public:
  Harness(const Options& options, std::vector<std::string> files)
      : options_(options), files_(std::move(files)), results_(files_.size()) {}

  bool Run();
  int Report();

 private:
  void Worker(int worker_id);
  FileResult ProcessFile(size_t index, SyntaxChecker* checker, int worker_id);
  RunSummary RunD8(const std::vector<std::string>& argv,
                   const std::string& path_a, const std::string& path_b,
                   double* run_ms);
  void KeepFailure(const std::string& path, const std::string& source,
                   const std::string& printed, const FileResult& result,
                   const RunSummary* a, const RunSummary* b);

  Options options_;
  std::vector<std::string> files_;
  std::vector<FileResult> results_;
  std::atomic<size_t> next_{0};
  std::atomic<size_t> done_{0};
  std::mutex mutex_;
  JsonlWriter events_;
  Stopwatch stopwatch_;
  uint64_t last_progress_us_ = 0;
};

bool Harness::Run() {
  if (!options_.events_path.empty() &&
      !events_.Open(options_.events_path, /*truncate=*/true)) {
    return false;
  }
  std::string error;
  bool clean_work_dir = false;
  if (options_.run) {
    if (options_.work_dir.empty()) {
      char tmpl[] = "/tmp/redline-roundtrip.XXXXXX";
      if (mkdtemp(tmpl) == nullptr) {
        REDLINE_ERROR(kLog, "mkdtemp failed: %s", strerror(errno));
        return false;
      }
      options_.work_dir = std::string(tmpl);
      clean_work_dir = true;
    } else if (!MakeDirectories(options_.work_dir, &error)) {
      REDLINE_ERROR(kLog, "%s", error.c_str());
      return false;
    }
  }
  if (!options_.out_dir.empty() && !MakeDirectories(options_.out_dir, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return false;
  }
  if (options_.v8_check && !InitializeV8(options_.v8_flags, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return false;
  }
  int jobs = std::max(
      1, std::min<int>(options_.jobs, static_cast<int>(files_.size())));
  REDLINE_LOG(kLog, "%zu files, %d workers, v8-check=%s, run=%s%s",
              files_.size(), jobs, options_.v8_check ? "yes" : "no",
              options_.run ? "yes" : "no",
              options_.run ? (" (" + options_.d8 + ")").c_str() : "");
  std::vector<std::thread> threads;
  threads.reserve(static_cast<size_t>(jobs));
  for (int i = 0; i < jobs; i++) {
    threads.emplace_back([this, i] { Worker(i); });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  if (clean_work_dir) {
    for (int i = 0; i < jobs; i++) {
      std::remove(
          StringPrintf("%s/%d.a.js", options_.work_dir.c_str(), i).c_str());
      std::remove(
          StringPrintf("%s/%d.b.js", options_.work_dir.c_str(), i).c_str());
    }
    rmdir(options_.work_dir.c_str());
  }
  events_.Flush();
  return true;
}

void Harness::Worker(int worker_id) {
  std::unique_ptr<SyntaxChecker> checker;
  if (options_.v8_check) {
    checker = std::make_unique<SyntaxChecker>();
  }
  while (true) {
    size_t index = next_.fetch_add(1);
    if (index >= files_.size()) {
      break;
    }
    FileResult result = ProcessFile(index, checker.get(), worker_id);
    results_[index] = result;
    if (events_.is_open()) {
      JsonObject record;
      record.Add("file", files_[index])
          .Add("verdict", kVerdictNames[static_cast<int>(result.verdict)])
          .Add("detail", result.detail)
          .Add("bytes", static_cast<uint64_t>(result.bytes))
          .Add("nodes", static_cast<uint64_t>(result.nodes))
          .Add("parse_ms", result.parse_ms)
          .Add("print_ms", result.print_ms)
          .Add("run_ms", result.run_ms)
          .Add("original_exit_zero", result.original_exit_zero);
      events_.Write(record);
    }
    size_t done = done_.fetch_add(1) + 1;
    uint64_t now = stopwatch_.ElapsedUs();
    std::scoped_lock<std::mutex> lock(mutex_);
    if (IsRealFailure(result.verdict)) {
      REDLINE_LOG(kLog, "%s: %s: %s",
                  kVerdictNames[static_cast<int>(result.verdict)],
                  files_[index].c_str(), result.detail.c_str());
    }
    if (now - last_progress_us_ > 5000000 || done == files_.size()) {
      last_progress_us_ = now;
      REDLINE_LOG(kLog, "%zu/%zu files (%.0f s)", done, files_.size(),
                  static_cast<double>(now) / 1e6);
    }
  }
}

RunSummary Harness::RunD8(const std::vector<std::string>& argv,
                          const std::string& path_a, const std::string& path_b,
                          double* run_ms) {
  SubprocessResult r = RunSubprocess(argv, options_.timeout_ms);
  *run_ms += static_cast<double>(r.elapsed_us) / 1000.0;
  RunSummary summary;
  summary.timed_out = r.timed_out;
  summary.exit_code = r.exit_code;
  summary.signal = r.signal;
  summary.raw_stdout = r.stdout_data;
  summary.raw_stderr = r.stderr_data;
  summary.stdout_norm = NormalizeStdout(r.stdout_data, path_a, path_b);
  summary.stderr_norm = NormalizeStderr(r.stderr_data, path_a, path_b);
  if (!r.started) {
    summary.stderr_norm = "redline: " + r.error;
  }
  return summary;
}

void Harness::KeepFailure(const std::string& path, const std::string& source,
                          const std::string& printed, const FileResult& result,
                          const RunSummary* a, const RunSummary* b) {
  if (options_.out_dir.empty()) {
    return;
  }
  std::string base = options_.out_dir + "/" + BaseName(path);
  std::string error;
  WriteStringToFile(base, source, &error);
  if (!printed.empty()) {
    WriteStringToFile(base + ".printed.js", printed, &error);
  }
  std::string report =
      std::string(kVerdictNames[static_cast<int>(result.verdict)]) + ": " +
      result.detail + "\n";
  if (a != nullptr) {
    report += "\n=== original: " + a->Describe() + "\n--- stdout\n" +
              a->raw_stdout + "\n--- stderr\n" + a->raw_stderr;
  }
  if (b != nullptr) {
    report += "\n=== printed: " + b->Describe() + "\n--- stdout\n" +
              b->raw_stdout + "\n--- stderr\n" + b->raw_stderr;
  }
  WriteStringToFile(base + ".report.txt", report, &error);
}

FileResult Harness::ProcessFile(size_t index, SyntaxChecker* checker,
                                int worker_id) {
  const std::string& path = files_[index];
  FileResult result;
  std::string source, error;
  if (!ReadFileToString(path, &source, &error)) {
    result.verdict = Verdict::kSkipped;
    result.detail = error;
    return result;
  }
  result.bytes = source.size();
  TestMeta meta = ExtractMeta(source, path);
  if (meta.is_module) {
    result.verdict = Verdict::kSkipped;
    result.detail = "module";
    return result;
  }
  // test262 onlyStrict tests are run with a "use strict" prologue.
  std::string effective_source = source;
  if (meta.only_strict && !meta.raw) {
    effective_source = "\"use strict\";\n" + source;
  }

  // 1. V8's verdict on the original.
  bool v8_accepts = true;
  std::string v8_message;
  if (checker != nullptr) {
    v8_accepts = checker->Accepts(effective_source, &v8_message);
  }

  // 2. Ours.
  fast::ParseOptions parse_options;
  fast::ParseError parse_error;
  Stopwatch stopwatch;
  std::unique_ptr<fast::Tree> tree =
      fast::Parser::ParseScript(effective_source, parse_options, &parse_error);
  result.parse_ms = static_cast<double>(stopwatch.ElapsedUs()) / 1000.0;
  if (!tree) {
    if (!v8_accepts) {
      result.verdict = Verdict::kBothReject;
      result.detail = parse_error.ToString();
    } else {
      result.verdict = Verdict::kFalseReject;
      result.detail = parse_error.ToString();
      KeepFailure(path, source, "", result, nullptr, nullptr);
    }
    return result;
  }
  result.nodes = fast::CountNodes(tree->root());
  if (!v8_accepts) {
    result.verdict = Verdict::kFalseAccept;
    result.detail = v8_message;
    return result;
  }

  // 2b. Optionally rename every binding (scope analysis self-test).
  if (options_.rename) {
    std::unique_ptr<fast::ScopeAnalysis> analysis =
        fast::ScopeAnalysis::Analyze(tree.get());
    fast::RenameAllBindings(tree.get(), analysis.get(), "v",
                            options_.rename_scope);
  }

  // 3. Print and check the fixpoint.
  stopwatch.Restart();
  fast::PrintOptions print_options;
  std::string printed = fast::Print(tree->root(), print_options);
  result.print_ms = static_cast<double>(stopwatch.ElapsedUs()) / 1000.0;
  fast::ParseError reparse_error;
  std::unique_ptr<fast::Tree> second =
      fast::Parser::ParseScript(printed, parse_options, &reparse_error);
  if (!second) {
    result.verdict = Verdict::kReparseError;
    result.detail = reparse_error.ToString();
    KeepFailure(path, source, printed, result, nullptr, nullptr);
    return result;
  }
  std::string printed_again = fast::Print(second->root(), print_options);
  if (printed_again != printed) {
    result.verdict = Verdict::kFixpointMismatch;
    result.detail = FirstDifference(printed, printed_again);
    KeepFailure(path, source, printed, result, nullptr, nullptr);
    return result;
  }

  // 4. V8's verdict on the printed program.
  if (checker != nullptr && !checker->Accepts(printed, &v8_message)) {
    result.verdict = Verdict::kPrintedRejectedByV8;
    result.detail = v8_message;
    KeepFailure(path, source, printed, result, nullptr, nullptr);
    return result;
  }

  if (!options_.run) {
    result.verdict = Verdict::kOk;
    return result;
  }

  // 5. Behavioural comparison on d8.
  std::string path_a =
      StringPrintf("%s/%d.a.js", options_.work_dir.c_str(), worker_id);
  std::string path_b =
      StringPrintf("%s/%d.b.js", options_.work_dir.c_str(), worker_id);
  if (!WriteStringToFile(path_a, effective_source, &error) ||
      !WriteStringToFile(path_b, printed, &error)) {
    result.verdict = Verdict::kEnvError;
    result.detail = error;
    return result;
  }
  std::vector<std::string> argv;
  argv.push_back(options_.d8);
  for (const std::string& arg : options_.d8_args) {
    argv.push_back(arg);
  }
  for (const std::string& flag : meta.flags) {
    argv.push_back(flag);
  }
  for (const std::string& preload : options_.preload) {
    argv.push_back(preload);
  }
  for (const std::string& file : meta.files) {
    if (file.empty() || file[0] == '/' || FileExists(file)) {
      argv.push_back(file);
    } else {
      argv.push_back(options_.files_base + "/" + file);
    }
  }
  argv.push_back(path_a);
  RunSummary a = RunD8(argv, path_a, path_b, &result.run_ms);
  if (a.timed_out) {
    result.verdict = Verdict::kTimeout;
    result.detail = StringPrintf("original timed out after %lld ms",
                                 static_cast<long long>(options_.timeout_ms));
    return result;
  }
  if (a.raw_stderr.find("unrecognized flag") != std::string::npos ||
      a.raw_stderr.find("Error: unknown flag") != std::string::npos ||
      a.raw_stderr.find("Error loading file") != std::string::npos) {
    result.verdict = Verdict::kEnvError;
    size_t end = a.raw_stderr.find('\n');
    result.detail = a.raw_stderr.substr(0, end);
    return result;
  }
  result.original_exit_zero = a.exit_code == 0;
  argv.back() = path_b;
  RunSummary b = RunD8(argv, path_a, path_b, &result.run_ms);
  if (a == b) {
    result.verdict = Verdict::kOk;
    return result;
  }
  // Re-run both: is the test deterministic at all?
  argv.back() = path_a;
  RunSummary a2 = RunD8(argv, path_a, path_b, &result.run_ms);
  if (!(a == a2)) {
    result.verdict = Verdict::kFlaky;
    result.detail = "original differs from itself: " + a.Describe() + " vs " +
                    a2.Describe();
    return result;
  }
  argv.back() = path_b;
  RunSummary b2 = RunD8(argv, path_a, path_b, &result.run_ms);
  if (!(b == b2)) {
    result.verdict = Verdict::kFlaky;
    result.detail =
        "printed differs from itself: " + b.Describe() + " vs " + b2.Describe();
    return result;
  }
  result.verdict = Verdict::kBehaviourMismatch;
  if (a.timed_out != b.timed_out || a.exit_code != b.exit_code ||
      a.signal != b.signal) {
    result.detail = "status " + a.Describe() + " vs " + b.Describe();
    if (!b.stderr_norm.empty()) {
      size_t end = b.stderr_norm.find('\n');
      result.detail += " (" + b.stderr_norm.substr(0, end) + ")";
    }
  } else if (a.stdout_norm != b.stdout_norm) {
    result.detail = "stdout: " + FirstDifference(a.stdout_norm, b.stdout_norm);
  } else {
    result.detail = "stderr: " + FirstDifference(a.stderr_norm, b.stderr_norm);
  }
  KeepFailure(path, source, printed, result, &a, &b);
  return result;
}

int Harness::Report() {
  size_t counts[static_cast<size_t>(Verdict::kCount)] = {};
  size_t ok_passing = 0;
  size_t total_bytes = 0;
  double total_parse_ms = 0;
  for (const FileResult& r : results_) {
    counts[static_cast<size_t>(r.verdict)]++;
    if (r.verdict == Verdict::kOk && r.original_exit_zero) {
      ok_passing++;
    }
    total_bytes += r.bytes;
    total_parse_ms += r.parse_ms;
  }
  size_t total = results_.size();
  size_t v8_accepted = 0, real_failures = 0;
  for (size_t v = 0; v < static_cast<size_t>(Verdict::kCount); v++) {
    Verdict verdict = static_cast<Verdict>(v);
    if (verdict != Verdict::kBothReject && verdict != Verdict::kFalseAccept &&
        verdict != Verdict::kSkipped) {
      v8_accepted += counts[v];
    }
    if (IsRealFailure(verdict)) {
      real_failures += counts[v];
    }
  }
  size_t compared = counts[static_cast<size_t>(Verdict::kOk)] +
                    counts[static_cast<size_t>(Verdict::kBehaviourMismatch)];

  printf("\nroundtrip: %zu files, %.1f MB, %.1f s wall, parse %.0f ms total\n",
         total, static_cast<double>(total_bytes) / 1e6,
         stopwatch_.ElapsedSeconds(), total_parse_ms);
  for (size_t v = 0; v < static_cast<size_t>(Verdict::kCount); v++) {
    if (counts[v] == 0 && !IsRealFailure(static_cast<Verdict>(v))) {
      continue;
    }
    printf("  %-24s %6zu", kVerdictNames[v], counts[v]);
    if (static_cast<Verdict>(v) == Verdict::kOk && options_.run) {
      printf("   (%zu with exit 0)", ok_passing);
    }
    printf("\n");
  }
  if (v8_accepted > 0) {
    printf("false-reject rate:   %zu / %zu V8-accepted files = %.2f%%\n",
           counts[static_cast<size_t>(Verdict::kFalseReject)], v8_accepted,
           100.0 *
               static_cast<double>(
                   counts[static_cast<size_t>(Verdict::kFalseReject)]) /
               static_cast<double>(v8_accepted));
  }
  if (compared > 0) {
    printf("semantic round-trip: %zu / %zu compared files = %.2f%%\n",
           counts[static_cast<size_t>(Verdict::kOk)], compared,
           100.0 *
               static_cast<double>(counts[static_cast<size_t>(Verdict::kOk)]) /
               static_cast<double>(compared));
  }

  // List failing files per category.
  for (size_t v = 0; v < static_cast<size_t>(Verdict::kCount); v++) {
    Verdict verdict = static_cast<Verdict>(v);
    if (!IsRealFailure(verdict) && verdict != Verdict::kFalseAccept) {
      continue;
    }
    if (counts[v] == 0) {
      continue;
    }
    printf("\n%s (%zu):\n", kVerdictNames[v], counts[v]);
    int64_t listed = 0;
    for (size_t i = 0; i < results_.size() && listed < options_.list; i++) {
      if (results_[i].verdict != verdict) {
        continue;
      }
      printf("  %s: %s\n", files_[i].c_str(), results_[i].detail.c_str());
      listed++;
    }
    if (static_cast<int64_t>(counts[v]) > listed) {
      printf("  ... %zu more\n", counts[v] - static_cast<size_t>(listed));
    }
  }
  bool pass = static_cast<int64_t>(real_failures) <= options_.max_failures;
  printf("\n%s: %zu real failures (max %lld)\n", pass ? "PASS" : "FAIL",
         real_failures, static_cast<long long>(options_.max_failures));
  return pass ? kExitOk : kExitFailure;
}

bool ParseOptions(const Flags& flags, Options* options) {
  options->dirs = flags.GetList("dir");
  options->files = flags.positional();
  options->suffix = flags.GetString("suffix", ".js");
  options->max_files = flags.GetInt("max-files", 0);
  options->sample = flags.GetInt("sample", 0);
  options->seed = static_cast<uint64_t>(flags.GetInt("seed", 1));
  options->v8_check = flags.GetBool("v8-check", true);
  options->run = flags.GetBool("run", true);
  if (flags.Has("rename")) {
    options->rename = true;
    options->rename_scope = flags.GetString("rename", "") == "all"
                                ? fast::RenameScope::kAll
                                : fast::RenameScope::kVariables;
  }
  options->v8_flags = flags.GetString(
      "v8-flags", "--allow-natives-syntax --js-staging --js-decorators");
  options->d8 = flags.GetString("d8");
  if (options->d8.empty()) {
    const char* env = getenv("REDLINE_D8");
    if (env != nullptr && env[0] != '\0') {
      options->d8 = env;
    } else if (FileExists("out/x64.release/d8")) {
      options->d8 = "out/x64.release/d8";
    }
  }
  options->d8_args =
      SplitWhitespace(flags.GetString("d8-args", "--allow-natives-syntax"));
  options->preload = flags.GetList("preload");
  options->files_base = flags.GetString("files-base");
  options->timeout_ms = flags.GetInt("timeout-ms", 10000);
  options->work_dir = flags.GetString("work-dir");
  options->jobs = static_cast<int>(flags.GetInt("jobs", 8));
  options->events_path = flags.GetString("events");
  options->out_dir = flags.GetString("out-dir");
  options->list = flags.GetInt("list", 20);
  options->max_failures = flags.GetInt("max-failures", 0);

  // mjsunit conventions.
  bool under_mjsunit = false;
  for (const std::string& dir : options->dirs) {
    if (dir.find("mjsunit") != std::string::npos) {
      under_mjsunit = true;
    }
  }
  for (const std::string& file : options->files) {
    if (file.find("mjsunit") != std::string::npos) {
      under_mjsunit = true;
    }
  }
  if (under_mjsunit) {
    if (!flags.Has("preload") && FileExists("test/mjsunit/mjsunit.js")) {
      options->preload = {"test/mjsunit/mjsunit.js"};
    }
    if (options->files_base.empty()) {
      options->files_base = "test/mjsunit";
    }
  }
  if (options->files_base.empty() && !options->dirs.empty()) {
    options->files_base = options->dirs[0];
  }
  if (options->run && (options->d8.empty() || !FileExists(options->d8))) {
    REDLINE_ERROR(kLog,
                  "No d8 found (--d8=PATH or $REDLINE_D8); use --no-run to "
                  "skip execution");
    return false;
  }
  return true;
}

int RunRoundtrip(const Flags& flags) {
  Options options;
  if (!ParseOptions(flags, &options)) {
    return kExitUsage;
  }
  std::vector<std::string> files = options.files;
  for (const std::string& dir : options.dirs) {
    std::vector<std::string> found = ListFiles(dir, options.suffix);
    if (found.empty()) {
      REDLINE_WARN(kLog, "No %s files under %s", options.suffix.c_str(),
                   dir.c_str());
    }
    files.insert(files.end(), found.begin(), found.end());
  }
  if (files.empty()) {
    fputs(kUsage, stderr);
    return kExitUsage;
  }
  if (options.sample > 0 &&
      static_cast<size_t>(options.sample) < files.size()) {
    std::mt19937_64 rng(options.seed);
    std::shuffle(files.begin(), files.end(), rng);
    files.resize(static_cast<size_t>(options.sample));
    std::sort(files.begin(), files.end());
  }
  if (options.max_files > 0 &&
      static_cast<size_t>(options.max_files) < files.size()) {
    files.resize(static_cast<size_t>(options.max_files));
  }
  Harness harness(options, std::move(files));
  if (!harness.Run()) {
    return kExitFailure;
  }
  return harness.Report();
}

}  // namespace

const Command kRoundtripCommand = {
    "roundtrip", "Check parse/print fidelity of the FAST frontend on a corpus",
    kUsage, RunRoundtrip};

}  // namespace redline
