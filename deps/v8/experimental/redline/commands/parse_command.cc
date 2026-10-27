// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// "redline parse": parses JavaScript into FAST, optionally prints it back and
// checks that printing is a fixpoint (parse -> print -> parse -> print gives
// the same text). This is the smallest black-box test of the frontend.

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/commands/command.h"
#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/parser.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/fast/scope.h"
#include "experimental/redline/fast/tree.h"

namespace redline {

namespace {

constexpr const char* kLog = "parse";

const char kUsage[] =
    "usage: redline parse [flags] FILE...\n"
    "       redline parse [flags] --source=CODE\n"
    "\n"
    "Parses JavaScript into the fuzzer's AST (FAST) and checks that\n"
    "printing it is a fixpoint. Exit status is non-zero if any input\n"
    "fails to parse or to round-trip.\n"
    "\n"
    "flags:\n"
    "  --source=CODE      parse this snippet instead of files\n"
    "  --print            print the program as JavaScript\n"
    "  --compact          print everything on one line\n"
    "  --ids              annotate printed statements with node ids\n"
    "  --no-fixpoint      skip the print/reparse/print check\n"
    "  --no-natives       reject %Native() calls\n"
    "  --strict           parse as strict mode code\n"
    "  --scopes           dump the scope analysis (scopes, bindings, refs)\n"
    "  --rename[=all]     rename all variables (or also functions/classes)\n"
    "                     to fresh names before printing\n"
    "  --stats            print node count and timings per input\n"
    "  --quiet            only report failures\n";

struct Outcome {
  bool parsed = false;
  bool fixpoint = true;
  size_t nodes = 0;
  double parse_ms = 0;
  double print_ms = 0;
  std::string message;
};

struct ScopeFlags {
  bool dump = false;
  bool rename = false;
  fast::RenameScope rename_scope = fast::RenameScope::kVariables;
};

Outcome ProcessOne(const std::string& name, const std::string& source,
                   const fast::ParseOptions& parse_options,
                   const fast::PrintOptions& print_options, bool do_print,
                   bool do_fixpoint, const ScopeFlags& scope_flags) {
  Outcome outcome;
  fast::ParseError error;
  Stopwatch stopwatch;
  std::unique_ptr<fast::Tree> tree =
      fast::Parser::ParseScript(source, parse_options, &error);
  outcome.parse_ms = static_cast<double>(stopwatch.ElapsedUs()) / 1000.0;
  if (!tree) {
    outcome.message = error.ToString();
    return outcome;
  }
  outcome.parsed = true;
  outcome.nodes = fast::CountNodes(tree->root());

  if (scope_flags.dump || scope_flags.rename) {
    stopwatch.Restart();
    std::unique_ptr<fast::ScopeAnalysis> analysis =
        fast::ScopeAnalysis::Analyze(tree.get());
    double scope_ms = static_cast<double>(stopwatch.ElapsedUs()) / 1000.0;
    if (scope_flags.dump) {
      std::string dump = analysis->Dump();
      fwrite(dump.data(), 1, dump.size(), stdout);
      printf("%zu scopes, %zu bindings, %zu unresolved, %.2f ms\n",
             analysis->scopes().size(), analysis->bindings().size(),
             analysis->unresolved().size(), scope_ms);
    }
    if (scope_flags.rename) {
      size_t renamed = fast::RenameAllBindings(tree.get(), analysis.get(), "v",
                                               scope_flags.rename_scope);
      REDLINE_VLOG(kLog, "%s: renamed %zu bindings", name.c_str(), renamed);
    }
  }

  if (!do_print && !do_fixpoint) {
    return outcome;
  }

  stopwatch.Restart();
  std::string printed = fast::Print(tree->root(), print_options);
  outcome.print_ms = static_cast<double>(stopwatch.ElapsedUs()) / 1000.0;
  if (do_print) {
    fwrite(printed.data(), 1, printed.size(), stdout);
    if (printed.empty() || printed.back() != '\n') {
      fputc('\n', stdout);
    }
  }

  if (do_fixpoint) {
    // Node id comments are not part of the language; reparse without them.
    fast::PrintOptions plain = print_options;
    plain.node_ids = false;
    std::string first = plain.node_ids == print_options.node_ids
                            ? printed
                            : fast::Print(tree->root(), plain);
    fast::ParseError reparse_error;
    std::unique_ptr<fast::Tree> second =
        fast::Parser::ParseScript(first, parse_options, &reparse_error);
    if (!second) {
      outcome.fixpoint = false;
      outcome.message =
          "printed program does not reparse: " + reparse_error.ToString();
      return outcome;
    }
    std::string again = fast::Print(second->root(), plain);
    if (again != first) {
      outcome.fixpoint = false;
      // Find the first differing line for the report.
      size_t pos = 0;
      size_t limit = std::min(first.size(), again.size());
      while (pos < limit && first[pos] == again[pos]) {
        pos++;
      }
      int line, column;
      fast::ComputeLineColumn(first, static_cast<int32_t>(pos), &line, &column);
      size_t line_start = first.rfind('\n', pos == 0 ? 0 : pos - 1);
      line_start = line_start == std::string::npos ? 0 : line_start + 1;
      size_t line_end = first.find('\n', pos);
      size_t line_end2 = again.find('\n', pos);
      outcome.message = StringPrintf(
          "print is not a fixpoint at %d:%d\n  first:  %s\n  second: %s", line,
          column, first.substr(line_start, line_end - line_start).c_str(),
          again.substr(line_start, line_end2 - line_start).c_str());
    }
    if (fast::CountNodes(second->root()) != outcome.nodes && outcome.fixpoint) {
      // Not necessarily wrong (e.g. parentheses are not nodes) but worth
      // knowing about while the printer is young.
      REDLINE_VLOG(kLog, "%s: node count changed on reparse: %zu -> %zu",
                   name.c_str(), outcome.nodes,
                   fast::CountNodes(second->root()));
    }
  }
  return outcome;
}

int RunParse(const Flags& flags) {
  fast::ParseOptions parse_options;
  parse_options.allow_natives_syntax = !flags.GetBool("no-natives", false);
  parse_options.strict = flags.GetBool("strict", false);
  fast::PrintOptions print_options;
  print_options.pretty = !flags.GetBool("compact", false);
  print_options.node_ids = flags.GetBool("ids", false);
  bool do_print = flags.GetBool("print", false) || print_options.node_ids;
  bool do_fixpoint = flags.GetBool("fixpoint", true);
  bool stats = flags.GetBool("stats", false);
  bool quiet = flags.GetBool("quiet", false);
  ScopeFlags scope_flags;
  scope_flags.dump = flags.GetBool("scopes", false);
  if (flags.Has("rename")) {
    scope_flags.rename = true;
    std::string which = flags.GetString("rename", "");
    scope_flags.rename_scope = which == "all" ? fast::RenameScope::kAll
                                              : fast::RenameScope::kVariables;
    do_print = do_print || !scope_flags.dump;
  }
  std::string inline_source = flags.GetString("source");

  std::vector<std::pair<std::string, std::string>> inputs;
  if (flags.Has("source")) {
    inputs.emplace_back("<source>", inline_source);
  }
  for (const std::string& path : flags.positional()) {
    std::string contents, error;
    if (!ReadFileToString(path, &contents, &error)) {
      REDLINE_ERROR(kLog, "%s", error.c_str());
      return kExitFailure;
    }
    inputs.emplace_back(path, std::move(contents));
  }
  if (inputs.empty()) {
    fputs(kUsage, stderr);
    return kExitUsage;
  }

  int failures = 0;
  uint64_t total_nodes = 0;
  double total_parse_ms = 0;
  for (const auto& [name, source] : inputs) {
    Outcome outcome = ProcessOne(name, source, parse_options, print_options,
                                 do_print, do_fixpoint, scope_flags);
    total_nodes += outcome.nodes;
    total_parse_ms += outcome.parse_ms;
    if (!outcome.parsed) {
      failures++;
      printf("FAIL %s: %s\n", name.c_str(), outcome.message.c_str());
      continue;
    }
    if (!outcome.fixpoint) {
      failures++;
      printf("FAIL %s: %s\n", name.c_str(), outcome.message.c_str());
      continue;
    }
    if (!quiet && !do_print) {
      printf("OK   %s: %zu nodes\n", name.c_str(), outcome.nodes);
    }
    if (stats) {
      printf("     %s: parse %.2f ms, print %.2f ms, %.1f MB/s\n", name.c_str(),
             outcome.parse_ms, outcome.print_ms,
             outcome.parse_ms > 0 ? static_cast<double>(source.size()) / 1e6 /
                                        (outcome.parse_ms / 1000.0)
                                  : 0.0);
    }
  }
  if (inputs.size() > 1 || stats) {
    printf("%zu inputs, %d failed, %llu nodes, parse %.1f ms total\n",
           inputs.size(), failures,
           static_cast<unsigned long long>(total_nodes), total_parse_ms);
  }
  return failures == 0 ? kExitOk : kExitFailure;
}

}  // namespace

const Command kParseCommand = {
    "parse", "Parse JavaScript into FAST and check print round-trip", kUsage,
    RunParse};

}  // namespace redline
