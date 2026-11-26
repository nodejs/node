// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/corpus/corpus.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <unordered_map>

#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/corpus/feature_weights.h"
#include "experimental/redline/fast/ast_visit.h"

namespace redline {

// ---------------------------------------------------------------------------
// Seed strategies: which corpus program a worker mutates next.
//
// To add one: derive from ProgramSelector, then add a Register() line in
// RegisterBuiltinSelectors<const Program> below. Picks return an index into
// `candidates` (the corpus in insertion order); rng.Index(n) draws a
// uniform index.

namespace {

using Candidates = ProgramSelector::Candidates;
using Filter = ProgramSelector::Filter;

// Every program is equally likely. The baseline every other strategy is
// measured against.
class UniformSeeds final : public ProgramSelector {
 public:
  std::string_view name() const override { return "uniform"; }
  size_t Pick(const Candidates& candidates, Random& rng,
              const Filter& /*eligible*/) override {
    return rng.Index(candidates.size());
  }
  JsonObject Params() const override { return JsonObject(); }
};

// Part of the picks go to the newest programs (the coverage frontier), the
// rest are uniform.
class RecencySeeds final : public ProgramSelector {
 public:
  static std::vector<ParamSpec> Specs() {
    return {
        {"window_fraction", ParamSpec::Type::kNumber, "0.1",
         "share of the corpus (newest first) that counts as recent"},
        {"min_window", ParamSpec::Type::kNumber, "8",
         "the recent window is at least this many programs"},
        {"recent_share", ParamSpec::Type::kNumber, "0.5",
         "probability of picking from the recent window"},
    };
  }
  explicit RecencySeeds(const JsonValue& params)
      : window_fraction_(params.GetNumber("window_fraction", 0.1)),
        min_window_(static_cast<size_t>(params.GetInt("min_window", 8))),
        recent_share_(params.GetNumber("recent_share", 0.5)) {}
  std::string_view name() const override { return "recency"; }
  size_t Pick(const Candidates& candidates, Random& rng,
              const Filter& /*eligible*/) override {
    size_t n = candidates.size();
    size_t window = std::max<size_t>(
        min_window_,
        static_cast<size_t>(static_cast<double>(n) * window_fraction_));
    // The historical draw for the default share is a coin flip (one RNG
    // word, low bit); other shares use the probability draw. Both consume
    // one word, so the sequence of later picks is the same either way.
    bool recent = recent_share_ == 0.5 ? rng.Coin() : rng.Chance(recent_share_);
    if (window < n && recent) {
      return n - window + rng.Index(window);
    }
    return rng.Index(n);
  }
  JsonObject Params() const override {
    JsonObject o;
    o.Add("window_fraction", window_fraction_);
    o.Add("min_window", static_cast<uint64_t>(min_window_));
    o.Add("recent_share", recent_share_);
    return o;
  }

 private:
  double window_fraction_;
  size_t min_window_;
  double recent_share_;
};

// Tournament selection on the number of times a program was already
// mutated: of `size` random candidates, the least mutated one wins. Gives
// every program a fair number of mutations (Fuzzilli's
// minMutationsPerSample) without any bookkeeping beyond a counter.
class TournamentSeeds final : public ProgramSelector {
 public:
  static std::vector<ParamSpec> Specs() {
    return {
        {"size", ParamSpec::Type::kNumber, "3", "candidates per tournament"}};
  }
  explicit TournamentSeeds(const JsonValue& params)
      : size_(static_cast<int>(params.GetInt("size", 3))) {}
  std::string_view name() const override { return "tournament"; }
  size_t Pick(const Candidates& candidates, Random& rng,
              const Filter& /*eligible*/) override {
    size_t best = rng.Index(candidates.size());
    for (int i = 1; i < size_; i++) {
      size_t candidate = rng.Index(candidates.size());
      if (candidates[candidate]->picks.load(std::memory_order_relaxed) <
          candidates[best]->picks.load(std::memory_order_relaxed)) {
        best = candidate;
      }
    }
    return best;
  }
  JsonObject Params() const override {
    JsonObject o;
    o.Add("size", size_);
    return o;
  }

 private:
  int size_;
};

// Prefers programs whose mutations already produced new corpus programs:
// of `size` random candidates, the one with most children wins (ties: least
// mutated), so that it stays O(1).
class ProductiveSeeds final : public ProgramSelector {
 public:
  static std::vector<ParamSpec> Specs() {
    return {
        {"size", ParamSpec::Type::kNumber, "3", "candidates per tournament"}};
  }
  explicit ProductiveSeeds(const JsonValue& params)
      : size_(static_cast<int>(params.GetInt("size", 3))) {}
  std::string_view name() const override { return "productive"; }
  size_t Pick(const Candidates& candidates, Random& rng,
              const Filter& /*eligible*/) override {
    size_t best = rng.Index(candidates.size());
    for (int i = 1; i < size_; i++) {
      size_t candidate = rng.Index(candidates.size());
      uint32_t bc = candidates[best]->children.load(std::memory_order_relaxed);
      uint32_t cc =
          candidates[candidate]->children.load(std::memory_order_relaxed);
      if (cc > bc ||
          (cc == bc &&
           candidates[candidate]->picks.load(std::memory_order_relaxed) <
               candidates[best]->picks.load(std::memory_order_relaxed))) {
        best = candidate;
      }
    }
    return best;
  }
  JsonObject Params() const override {
    JsonObject o;
    o.Add("size", size_);
    return o;
  }

 private:
  int size_;
};

// Partner strategy: a uniform pick that is not the anchor, in at most
// `tries` attempts (none: no partner, the mutation is skipped).
class UniformPartners final : public ProgramSelector {
 public:
  static std::vector<ParamSpec> Specs() {
    return {{"tries", ParamSpec::Type::kNumber, "4",
             "attempts to draw a program other than the anchor"}};
  }
  explicit UniformPartners(const JsonValue& params)
      : tries_(static_cast<int>(params.GetInt("tries", 4))) {}
  std::string_view name() const override { return "uniform-partner"; }
  size_t Pick(const Candidates& candidates, Random& rng,
              const Filter& /*eligible*/) override {
    return rng.Index(candidates.size());
  }
  size_t PickRelated(const Candidates& candidates, size_t anchor, Random& rng,
                     const Filter& /*eligible*/) override {
    if (candidates.size() < 2) {
      return kNoChoice;
    }
    for (int attempt = 0; attempt < tries_; attempt++) {
      size_t candidate = rng.Index(candidates.size());
      if (candidate != anchor) {
        return candidate;
      }
    }
    return kNoChoice;
  }
  JsonObject Params() const override {
    JsonObject o;
    o.Add("tries", tries_);
    return o;
  }

 private:
  int tries_;
};

// Returns true if `tree` contains a loop statement or a V8 native call.
bool HasJitActivity(const fast::Tree& tree) {
  if (tree.root() == nullptr) {
    return false;
  }
  std::vector<fast::Node*> stack;
  stack.push_back(tree.root());
  while (!stack.empty()) {
    fast::Node* node = stack.back();
    stack.pop_back();
    if (node == nullptr) {
      continue;
    }
    switch (node->kind) {
      case fast::NodeKind::kForStatement:
      case fast::NodeKind::kWhileStatement:
      case fast::NodeKind::kDoWhileStatement:
      case fast::NodeKind::kForInStatement:
      case fast::NodeKind::kForOfStatement:
      case fast::NodeKind::kNativeCallExpression:
        return true;
      default:
        break;
    }
    fast::ForEachChildSlot(node, [&](fast::Node** slot) {
      if (*slot != nullptr) {
        stack.push_back(*slot);
      }
    });
  }
  return false;
}

// --- rarity -----------------------------------------------------------------
//
// Fuzzilli's RarityCorpus (`RarityCorpusState.swift`), adapted for Redline's
// shared corpus:
//
//   energy(p) = rarity(p) / ((age(p) + 1)^age_exponent *
//                            (1 + ln(size(p))^size_exponent) *
//                            (1 + (exec_time_ms(p) /
//                            time_unit_ms)^time_exponent))
//   rarity(p) = sum over tracked edges `e` of `p` with `count(e) <
//   rare_edge_limit` (or `IsTarget(e)`) of `w(e) / count(e)^rarity_exponent`
//
// `active_heap_` is an indexed binary max-heap over unleased seeds with
// positive energy. Reverse edge trackers (`edge_trackers_`) update affected
// seeds in O(log N) when edge counts grow and evict zero-energy seeds from
// `active_heap_`. Leased seeds move to `leased_active_` while checked out so
// workers prefer distinct frontier seeds and fall back cleanly when workers
// hold leases on all active seeds.
class RarityState {
 public:
  struct Params {
    double age_exponent = 1.5;
    double size_exponent = 2.0;
    double rarity_exponent = 3.0;
    int rare_edge_limit = 8;
    int max_tracked_edges = 128;
    double size_unit = 4.0;
    int partner_pool = 3;
    double partner_age_increment = 0.2;
    // Change-based fuzzing (`corpus/feature_weights.h`): the probability that a
    // seed or partner pick takes a program that reaches a target.
    double target_pick = 0.5;
    int target_tournament = 4;
    // Change-based fuzzing: the number of cheapest programs that each target
    // feature keeps for the target picks (0 uses a tournament on energy).
    int target_favored = 4;
    // Change-based fuzzing: a target feature that more than this share of the
    // imported seeds reach is common, and the target picks skip it (0 is off).
    double target_common_share = 0.25;
    double time_exponent = 0.5;
    double time_unit_ms = 10.0;
    // Maximum resident AST trees in memory (0 disables eviction).
    int max_memory_trees = 0;
    // Energy factor for seeds with loops or native calls (1 disables).
    double jit_seed_boost = 1.0;
    // Exponent of the `OnOutcome` fertility multiplier (0 disables).
    double fertility_weight = 0.0;
  };

  static std::vector<ParamSpec> Specs() {
    return {
        {"age_exponent", ParamSpec::Type::kNumber, "1.5",
         "energy decay with uses: (age + 1)^-x"},
        {"size_exponent", ParamSpec::Type::kNumber, "2.0",
         "size penalty: 1 + ln(size)^x"},
        {"rarity_exponent", ParamSpec::Type::kNumber, "3.0",
         "edge weight 1 / count^x"},
        {"rare_edge_limit", ParamSpec::Type::kNumber, "8",
         "an edge reached by this many programs is common"},
        {"max_tracked_edges", ParamSpec::Type::kNumber, "128",
         "rare edges remembered per program"},
        {"size_unit", ParamSpec::Type::kNumber, "4",
         "AST nodes per size unit (Fuzzilli counts instructions)"},
        {"partner_pool", ParamSpec::Type::kNumber, "3",
         "tournament size of the partner pick"},
        {"partner_age_increment", ParamSpec::Type::kNumber, "0.2",
         "age added to a program picked as partner"},
        {"target_pick", ParamSpec::Type::kNumber, "0.5",
         "change-based fuzzing: share of picks among the programs that "
         "reach a target"},
        {"target_tournament", ParamSpec::Type::kNumber, "4",
         "change-based fuzzing: tournament size of a target pick"},
        {"target_favored", ParamSpec::Type::kNumber, "4",
         "change-based fuzzing: cheapest programs (size x time) kept per "
         "target feature; a target pick takes the rarest of "
         "target_tournament reached target features and its least used "
         "program (0: tournament on energy)"},
        {"target_common_share", ParamSpec::Type::kNumber, "0.25",
         "change-based fuzzing: target picks skip a target feature that "
         "more than this share of the imported seeds reach (0: off)"},
        {"time_exponent", ParamSpec::Type::kNumber, "0.5",
         "execution-time penalty exponent (0 disables)"},
        {"time_unit_ms", ParamSpec::Type::kNumber, "10",
         "execution time (ms) at which the time penalty doubles"},
        {"max_memory_trees", ParamSpec::Type::kNumber, "0",
         "maximum resident AST trees in memory (0 disables eviction)"},
        {"jit_seed_boost", ParamSpec::Type::kNumber, "1",
         "energy factor for seeds with loops or native calls (1 disables)"},
        {"fertility_weight", ParamSpec::Type::kNumber, "0",
         "exponent of the outcome fertility factor (0 disables)"},
    };
  }

  static Params ReadParams(const JsonValue& params) {
    Params p;
    p.age_exponent = params.GetNumber("age_exponent", p.age_exponent);
    p.size_exponent = params.GetNumber("size_exponent", p.size_exponent);
    p.rarity_exponent = params.GetNumber("rarity_exponent", p.rarity_exponent);
    p.rare_edge_limit =
        static_cast<int>(params.GetInt("rare_edge_limit", p.rare_edge_limit));
    p.max_tracked_edges = static_cast<int>(
        params.GetInt("max_tracked_edges", p.max_tracked_edges));
    p.size_unit = params.GetNumber("size_unit", p.size_unit);
    p.partner_pool =
        static_cast<int>(params.GetInt("partner_pool", p.partner_pool));
    p.partner_age_increment =
        params.GetNumber("partner_age_increment", p.partner_age_increment);
    p.target_pick = params.GetNumber("target_pick", p.target_pick);
    p.target_tournament = static_cast<int>(
        params.GetInt("target_tournament", p.target_tournament));
    p.target_favored =
        static_cast<int>(params.GetInt("target_favored", p.target_favored));
    p.target_common_share =
        params.GetNumber("target_common_share", p.target_common_share);
    p.time_exponent = params.GetNumber("time_exponent", p.time_exponent);
    p.time_unit_ms = params.GetNumber("time_unit_ms", p.time_unit_ms);
    p.max_memory_trees =
        static_cast<int>(params.GetInt("max_memory_trees", p.max_memory_trees));
    p.jit_seed_boost = params.GetNumber("jit_seed_boost", p.jit_seed_boost);
    p.fertility_weight =
        params.GetNumber("fertility_weight", p.fertility_weight);
    return p;
  }

  static bool Validate(const Params& p, std::string* error) {
    if (p.rare_edge_limit < 1 || p.rare_edge_limit > 254) {
      *error = "rare_edge_limit must be in [1, 254]";
      return false;
    }
    if (p.target_pick < 0.0 || p.target_pick > 1.0 || p.target_tournament < 1 ||
        p.target_favored < 0 || p.target_common_share < 0.0 ||
        p.target_common_share > 1.0) {
      *error =
          "target_pick and target_common_share must be in [0, 1], "
          "target_tournament >= 1, target_favored >= 0";
      return false;
    }
    if (p.max_tracked_edges < 1 || p.partner_pool < 1 || p.size_unit <= 0.0 ||
        p.time_unit_ms <= 0.0) {
      *error =
          "max_tracked_edges and partner_pool must be >= 1, size_unit and "
          "time_unit_ms > 0";
      return false;
    }
    if (p.max_memory_trees < 0) {
      *error = "max_memory_trees must be >= 0";
      return false;
    }
    if (!std::isfinite(p.jit_seed_boost) || p.jit_seed_boost <= 0.0) {
      *error = "jit_seed_boost must be finite and > 0";
      return false;
    }
    for (double x :
         {p.age_exponent, p.size_exponent, p.rarity_exponent,
          p.partner_age_increment, p.time_exponent, p.fertility_weight}) {
      if (!std::isfinite(x) || x < 0.0) {
        *error =
            "exponents, partner_age_increment and fertility_weight must be "
            "finite and >= 0";
        return false;
      }
    }
    return true;
  }

  explicit RarityState(const Params& params) : params_(params) {
    rarity_weights_.resize(static_cast<size_t>(params_.rare_edge_limit));
    for (int c = 0; c < params_.rare_edge_limit; c++) {
      rarity_weights_[static_cast<size_t>(c)] =
          1.0 / std::pow(static_cast<double>(std::max(1, c)),
                         params_.rarity_exponent);
    }
  }

  const Params& params() const { return params_; }

  JsonObject ParamsJson() const {
    JsonObject o;
    o.Add("age_exponent", params_.age_exponent)
        .Add("size_exponent", params_.size_exponent);
    o.Add("rarity_exponent", params_.rarity_exponent)
        .Add("rare_edge_limit", params_.rare_edge_limit);
    o.Add("max_tracked_edges", params_.max_tracked_edges)
        .Add("size_unit", params_.size_unit);
    o.Add("partner_pool", params_.partner_pool)
        .Add("partner_age_increment", params_.partner_age_increment);
    o.Add("target_pick", params_.target_pick)
        .Add("target_tournament", params_.target_tournament);
    o.Add("target_favored", params_.target_favored)
        .Add("target_common_share", params_.target_common_share);
    o.Add("time_exponent", params_.time_exponent)
        .Add("time_unit_ms", params_.time_unit_ms);
    o.Add("max_memory_trees", params_.max_memory_trees)
        .Add("jit_seed_boost", params_.jit_seed_boost)
        .Add("fertility_weight", params_.fertility_weight);
    return o;
  }

  void OnAdded(const Candidates& candidates, size_t index) {
    const Program& program = *candidates[index];
    // Without the full edge set, fall back to the new edges of the program.
    std::vector<uint32_t> edges =
        program.hit_edges.empty() ? program.edges : program.hit_edges;
    for (uint32_t edge : program.edges) {
      edges.push_back(edge);
    }
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    const uint16_t limit = static_cast<uint16_t>(params_.rare_edge_limit);
    if (!edges.empty() && edges.back() >= counts_.size()) {
      counts_.resize(static_cast<size_t>(edges.back()) + 4096, 0);
    }
    const FeatureWeights* weights = FeatureWeights::Active();
    affected_stamp_++;
    std::vector<size_t> affected;
    std::vector<uint32_t> rare;
    for (uint32_t edge : edges) {
      IncrementEdge(edge, weights, &affected);
      const bool is_target = weights != nullptr && weights->IsTarget(edge);
      if (is_target || counts_[edge] < limit) {
        rare.push_back(edge);
      }
    }
    for (size_t seed_idx : affected) {
      RefreshEnergy(seed_idx);
    }
    // Keep target features first so `max_tracked_edges` never drops them, then
    // sort by highest weighted rarity.
    std::stable_sort(rare.begin(), rare.end(), [&](uint32_t a, uint32_t b) {
      const bool ta = weights != nullptr && weights->IsTarget(a);
      const bool tb = weights != nullptr && weights->IsTarget(b);
      if (ta != tb) {
        return ta;
      }
      return Score(a, weights) > Score(b, weights);
    });
    if (rare.size() > static_cast<size_t>(params_.max_tracked_edges)) {
      rare.resize(static_cast<size_t>(params_.max_tracked_edges));
    }
    program.rare_edges = std::move(rare);
    if (seeds_.size() <= index) {
      seeds_.resize(index + 1);
    }
    SeedInfo& info = seeds_[index];
    info.program = &program;
    info.base_penalty = ComputeBasePenalty(program);
    info.has_jit_activity =
        params_.jit_seed_boost != 1.0 && HasJitActivity(program.tree());
    for (uint32_t edge : program.rare_edges) {
      const bool is_target = weights != nullptr && weights->IsTarget(edge);
      if ((!is_target && counts_[edge] < limit) ||
          (is_target && counts_[edge] <= kMaxTrackedTargetCount)) {
        edge_trackers_[edge].push_back(index);
      }
    }
    info.energy = ComputeEnergy(index);
    if (info.energy > 0.0) {
      HeapInsert(index);
    } else {
      EnqueueCold(index);
    }
    if (weights != nullptr) {
      const bool imported = program.lineage().origin == "import";
      if (imported) {
        imports_++;
      }
      bool reaches_target = false;
      for (uint32_t edge : edges) {
        if (weights->IsTarget(edge)) {
          reaches_target = true;
          AddFavored(candidates, edge, index);
          if (imported) {
            import_target_hits_[edge]++;
          }
        }
      }
      if (reaches_target) {
        target_programs_.push_back(index);
      }
    }
  }

  // Records a mutation outcome for parent fertility tracking and refreshes its
  // energy when `fertility_weight` is active.
  void OnOutcome(const Program& program, bool interesting) {
    if (program.id == 0) {
      return;
    }
    const size_t index = static_cast<size_t>(program.id - 1);
    if (index >= seeds_.size() || seeds_[index].program != &program) {
      return;
    }
    SeedInfo& info = seeds_[index];
    info.outcome_trials++;
    if (interesting) {
      info.outcome_hits++;
    }
    if (params_.fertility_weight > 0.0) {
      RefreshEnergy(index);
    }
  }

  // The cost of a program for the favored lists: its size times its
  // execution time, like the "top rated" entries of AFL.
  double Cost(const Program& program) const {
    const double ms = static_cast<double>(program.exec_time_us) / 1000.0;
    return std::max(1.0, static_cast<double>(program.node_count())) *
           (1.0 + ms / params_.time_unit_ms);
  }

  // Keeps the `target_favored` cheapest programs that reach `edge`, cheapest
  // first.
  void AddFavored(const Candidates& candidates, uint32_t edge, size_t index) {
    if (params_.target_favored <= 0) {
      return;
    }
    std::vector<size_t>& favored = favored_[edge];
    if (favored.empty()) {
      reached_targets_.push_back(edge);
    }
    const double cost = Cost(*candidates[index]);
    auto at = std::find_if(favored.begin(), favored.end(), [&](size_t other) {
      return cost < Cost(*candidates[other]);
    });
    if (at == favored.end() &&
        favored.size() >= static_cast<size_t>(params_.target_favored)) {
      return;
    }
    favored.insert(at, index);
    if (favored.size() > static_cast<size_t>(params_.target_favored)) {
      favored.pop_back();
    }
  }

  // True if more than `target_common_share` of the imported seeds reach the
  // target feature `edge`.
  bool CommonTarget(uint32_t edge) const {
    if (imports_ == 0 || params_.target_common_share <= 0.0) {
      return false;
    }
    auto it = import_target_hits_.find(edge);
    return it != import_target_hits_.end() &&
           static_cast<double>(it->second) >
               params_.target_common_share * static_cast<double>(imports_);
  }

  // Returns a favored program of a rare reached target feature, other than
  // `anchor` (`kNoChoice` if none). When `prefer_unleased` is true, prefers an
  // unleased program and falls back to the least aged favored program.
  size_t PickFavored(const Candidates& candidates, Random& rng, size_t anchor,
                     bool prefer_unleased) const {
    const FeatureWeights* weights = FeatureWeights::Active();
    uint32_t best = 0;
    double best_score = -1.0;
    for (int i = 0, tries = 0;
         i < params_.target_tournament &&
         tries < 4 * params_.target_tournament && !reached_targets_.empty();
         tries++) {
      const uint32_t edge =
          reached_targets_[rng.Index(reached_targets_.size())];
      if (CommonTarget(edge)) {
        continue;
      }
      i++;
      const double score = Score(edge, weights);
      if (score > best_score) {
        best = edge;
        best_score = score;
      }
    }
    if (best_score < 0.0) {
      return ProgramSelector::kNoChoice;
    }
    size_t pick = ProgramSelector::kNoChoice;
    size_t unleased_pick = ProgramSelector::kNoChoice;
    for (size_t index : favored_.at(best)) {
      if (index == anchor) {
        continue;
      }
      if (pick == ProgramSelector::kNoChoice ||
          candidates[index]->age < candidates[pick]->age) {
        pick = index;
      }
      if (prefer_unleased && seeds_[index].lease_count == 0 &&
          (unleased_pick == ProgramSelector::kNoChoice ||
           candidates[index]->age < candidates[unleased_pick]->age)) {
        unleased_pick = index;
      }
    }
    return unleased_pick != ProgramSelector::kNoChoice ? unleased_pick : pick;
  }

  size_t target_programs() const { return target_programs_.size(); }
  size_t active_size() const {
    return active_heap_.size() + leased_active_.size();
  }
  size_t leased_size() const { return leased_distinct_; }

  // Counts the edges of a reproducible crash as seen once more and updates
  // every seed tracking those edges.
  void RecordCrashEdges(const std::vector<uint32_t>& edges) {
    const FeatureWeights* weights = FeatureWeights::Active();
    affected_stamp_++;
    std::vector<size_t> affected;
    for (uint32_t edge : edges) {
      if (edge >= counts_.size()) {
        counts_.resize(static_cast<size_t>(edge) + 4096, 0);
      }
      IncrementEdge(edge, weights, &affected);
    }
    for (size_t seed_idx : affected) {
      RefreshEnergy(seed_idx);
    }
  }

  // The contribution of one feature to the rarity of a program: its weight
  // (`corpus/feature_weights.h`) divided by `count^rarity_exponent`.
  double Score(uint32_t edge, const FeatureWeights* weights) const {
    const double w = weights != nullptr ? weights->Of(edge) : 1.0;
    const uint16_t count = counts_[edge];
    if (count < rarity_weights_.size()) {
      return w * rarity_weights_[count];
    }
    return w / std::pow(static_cast<double>(std::max<uint16_t>(1, count)),
                        params_.rarity_exponent);
  }

  // Inspects up to `tries` candidates without changing `active_heap_` or `age`.
  // Commits `age` (and optional lease checkout) only on the chosen seed.
  size_t PickPreferred(const Candidates& candidates, Random& rng, int tries,
                       const std::function<bool(const Program&)>& prefer,
                       bool acquire_lease) {
    if (candidates.empty()) {
      return ProgramSelector::kNoChoice;
    }
    SyncTop();
    size_t chosen = ProgramSelector::kNoChoice;
    if (!target_programs_.empty() && rng.Chance(params_.target_pick)) {
      const int limit = std::max(1, tries);
      for (int i = 0; i < limit; i++) {
        const size_t candidate = PeekTargetCandidate(candidates, rng);
        if (candidate == ProgramSelector::kNoChoice) {
          break;
        }
        if (chosen == ProgramSelector::kNoChoice) {
          chosen = candidate;
        }
        if (prefer == nullptr || prefer(*candidates[candidate])) {
          chosen = candidate;
          break;
        }
      }
    }
    if (chosen == ProgramSelector::kNoChoice) {
      chosen = PeekActiveHeap(candidates, tries, prefer);
    }
    if (chosen == ProgramSelector::kNoChoice) {
      chosen = PeekLeasedActive(candidates, tries, prefer);
    }
    if (chosen == ProgramSelector::kNoChoice) {
      chosen = PeekUniformFallback(candidates, rng, tries, prefer);
    }
    if (chosen == ProgramSelector::kNoChoice) {
      return ProgramSelector::kNoChoice;
    }
    CommitPick(candidates, chosen, acquire_lease);
    return chosen;
  }

  // Releases one active lease on `index`. Re-inserts the seed into
  // `active_heap_` when its last lease ends and its energy stays positive.
  void ReleaseLease(size_t index) {
    if (index >= seeds_.size()) {
      return;
    }
    SeedInfo& info = seeds_[index];
    if (info.lease_count == 0) {
      Fatal("RarityState::ReleaseLease: lease underflow for index %zu", index);
    }
    info.lease_count--;
    if (info.lease_count > 0) {
      return;
    }
    leased_distinct_--;
    if (info.leased_pos >= 0) {
      LeasedRemove(index);
    }
    RefreshEnergy(index);
  }

  // Samples partners uniformly from active (`energy > 0`) seeds in O(1),
  // falling back to the full corpus only when no other active seed exists.
  size_t PickPartner(const Candidates& candidates, size_t anchor, Random& rng) {
    const size_t n = candidates.size();
    if (n < 2) {
      return ProgramSelector::kNoChoice;
    }
    const bool has_other_target =
        target_programs_.size() > 1 ||
        (target_programs_.size() == 1 && target_programs_[0] != anchor);
    bool use_targets = has_other_target && rng.Chance(params_.target_pick);
    if (use_targets && params_.target_favored > 0) {
      const size_t index =
          PickFavored(candidates, rng, anchor, /*prefer_unleased=*/false);
      if (index != ProgramSelector::kNoChoice) {
        candidates[index]->age += params_.partner_age_increment;
        RefreshEnergy(index);
        return index;
      }
      // No rare target feature: fall through to a normal active partner pick.
      use_targets = false;
    }
    size_t best = ProgramSelector::kNoChoice;
    double best_energy = 0.0;
    if (use_targets) {
      for (int i = 0, attempts = 0;
           i < params_.partner_pool && attempts < 4 * params_.partner_pool;
           attempts++) {
        const size_t candidate =
            target_programs_[rng.Index(target_programs_.size())];
        if (candidate == anchor) {
          continue;
        }
        const double energy = seeds_[candidate].energy;
        if (energy <= 0.0) {
          continue;
        }
        i++;
        if (best == ProgramSelector::kNoChoice || energy > best_energy) {
          best = candidate;
          best_energy = energy;
        }
      }
    } else {
      const size_t active_count = active_heap_.size() + leased_active_.size();
      size_t anchor_slot = active_count;
      if (anchor < seeds_.size()) {
        if (seeds_[anchor].heap_pos >= 0) {
          anchor_slot = static_cast<size_t>(seeds_[anchor].heap_pos);
        } else if (seeds_[anchor].leased_pos >= 0) {
          anchor_slot = active_heap_.size() +
                        static_cast<size_t>(seeds_[anchor].leased_pos);
        }
      }
      const size_t eligible =
          active_count - (anchor_slot < active_count ? 1 : 0);
      if (eligible > 0) {
        const int draws =
            std::min<int>(params_.partner_pool,
                          static_cast<int>(std::min<size_t>(eligible, 1024)));
        for (int i = 0; i < draws; i++) {
          size_t slot = rng.Index(eligible);
          if (slot >= anchor_slot) {
            slot++;
          }
          const size_t candidate =
              slot < active_heap_.size()
                  ? active_heap_[slot]
                  : leased_active_[slot - active_heap_.size()];
          const double energy = seeds_[candidate].energy;
          if (best == ProgramSelector::kNoChoice || energy > best_energy) {
            best = candidate;
            best_energy = energy;
          }
        }
      }
    }
    if (best == ProgramSelector::kNoChoice) {
      if (anchor < n) {
        size_t slot = rng.Index(n - 1);
        if (slot >= anchor) {
          slot++;
        }
        return slot;
      }
      return rng.Index(n);
    }
    candidates[best]->age += params_.partner_age_increment;
    RefreshEnergy(best);
    return best;
  }

  size_t ColdQueueSize() const { return cold_queue_.size(); }

  bool PopCold(size_t* out) {
    if (cold_queue_.empty()) {
      return false;
    }
    const size_t idx = cold_queue_.front();
    cold_queue_.pop_front();
    seeds_[idx].in_cold_queue = false;
    *out = idx;
    return true;
  }

  void RequeueCold(size_t index) {
    if (index < seeds_.size()) {
      EnqueueCold(index);
    }
  }

  bool IsLeased(size_t index) const {
    return index < seeds_.size() && seeds_[index].lease_count > 0;
  }

  bool IsZeroEnergy(size_t index) const {
    return index < seeds_.size() && seeds_[index].energy <= 0.0;
  }

  // Verifies heap order, index tables, lease states and reverse edge trackers.
  bool CheckInvariants(std::string* error) const {
    size_t in_heap = 0, in_leased = 0, leased_n = 0;
    const uint16_t limit = static_cast<uint16_t>(params_.rare_edge_limit);
    const FeatureWeights* weights = FeatureWeights::Active();
    for (size_t i = 0; i < seeds_.size(); i++) {
      const SeedInfo& s = seeds_[i];
      if (s.program == nullptr) {
        *error = StringPrintf("seed %zu null program", i);
        return false;
      }
      if (!has_high_count_targets_) {
        const double expected = ComputeEnergy(i);
        if (std::fabs(s.energy - expected) >
            1e-9 * std::max({1.0, expected, s.energy})) {
          *error =
              StringPrintf("seed %zu energy %g != %g", i, s.energy, expected);
          return false;
        }
      }
      const bool ok_heap =
          s.heap_pos == kNotIndexed ||
          (s.heap_pos >= 0 &&
           static_cast<size_t>(s.heap_pos) < active_heap_.size() &&
           active_heap_[static_cast<size_t>(s.heap_pos)] == i);
      const bool ok_leased =
          s.leased_pos == kNotIndexed ||
          (s.leased_pos >= 0 &&
           static_cast<size_t>(s.leased_pos) < leased_active_.size() &&
           leased_active_[static_cast<size_t>(s.leased_pos)] == i);
      const bool want_heap = s.lease_count == 0 && s.energy > 0.0;
      const bool want_leased = s.lease_count > 0 && s.energy > 0.0;
      if (!ok_heap || !ok_leased || (s.heap_pos != kNotIndexed) != want_heap ||
          (s.leased_pos != kNotIndexed) != want_leased) {
        *error = StringPrintf("seed %zu bad index state", i);
        return false;
      }
      in_heap += want_heap ? 1 : 0;
      in_leased += want_leased ? 1 : 0;
      leased_n += s.lease_count > 0 ? 1 : 0;
      for (uint32_t edge : s.program->rare_edges) {
        if ((weights == nullptr || !weights->IsTarget(edge)) &&
            counts_[edge] < limit) {
          auto it = edge_trackers_.find(edge);
          if (it == edge_trackers_.end() ||
              std::find(it->second.begin(), it->second.end(), i) ==
                  it->second.end()) {
            *error = StringPrintf("missing tracker edge %u seed %zu", edge, i);
            return false;
          }
        }
      }
    }
    if (in_heap != active_heap_.size() || in_leased != leased_active_.size() ||
        leased_n != leased_distinct_) {
      *error = "active/leased size mismatch";
      return false;
    }
    for (size_t pos = 1; pos < active_heap_.size(); pos++) {
      if (Better(active_heap_[pos], active_heap_[(pos - 1) / 2])) {
        *error = StringPrintf("heap order violated at %zu", pos);
        return false;
      }
    }
    for (const auto& [edge, list] : edge_trackers_) {
      if ((weights == nullptr || !weights->IsTarget(edge)) &&
          counts_[edge] >= limit) {
        *error = StringPrintf("saturated edge %u in edge_trackers_", edge);
        return false;
      }
    }
    return true;
  }

 private:
  static constexpr int32_t kNotIndexed = -1;
  // Cap reverse tracking per target edge to keep `OnAdded` O(1) on large
  // change-based corpora. Target edges never reach energy 0.
  static constexpr uint16_t kMaxTrackedTargetCount = 64;

  struct SeedInfo {
    const Program* program = nullptr;
    double energy = 0.0;
    double base_penalty = 1.0;
    int32_t heap_pos = kNotIndexed;
    uint32_t lease_count = 0;
    int32_t leased_pos = kNotIndexed;
    uint64_t affected_stamp = 0;
    uint32_t outcome_trials = 0;
    uint32_t outcome_hits = 0;
    bool has_jit_activity = false;
    bool in_cold_queue = false;
  };

  bool Better(size_t a, size_t b) const {
    const double ea = seeds_[a].energy;
    const double eb = seeds_[b].energy;
    if (ea != eb) {
      return ea > eb;
    }
    return a < b;
  }

  double ComputeBasePenalty(const Program& program) const {
    const double size = std::max(
        1.0, static_cast<double>(program.node_count()) / params_.size_unit);
    const double ln_size = std::log(size);
    const double size_pen = 1.0 + std::pow(ln_size, params_.size_exponent);
    double time_pen = 1.0;
    if (program.exec_time_us > 0 && params_.time_exponent > 0.0) {
      const double time_units = static_cast<double>(program.exec_time_us) /
                                (params_.time_unit_ms * 1000.0);
      time_pen += std::pow(time_units, params_.time_exponent);
    }
    return size_pen * time_pen;
  }

  double ComputeEnergy(size_t index) const {
    const SeedInfo& info = seeds_[index];
    const Program& program = *info.program;
    double rarity = 0.0;
    const FeatureWeights* weights = FeatureWeights::Active();
    const uint16_t limit = static_cast<uint16_t>(params_.rare_edge_limit);
    for (uint32_t edge : program.rare_edges) {
      const bool is_target = weights != nullptr && weights->IsTarget(edge);
      if (is_target || counts_[edge] < limit) {
        rarity += Score(edge, weights);
      }
    }
    if (rarity == 0.0) {
      return 0.0;
    }
    double energy =
        rarity /
        (std::pow(program.age + 1.0, params_.age_exponent) * info.base_penalty);
    if (params_.jit_seed_boost != 1.0 && info.has_jit_activity) {
      energy *= params_.jit_seed_boost;
    }
    if (params_.fertility_weight > 0.0) {
      // Laplace-smoothed fertility ratio normalized to 1.0 at (0 hits, 0
      // trials) with a 1/20 prior and clamped to [0.5, 2.0].
      const double ratio = 20.0 *
                           (1.0 + static_cast<double>(info.outcome_hits)) /
                           (20.0 + static_cast<double>(info.outcome_trials));
      const double clamped = std::clamp(ratio, 0.5, 2.0);
      energy *= params_.fertility_weight == 1.0
                    ? clamped
                    : std::pow(clamped, params_.fertility_weight);
    }
    return energy;
  }

  void IncrementEdge(uint32_t edge, const FeatureWeights* weights,
                     std::vector<size_t>* affected) {
    const uint16_t limit = static_cast<uint16_t>(params_.rare_edge_limit);
    const bool is_target = weights != nullptr && weights->IsTarget(edge);
    const uint16_t cap = is_target ? UINT16_MAX : limit;
    uint16_t& count = counts_[edge];
    if (count >= cap) {
      return;
    }
    count++;
    auto it = edge_trackers_.find(edge);
    if (it == edge_trackers_.end()) {
      return;
    }
    for (size_t seed_idx : it->second) {
      if (seeds_[seed_idx].affected_stamp != affected_stamp_) {
        seeds_[seed_idx].affected_stamp = affected_stamp_;
        affected->push_back(seed_idx);
      }
    }
    if ((!is_target && count >= limit) ||
        (is_target && count > kMaxTrackedTargetCount)) {
      edge_trackers_.erase(it);
      if (is_target) {
        has_high_count_targets_ = true;
      }
    }
  }

  void EnqueueCold(size_t index) {
    SeedInfo& info = seeds_[index];
    if (info.in_cold_queue) {
      return;
    }
    info.in_cold_queue = true;
    cold_queue_.push_back(index);
  }

  void HeapSwap(size_t pos_a, size_t pos_b) {
    std::swap(active_heap_[pos_a], active_heap_[pos_b]);
    seeds_[active_heap_[pos_a]].heap_pos = static_cast<int32_t>(pos_a);
    seeds_[active_heap_[pos_b]].heap_pos = static_cast<int32_t>(pos_b);
  }

  void HeapSiftUp(size_t pos) {
    while (pos > 0) {
      const size_t parent = (pos - 1) / 2;
      if (!Better(active_heap_[pos], active_heap_[parent])) {
        break;
      }
      HeapSwap(pos, parent);
      pos = parent;
    }
  }

  void HeapSiftDown(size_t pos) {
    const size_t n = active_heap_.size();
    while (true) {
      const size_t left = 2 * pos + 1;
      if (left >= n) {
        break;
      }
      size_t best = left;
      const size_t right = left + 1;
      if (right < n && Better(active_heap_[right], active_heap_[left])) {
        best = right;
      }
      if (!Better(active_heap_[best], active_heap_[pos])) {
        break;
      }
      HeapSwap(pos, best);
      pos = best;
    }
  }

  void HeapInsert(size_t index) {
    const size_t pos = active_heap_.size();
    seeds_[index].heap_pos = static_cast<int32_t>(pos);
    active_heap_.push_back(index);
    HeapSiftUp(pos);
  }

  void HeapUpdate(size_t index) {
    const size_t pos = static_cast<size_t>(seeds_[index].heap_pos);
    if (pos > 0 && Better(active_heap_[pos], active_heap_[(pos - 1) / 2])) {
      HeapSiftUp(pos);
    } else {
      HeapSiftDown(pos);
    }
  }

  void HeapRemove(size_t index) {
    const size_t pos = static_cast<size_t>(seeds_[index].heap_pos);
    const size_t last = active_heap_.back();
    active_heap_.pop_back();
    seeds_[index].heap_pos = kNotIndexed;
    if (pos < active_heap_.size()) {
      active_heap_[pos] = last;
      seeds_[last].heap_pos = static_cast<int32_t>(pos);
      HeapUpdate(last);
    }
  }

  void LeasedInsert(size_t index) {
    seeds_[index].leased_pos = static_cast<int32_t>(leased_active_.size());
    leased_active_.push_back(index);
  }

  void LeasedRemove(size_t index) {
    const size_t pos = static_cast<size_t>(seeds_[index].leased_pos);
    const size_t last = leased_active_.back();
    leased_active_.pop_back();
    seeds_[index].leased_pos = kNotIndexed;
    if (pos < leased_active_.size()) {
      leased_active_[pos] = last;
      seeds_[last].leased_pos = static_cast<int32_t>(pos);
    }
  }

  void RefreshEnergy(size_t index) {
    SeedInfo& info = seeds_[index];
    const double energy = ComputeEnergy(index);
    info.energy = energy;
    if (info.lease_count > 0) {
      if (energy > 0.0) {
        if (info.leased_pos == kNotIndexed) {
          LeasedInsert(index);
        }
      } else if (info.leased_pos != kNotIndexed) {
        LeasedRemove(index);
      }
      return;
    }
    if (energy > 0.0) {
      if (info.heap_pos != kNotIndexed) {
        HeapUpdate(index);
      } else {
        HeapInsert(index);
      }
    } else {
      if (info.heap_pos != kNotIndexed) {
        HeapRemove(index);
      }
      EnqueueCold(index);
    }
  }

  void SyncTop() {
    if (!has_high_count_targets_) {
      return;
    }
    while (!active_heap_.empty()) {
      const size_t top = active_heap_[0];
      const double current = ComputeEnergy(top);
      if (std::fabs(current - seeds_[top].energy) <=
          seeds_[top].energy * 1e-9) {
        break;
      }
      RefreshEnergy(top);
    }
  }

  size_t PeekTargetCandidate(const Candidates& candidates, Random& rng) const {
    if (params_.target_favored > 0) {
      return PickFavored(candidates, rng, ProgramSelector::kNoChoice,
                         /*prefer_unleased=*/true);
    }
    size_t best = ProgramSelector::kNoChoice;
    double best_energy = -1.0;
    size_t best_unleased = ProgramSelector::kNoChoice;
    double best_unleased_energy = -1.0;
    for (int i = 0; i < params_.target_tournament; i++) {
      const size_t candidate =
          target_programs_[rng.Index(target_programs_.size())];
      const double energy = seeds_[candidate].energy;
      if (best == ProgramSelector::kNoChoice || energy > best_energy) {
        best = candidate;
        best_energy = energy;
      }
      if (seeds_[candidate].lease_count == 0 &&
          (best_unleased == ProgramSelector::kNoChoice ||
           energy > best_unleased_energy)) {
        best_unleased = candidate;
        best_unleased_energy = energy;
      }
    }
    return best_unleased != ProgramSelector::kNoChoice ? best_unleased : best;
  }

  // Inspects the top `tries` entries of `active_heap_` in descending energy
  // order without mutating the heap or any seed state.
  size_t PeekActiveHeap(
      const Candidates& candidates, int tries,
      const std::function<bool(const Program&)>& prefer) const {
    if (active_heap_.empty()) {
      return ProgramSelector::kNoChoice;
    }
    const int limit = std::max(1, tries);
    if (limit == 1 || prefer == nullptr) {
      return active_heap_[0];
    }
    std::vector<size_t> frontier;
    frontier.reserve(static_cast<size_t>(limit) + 2);
    frontier.push_back(0);
    size_t fallback = ProgramSelector::kNoChoice;
    for (int i = 0; i < limit && !frontier.empty(); i++) {
      size_t best_fi = 0;
      for (size_t fi = 1; fi < frontier.size(); fi++) {
        if (Better(active_heap_[frontier[fi]],
                   active_heap_[frontier[best_fi]])) {
          best_fi = fi;
        }
      }
      const size_t pos = frontier[best_fi];
      frontier[best_fi] = frontier.back();
      frontier.pop_back();
      const size_t idx = active_heap_[pos];
      if (fallback == ProgramSelector::kNoChoice) {
        fallback = idx;
      }
      if (prefer(*candidates[idx])) {
        return idx;
      }
      const size_t left = 2 * pos + 1;
      if (left < active_heap_.size()) {
        frontier.push_back(left);
      }
      const size_t right = left + 1;
      if (right < active_heap_.size()) {
        frontier.push_back(right);
      }
    }
    return fallback;
  }

  // Fallback when all active seeds are currently leased: inspects the top
  // `tries` entries of `leased_active_` without mutating state.
  size_t PeekLeasedActive(
      const Candidates& candidates, int tries,
      const std::function<bool(const Program&)>& prefer) const {
    if (leased_active_.empty()) {
      return ProgramSelector::kNoChoice;
    }
    const int limit = std::max(1, tries);
    if (limit == 1 || prefer == nullptr) {
      size_t best = leased_active_[0];
      for (size_t i = 1; i < leased_active_.size(); i++) {
        if (Better(leased_active_[i], best)) {
          best = leased_active_[i];
        }
      }
      return best;
    }
    std::vector<size_t> ranked = leased_active_;
    const size_t k = std::min(ranked.size(), static_cast<size_t>(limit));
    std::partial_sort(ranked.begin(),
                      ranked.begin() + static_cast<ptrdiff_t>(k), ranked.end(),
                      [this](size_t a, size_t b) { return Better(a, b); });
    for (size_t i = 0; i < k; i++) {
      if (prefer(*candidates[ranked[i]])) {
        return ranked[i];
      }
    }
    return ranked[0];
  }

  size_t PeekUniformFallback(
      const Candidates& candidates, Random& rng, int tries,
      const std::function<bool(const Program&)>& prefer) const {
    const size_t n = candidates.size();
    if (n == 0) {
      return ProgramSelector::kNoChoice;
    }
    const int limit = std::max(1, tries);
    size_t fallback = ProgramSelector::kNoChoice;
    for (int i = 0; i < limit; i++) {
      const size_t candidate = rng.Index(n);
      if (fallback == ProgramSelector::kNoChoice) {
        fallback = candidate;
      }
      if (prefer == nullptr || prefer(*candidates[candidate])) {
        return candidate;
      }
    }
    return fallback;
  }

  void CommitPick(const Candidates& candidates, size_t chosen,
                  bool acquire_lease) {
    candidates[chosen]->age += 1.0;
    if (acquire_lease) {
      SeedInfo& info = seeds_[chosen];
      if (info.lease_count == 0) {
        leased_distinct_++;
        if (info.heap_pos != kNotIndexed) {
          HeapRemove(chosen);
        }
      }
      info.lease_count++;
      if (info.energy > 0.0 && info.leased_pos == kNotIndexed) {
        LeasedInsert(chosen);
      }
    }
    RefreshEnergy(chosen);
  }

  Params params_;
  std::vector<double> rarity_weights_;
  std::vector<uint16_t> counts_;
  std::vector<SeedInfo> seeds_;
  std::vector<size_t> active_heap_;
  std::vector<size_t> leased_active_;
  size_t leased_distinct_ = 0;
  std::unordered_map<uint32_t, std::vector<size_t>> edge_trackers_;
  uint64_t affected_stamp_ = 0;
  bool has_high_count_targets_ = false;
  std::deque<size_t> cold_queue_;
  std::vector<size_t> target_programs_;
  // Change-based fuzzing: the favored programs of each reached target feature,
  // and the reached target features in the order of their first program.
  std::unordered_map<uint32_t, std::vector<size_t>> favored_;
  std::vector<uint32_t> reached_targets_;
  // The imported seeds, and how many of them reach each target feature.
  size_t imports_ = 0;
  std::unordered_map<uint32_t, uint32_t> import_target_hits_;
};

class RaritySeeds final : public ProgramSelector {
 public:
  explicit RaritySeeds(const RarityState::Params& params)
      : state_(std::make_shared<RarityState>(params)) {}
  std::string_view name() const override { return "rarity"; }
  bool WantsHitEdges() const override { return true; }
  void OnAdded(const Candidates& candidates, size_t index) override {
    state_->OnAdded(candidates, index);
  }
  void OnCrashEdges(const std::vector<uint32_t>& edges) override {
    state_->RecordCrashEdges(edges);
  }
  void OnOutcome(const Program& picked,
                 const SelectionOutcome& outcome) override {
    state_->OnOutcome(picked, outcome.interesting);
  }
  size_t Pick(const Candidates& candidates, Random& rng,
              const Filter& /*eligible*/) override {
    return state_->PickPreferred(candidates, rng, /*tries=*/1, nullptr,
                                 /*acquire_lease=*/false);
  }
  size_t PickRelated(const Candidates& candidates, size_t anchor, Random& rng,
                     const Filter& /*eligible*/) override {
    return state_->PickPartner(candidates, anchor, rng);
  }
  JsonObject Params() const override { return state_->ParamsJson(); }
  std::shared_ptr<RarityState> state() const { return state_; }

 private:
  std::shared_ptr<RarityState> state_;
};

// The partner side of "rarity": shares the seed strategy's state (counts and
// ages) when the seeds use "rarity" too, otherwise keeps its own.
class RarityPartners final : public ProgramSelector {
 public:
  explicit RarityPartners(const RarityState::Params& params)
      : own_(std::make_shared<RarityState>(params)) {}
  std::string_view name() const override { return "rarity-partner"; }
  bool WantsHitEdges() const override { return true; }
  void AttachSeedStrategy(ProgramSelector* seeds) override {
    // No RTTI in V8 builds: the name identifies the class.
    if (seeds != nullptr && seeds->name() == "rarity") {
      shared_ = static_cast<RaritySeeds*>(seeds)->state();
    } else if (seeds != nullptr && seeds->name() == "rarity-partner") {
      shared_ = static_cast<RarityPartners*>(seeds)->own_;
    }
  }
  void OnAdded(const Candidates& candidates, size_t index) override {
    if (shared_ == nullptr) {
      own_->OnAdded(candidates, index);
    }
  }
  void OnCrashEdges(const std::vector<uint32_t>& edges) override {
    if (shared_ == nullptr) {
      own_->RecordCrashEdges(edges);
    }
  }
  size_t Pick(const Candidates& candidates, Random& rng,
              const Filter& /*eligible*/) override {
    return rng.Index(candidates.size());
  }
  size_t PickRelated(const Candidates& candidates, size_t anchor, Random& rng,
                     const Filter& /*eligible*/) override {
    return (shared_ != nullptr ? shared_ : own_)
        ->PickPartner(candidates, anchor, rng);
  }
  JsonObject Params() const override {
    JsonObject o = own_->ParamsJson();
    o.Add("shares_seed_state", shared_ != nullptr);
    return o;
  }

 private:
  std::shared_ptr<RarityState> own_;
  std::shared_ptr<RarityState> shared_;
};

// Returns the `RarityState` owned by `selector`, or nullptr for other
// strategies. No RTTI in V8 builds: the strategy name identifies the class.
RarityState* RarityStateOf(ProgramSelector* selector) {
  if (selector != nullptr && selector->name() == "rarity") {
    return static_cast<RaritySeeds*>(selector)->state().get();
  }
  return nullptr;
}

}  // namespace

// Seeds and partners share one registry (both pick corpus programs); the
// seed strategies ignore the anchor and the partner strategy has a plain
// Pick() too, so either can be configured at either point.
template <>
void RegisterBuiltinSelectors<const Program>(
    SelectorRegistry<const Program>& registry) {
  using Entry = SelectorRegistry<const Program>::Entry;
  registry.Register(Entry{
      "uniform",
      "every program equally likely",
      {},
      [](const JsonValue&, std::string*) -> std::unique_ptr<ProgramSelector> {
        return std::make_unique<UniformSeeds>();
      }});
  registry.Register(
      Entry{"recency",
            "some picks from the newest part of the corpus, the rest uniform",
            RecencySeeds::Specs(),
            [](const JsonValue& params,
               std::string*) -> std::unique_ptr<ProgramSelector> {
              return std::make_unique<RecencySeeds>(params);
            }});
  registry.Register(Entry{"tournament",
                          "least mutated of a few random candidates",
                          TournamentSeeds::Specs(),
                          [](const JsonValue& params,
                             std::string*) -> std::unique_ptr<ProgramSelector> {
                            return std::make_unique<TournamentSeeds>(params);
                          }});
  registry.Register(
      Entry{"productive",
            "most productive of a few random candidates (ties: least mutated)",
            ProductiveSeeds::Specs(),
            [](const JsonValue& params,
               std::string*) -> std::unique_ptr<ProgramSelector> {
              return std::make_unique<ProductiveSeeds>(params);
            }});
  registry.Register(
      Entry{"uniform-partner",
            "a uniform pick other than the anchor (the partner strategy)",
            UniformPartners::Specs(),
            [](const JsonValue& params,
               std::string*) -> std::unique_ptr<ProgramSelector> {
              return std::make_unique<UniformPartners>(params);
            }});
  registry.Register(
      Entry{"rarity",
            "Fuzzilli's RarityCorpus: highest energy (rare edges, age, size) "
            "as seed, tournament as partner",
            RarityState::Specs(),
            [](const JsonValue& params,
               std::string* error) -> std::unique_ptr<ProgramSelector> {
              RarityState::Params p = RarityState::ReadParams(params);
              if (!RarityState::Validate(p, error)) {
                return nullptr;
              }
              return std::make_unique<RaritySeeds>(p);
            }});
  registry.Register(
      Entry{"rarity-partner",
            "partner strategy of \"rarity\": best of a few random programs by "
            "energy (shares the seed state)",
            RarityState::Specs(),
            [](const JsonValue& params,
               std::string* error) -> std::unique_ptr<ProgramSelector> {
              RarityState::Params p = RarityState::ReadParams(params);
              if (!RarityState::Validate(p, error)) {
                return nullptr;
              }
              return std::make_unique<RarityPartners>(p);
            }});
  registry.SetDefault("uniform");
}

// ---------------------------------------------------------------------------
// SeedLease

SeedLease::SeedLease(Corpus* corpus, ProgramPtr program, size_t index)
    : corpus_(corpus), program_(std::move(program)), index_(index) {}

SeedLease::~SeedLease() { Release(); }

SeedLease::SeedLease(SeedLease&& other) noexcept
    : corpus_(other.corpus_),
      program_(std::move(other.program_)),
      index_(other.index_) {
  other.corpus_ = nullptr;
  other.index_ = ProgramSelector::kNoChoice;
}

SeedLease& SeedLease::operator=(SeedLease&& other) noexcept {
  if (this != &other) {
    Release();
    corpus_ = other.corpus_;
    program_ = std::move(other.program_);
    index_ = other.index_;
    other.corpus_ = nullptr;
    other.index_ = ProgramSelector::kNoChoice;
  }
  return *this;
}

void SeedLease::Release() {
  if (corpus_ != nullptr && index_ != ProgramSelector::kNoChoice) {
    corpus_->ReleaseLease(index_);
  }
  corpus_ = nullptr;
  index_ = ProgramSelector::kNoChoice;
  program_.reset();
}

// ---------------------------------------------------------------------------
// Corpus

Corpus::Corpus(std::unique_ptr<ProgramSelector> seeds,
               std::unique_ptr<ProgramSelector> partners)
    : seeds_(std::move(seeds)),
      partners_(std::move(partners)),
      seed_stats_(std::string(seeds_->name())) {
  partners_->AttachSeedStrategy(seeds_.get());
}

bool Corpus::WantsHitEdges() const {
  return seeds_->WantsHitEdges() || partners_->WantsHitEdges();
}

void Corpus::RecordCrashEdges(const std::vector<uint32_t>& edges) {
  std::scoped_lock<std::mutex> lock(mutex_);
  seeds_->OnCrashEdges(edges);
  partners_->OnCrashEdges(edges);
  EvictExcessTreesLocked();
}

Corpus::~Corpus() = default;

ProgramPtr Corpus::Add(std::shared_ptr<Program> program) {
  std::scoped_lock<std::mutex> lock(mutex_);
  program->id = static_cast<uint32_t>(programs_.size() + 1);
  if (program->added_at_ms == 0) {
    program->added_at_ms = WallClockNowMs();
  }
  total_nodes_.fetch_add(program->node_count(), std::memory_order_relaxed);
  total_edges_.fetch_add(program->edges.size(), std::memory_order_relaxed);
  ProgramPtr published = std::move(program);
  programs_.push_back(published);
  candidates_.push_back(published.get());
  tree_evicted_.push_back(0);
  resident_trees_++;
  seeds_->OnAdded(candidates_, candidates_.size() - 1);
  partners_->OnAdded(candidates_, candidates_.size() - 1);
  std::vector<uint32_t>().swap(published->hit_edges);
  EvictExcessTreesLocked();
  return published;
}

ProgramPtr Corpus::Pick(Random& rng) {
  return LeasePreferredInternal(rng, 1, nullptr, /*acquire_lease=*/false)
      .program();
}

ProgramPtr Corpus::PickPreferred(
    Random& rng, int tries, const std::function<bool(const Program&)>& prefer) {
  return LeasePreferredInternal(rng, tries, prefer, /*acquire_lease=*/false)
      .program();
}

SeedLease Corpus::Lease(Random& rng) {
  return LeasePreferredInternal(rng, 1, nullptr, /*acquire_lease=*/true);
}

SeedLease Corpus::LeasePreferred(
    Random& rng, int tries, const std::function<bool(const Program&)>& prefer) {
  return LeasePreferredInternal(rng, tries, prefer, /*acquire_lease=*/true);
}

SeedLease Corpus::LeasePreferredInternal(
    Random& rng, int tries, const std::function<bool(const Program&)>& prefer,
    bool acquire_lease) {
  std::scoped_lock<std::mutex> lock(mutex_);
  if (programs_.empty()) {
    return {};
  }
  seed_stats_.stats().invocations++;
  size_t index = ProgramSelector::kNoChoice;
  RarityState* rarity = RarityStateOf(seeds_.get());
  if (rarity != nullptr) {
    index =
        rarity->PickPreferred(candidates_, rng, tries, prefer, acquire_lease);
  } else {
    for (int i = 0; i < std::max(1, tries); i++) {
      const size_t candidate =
          seeds_->Pick(candidates_, rng, [](const Program&) { return true; });
      if (candidate == ProgramSelector::kNoChoice) {
        break;
      }
      if (index == ProgramSelector::kNoChoice) {
        index = candidate;
      }
      if (prefer == nullptr || prefer(*programs_[candidate])) {
        index = candidate;
        break;
      }
    }
  }
  if (index == ProgramSelector::kNoChoice) {
    return {};
  }
  NoteTreeAccessedLocked(index);
  const ProgramPtr& picked = programs_[index];
  picked->picks.fetch_add(1, std::memory_order_relaxed);
  seed_stats_.stats().produced++;
  if (acquire_lease && rarity != nullptr) {
    return SeedLease(this, picked, index);
  }
  return SeedLease(nullptr, picked, ProgramSelector::kNoChoice);
}

void Corpus::ReleaseLease(size_t index) {
  std::scoped_lock<std::mutex> lock(mutex_);
  if (RarityState* rarity = RarityStateOf(seeds_.get())) {
    rarity->ReleaseLease(index);
  }
  EvictExcessTreesLocked();
}

void Corpus::NoteTreeAccessedLocked(size_t index) const {
  if (index >= tree_evicted_.size() || tree_evicted_[index] == 0) {
    return;
  }
  tree_evicted_[index] = 0;
  resident_trees_++;
  if (RarityState* rarity = RarityStateOf(seeds_.get())) {
    if (rarity->IsZeroEnergy(index)) {
      rarity->RequeueCold(index);
    }
  }
}

void Corpus::EvictExcessTreesLocked() {
  RarityState* rarity = RarityStateOf(seeds_.get());
  if (rarity == nullptr || rarity->params().max_memory_trees <= 0) {
    return;
  }
  const size_t cap = static_cast<size_t>(rarity->params().max_memory_trees);
  const size_t budget = rarity->ColdQueueSize();
  for (size_t tried = 0; tried < budget && resident_trees_ > cap; tried++) {
    size_t idx = ProgramSelector::kNoChoice;
    if (!rarity->PopCold(&idx)) {
      break;
    }
    if (idx >= programs_.size() || tree_evicted_[idx] != 0) {
      continue;
    }
    if (rarity->IsLeased(idx) || programs_[idx].use_count() != 1) {
      rarity->RequeueCold(idx);
      continue;
    }
    // Synchronize with the last external `shared_ptr` release before dropping
    // the tree.
    std::atomic_thread_fence(std::memory_order_acquire);
    if (programs_[idx]->has_tree()) {
      programs_[idx]->ReleaseTree();
    }
    tree_evicted_[idx] = 1;
    if (resident_trees_ > 0) {
      resident_trees_--;
    }
  }
}

ProgramPtr Corpus::PickPartner(const Program& parent, Random& rng) {
  std::scoped_lock<std::mutex> lock(mutex_);
  // The anchor is the parent's position in the corpus; an intermediate
  // program of a mutation chain is not in the corpus, so nothing is
  // excluded for it (anchor past the end).
  size_t anchor = candidates_.size();
  if (parent.id != 0 && parent.id <= programs_.size() &&
      programs_[parent.id - 1].get() == &parent) {
    anchor = parent.id - 1;
  }
  size_t index = partners_->PickRelated(candidates_, anchor, rng,
                                        [](const Program&) { return true; });
  if (index == ProgramSelector::kNoChoice) {
    return nullptr;
  }
  NoteTreeAccessedLocked(index);
  return programs_[index];
}

void Corpus::OnResult(const Program& parent, bool interesting) {
  if (interesting) {
    parent.children.fetch_add(1, std::memory_order_relaxed);
    seed_stats_.stats().interesting++;
  }
  std::scoped_lock<std::mutex> lock(mutex_);
  SelectionOutcome outcome;
  outcome.kind = SelectionOutcome::Kind::kSucceeded;
  outcome.interesting = interesting;
  seeds_->OnOutcome(parent, outcome);
}

size_t Corpus::size() const {
  std::scoped_lock<std::mutex> lock(mutex_);
  return programs_.size();
}

size_t Corpus::active_size() const {
  std::scoped_lock<std::mutex> lock(mutex_);
  if (const RarityState* rarity = RarityStateOf(seeds_.get())) {
    return rarity->active_size();
  }
  return programs_.size();
}

size_t Corpus::leased_size() const {
  std::scoped_lock<std::mutex> lock(mutex_);
  if (const RarityState* rarity = RarityStateOf(seeds_.get())) {
    return rarity->leased_size();
  }
  return 0;
}

size_t Corpus::resident_trees() const {
  std::scoped_lock<std::mutex> lock(mutex_);
  size_t count = 0;
  for (const ProgramPtr& program : programs_) {
    if (program->has_tree()) {
      count++;
    }
  }
  return count;
}

bool Corpus::CheckInvariantsForTest(std::string* error) const {
  std::scoped_lock<std::mutex> lock(mutex_);
  if (const RarityState* rarity = RarityStateOf(seeds_.get())) {
    return rarity->CheckInvariants(error);
  }
  return true;
}

ProgramPtr Corpus::Get(uint32_t id) const {
  std::scoped_lock<std::mutex> lock(mutex_);
  if (id == 0 || id > programs_.size()) {
    return nullptr;
  }
  NoteTreeAccessedLocked(static_cast<size_t>(id - 1));
  return programs_[id - 1];
}

std::vector<ProgramPtr> Corpus::Snapshot() const {
  std::scoped_lock<std::mutex> lock(mutex_);
  for (size_t i = 0; i < programs_.size(); i++) {
    NoteTreeAccessedLocked(i);
  }
  return programs_;
}

}  // namespace redline
