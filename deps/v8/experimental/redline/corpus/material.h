// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_CORPUS_MATERIAL_H_
#define V8_EXPERIMENTAL_REDLINE_CORPUS_MATERIAL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/core/program.h"
#include "experimental/redline/core/random.h"
#include "experimental/redline/fast/tree.h"

namespace redline {

// Holds unminimized AST programs loaded from `--material` without execution.
// Call `Load()` once at startup before workers start. After `Load()`, all
// reads and `Pick()` calls are thread-safe.
class MaterialBank {
 public:
  static constexpr size_t kMaxFileBytes = size_t{128} * 1024;
  static constexpr size_t kDefaultMaxPrograms = 20000;

  struct Options {
    std::vector<std::string> paths;
    std::string import_root;
    size_t max_programs = kDefaultMaxPrograms;
    uint64_t seed = 1;
  };

  struct LoadStats {
    size_t scanned = 0;
    size_t loaded = 0;
    size_t skipped = 0;
    size_t errors = 0;
    double elapsed_ms = 0.0;
  };

  LoadStats Load(const Options& options);

  bool empty() const { return programs_.empty(); }
  size_t size() const { return programs_.size(); }
  const std::vector<ProgramPtr>& programs() const { return programs_; }

  // Picks 1 donor at uniform random, or returns `nullptr` when empty.
  ProgramPtr Pick(Random& rng) const;

 private:
  std::vector<ProgramPtr> programs_;
};

// Returns true when `path` matches the ClusterFuzz file or directory skip
// rules.
bool IsMaterialSkippedPath(std::string_view path);

// Returns true when `source` declares discouraged V8 flags in `// Flags:`
// comments.
bool HasDiscouragedFlags(std::string_view source);

// Unwraps safe zero-parameter top-level IIFEs in `tree` into top-level
// statements. Returns true when `tree` changed.
bool UnwrapTopLevelIIFEs(fast::Tree* tree);

// Extracts a self-contained statement window of at most `max_nodes` AST nodes
// from `tree` in place. Keeps trees within budget intact. Returns `false` if
// `tree` is empty or cannot fit in `max_nodes`.
bool ExtractCompactWindow(fast::Tree* tree, size_t max_nodes, Random& rng);

// Merges 2 or more donors into 1 isolated top-level script with `lineage`.
// Compacts oversized donors, caps total AST size, renames colliding bindings,
// wraps executable statements per donor in `try {} catch {}`, and adds
// cross-donor glue statements. Returns `nullptr` when empty.
std::shared_ptr<Program> FusePrograms(const std::vector<ProgramPtr>& donors,
                                      Lineage lineage, Random* rng = nullptr);
std::shared_ptr<Program> FusePrograms(const std::vector<ProgramPtr>& donors,
                                      const std::string& detail,
                                      Random* rng = nullptr);

double MaterialPartnerChance();
double MaterialFuseChance();
size_t MaterialMaxDonorNodes();
size_t MaterialMaxFuseNodes();
size_t MaterialMaxCombineNodes();
int MaterialGlueStatements();

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORPUS_MATERIAL_H_
