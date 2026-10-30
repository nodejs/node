// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_CORE_PROGRAM_H_
#define V8_EXPERIMENTAL_REDLINE_CORE_PROGRAM_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/fast/scope.h"
#include "experimental/redline/fast/tree.h"

namespace redline {

// Where a program came from.
struct Lineage {
  // "import", "generate", or the name of the mutator that produced it.
  std::string origin;
  // For imports: the file path. For mutations: the mutator parameters.
  std::string detail;
  // The immediate parent (which may be an intermediate program of a chain
  // of consecutive mutations that never entered the corpus) ...
  uint64_t parent_hash = 0;
  // ... and the corpus program the chain started from (0: none).
  uint32_t parent_id = 0;
  // Mutations applied since that corpus program (1: a direct child).
  int chain = 0;
  // For splice/combine: the second parent.
  uint64_t partner_hash = 0;
  int generation = 0;
  int worker = -1;
};

// A JavaScript program held by the fuzzer. Immutable once created: mutators
// copy the tree. Corpus bookkeeping fields (id, edges) are assigned by the
// coordinator before the program is published.
class Program {
 public:
  // Takes ownership of `tree`; prints it once to obtain the canonical source.
  static std::shared_ptr<Program> FromTree(std::unique_ptr<fast::Tree> tree,
                                           Lineage lineage);
  // Same, for a caller that already holds Print(tree->root()) (the default
  // pretty form): saves printing the tree a second time.
  static std::shared_ptr<Program> FromTree(std::unique_ptr<fast::Tree> tree,
                                           Lineage lineage, std::string source);
  // Parses `source` (nullptr with `error` set on failure).
  static std::shared_ptr<Program> Parse(std::string_view source,
                                        Lineage lineage, std::string* error);

  const fast::Tree& tree() const { return *tree_; }
  const std::string& source() const { return source_; }
  uint64_t hash() const { return hash_; }
  size_t node_count() const { return node_count_; }
  const Lineage& lineage() const { return lineage_; }
  // Lazily computed scope analysis of the (immutable) tree.
  const fast::ScopeAnalysis& scopes() const;

  // A fresh mutable copy of the tree, for mutation.
  std::unique_ptr<fast::Tree> CopyTree() const { return tree_->Copy(); }

  // --- corpus bookkeeping (written once by the coordinator) ---
  uint32_t id = 0;
  // Deterministic edges this program contributed when it was added.
  std::vector<uint32_t> edges;
  // Edges set by a successful execution (all of them, not only new ones).
  uint32_t edges_hit = 0;
  uint64_t exec_time_us = 0;
  uint64_t added_at_ms = 0;
  // All edges the program reaches deterministically (set only when the
  // seed strategy asks for it, see Selector::WantsHitEdges). The strategy
  // consumes and clears it when the program is added.
  mutable std::vector<uint32_t> hit_edges;

  // --- scheduler state (owned by the seed strategy, under the corpus lock) ---
  // Edges of this program that were still rare when it was added.
  mutable std::vector<uint32_t> rare_edges;
  // Uses as a parent (1 each) and as a splice partner (a fraction each).
  mutable double age = 0.0;

  // --- scheduler feedback (updated concurrently by workers) ---
  // Times this program was picked as a parent for mutation.
  mutable std::atomic<uint32_t> picks{0};
  // Mutations of this program that produced a new corpus program.
  mutable std::atomic<uint32_t> children{0};

  static uint64_t HashSource(std::string_view source);

 private:
  Program() = default;
  std::unique_ptr<fast::Tree> tree_;
  std::string source_;
  uint64_t hash_ = 0;
  size_t node_count_ = 0;
  Lineage lineage_;
  mutable std::once_flag scopes_once_;
  mutable std::unique_ptr<fast::ScopeAnalysis> scopes_;
};

using ProgramPtr = std::shared_ptr<const Program>;

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORE_PROGRAM_H_
