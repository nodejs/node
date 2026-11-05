// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_CORPUS_FEATURE_WEIGHTS_H_
#define V8_EXPERIMENTAL_REDLINE_CORPUS_FEATURE_WEIGHTS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace redline {

class CoverageMap;

// The weight of each coverage feature for the rarity strategy
// (corpus/corpus.cc). A feature is an edge or a hit count bucket of a watched
// edge (exec/coverage.h). The weight is
//
//   w(f) = region_weight(f) * (1 + target_boost * target(e)) * bucket_factor(f)
//          * focus_factor(f)
//
// where e is the edge of f. target(e) is the target weight of e: 1.0 for a
// changed line, 0.3 for a changed function, 0 for other edges (see
// tools/change_targets.py). bucket_factor(f) is `bucket` for a bucket feature
// and 1 for an edge. Bucket 0 (the edge ran once) repeats the edge bit, so
// its weight is 0.
//
// The focus is a set of code edges (for example, all edges of src/maglev/).
// With a focus, the focused features are the focus edges, the target edges and
// their buckets. Optimized code features are also focused when
// optimized_in_focus is true. Without a focus, all non-optimized and target
// features pass InFocus(). Optimized code features then pass InFocus() only
// when optimized_in_focus is true. focus_factor(f) is 1 for a feature in the
// focus and `unfocused` for non-optimized features outside a code-edge focus.
// The fuzzer keeps a new program only if it has a new feature in the focus
// (Fuzzer::KeepFocused).
//
// The fuzzer builds the table once, after the targets start, and publishes it
// with SetActive(). The rarity strategy reads it with Active(). Without a
// table, all weights are 1.
class FeatureWeights {
 public:
  struct Params {
    double builtins = 0.5;
    double optimized = 5.0;
    double bucket = 0.5;
    double target_boost = 20.0;
    double unfocused = 0.1;
    // Whether optimized code features pass Fuzzer::KeepFocused. When false,
    // they only weigh in the rarity strategy, and a new program needs a new
    // non-optimized or target feature.
    bool optimized_in_focus = false;
    // With a focus: whether Fuzzer::KeepFocused drops a new program whose new
    // code edges are all outside the focus. When false, such a program enters
    // the corpus, and the focus only lowers the weight of its edges
    // (`unfocused`).
    bool drop_unfocused = true;
  };
  // Inclusive ranges [first, last] of edge indices.
  using EdgeRanges = std::vector<std::pair<uint32_t, uint32_t>>;

  // `targets` maps edge indices to target weights (pairs of edge and weight).
  // `focus` holds the focus edges (empty: no focus).
  static std::unique_ptr<FeatureWeights> Build(
      const CoverageMap& coverage, const Params& params,
      const std::vector<std::pair<uint32_t, double>>& targets,
      const EdgeRanges& focus = {});

  // Weight of feature `index` (1.0 outside the table).
  double Of(uint32_t index) const {
    return index < weights_.size() ? static_cast<double>(weights_[index]) : 1.0;
  }
  // True if a feature has a weight other than 1.
  bool non_trivial() const { return non_trivial_; }
  // True if feature `index` is a target edge or a bucket of a target edge.
  bool IsTarget(uint32_t index) const {
    return index < is_target_.size() && is_target_[index];
  }
  // True if there is a code-edge focus.
  bool has_focus() const { return !focus_.empty(); }
  // True if InFocus filters corpus admission (a focus is active or
  // optimized_in_focus is false).
  bool filters_admission() const { return !in_focus_.empty(); }
  // True if feature `index` passes corpus admission.
  bool InFocus(uint32_t index) const {
    return in_focus_.empty() || (index < in_focus_.size() && in_focus_[index]);
  }
  // The focus edges (for the statistics).
  const EdgeRanges& focus() const { return focus_; }
  uint32_t focus_edges() const { return focus_edges_; }
  // The target edges, sorted (for the statistics), and the subset on changed
  // lines (target weight 1 or more).
  const std::vector<uint32_t>& target_edges() const { return target_edges_; }
  const std::vector<uint32_t>& line_target_edges() const {
    return line_target_edges_;
  }

  // The table that the rarity strategy uses (nullptr: all weights are 1).
  // SetActive() must be called before the workers start, and the table must
  // stay alive until they stop.
  static const FeatureWeights* Active();
  static void SetActive(const FeatureWeights* weights);

 private:
  std::vector<float> weights_;
  std::vector<bool> is_target_;
  std::vector<bool> in_focus_;
  EdgeRanges focus_;
  uint32_t focus_edges_ = 0;
  std::vector<uint32_t> target_edges_;
  std::vector<uint32_t> line_target_edges_;
  bool non_trivial_ = false;
};

// Reads a targets file written by tools/change_targets.py. Returns the pairs of
// edge and weight, and the focus ranges (empty if the file has no focus).
// Also returns the number of code edges of the d8 that the file was made for
// (0 if the file does not say).
bool ReadTargetsFile(const std::string& path,
                     std::vector<std::pair<uint32_t, double>>* targets,
                     FeatureWeights::EdgeRanges* focus, uint32_t* code_edges,
                     std::string* error);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORPUS_FEATURE_WEIGHTS_H_
