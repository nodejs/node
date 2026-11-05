// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/corpus/corpus.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/corpus/feature_weights.h"

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

// --- rarity -----------------------------------------------------------------
//
// Fuzzilli's RarityCorpus (Sources/Fuzzilli/Corpus/RarityCorpus.swift), on
// Redline's shared corpus (so it is also the --sharedCorpus variant):
//
//   energy(p) = rarity(p) / ((age(p) + 1)^age_exponent *
//                            (1 + ln(size(p))^size_exponent) *
//                            (1 + (exec_time_ms(p) /
//                            time_unit_ms)^time_exponent))
//   rarity(p) = sum over the tracked edges e of p with count(e) <
//   rare_edge_limit
//               (or IsTarget(e)) of w(e) / count(e)^rarity_exponent
//
// count(e) is the number of corpus programs that reach e deterministically.
// Non-target features saturate at rare_edge_limit. Target features saturate
// at UINT16_MAX and stay active in rarity(p). A program tracks at most
// max_tracked_edges of its features. Target features come first, followed by
// the highest weighted rarity. The seed pick takes the program with the
// highest energy and ages it by 1. The partner pick is a tournament of
// partner_pool programs with non-zero energy and ages the winner by
// partner_age_increment. Energies only decrease as counts and ages grow, so a
// lazy max-heap is exact. A popped entry whose energy dropped is re-inserted
// with its current value.
//
// Change-based fuzzing: a share target_pick of the seed and partner picks
// takes a program that reaches a target feature. Each target feature keeps its
// target_favored cheapest programs (size times execution time, the "top
// rated" entries of AFL). The pick takes the rarest of target_tournament
// random reached target features, and its least used favored program. It
// skips the common target features: those that more than
// target_common_share of the imported seeds reach. They run in most
// compilations and do not tell the programs apart. If only common target
// features were reached, the pick is a normal one.
// Thus the picks go to the rare parts of the change, and the picked programs
// stay small. A tournament on energy prefers the large programs that reach
// many targets, and the corpus then grows big and slow.
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
    // Change-based fuzzing (corpus/feature_weights.h): the probability that a
    // seed or partner pick takes a program that reaches a target.
    double target_pick = 0.5;
    int target_tournament = 4;
    // Change-based fuzzing: the number of cheapest programs that each target
    // feature keeps for the target picks (0: a tournament on energy instead).
    int target_favored = 4;
    // Change-based fuzzing: a target feature that more than this share of the
    // imported seeds reach is common, and the target picks skip it (0: off).
    double target_common_share = 0.25;
    double time_exponent = 0.5;
    double time_unit_ms = 10.0;
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
    for (double x : {p.age_exponent, p.size_exponent, p.rarity_exponent,
                     p.partner_age_increment, p.time_exponent}) {
      if (!std::isfinite(x) || x < 0.0) {
        *error = "exponents and partner_age_increment must be finite and >= 0";
        return false;
      }
    }
    return true;
  }

  explicit RarityState(const Params& params) : params_(params) {}

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
    return o;
  }

  void OnAdded(const Candidates& candidates, size_t index) {
    const Program& program = *candidates[index];
    // Without the full edge set (e.g. imports that were not re-run), fall
    // back to the program's new edges.
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
    std::vector<uint32_t> rare;
    for (uint32_t edge : edges) {
      const bool is_target = weights != nullptr && weights->IsTarget(edge);
      const uint16_t cap = is_target ? UINT16_MAX : limit;
      uint16_t& count = counts_[edge];
      if (count < cap) {
        count++;
      }
      if (is_target || count < limit) {
        rare.push_back(edge);
      }
    }
    // Keep target features first so max_tracked_edges never drops them, then
    // sort by highest weighted rarity (rarest first when all weights are 1).
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
    heap_.push({Energy(program), index});
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

  // The cost of a program for the favored lists: its size times its
  // execution time, like the "top rated" entries of AFL.
  double Cost(const Program& program) const {
    const double ms = static_cast<double>(program.exec_time_us) / 1000.0;
    return std::max(1.0, static_cast<double>(program.node_count())) *
           (1.0 + ms / params_.time_unit_ms);
  }

  // Keeps the target_favored cheapest programs that reach `edge`, cheapest
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

  // True if more than target_common_share of the imported seeds reach the
  // target feature `edge`. Such a feature runs in most compilations, so it
  // does not tell the programs apart. The seeds are a sample that the
  // target picks did not bias.
  bool CommonTarget(uint32_t edge) const {
    if (imports_ == 0 || params_.target_common_share <= 0.0) {
      return false;
    }
    auto it = import_target_hits_.find(edge);
    return it != import_target_hits_.end() &&
           static_cast<double>(it->second) >
               params_.target_common_share * static_cast<double>(imports_);
  }

  // A favored program of a reached target feature, other than `anchor`
  // (kNoChoice if none). The feature is the rarest of target_tournament
  // random reached target features that are not common (CommonTarget). The
  // program is the least used one of the feature.
  size_t PickFavored(const Candidates& candidates, Random& rng,
                     size_t anchor) const {
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
    for (size_t index : favored_.at(best)) {
      if (index != anchor && (pick == ProgramSelector::kNoChoice ||
                              candidates[index]->age < candidates[pick]->age)) {
        pick = index;
      }
    }
    return pick;
  }

  // Change-based fuzzing: with probability target_pick, a program that reaches
  // a target (kNoChoice otherwise). With target_favored, a favored program of a
  // rare reached target feature (PickFavored). Otherwise the best of
  // target_tournament random programs that reach a target.
  size_t PickTarget(const Candidates& candidates, Random& rng) {
    if (target_programs_.empty() || !rng.Chance(params_.target_pick)) {
      return ProgramSelector::kNoChoice;
    }
    if (params_.target_favored > 0) {
      const size_t index =
          PickFavored(candidates, rng, ProgramSelector::kNoChoice);
      if (index != ProgramSelector::kNoChoice) {
        candidates[index]->age += 1.0;
      }
      return index;
    }
    size_t best = target_programs_[rng.Index(target_programs_.size())];
    double best_energy = Energy(*candidates[best]);
    for (int i = 1; i < params_.target_tournament; i++) {
      const size_t candidate =
          target_programs_[rng.Index(target_programs_.size())];
      const double energy = Energy(*candidates[candidate]);
      if (energy > best_energy) {
        best = candidate;
        best_energy = energy;
      }
    }
    candidates[best]->age += 1.0;
    return best;
  }

  size_t target_programs() const { return target_programs_.size(); }

  // Fuzzilli's RarityCorpus counts the edges of a reproducible crash as
  // seen once more, which lowers the energy of the programs leading there.
  void RecordCrashEdges(const std::vector<uint32_t>& edges) {
    const uint16_t limit = static_cast<uint16_t>(params_.rare_edge_limit);
    const FeatureWeights* weights = FeatureWeights::Active();
    for (uint32_t edge : edges) {
      if (edge >= counts_.size()) {
        counts_.resize(static_cast<size_t>(edge) + 4096, 0);
      }
      const bool is_target = weights != nullptr && weights->IsTarget(edge);
      const uint16_t cap = is_target ? UINT16_MAX : limit;
      if (counts_[edge] < cap) {
        counts_[edge]++;
      }
    }
  }

  // The contribution of one feature to the rarity of a program: its weight
  // (corpus/feature_weights.h) divided by count^rarity_exponent.
  double Score(uint32_t edge, const FeatureWeights* weights) const {
    const double w = weights != nullptr ? weights->Of(edge) : 1.0;
    return w /
           std::pow(static_cast<double>(std::max<uint16_t>(1, counts_[edge])),
                    params_.rarity_exponent);
  }

  double Energy(const Program& program) const {
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
    double size = std::max(
        1.0, static_cast<double>(program.node_count()) / params_.size_unit);
    double ln_size = std::log(size);
    double size_pen = 1.0 + std::pow(ln_size, params_.size_exponent);
    double time_pen = 1.0;
    if (program.exec_time_us > 0 && params_.time_exponent > 0.0) {
      double time_units = static_cast<double>(program.exec_time_us) /
                          (params_.time_unit_ms * 1000.0);
      time_pen += std::pow(time_units, params_.time_exponent);
    }
    return rarity / (std::pow(program.age + 1.0, params_.age_exponent) *
                     size_pen * time_pen);
  }

  // The seed pick: highest current energy (kNoChoice if every program's
  // energy is 0; the caller then picks uniformly).
  size_t PickSeed(const Candidates& candidates) {
    while (!heap_.empty()) {
      Entry top = heap_.top();
      heap_.pop();
      double current = Energy(*candidates[top.index]);
      if (current <= 0.0) {
        continue;  // never schedulable again: drop it from the heap
      }
      if (current < top.energy * (1.0 - 1e-9)) {
        heap_.push({current, top.index});
        continue;
      }
      const Program& program = *candidates[top.index];
      program.age += 1.0;
      heap_.push({Energy(program), top.index});
      return top.index;
    }
    return ProgramSelector::kNoChoice;
  }

  // The partner pick: the best of `partner_pool` random programs with
  // energy (uniform among all if none has energy left).
  size_t PickPartner(const Candidates& candidates, size_t anchor, Random& rng) {
    size_t n = candidates.size();
    if (n < 2) {
      return ProgramSelector::kNoChoice;
    }
    const bool has_other_target =
        target_programs_.size() > 1 ||
        (target_programs_.size() == 1 && target_programs_[0] != anchor);
    bool use_targets = has_other_target && rng.Chance(params_.target_pick);
    if (use_targets && params_.target_favored > 0) {
      const size_t index = PickFavored(candidates, rng, anchor);
      if (index != ProgramSelector::kNoChoice) {
        candidates[index]->age += params_.partner_age_increment;
        return index;
      }
      // No rare target feature: a normal partner pick.
      use_targets = false;
    }
    size_t best = ProgramSelector::kNoChoice;
    double best_energy = 0.0;
    for (int i = 0, attempts = 0;
         i < params_.partner_pool && attempts < 4 * params_.partner_pool;
         attempts++) {
      size_t candidate =
          use_targets ? target_programs_[rng.Index(target_programs_.size())]
                      : rng.Index(n);
      if (candidate == anchor) {
        continue;
      }
      double energy = Energy(*candidates[candidate]);
      if (energy <= 0.0) {
        continue;
      }
      i++;
      if (best == ProgramSelector::kNoChoice || energy > best_energy) {
        best = candidate;
        best_energy = energy;
      }
    }
    if (best == ProgramSelector::kNoChoice) {
      for (int attempt = 0; attempt < 4 && best == ProgramSelector::kNoChoice;
           attempt++) {
        size_t candidate = rng.Index(n);
        if (candidate != anchor) {
          best = candidate;
        }
      }
      return best;
    }
    candidates[best]->age += params_.partner_age_increment;
    return best;
  }

 private:
  struct Entry {
    double energy;
    size_t index;
    bool operator<(const Entry& other) const {
      return energy != other.energy ? energy < other.energy
                                    : index > other.index;
    }
  };
  Params params_;
  std::vector<uint16_t> counts_;
  std::priority_queue<Entry> heap_;
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
  size_t Pick(const Candidates& candidates, Random& rng,
              const Filter& /*eligible*/) override {
    size_t index = state_->PickTarget(candidates, rng);
    if (index == kNoChoice) {
      index = state_->PickSeed(candidates);
    }
    return index != kNoChoice ? index : rng.Index(candidates.size());
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
  seeds_->OnAdded(candidates_, candidates_.size() - 1);
  partners_->OnAdded(candidates_, candidates_.size() - 1);
  std::vector<uint32_t>().swap(published->hit_edges);
  return published;
}

ProgramPtr Corpus::Pick(Random& rng) {
  std::scoped_lock<std::mutex> lock(mutex_);
  if (programs_.empty()) {
    return nullptr;
  }
  seed_stats_.stats().invocations++;
  size_t index =
      seeds_->Pick(candidates_, rng, [](const Program&) { return true; });
  if (index == ProgramSelector::kNoChoice) {
    return nullptr;
  }
  const ProgramPtr& picked = programs_[index];
  picked->picks.fetch_add(1, std::memory_order_relaxed);
  seed_stats_.stats().produced++;
  return picked;
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
  return index == ProgramSelector::kNoChoice ? nullptr : programs_[index];
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

ProgramPtr Corpus::Get(uint32_t id) const {
  std::scoped_lock<std::mutex> lock(mutex_);
  if (id == 0 || id > programs_.size()) {
    return nullptr;
  }
  return programs_[id - 1];
}

std::vector<ProgramPtr> Corpus::Snapshot() const {
  std::scoped_lock<std::mutex> lock(mutex_);
  return programs_;
}

}  // namespace redline
