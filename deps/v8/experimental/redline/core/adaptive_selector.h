// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_CORE_ADAPTIVE_SELECTOR_H_
#define V8_EXPERIMENTAL_REDLINE_CORE_ADAPTIVE_SELECTOR_H_

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "experimental/redline/base/time_util.h"
#include "experimental/redline/core/selection.h"

namespace redline {

// A weighted draw whose weights follow the candidates' recent yield. Every
// `window_sec` seconds (or every `window` picks, if set), the strategy reads
// each candidate's ComponentStats, turns the increments since the previous
// update into a yield (with `reward: edges`, the default: new edges per target
// execution spent; with `reward: adds`: corpus adds per executed result),
// smooths it with `prior_executions` pseudo-executions at the overall rate,
// folds it into an exponential moving average (`ema`), and sets
//
//   weight = base * clamp(rate / mean rate, min_factor, max_factor)
//
// so productive candidates are drawn more often while every enabled one keeps
// at least `min_factor` of its base weight (the yield of a candidate changes as
// the corpus grows, so none is ever starved). The base weights are the
// built-in ones, overridden by `weights` like the "weighted" strategy.
//
// Thread safety: shared by all workers; the state is guarded by a mutex (one
// pick per mutation, negligible next to an execution).
//
// T must provide name(), weight() and stats() (a ComponentStats).
template <typename T>
class AdaptiveSelector final : public Selector<T> {
 public:
  using typename Selector<T>::Candidates;
  using typename Selector<T>::Filter;
  using Selector<T>::kNoChoice;

  static constexpr std::string_view kName = "adaptive";

  struct Settings {
    double window = 0;        // picks between updates; 0: off
    double window_sec = 600;  // seconds between updates; 0: off
    bool reward_edges =
        true;  // edges per spent execution (else adds per execution)
    double ema = 0.3;
    double prior_executions = 30000;
    double min_factor = 0.25;
    double max_factor = 4.0;
  };

  static std::vector<ParamSpec> Specs() {
    return {
        {"weights", ParamSpec::Type::kObject, "{}",
         "{\"Name\": number, ...}: overrides the built-in base weights, 0 "
         "disables"},
        {"window", ParamSpec::Type::kNumber, "0",
         "picks between two weight updates (0: off)"},
        {"window_sec", ParamSpec::Type::kNumber, "600",
         "seconds between two weight updates (0: off)"},
        {"reward", ParamSpec::Type::kString, "\"edges\"",
         "\"edges\": new edges per execution spent; \"adds\": corpus adds per "
         "execution"},
        {"ema", ParamSpec::Type::kNumber, "0.3",
         "weight of the newest window in the moving average"},
        {"prior_executions", ParamSpec::Type::kNumber, "30000",
         "pseudo-executions at the overall rate added to every candidate's "
         "window"},
        {"min_factor", ParamSpec::Type::kNumber, "0.25",
         "lowest multiple of the base weight"},
        {"max_factor", ParamSpec::Type::kNumber, "4",
         "highest multiple of the base weight"},
    };
  }

  static Settings ReadParams(const JsonValue& params) {
    Settings p;
    p.window = params.GetNumber("window", p.window);
    p.window_sec = params.GetNumber("window_sec", p.window_sec);
    p.reward_edges = params.GetString("reward", "edges") != "adds";
    p.ema = params.GetNumber("ema", p.ema);
    p.prior_executions =
        params.GetNumber("prior_executions", p.prior_executions);
    p.min_factor = params.GetNumber("min_factor", p.min_factor);
    p.max_factor = params.GetNumber("max_factor", p.max_factor);
    return p;
  }

  static bool Validate(const Settings& p, std::string* error) {
    if (p.window < 0 || p.window_sec < 0 ||
        (p.window == 0 && p.window_sec == 0) || p.ema <= 0 || p.ema > 1 ||
        p.prior_executions < 0 || p.min_factor <= 0 ||
        p.max_factor < p.min_factor) {
      *error =
          "need window >= 0, window_sec >= 0 (not both 0), 0 < ema <= 1, "
          "prior_executions >= 0, "
          "0 < min_factor <= max_factor";
      return false;
    }
    return true;
  }

  AdaptiveSelector(std::string kind,
                   std::vector<std::pair<std::string, double>> overrides,
                   const Settings& params)
      : kind_(std::move(kind)),
        overrides_(std::move(overrides)),
        params_(params),
        last_update_us_(NowUs()) {}

  // Test hook: replaces the monotonic clock (microseconds).
  void SetClockForTesting(std::function<uint64_t()> clock) {
    std::scoped_lock<std::mutex> lock(mutex_);
    clock_ = std::move(clock);
    last_update_us_ = NowUs();
  }

  std::string_view name() const override { return kName; }

  bool Initialize(const Candidates& candidates, std::string* error) override {
    std::scoped_lock<std::mutex> lock(mutex_);
    candidates_ = candidates;
    base_.clear();
    for (const T* c : candidates) {
      base_.push_back(c->weight());
    }
    for (const auto& [name, weight] : overrides_) {
      if (!SetBaseLocked(name, weight)) {
        *error = "unknown " + kind_ + " in weights: " + name;
        return false;
      }
    }
    factor_.assign(candidates.size(), 1.0);
    rate_.assign(candidates.size(), -1.0);  // no estimate yet
    last_cost_.assign(candidates.size(), 0);
    last_gain_.assign(candidates.size(), 0);
    return true;
  }

  size_t Pick(const Candidates& candidates, Random& rng,
              const Filter& eligible) override {
    std::scoped_lock<std::mutex> lock(mutex_);
    ++picks_since_update_;
    const bool picks_due =
        params_.window > 0 &&
        picks_since_update_ >= static_cast<uint64_t>(params_.window);
    const bool time_due = params_.window_sec > 0 &&
                          NowUs() - last_update_us_ >=
                              static_cast<uint64_t>(params_.window_sec * 1e6);
    if (picks_due || time_due) {
      UpdateLocked();
    }
    std::vector<double> weights(candidates.size(), 0.0);
    double total = 0;
    for (size_t i = 0; i < candidates.size() && i < base_.size(); i++) {
      if (base_[i] <= 0 || !eligible(*candidates[i])) {
        continue;
      }
      weights[i] = base_[i] * factor_[i];
      total += weights[i];
    }
    if (total <= 0) {
      return kNoChoice;
    }
    return rng.Weighted(weights);
  }

  double WeightOf(const T& candidate) const override {
    std::scoped_lock<std::mutex> lock(mutex_);
    for (size_t i = 0; i < candidates_.size(); i++) {
      if (candidates_[i] == &candidate) {
        return base_[i] * factor_[i];
      }
    }
    return 0.0;
  }

  bool SetWeight(std::string_view candidate_name, double weight) override {
    std::scoped_lock<std::mutex> lock(mutex_);
    return SetBaseLocked(candidate_name, weight);
  }

  JsonObject Params() const override {
    std::scoped_lock<std::mutex> lock(mutex_);
    JsonObject weights;
    for (size_t i = 0; i < candidates_.size(); i++) {
      weights.Add(candidates_[i]->name(), base_[i]);
    }
    JsonObject params;
    params.AddRaw("weights", weights.ToString());
    params.Add("window", params_.window).Add("window_sec", params_.window_sec);
    params.Add("reward", params_.reward_edges ? "edges" : "adds")
        .Add("ema", params_.ema);
    params.Add("prior_executions", params_.prior_executions);
    params.Add("min_factor", params_.min_factor)
        .Add("max_factor", params_.max_factor);
    return params;
  }

 private:
  uint64_t NowUs() const { return clock_ ? clock_() : MonotonicNowUs(); }

  bool SetBaseLocked(std::string_view candidate_name, double weight) {
    for (size_t i = 0; i < candidates_.size(); i++) {
      if (candidates_[i]->name() == candidate_name) {
        base_[i] = weight;
        return true;
      }
    }
    return false;
  }

  // The yield of a candidate over a window is gain / cost. With
  // reward_edges: gain = new edges of its corpus programs, cost = every
  // target execution it caused (`spent`, or `executed` if nothing was
  // spent). Otherwise: gain = corpus adds, cost = executed results.
  void UpdateLocked() {
    picks_since_update_ = 0;
    last_update_us_ = NowUs();
    const size_t n = candidates_.size();
    std::vector<uint64_t> cost(n), gain(n);
    uint64_t total_cost = 0, total_gain = 0;
    for (size_t i = 0; i < n; i++) {
      const auto& stats = candidates_[i]->stats();
      const uint64_t executed = stats.executed.load(std::memory_order_relaxed);
      const uint64_t spent = stats.spent.load(std::memory_order_relaxed);
      uint64_t c = executed;
      uint64_t g = stats.interesting.load(std::memory_order_relaxed);
      if (params_.reward_edges) {
        c = spent > 0 ? spent : executed;
        g = stats.new_edges.load(std::memory_order_relaxed);
      }
      cost[i] = c - std::min(c, last_cost_[i]);
      gain[i] = g - std::min(g, last_gain_[i]);
      last_cost_[i] = c;
      last_gain_[i] = g;
      if (base_[i] > 0) {
        total_cost += cost[i];
        total_gain += gain[i];
      }
    }
    if (total_cost == 0) {
      return;
    }
    const double overall =
        static_cast<double>(total_gain) / static_cast<double>(total_cost);
    double sum = 0;
    int active = 0;
    for (size_t i = 0; i < n; i++) {
      if (base_[i] <= 0) {
        continue;
      }
      double window_rate =
          (static_cast<double>(gain[i]) + params_.prior_executions * overall) /
          (static_cast<double>(cost[i]) + params_.prior_executions);
      rate_[i] = rate_[i] < 0
                     ? window_rate
                     : (1 - params_.ema) * rate_[i] + params_.ema * window_rate;
      sum += rate_[i];
      active++;
    }
    const double mean = active > 0 ? sum / active : 0;
    for (size_t i = 0; i < n; i++) {
      factor_[i] = (base_[i] <= 0 || mean <= 0)
                       ? 1.0
                       : std::clamp(rate_[i] / mean, params_.min_factor,
                                    params_.max_factor);
    }
  }

  std::string kind_;
  std::vector<std::pair<std::string, double>> overrides_;
  Settings params_;
  mutable std::mutex mutex_;
  Candidates candidates_;
  std::vector<double> base_;
  std::vector<double> factor_;
  std::vector<double> rate_;
  std::vector<uint64_t> last_cost_;
  std::vector<uint64_t> last_gain_;
  uint64_t picks_since_update_ = 0;
  std::function<uint64_t()> clock_;  // empty: MonotonicNowUs
  uint64_t last_update_us_ = 0;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORE_ADAPTIVE_SELECTOR_H_
