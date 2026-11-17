// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// "redline shell": an interactive workbench for one program. Load a file,
// look at it in every representation FAST offers (JavaScript, node ids,
// AST tree, scopes), apply a named mutator / generator / reducer with a
// chosen seed, run the result in d8 and see its outcome and coverage,
// undo, save. Everything the fuzzer does to a program, one step at a time.
//
// Reads commands from stdin (interactive with a prompt on a terminal, or
// scripted through a pipe / here-document).

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/flags.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/base/v8_runtime.h"
#include "experimental/redline/commands/command.h"
#include "experimental/redline/core/builder.h"
#include "experimental/redline/core/component.h"
#include "experimental/redline/core/environment.h"
#include "experimental/redline/core/program.h"
#include "experimental/redline/core/random.h"
#include "experimental/redline/corpus/corpus.h"
#include "experimental/redline/engine/prelude.h"
#include "experimental/redline/exec/runner.h"
#include "experimental/redline/exec/target_options.h"
#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/fast/scope.h"
#include "experimental/redline/fast/tree.h"
#include "experimental/redline/generators/generator.h"
#include "experimental/redline/minimizer/minimizer.h"
#include "experimental/redline/mutators/mutator.h"

namespace redline {

namespace {

constexpr const char* kLog = "shell";

const char kUsage[] =
    "usage: redline shell [--d8=PATH] [FILE.js]\n"
    "\n"
    "Interactive workbench for one program: inspect it in every FAST\n"
    "representation, apply mutators / generators / reducers one at a time,\n"
    "run the result in d8, undo, save. Commands are read from stdin, so the\n"
    "shell can also be scripted (`redline shell f.js <<< 'mutate Splice'`).\n"
    "Type `help` inside the shell for the command list.\n"
    "\n"
    "flags:\n"
    "  --d8=PATH            enables run/reduce (or $REDLINE_D8)\n"
    "  --seed=S             first seed used by mutate/generate/random (1)\n"
    "  --no-auto-print      do not print the program after every change\n"
    "  --prelude=FILE|none  JS prepended at execution time (default: the\n"
    "                       mjsunit shim)\n"
    "  --v8-flags=\"...\"     flags for the in-process V8 syntax oracle\n"
    "\n";

const char kHelp[] =
    "program\n"
    "  load FILE            parse FILE as the current program (history reset)\n"
    "  source CODE          parse CODE (rest of the line) as the program\n"
    "  new                  start from an empty program\n"
    "  save FILE            write the current program\n"
    "  partner FILE|self    donor program for Splice/Combine (default self)\n"
    "views\n"
    "  print | p            the program as JavaScript (canonical printer)\n"
    "  ids                  ... with `// nN` node id comments per statement\n"
    "  compact              ... on one line\n"
    "  tree | t [ID]        the AST (kind #id and a source snippet), or the\n"
    "                       subtree of node ID\n"
    "  scopes               scope analysis: scopes, bindings, references\n"
    "  info                 node count, size, syntax verdict, lineage\n"
    "  diff                 what the last step changed (line diff)\n"
    "  check | c            ask the in-process V8 parser about validity\n"
    "components\n"
    "  list [mutators|generators|reducers|strategies]\n"
    "  mutate | m NAME [SEED]      apply mutator NAME (seed: next in "
    "sequence)\n"
    "  generate | g NAME [SEED]    insert code of generator NAME at a random\n"
    "                              statement position\n"
    "  random | r [N]              N weighted-random mutations, like a worker\n"
    "  reduce NAMES|default|all [ORACLE]\n"
    "                              run reducer(s): one name, a comma list,\n"
    "                              `default` (the fuzzer's order) or `all`;\n"
    "                              ORACLE: edges (default,\n"
    "                              edges of the current program minus those\n"
    "                              of the previous one, if any), all-edges,\n"
    "                              output, fail, crash (needs --d8)\n"
    "  seed [N]                    show/set the next seed\n"
    "  rounds [N]                  sub-mutations per mutate (1)\n"
    "execution (needs --d8)\n"
    "  run | x [verbose]    execute in d8: outcome, time, edges, new edges\n"
    "                       since the previous run; verbose: stdout/stderr\n"
    "  accept [N]           the Accept stage: N runs (10), deterministic and\n"
    "                       flaky edges, and whether the step from the\n"
    "                       previous program would be accepted\n"
    "history\n"
    "  undo | u             back to the previous program\n"
    "  reset                back to the loaded program\n"
    "  history | h          the steps so far\n"
    "misc\n"
    "  stats                component statistics tables\n"
    "  auto on|off          print the program after every change (on)\n"
    "  help, quit\n";

std::vector<std::string> Split(const std::string& line) {
  std::vector<std::string> words;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && isspace(static_cast<unsigned char>(line[i]))) {
      i++;
    }
    size_t start = i;
    while (i < line.size() && !isspace(static_cast<unsigned char>(line[i]))) {
      i++;
    }
    if (i > start) {
      words.push_back(line.substr(start, i - start));
    }
  }
  return words;
}

std::vector<std::string> SplitComma(const std::string& text) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (start <= text.size()) {
    size_t comma = text.find(',', start);
    if (comma == std::string::npos) {
      comma = text.size();
    }
    if (comma > start) {
      parts.push_back(text.substr(start, comma - start));
    }
    start = comma + 1;
  }
  return parts;
}

std::vector<std::string> Lines(const std::string& text) {
  std::vector<std::string> lines;
  size_t start = 0;
  while (start < text.size()) {
    size_t nl = text.find('\n', start);
    if (nl == std::string::npos) {
      nl = text.size();
    }
    lines.push_back(text.substr(start, nl - start));
    start = nl + 1;
  }
  return lines;
}

// Minimal LCS line diff, enough to show what one step changed.
void PrintLineDiff(const std::string& before, const std::string& after) {
  std::vector<std::string> a = Lines(before), b = Lines(after);
  const size_t kLimit = 1500;
  if (a.size() > kLimit || b.size() > kLimit) {
    printf("(programs too large for the diff view: %zu -> %zu lines)\n",
           a.size(), b.size());
    return;
  }
  size_t n = a.size(), m = b.size();
  std::vector<std::vector<uint16_t>> lcs(n + 1,
                                         std::vector<uint16_t>(m + 1, 0));
  for (size_t i = n; i-- > 0;) {
    for (size_t j = m; j-- > 0;) {
      lcs[i][j] = a[i] == b[j] ? static_cast<uint16_t>(lcs[i + 1][j + 1] + 1)
                               : std::max(lcs[i + 1][j], lcs[i][j + 1]);
    }
  }
  size_t i = 0, j = 0;
  int changes = 0;
  while (i < n || j < m) {
    if (i < n && j < m && a[i] == b[j]) {
      i++;
      j++;
    } else if (i < n && (j == m || lcs[i + 1][j] >= lcs[i][j + 1])) {
      printf("- %s\n", a[i++].c_str());
      changes++;
    } else {
      printf("+ %s\n", b[j++].c_str());
      changes++;
    }
  }
  if (changes == 0) {
    printf("(no textual change)\n");
  }
}

std::string Snippet(const fast::Node* node, size_t limit = 60) {
  fast::PrintOptions options;
  options.pretty = false;
  std::string text = fast::Print(node, options);
  for (char& c : text) {
    if (c == '\n') {
      c = ' ';
    }
  }
  if (text.size() > limit) {
    text = text.substr(0, limit - 3) + "...";
  }
  return text;
}

void DumpTree(fast::Node* node, int depth, int* count) {
  if (node == nullptr) {
    return;
  }
  (*count)++;
  std::string detail;
  switch (node->kind) {
    case fast::NodeKind::kIdentifier:
      detail = std::string(node->Cast<fast::Identifier>()->name);
      break;
    case fast::NodeKind::kNumericLiteral:
    case fast::NodeKind::kStringLiteral:
    case fast::NodeKind::kBooleanLiteral:
    case fast::NodeKind::kNullLiteral:
    case fast::NodeKind::kBigIntLiteral:
    case fast::NodeKind::kRegExpLiteral:
      detail = Snippet(node, 40);
      break;
    default:
      if (depth > 0 && (node->IsStatement() || node->IsExpression())) {
        detail = Snippet(node);
      }
      break;
  }
  printf("%*s%s #%u%s%s\n", depth * 2, "", node->kind_name(), node->id,
         detail.empty() ? "" : "  ", detail.c_str());
  fast::ForEachChild(
      node, [&](fast::Node* child) { DumpTree(child, depth + 1, count); });
}

class Shell {
 public:
  Shell(const Flags& flags, std::unique_ptr<ScriptRunner> runner,
        std::string prelude)
      : mutators_(MutatorSet::CreateDefault()),
        generators_(GeneratorSet::CreateDefault()),
        env_(Environment::Default()),
        runner_(std::move(runner)),
        prelude_(std::move(prelude)),
        seed_(static_cast<uint64_t>(flags.GetInt("seed", 1))),
        auto_print_(!flags.GetBool("no-auto-print", false) &&
                    flags.GetBool("auto-print", true)) {
    interactive_ = isatty(STDIN_FILENO) != 0;
  }

  bool Load(const std::string& path) {
    std::string source, error;
    if (!ReadFileToString(path, &source, &error)) {
      Error(error);
      return false;
    }
    return SetSource(source, "load", path);
  }

  bool SetSource(const std::string& source, const std::string& origin,
                 const std::string& detail) {
    std::string error;
    Lineage lineage;
    lineage.origin = origin;
    lineage.detail = detail;
    ProgramPtr program = Program::Parse(source, lineage, &error);
    if (program == nullptr) {
      Error(error);
      return false;
    }
    history_.clear();
    steps_.clear();
    current_ = program;
    if (partner_ == nullptr) {
      partner_ = program;
    }
    last_edges_.clear();
    printf("loaded %s: %zu nodes, %zu bytes\n", detail.c_str(),
           program->node_count(), program->source().size());
    return true;
  }

  int Loop() {
    std::string line;
    char buffer[4096];
    while (true) {
      if (interactive_) {
        fputs("redline> ", stdout);
        fflush(stdout);
      }
      if (fgets(buffer, sizeof buffer, stdin) == nullptr) {
        break;
      }
      line = buffer;
      while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
      }
      if (!interactive_) {
        printf("redline> %s\n", line.c_str());
      }
      if (!Execute(line)) {
        break;
      }
      fflush(stdout);
    }
    return kExitOk;
  }

 private:
  void Error(const std::string& message) {
    printf("error: %s\n", message.c_str());
  }

  bool Require() {
    if (current_ != nullptr) {
      return true;
    }
    Error("no program loaded (load FILE, source CODE or new)");
    return false;
  }

  bool RequireTarget() {
    if (runner_ != nullptr) {
      return true;
    }
    Error("no target: start the shell with --d8=PATH (or $REDLINE_D8)");
    return false;
  }

  void Push(ProgramPtr next, const std::string& step) {
    history_.push_back(current_);
    steps_.push_back(step);
    current_ = std::move(next);
  }

  void ShowProgram() {
    fast::PrintOptions options;
    std::string text = fast::Print(current_->tree().root(), options);
    fwrite(text.data(), 1, text.size(), stdout);
    if (text.empty() || text.back() != '\n') {
      fputc('\n', stdout);
    }
  }

  void AfterChange(const std::string& step) {
    std::string message;
    bool accepted = checker_.Accepts(current_->source(), &message);
    printf("=> %s: %zu nodes, V8 %s%s%s\n", step.c_str(),
           current_->node_count(), accepted ? "accepts" : "REJECTS",
           accepted ? "" : ": ", accepted ? "" : message.c_str());
    if (auto_print_) {
      if (!history_.empty()) {
        PrintLineDiff(history_.back()->source(), current_->source());
      } else {
        ShowProgram();
      }
    }
  }

  // --- commands ---------------------------------------------------------

  bool Execute(const std::string& line) {
    std::vector<std::string> words = Split(line);
    if (words.empty()) {
      return true;
    }
    const std::string& cmd = words[0];
    if (cmd[0] == '#') {
      return true;
    }
    if (cmd == "quit" || cmd == "exit" || cmd == "q") {
      return false;
    }
    if (cmd == "help" || cmd == "?") {
      fputs(kHelp, stdout);
    } else if (cmd == "load") {
      if (words.size() < 2) {
        return Usage("load FILE");
      }
      Load(words[1]);
      if (current_ != nullptr && auto_print_) {
        ShowProgram();
      }
    } else if (cmd == "source") {
      size_t pos = line.find("source");
      std::string code = line.substr(pos + 6);
      SetSource(code, "source", "<source>");
      if (current_ != nullptr && auto_print_) {
        ShowProgram();
      }
    } else if (cmd == "new") {
      SetSource("", "new", "<empty>");
    } else if (cmd == "save") {
      if (words.size() < 2) {
        return Usage("save FILE");
      }
      if (!Require()) {
        return true;
      }
      std::string error;
      if (!WriteStringToFile(words[1], current_->source(), &error)) {
        Error(error);
      } else {
        printf("saved %s (%zu bytes)\n", words[1].c_str(),
               current_->source().size());
      }
    } else if (cmd == "partner") {
      if (words.size() < 2) {
        return Usage("partner FILE|self");
      }
      if (words[1] == "self") {
        partner_ = current_;
        printf("partner: the program itself\n");
      } else {
        std::string source, error;
        if (!ReadFileToString(words[1], &source, &error)) {
          Error(error);
        } else {
          Lineage lineage;
          lineage.origin = "import";
          lineage.detail = words[1];
          ProgramPtr p = Program::Parse(source, lineage, &error);
          if (p == nullptr) {
            Error(error);
          } else {
            partner_ = p;
            printf("partner: %s (%zu nodes)\n", words[1].c_str(),
                   p->node_count());
          }
        }
      }
    } else if (cmd == "print" || cmd == "p") {
      if (Require()) {
        ShowProgram();
      }
    } else if (cmd == "ids") {
      if (Require()) {
        fast::PrintOptions options;
        options.node_ids = true;
        printf("%s\n", fast::Print(current_->tree().root(), options).c_str());
      }
    } else if (cmd == "compact") {
      if (Require()) {
        fast::PrintOptions options;
        options.pretty = false;
        printf("%s\n", fast::Print(current_->tree().root(), options).c_str());
      }
    } else if (cmd == "tree" || cmd == "t") {
      if (Require()) {
        Tree(words);
      }
    } else if (cmd == "scopes") {
      if (Require()) {
        std::unique_ptr<fast::Tree> copy = current_->CopyTree();
        std::unique_ptr<fast::ScopeAnalysis> analysis =
            fast::ScopeAnalysis::Analyze(copy.get());
        fputs(analysis->Dump().c_str(), stdout);
      }
    } else if (cmd == "info") {
      if (Require()) {
        Info();
      }
    } else if (cmd == "diff") {
      if (Require()) {
        if (history_.empty()) {
          printf("(no previous step)\n");
        } else {
          PrintLineDiff(history_.back()->source(), current_->source());
        }
      }
    } else if (cmd == "check" || cmd == "c") {
      if (Require()) {
        std::string message;
        Stopwatch sw;
        bool ok = checker_.Accepts(current_->source(), &message);
        printf("V8 %s (%.2f ms)%s%s\n",
               ok ? "accepts the program" : "REJECTS the program",
               static_cast<double>(sw.ElapsedUs()) / 1000.0, ok ? "" : ": ",
               message.c_str());
      }
    } else if (cmd == "list" || cmd == "ls") {
      List(words.size() > 1 ? words[1] : "");
    } else if (cmd == "mutate" || cmd == "m") {
      if (words.size() < 2) {
        return Usage("mutate NAME [SEED]");
      }
      if (Require()) {
        Mutate(words[1], words.size() > 2 ? ParseSeed(words[2]) : NextSeed());
      }
    } else if (cmd == "generate" || cmd == "g") {
      if (words.size() < 2) {
        return Usage("generate NAME [SEED]");
      }
      if (Require()) {
        Generate(words[1], words.size() > 2 ? ParseSeed(words[2]) : NextSeed());
      }
    } else if (cmd == "random" || cmd == "r") {
      int n = words.size() > 1
                  ? static_cast<int>(strtol(words[1].c_str(), nullptr, 10))
                  : 1;
      if (Require()) {
        for (int i = 0; i < std::max(n, 1); i++) {
          RandomMutation(NextSeed());
        }
      }
    } else if (cmd == "reduce") {
      if (words.size() < 2) {
        return Usage(
            "reduce NAME[,NAME..]|default|all "
            "[edges|all-edges|output|fail|crash]");
      }
      if (Require() && RequireTarget()) {
        Reduce(words[1], words.size() > 2 ? words[2] : "edges");
      }
    } else if (cmd == "accept") {
      if (Require() && RequireTarget()) {
        int runs = words.size() > 1
                       ? static_cast<int>(strtol(words[1].c_str(), nullptr, 10))
                       : 10;
        Accept(std::max(2, runs));
      }
    } else if (cmd == "seed") {
      if (words.size() > 1) {
        seed_ = ParseSeed(words[1]);
      }
      printf("next seed: %llu\n", static_cast<unsigned long long>(seed_));
    } else if (cmd == "rounds") {
      if (words.size() > 1) {
        rounds_ = std::max(
            1, static_cast<int>(strtol(words[1].c_str(), nullptr, 10)));
      }
      printf("rounds: %d\n", rounds_);
    } else if (cmd == "run" || cmd == "x") {
      if (Require() && RequireTarget()) {
        Run(words.size() > 1 && words[1] == "verbose");
      }
    } else if (cmd == "undo" || cmd == "u") {
      if (history_.empty()) {
        printf("(nothing to undo)\n");
      } else {
        current_ = history_.back();
        history_.pop_back();
        printf("undid %s; %zu nodes\n", steps_.back().c_str(),
               current_->node_count());
        steps_.pop_back();
        if (auto_print_) {
          ShowProgram();
        }
      }
    } else if (cmd == "reset") {
      if (!history_.empty()) {
        current_ = history_.front();
        history_.clear();
        steps_.clear();
      }
      if (current_ != nullptr) {
        printf("back to the loaded program (%zu nodes)\n",
               current_->node_count());
      }
    } else if (cmd == "history" || cmd == "h") {
      for (size_t i = 0; i < steps_.size(); i++) {
        printf("  %zu. %s  (%zu nodes)\n", i + 1, steps_[i].c_str(),
               i + 1 < history_.size() ? history_[i + 1]->node_count()
                                       : current_->node_count());
      }
      if (steps_.empty()) {
        printf("  (no steps yet)\n");
      }
    } else if (cmd == "stats") {
      fputs(FormatStatsTable("mutators", mutators_->Components()).c_str(),
            stdout);
      fputs(FormatStatsTable("reducers", minimizer_.Components()).c_str(),
            stdout);
      fputs(FormatStatsTable("generators", generators_->Components()).c_str(),
            stdout);
    } else if (cmd == "auto") {
      if (words.size() > 1) {
        auto_print_ = words[1] != "off";
      }
      printf("auto print: %s\n", auto_print_ ? "on" : "off");
    } else {
      Error("unknown command '" + cmd + "' (try help)");
    }
    return true;
  }

  bool Usage(const char* usage) {
    printf("usage: %s\n", usage);
    return true;
  }

  uint64_t NextSeed() { return seed_++; }
  static uint64_t ParseSeed(const std::string& word) {
    return strtoull(word.c_str(), nullptr, 10);
  }

  void Tree(const std::vector<std::string>& words) {
    if (words.size() > 1) {
      uint32_t id =
          static_cast<uint32_t>(strtoul(words[1].c_str(), nullptr, 10));
      fast::Node* node = current_->tree().FindNode(id);
      if (node == nullptr) {
        Error("no node #" + words[1] + " (see `ids`)");
        return;
      }
      int count = 0;
      DumpTree(node, 0, &count);
      printf("(%d nodes in the subtree)\n", count);
      return;
    }
    int count = 0;
    DumpTree(current_->tree().root(), 0, &count);
    printf("(%d nodes)\n", count);
  }

  void Info() {
    std::string message;
    bool ok = checker_.Accepts(current_->source(), &message);
    const Lineage& l = current_->lineage();
    printf("nodes: %zu   bytes: %zu   V8: %s%s%s\n", current_->node_count(),
           current_->source().size(), ok ? "accepts" : "REJECTS",
           ok ? "" : " - ", ok ? "" : message.c_str());
    printf("origin: %s%s%s   generation: %d   steps: %zu\n", l.origin.c_str(),
           l.detail.empty() ? "" : " ", l.detail.c_str(), l.generation,
           steps_.size());
    if (!last_edges_.empty()) {
      printf("last run: %zu edges\n", last_edges_.size());
    }
    std::unique_ptr<fast::Tree> copy = current_->CopyTree();
    std::unique_ptr<fast::ScopeAnalysis> analysis =
        fast::ScopeAnalysis::Analyze(copy.get());
    printf("scopes: %zu   bindings: %zu   unresolved: %zu\n",
           analysis->scopes().size(), analysis->bindings().size(),
           analysis->unresolved().size());
  }

  void List(const std::string& what) {
    if (what.empty() || what == "mutators") {
      printf("mutators:\n");
      for (const auto& m : mutators_->all()) {
        printf("  %-14s w=%-4g %s%s\n", m->name().c_str(), m->weight(),
               std::string(m->description()).c_str(),
               m->needs_partner() ? " [uses partner]" : "");
      }
    }
    if (what.empty() || what == "generators") {
      printf("generators:\n");
      for (const auto& g : generators_->all()) {
        printf("  %-18s w=%-4g %s\n", g->name().c_str(), g->weight(),
               std::string(g->description()).c_str());
      }
    }
    if (what.empty() || what == "reducers") {
      printf("reducers:\n");
      for (const auto& r : minimizer_.reducers()) {
        printf("  %-14s %s\n", r->name().c_str(),
               std::string(r->description()).c_str());
      }
    }
    if (what.empty() || what == "schedulers" || what == "strategies") {
      printf(
          "selection strategies (fuzz --config, \"selection\" block; fuzz "
          "--scheduler=NAME for seeds):\n");
      fputs(SelectorRegistry<const Program>::Instance()
                .Help("seeds / partners")
                .c_str(),
            stdout);
      fputs(SelectorRegistry<Mutator>::Instance().Help("mutators").c_str(),
            stdout);
      fputs(SelectorRegistry<Generator>::Instance().Help("generators").c_str(),
            stdout);
    }
  }

  void Mutate(const std::string& name, uint64_t seed) {
    Mutator* mutator = mutators_->Find(name);
    if (mutator == nullptr) {
      Error("unknown mutator '" + name + "' (list mutators)");
      return;
    }
    Random rng(seed);
    std::string detail;
    Stopwatch sw;
    std::unique_ptr<fast::Tree> tree =
        ApplyMutation(*mutator, *current_, partner_, rng, env_, *generators_,
                      rounds_, &detail);
    uint64_t us = sw.ElapsedUs();
    if (tree == nullptr) {
      printf("%s (seed %llu): nothing to do on this program\n", name.c_str(),
             static_cast<unsigned long long>(seed));
      return;
    }
    Lineage lineage = current_->lineage();
    lineage.origin = name;
    lineage.detail = detail;
    lineage.generation++;
    Push(Program::FromTree(std::move(tree), lineage),
         StringPrintf("%s seed=%llu: %s", name.c_str(),
                      static_cast<unsigned long long>(seed), detail.c_str()));
    AfterChange(StringPrintf("%s (seed %llu, %.2f ms): %s", name.c_str(),
                             static_cast<unsigned long long>(seed),
                             static_cast<double>(us) / 1000.0, detail.c_str()));
  }

  void Generate(const std::string& name, uint64_t seed) {
    Generator* generator = generators_->Find(name);
    if (generator == nullptr) {
      Error("unknown generator '" + name + "' (list generators)");
      return;
    }
    Random rng(seed);
    std::unique_ptr<fast::Tree> tree = current_->CopyTree();
    Builder builder(tree.get(), &rng);
    std::vector<fast::Statement*> out;
    Builder::Slot slot;
    bool generated = false;
    for (int attempt = 0; attempt < 10 && !generated; attempt++) {
      slot = builder.RandomInsertionSlot();
      if (!slot.valid()) {
        break;
      }
      GenContext ctx = MakeGenContext(builder, slot, *generators_, env_);
      if (!generator->Applicable(ctx)) {
        continue;
      }
      ctx.out = &out;
      generator->stats().invocations++;
      if (generator->Generate(ctx, &out) && !out.empty()) {
        generator->stats().produced++;
        generated = true;
      } else {
        out.clear();
      }
    }
    if (!generated) {
      printf("%s (seed %llu): not applicable here\n", name.c_str(),
             static_cast<unsigned long long>(seed));
      return;
    }
    builder.InsertAll(slot, out);
    Lineage lineage = current_->lineage();
    lineage.origin = "generate";
    lineage.detail = name;
    lineage.generation++;
    Push(Program::FromTree(std::move(tree), lineage),
         StringPrintf("generate %s seed=%llu", name.c_str(),
                      static_cast<unsigned long long>(seed)));
    AfterChange(
        StringPrintf("%s (seed %llu): inserted %zu statement(s)", name.c_str(),
                     static_cast<unsigned long long>(seed), out.size()));
  }

  void RandomMutation(uint64_t seed) {
    Random rng(seed);
    Mutator* mutator = mutators_->Pick(rng, partner_ != nullptr);
    if (mutator == nullptr) {
      return;
    }
    Mutate(mutator->name(), seed);
  }

  bool Exec(const std::string& source, Execution* execution,
            uint64_t timeout_us = 0) {
    std::string script = prelude_.empty() ? source : prelude_ + "\n" + source;
    std::string error;
    if (timeout_us == 0) {
      timeout_us = runner_->options().timeout_us;
    }
    if (!runner_->RunWithoutEvaluation(script, timeout_us, execution, &error)) {
      Error("execution failed: " + error);
      return false;
    }
    return true;
  }

  static const char* OutcomeName(Outcome outcome) {
    switch (outcome) {
      case Outcome::kSucceeded:
        return "succeeded";
      case Outcome::kFailed:
        return "failed (uncaught exception)";
      case Outcome::kCrashed:
        return "CRASHED";
      case Outcome::kTimedOut:
        return "timed out";
    }
    return "?";
  }

  void Run(bool verbose) {
    Execution execution;
    if (!Exec(current_->source(), &execution)) {
      return;
    }
    std::vector<uint32_t> edges = runner_->coverage().CurrentEdges();
    std::vector<uint32_t> gained, lost;
    std::set_difference(edges.begin(), edges.end(), last_edges_.begin(),
                        last_edges_.end(), std::back_inserter(gained));
    std::set_difference(last_edges_.begin(), last_edges_.end(), edges.begin(),
                        edges.end(), std::back_inserter(lost));
    printf(
        "%s in %.2f ms, exit %d%s; %zu edges", OutcomeName(execution.outcome),
        static_cast<double>(execution.exec_time_us) / 1000.0,
        execution.exit_code,
        execution.signal ? StringPrintf(", signal %d", execution.signal).c_str()
                         : "",
        edges.size());
    if (!last_edges_.empty()) {
      printf(" (+%zu new, -%zu gone since the previous run)", gained.size(),
             lost.size());
    }
    printf("\n");
    if (verbose || execution.outcome != Outcome::kSucceeded) {
      std::vector<std::string> out = Lines(execution.stdout_output);
      std::vector<std::string> err = Lines(execution.stderr_output);
      for (size_t i = 0; i < out.size() && i < 20; i++) {
        printf("  stdout: %s\n", out[i].c_str());
      }
      for (size_t i = 0; i < err.size() && i < 20; i++) {
        printf("  stderr: %s\n", err[i].c_str());
      }
      if (!execution.fuzzout.empty()) {
        printf("  fuzzout: %s\n", Lines(execution.fuzzout)[0].c_str());
      }
    }
    last_edges_ = std::move(edges);
  }

  // The Accept stage alone: runs the program `runs` times and splits its
  // edges into deterministic ones (set by every run) and flaky ones (set by
  // some runs only). Relative to the previous program of the history, the
  // deterministic edges it gained are what the fuzzer's default criterion
  // (`new-edges`) would accept and keep through minimization.
  void Accept(int runs) {
    std::vector<uint32_t> all, every;
    for (int i = 0; i < runs; i++) {
      Execution execution;
      if (!Exec(current_->source(), &execution)) {
        return;
      }
      if (!execution.succeeded()) {
        printf("run %d: %s; only successful runs count as coverage\n", i + 1,
               OutcomeName(execution.outcome));
        return;
      }
      std::vector<uint32_t> edges = runner_->coverage().CurrentEdges();
      std::vector<uint32_t> merged, common;
      std::set_union(all.begin(), all.end(), edges.begin(), edges.end(),
                     std::back_inserter(merged));
      all.swap(merged);
      if (i == 0) {
        every = edges;
      } else {
        std::set_intersection(every.begin(), every.end(), edges.begin(),
                              edges.end(), std::back_inserter(common));
        every.swap(common);
      }
    }
    printf("%d runs: %zu deterministic edges, %zu flaky\n", runs, every.size(),
           all.size() - every.size());
    if (history_.empty()) {
      printf(
          "(no previous program: load or mutate one to see what the step "
          "gained)\n");
      return;
    }
    std::vector<uint8_t> seen(runner_->coverage().num_edges() + 1, 0);
    for (int i = 0; i < runs; i++) {
      Execution run;
      if (!Exec(history_.back()->source(), &run)) {
        return;
      }
      for (uint32_t e : runner_->coverage().CurrentEdges()) {
        if (e < seen.size()) {
          seen[e] = 1;
        }
      }
    }
    size_t gained = 0;
    for (uint32_t e : every) {
      gained += (e >= seen.size() || !seen[e]) ? 1 : 0;
    }
    printf(
        "vs the previous program: %zu deterministic edges gained -> %s by "
        "`new-edges`\n",
        gained, gained > 0 ? "accepted" : "rejected");
  }

  void Reduce(const std::string& which, const std::string& oracle_name) {
    std::vector<Reducer*> reducers;
    if (which == "all") {
      for (const auto& r : minimizer_.reducers()) {
        reducers.push_back(r.get());
      }
    } else if (which == "default") {
      for (std::string_view name : Minimizer::DefaultOrder()) {
        reducers.push_back(minimizer_.Find(name));
      }
    } else {
      for (const std::string& name : SplitComma(which)) {
        Reducer* r = minimizer_.Find(name);
        if (r == nullptr) {
          Error("unknown reducer '" + name + "' (list reducers)");
          return;
        }
        reducers.push_back(r);
      }
    }
    // Establish the property.
    Execution first;
    if (!Exec(current_->source(), &first)) {
      return;
    }
    uint64_t probe_timeout_us =
        std::min(std::max<uint64_t>(5 * first.exec_time_us + 20'000, 50'000),
                 runner_->options().timeout_us);
    std::vector<uint32_t> edges;
    MinimizationOracle oracle;
    if (oracle_name == "edges" || oracle_name == "all-edges") {
      if (!first.succeeded()) {
        Error(
            "the program does not run successfully; use the output/fail/crash "
            "oracle");
        return;
      }
      edges = runner_->coverage().CurrentEdges();
      // Settle over two re-runs.
      for (int i = 0; i < 2; i++) {
        Execution again;
        if (!Exec(current_->source(), &again) || !again.succeeded()) {
          return;
        }
        std::vector<uint32_t> missing = runner_->coverage().MissingEdges(edges);
        std::vector<uint32_t> kept;
        std::set_difference(edges.begin(), edges.end(), missing.begin(),
                            missing.end(), std::back_inserter(kept));
        edges.swap(kept);
      }
      size_t settled = edges.size();
      if (oracle_name == "edges" && !history_.empty()) {
        // Relative to the previous program of the history: the edges the
        // last step(s) gained, exactly what the fuzzer would preserve.
        std::vector<uint8_t> seen(runner_->coverage().num_edges() + 1, 0);
        for (int i = 0; i < 3; i++) {
          Execution run;
          if (!Exec(history_.back()->source(), &run)) {
            return;
          }
          for (uint32_t e : runner_->coverage().CurrentEdges()) {
            if (e < seen.size()) {
              seen[e] = 1;
            }
          }
        }
        std::vector<uint32_t> remaining;
        for (uint32_t e : edges) {
          if (e >= seen.size() || !seen[e]) {
            remaining.push_back(e);
          }
        }
        edges.swap(remaining);
        printf(
            "preserving %zu edges: the %zu settled edges of the program minus "
            "those of the "
            "previous program\n",
            edges.size(), settled);
        if (edges.empty()) {
          Error(
              "the last step added no edges; nothing to preserve (try "
              "all-edges or output)");
          return;
        }
      } else {
        printf("preserving all %zu settled edges of the program\n",
               edges.size());
      }
      oracle = [&](const std::string& candidate) {
        Execution e;
        return Exec(candidate, &e, probe_timeout_us) && e.succeeded() &&
               runner_->coverage().AllEdgesSet(edges);
      };
    } else if (oracle_name == "output") {
      std::string expected = first.stdout_output;
      Outcome outcome = first.outcome;
      printf("preserving the outcome (%s) and %zu bytes of stdout\n",
             OutcomeName(outcome), expected.size());
      oracle = [&, expected, outcome](const std::string& candidate) {
        Execution e;
        return Exec(candidate, &e, probe_timeout_us) && e.outcome == outcome &&
               e.stdout_output == expected;
      };
    } else if (oracle_name == "fail") {
      if (first.outcome != Outcome::kFailed) {
        Error("the program does not fail");
        return;
      }
      std::string signature = first.ExceptionSignature();
      printf("preserving the failure `%s`\n",
             signature.empty() ? "(no exception message captured)"
                               : signature.c_str());
      oracle = [&, signature](const std::string& candidate) {
        Execution e;
        return Exec(candidate, &e, probe_timeout_us) &&
               e.outcome == Outcome::kFailed &&
               e.ExceptionSignature() == signature;
      };
    } else if (oracle_name == "crash") {
      if (!first.crashed()) {
        Error("the program does not crash");
        return;
      }
      oracle = [&](const std::string& candidate) {
        Execution e;
        return Exec(candidate, &e, probe_timeout_us) && e.crashed();
      };
    } else {
      Error("unknown oracle '" + oracle_name +
            "' (edges, all-edges, output, fail, crash)");
      return;
    }
    std::unique_ptr<fast::Tree> work = current_->CopyTree();
    MinimizationResult result =
        minimizer_.MinimizeWith(reducers, work.get(), oracle, &checker_);
    if (result.accepted == 0) {
      printf("%s: nothing could be removed (%d tests, %.1f s)\n", which.c_str(),
             result.tests, static_cast<double>(result.time_us) / 1e6);
      return;
    }
    Lineage lineage = current_->lineage();
    lineage.origin = "reduce";
    lineage.detail = which;
    Push(Program::FromTree(std::move(work), lineage),
         StringPrintf("reduce %s (%s oracle)", which.c_str(),
                      oracle_name.c_str()));
    AfterChange(StringPrintf(
        "%s: %zu -> %zu nodes, %d tests (%d accepted) in %.1f s%s",
        which.c_str(), result.nodes_before, result.nodes_after, result.tests,
        result.accepted, static_cast<double>(result.time_us) / 1e6,
        result.budget_exhausted ? " (budget exhausted)" : ""));
  }

  std::unique_ptr<MutatorSet> mutators_;
  std::unique_ptr<GeneratorSet> generators_;
  const Environment& env_;
  Minimizer minimizer_;
  SyntaxChecker checker_;
  std::unique_ptr<ScriptRunner> runner_;
  std::string prelude_;
  uint64_t seed_;
  int rounds_ = 1;
  bool auto_print_;
  bool interactive_ = false;
  ProgramPtr current_;
  ProgramPtr partner_;
  std::vector<ProgramPtr> history_;
  std::vector<std::string> steps_;
  std::vector<uint32_t> last_edges_;
};

int Run(const Flags& flags) {
  if (flags.positional().size() > 1) {
    fputs(kUsage, stderr);
    return kExitUsage;
  }
  std::string error;
  if (!InitializeV8(flags.GetString(
                        "v8-flags",
                        "--allow-natives-syntax --js-staging --js-decorators"),
                    &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitFailure;
  }
  std::string prelude;
  std::string prelude_flag = flags.GetString("prelude");
  if (prelude_flag.empty()) {
    prelude = std::string(DefaultPrelude());
  } else if (prelude_flag != "none" &&
             !ReadFileToString(prelude_flag, &prelude, &error)) {
    REDLINE_ERROR(kLog, "%s", error.c_str());
    return kExitFailure;
  }
  std::unique_ptr<ScriptRunner> runner;
  if (flags.Has("d8") || getenv("REDLINE_D8") != nullptr) {
    TargetOptions target;
    if (!TargetOptions::FromFlags(flags, &target, &error)) {
      REDLINE_ERROR(kLog, "%s", error.c_str());
      return kExitUsage;
    }
    runner = ScriptRunner::Create(target, 0, &error);
    if (runner == nullptr || !runner->Initialize(&error)) {
      REDLINE_ERROR(kLog, "%s", error.c_str());
      return kExitFailure;
    }
    printf("target: %s (%u edges)\n", target.executable.c_str(),
           runner->coverage().num_edges());
  } else {
    printf("no --d8: run/reduce are disabled\n");
  }
  Shell shell(flags, std::move(runner), std::move(prelude));
  if (!flags.positional().empty() && !shell.Load(flags.positional()[0])) {
    return kExitFailure;
  }
  if (isatty(STDIN_FILENO)) {
    printf("type help for the commands\n");
  }
  return shell.Loop();
}

}  // namespace

const Command kShellCommand = {
    "shell",
    "Interactive workbench: inspect, mutate, generate, reduce, run one program",
    kUsage,
    Run,
};

}  // namespace redline
