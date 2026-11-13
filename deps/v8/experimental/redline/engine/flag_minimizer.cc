// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/flag_minimizer.h"

#include <algorithm>
#include <memory>

#include "experimental/redline/exec/process_run.h"
#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/parser.h"

namespace redline {

PlainCrashRunner::PlainCrashRunner(const TargetOptions& target,
                                   CrashSignature signature,
                                   uint64_t timeout_us)
    : executable_(target.executable),
      env_(target.PlainEnvironment()),
      signature_(std::move(signature)),
      timeout_us_(timeout_us) {}

bool PlainCrashRunner::Crashes(const std::vector<std::string>& args, int runs,
                               int needed) {
  last_runs_ = 0;
  last_hits_ = 0;
  error_.clear();
  for (int run = 0; run < runs; run++) {
    // Stop when the hits so far decide the result.
    if (last_hits_ >= needed || last_hits_ + (runs - run) < needed ||
        (stop_ && stop_())) {
      break;
    }
    const ProcessResult result =
        RunProcess(executable_, args, env_, timeout_us_);
    if (!result.started) {
      error_ = result.error;
      return false;
    }
    processes_++;
    last_runs_++;
    if (!result.timed_out &&
        CrashSignature::FromOutput(result.signal, result.exit_code,
                                   result.stderr_text) == signature_) {
      last_hits_++;
    }
  }
  return last_hits_ >= needed;
}

int PlainCrashRunner::Count(const std::vector<std::string>& args, int runs) {
  last_runs_ = 0;
  last_hits_ = 0;
  error_.clear();
  for (int run = 0; run < runs && !(stop_ && stop_()); run++) {
    const ProcessResult result =
        RunProcess(executable_, args, env_, timeout_us_);
    if (!result.started) {
      error_ = result.error;
      break;
    }
    processes_++;
    last_runs_++;
    if (!result.timed_out &&
        CrashSignature::FromOutput(result.signal, result.exit_code,
                                   result.stderr_text) == signature_) {
      last_hits_++;
    }
  }
  return last_hits_;
}

FlagMinimization MinimizeFlags(
    const std::vector<std::string>& flags,
    const std::function<bool(const std::string& flag)>& keep,
    const std::function<bool(const std::vector<std::string>& candidate)>&
        oracle,
    const std::function<bool()>& exhausted) {
  FlagMinimization result;
  result.flags = flags;
  // The positions in result.flags of the flags that can go.
  auto removable = [&] {
    std::vector<size_t> positions;
    for (size_t i = 0; i < result.flags.size(); i++) {
      if (!keep(result.flags[i])) {
        positions.push_back(i);
      }
    }
    return positions;
  };
  size_t chunk = std::max<size_t>(1, removable().size() / 2);
  while (true) {
    bool removed = false;
    std::vector<size_t> positions = removable();
    size_t end = positions.size();
    while (end > 0) {
      if (exhausted()) {
        result.budget_exhausted = true;
        return result;
      }
      const size_t begin = end > chunk ? end - chunk : 0;
      std::vector<bool> drop(result.flags.size(), false);
      for (size_t k = begin; k < end; k++) {
        drop[positions[k]] = true;
      }
      std::vector<std::string> candidate;
      for (size_t i = 0; i < result.flags.size(); i++) {
        if (!drop[i]) {
          candidate.push_back(result.flags[i]);
        }
      }
      result.tests++;
      if (oracle(candidate)) {
        result.flags = std::move(candidate);
        removed = true;
        // The flags before `begin` keep their positions.
        positions = removable();
      }
      end = begin;
    }
    if (chunk > 1) {
      chunk = std::max<size_t>(1, chunk / 2);
    } else if (!removed) {
      break;  // a pass with single flags removed nothing
    }
  }
  return result;
}

std::vector<std::string> MinimizeBundleParts(
    std::vector<std::string> parts,
    const std::function<bool(const std::vector<std::string>& candidate)>&
        crashes,
    const std::function<void(std::vector<std::string>* parts, size_t i)>&
        reduce,
    const std::function<bool()>& exhausted) {
  if (parts.empty()) {
    return parts;
  }
  // MinimizeFlags works on each list of strings. The crash part stays last.
  auto remove_parts = [&] {
    if (exhausted()) {
      return;
    }
    const std::string last = parts.back();
    parts.pop_back();
    const FlagMinimization kept = MinimizeFlags(
        parts, [](const std::string&) { return false; },
        [&](const std::vector<std::string>& candidate) {
          std::vector<std::string> all = candidate;
          all.push_back(last);
          return crashes(all);
        },
        exhausted);
    parts = kept.flags;
    parts.push_back(last);
  };
  remove_parts();
  if (!exhausted()) {
    reduce(&parts, parts.size() - 1);
  }
  remove_parts();
  for (size_t i = parts.size() - 1; i-- > 0 && !exhausted();) {
    reduce(&parts, i);
  }
  remove_parts();
  return parts;
}

bool IsFlag(std::string_view flag, std::string_view name) {
  if (!flag.starts_with("--")) {
    return false;
  }
  flag.remove_prefix(2);
  flag = flag.substr(0, flag.find('='));
  if (flag.size() != name.size()) {
    return false;
  }
  for (size_t i = 0; i < flag.size(); i++) {
    const char a = flag[i] == '_' ? '-' : flag[i];
    const char b = name[i] == '_' ? '-' : name[i];
    if (a != b) {
      return false;
    }
  }
  return true;
}

bool HasNativeCall(std::string_view script) {
  std::unique_ptr<fast::Tree> tree =
      fast::Parser::ParseScript(script, fast::ParseOptions(), nullptr);
  if (tree == nullptr) {
    return true;
  }
  bool found = false;
  fast::Walk(tree->root(), [&found](fast::Node* node) {
    if (node->kind == fast::NodeKind::kNativeCallExpression) {
      found = true;
    }
    return !found;
  });
  return found;
}

bool KeepReproFlag(std::string_view flag, bool natives) {
  return (natives && IsFlag(flag, "allow-natives-syntax")) ||
         IsFlag(flag, "single-threaded") || IsFlag(flag, "random-seed") ||
         IsFlag(flag, "stress-runs");
}

}  // namespace redline
