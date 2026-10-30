// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_CORE_WEIGHTED_SELECTOR_H_
#define V8_EXPERIMENTAL_REDLINE_CORE_WEIGHTED_SELECTOR_H_

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "experimental/redline/core/selection.h"

namespace redline {

// The default strategy for mutators and generators: one weighted random draw
// among the eligible candidates ("flat but configurable"). Every candidate
// starts with its built-in weight (T::weight()); the `weights` parameter
// overrides them by name, 0 disables a candidate. Feedback is ignored.
//
// This is Redline's historical pick, kept draw-for-draw identical: the
// weights are laid out in candidate order, non-eligible candidates get 0,
// and one Random::Weighted() call chooses.
//
// T must provide `const std::string& name() const` and `double weight() const`.
template <typename T>
class WeightedSelector final : public Selector<T> {
 public:
  using typename Selector<T>::Candidates;
  using typename Selector<T>::Filter;
  using Selector<T>::kNoChoice;

  static constexpr std::string_view kName = "weighted";

  static std::vector<ParamSpec> Specs() {
    return {{"weights", ParamSpec::Type::kObject, "{}",
             "{\"Name\": number, ...}: overrides the built-in weights, 0 "
             "disables"}};
  }

  // `kind` names the candidates in error messages ("mutator", "generator").
  WeightedSelector(std::string kind,
                   std::vector<std::pair<std::string, double>> overrides)
      : kind_(std::move(kind)), overrides_(std::move(overrides)) {}

  std::string_view name() const override { return kName; }

  bool Initialize(const Candidates& candidates, std::string* error) override {
    candidates_ = candidates;
    weights_.clear();
    weights_.reserve(candidates.size());
    for (const T* c : candidates) {
      weights_.push_back(c->weight());
    }
    for (const auto& [name, weight] : overrides_) {
      if (!SetWeight(name, weight)) {
        *error = "unknown " + kind_ + " in weights: " + name;
        return false;
      }
    }
    return true;
  }

  size_t Pick(const Candidates& candidates, Random& rng,
              const Filter& eligible) override {
    std::vector<double> weights(candidates.size(), 0.0);
    double total = 0;
    for (size_t i = 0; i < candidates.size(); i++) {
      if (weights_[i] <= 0) {
        continue;
      }
      if (!eligible(*candidates[i])) {
        continue;
      }
      weights[i] = weights_[i];
      total += weights_[i];
    }
    if (total <= 0) {
      return kNoChoice;
    }
    return rng.Weighted(weights);
  }

  double WeightOf(const T& candidate) const override {
    for (size_t i = 0; i < candidates_.size(); i++) {
      if (candidates_[i] == &candidate) {
        return weights_[i];
      }
    }
    return 0.0;
  }

  bool SetWeight(std::string_view candidate_name, double weight) override {
    for (size_t i = 0; i < candidates_.size(); i++) {
      if (candidates_[i]->name() == candidate_name) {
        weights_[i] = weight;
        return true;
      }
    }
    return false;
  }

  JsonObject Params() const override {
    JsonObject weights;
    for (size_t i = 0; i < candidates_.size(); i++) {
      weights.Add(candidates_[i]->name(), weights_[i]);
    }
    JsonObject params;
    params.AddRaw("weights", weights.ToString());
    return params;
  }

 private:
  std::string kind_;
  std::vector<std::pair<std::string, double>> overrides_;
  Candidates candidates_;
  std::vector<double> weights_;  // effective, by candidate index
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORE_WEIGHTED_SELECTOR_H_
