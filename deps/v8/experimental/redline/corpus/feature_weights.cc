// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/corpus/feature_weights.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <utility>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/json_parser.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/exec/coverage.h"

namespace redline {

namespace {
std::atomic<const FeatureWeights*> active_weights{nullptr};
}  // namespace

// static
const FeatureWeights* FeatureWeights::Active() {
  return active_weights.load(std::memory_order_acquire);
}

// static
void FeatureWeights::SetActive(const FeatureWeights* weights) {
  active_weights.store(weights, std::memory_order_release);
}

// static
std::unique_ptr<FeatureWeights> FeatureWeights::Build(
    const CoverageMap& coverage, const Params& params,
    const std::vector<std::pair<uint32_t, double>>& targets,
    const EdgeRanges& focus) {
  auto result = std::make_unique<FeatureWeights>();
  const uint32_t features = coverage.num_edges();
  const uint32_t edges = coverage.real_edges();
  std::vector<float> region(static_cast<size_t>(edges) + 1, 1.0f);
  // Whether each edge is an optimized code feature.
  constexpr bool kOptimizedRegion = true;
  std::vector<bool> region_kind(static_cast<size_t>(edges) + 1, false);
  // With a focus: whether each edge is in the focus.
  std::vector<bool> edge_focus;
  if (!focus.empty()) {
    edge_focus.assign(static_cast<size_t>(edges) + 1, false);
  }
  for (const CoverageMap::Region& r : coverage.regions()) {
    double w = 1.0;
    if (r.kind == CoverageMap::RegionKind::kBuiltins) {
      w = params.builtins;
    } else if (r.kind == CoverageMap::RegionKind::kOptimized) {
      w = params.optimized;
    }
    for (uint32_t i = 0; i < r.count; i++) {
      region[r.first + i] = static_cast<float>(w);
      if (r.kind == CoverageMap::RegionKind::kOptimized) {
        region_kind[r.first + i] = kOptimizedRegion;
      }
      if (!edge_focus.empty() && params.optimized_in_focus &&
          r.kind == CoverageMap::RegionKind::kOptimized) {
        edge_focus[r.first + i] = true;
      }
    }
  }
  for (const auto& [first, last] : focus) {
    for (uint32_t edge = std::max<uint32_t>(first, 1);
         edge <= std::min(last, edges); edge++) {
      if (!edge_focus[edge]) {
        result->focus_edges_++;
      }
      edge_focus[edge] = true;
    }
  }
  if (result->focus_edges_ != 0) {
    result->focus_ = focus;
  } else {
    edge_focus.clear();
  }
  std::vector<float> target(static_cast<size_t>(edges) + 1, 0.0f);
  for (const auto& [edge, weight] : targets) {
    if (edge == 0 || edge > edges) {
      continue;
    }
    target[edge] = std::max(target[edge], static_cast<float>(weight));
    if (!edge_focus.empty()) {
      edge_focus[edge] = true;
    }
  }
  for (uint32_t edge = 1; edge <= edges; edge++) {
    if (target[edge] > 0.0f) {
      result->target_edges_.push_back(edge);
    }
    if (target[edge] >= 1.0f) {
      result->line_target_edges_.push_back(edge);
    }
  }
  result->weights_.assign(static_cast<size_t>(features) + 1, 1.0f);
  result->is_target_.assign(static_cast<size_t>(features) + 1, false);
  if (!edge_focus.empty()) {
    result->in_focus_.assign(static_cast<size_t>(features) + 1, false);
  } else if (!params.optimized_in_focus) {
    result->in_focus_.assign(static_cast<size_t>(features) + 1, true);
  }
  for (uint32_t f = 1; f <= features; f++) {
    const uint32_t edge = coverage.EdgeOf(f);
    if (edge == 0) {
      continue;
    }
    // Only the code edge of a target is a target feature. Its hit count
    // buckets keep the boosted weight, but they change with most edits of a
    // program, and only large programs reach the high buckets.
    result->is_target_[f] = f <= edges && target[edge] > 0.0f;
    double w = static_cast<double>(region[edge]) *
               (1.0 + params.target_boost * static_cast<double>(target[edge]));
    if (f > edges) {
      const uint32_t bucket =
          (f - coverage.bucket_base()) % CoverageMap::kBuckets;
      w *= bucket == 0 ? 0.0 : params.bucket;
    }
    if (!edge_focus.empty()) {
      result->in_focus_[f] = edge_focus[edge];
      // Optimized code features outside the focus keep their weight.
      if (!edge_focus[edge] && region_kind[edge] != kOptimizedRegion) {
        w *= params.unfocused;
        // Without drop_unfocused, a code edge outside the focus still admits
        // a program, with its lower weight.
        if (!params.drop_unfocused) {
          result->in_focus_[f] = true;
        }
      }
    } else if (!params.optimized_in_focus &&
               region_kind[edge] == kOptimizedRegion &&
               !result->is_target_[f]) {
      result->in_focus_[f] = false;
    }
    result->weights_[f] = static_cast<float>(w);
    if (w != 1.0) {
      result->non_trivial_ = true;
    }
  }
  return result;
}

bool ReadTargetsFile(const std::string& path,
                     std::vector<std::pair<uint32_t, double>>* targets,
                     FeatureWeights::EdgeRanges* focus, uint32_t* code_edges,
                     std::string* error) {
  std::string text;
  if (!ReadFileToString(path, &text, error)) {
    return false;
  }
  JsonValue json;
  if (!JsonValue::Parse(text, &json, error)) {
    *error = path + ": " + *error;
    return false;
  }
  if (json.GetInt("version", 0) != 1 || !json.Get("targets").is_array()) {
    *error =
        path + ": not a targets file (version 1) of tools/change_targets.py";
    return false;
  }
  const int64_t file_edges = json.GetInt("code_edges", 0);
  *code_edges = file_edges > 0 && file_edges <= static_cast<int64_t>(UINT32_MAX)
                    ? static_cast<uint32_t>(file_edges)
                    : 0;
  for (const JsonValue& entry : json.Get("targets").as_array()) {
    const int64_t edge = entry.GetInt("edge", 0);
    const double weight = entry.GetNumber("w", 1.0);
    if (edge <= 0 || edge > static_cast<int64_t>(UINT32_MAX) ||
        !std::isfinite(weight) || weight < 0.0) {
      *error = path + ": invalid target entry " + entry.ToString();
      return false;
    }
    targets->emplace_back(static_cast<uint32_t>(edge), weight);
  }
  const JsonValue& ranges = json.Get("focus").Get("ranges");
  if (ranges.is_array()) {
    for (const JsonValue& range : ranges.as_array()) {
      if (!range.is_array() || range.as_array().size() != 2) {
        *error = path + ": invalid focus range " + range.ToString();
        return false;
      }
      const int64_t first = range.as_array()[0].as_int();
      const int64_t last = range.as_array()[1].as_int();
      if (first <= 0 || last < first ||
          last > static_cast<int64_t>(UINT32_MAX)) {
        *error = path + ": invalid focus range " + range.ToString();
        return false;
      }
      focus->emplace_back(static_cast<uint32_t>(first),
                          static_cast<uint32_t>(last));
    }
  }
  return true;
}

}  // namespace redline
