// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/core/swarm.h"

#include <cmath>
#include <cstddef>

#include "experimental/redline/core/random.h"
#include "experimental/redline/core/tunable.h"

namespace redline {

namespace {

REDLINE_TUNABLE(kSwarmEpochExecs, "selection.swarm.epoch_execs", 5000, 100,
                100000,
                "swarm testing: fuzz executions of a worker before it draws a "
                "new mask");
REDLINE_TUNABLE(kSwarmKeepProb, "selection.swarm.keep_prob", 0.45, 0.1, 1.0,
                "swarm testing: chance that a worker keeps a generator that "
                "is not core");
REDLINE_TUNABLE(kSwarmMinActiveGenerators,
                "selection.swarm.min_active_generators", 12, 4, 64,
                "swarm testing: the minimum number of kept generators that are "
                "not core");
REDLINE_TUNABLE(kSwarmMaxBoost, "selection.swarm.max_boost", 4.0, 1.0, 16.0,
                "swarm testing: a kept generator gets a weight factor in "
                "[1, max_boost] (log-uniform)");
REDLINE_TUNABLE(kSwarmMutatorVariation, "selection.swarm.mutator_variation", 1,
                0, 1,
                "swarm testing: 1 gives each mutator a weight factor in "
                "[0.5, 2] and sometimes turns on a structural mutator");

// The factors of the mutators are log-uniform in [kMinMutatorFactor,
// kMaxMutatorFactor].
constexpr double kMinMutatorFactor = 0.5;
constexpr double kMaxMutatorFactor = 2.0;
// The chance that the mask of a worker turns on one structural mutator with
// weight 0, and the weight that this mutator gets.
constexpr double kStructuralChance = 0.25;
constexpr double kStructuralWeight = 1.0;

// The finalizer of SplitMix64.
uint64_t Mix(uint64_t x) {
  x ^= x >> 30;
  x *= 0xBF58476D1CE4E5B9ull;
  x ^= x >> 27;
  x *= 0x94D049BB133111EBull;
  x ^= x >> 31;
  return x;
}

uint64_t SwarmSeed(uint64_t seed, int worker, uint64_t epoch) {
  uint64_t x = Mix(seed ^ 0x5A4D5357524D0001ull);
  x = Mix(x ^ (static_cast<uint64_t>(worker) + 1) * 0x9E3779B97F4A7C15ull);
  return Mix(x ^ (epoch + 1) * 0xC2B2AE3D27D4EB4Full);
}

// A log-uniform value in [lo, hi].
double LogUniform(Random& rng, double lo, double hi) {
  return lo * std::exp(rng.Double() * std::log(hi / lo));
}

thread_local const SwarmMask* active_mask = nullptr;

}  // namespace

// static
SwarmSettings SwarmSettings::FromTunables() {
  SwarmSettings s;
  s.epoch_execs = kSwarmEpochExecs.AsInt();
  s.keep_prob = kSwarmKeepProb();
  s.min_active_generators = kSwarmMinActiveGenerators.AsInt();
  s.max_boost = kSwarmMaxBoost();
  s.mutator_variation = kSwarmMutatorVariation() != 0;
  return s;
}

SwarmMask DrawSwarmMask(const SwarmSettings& settings,
                        const SwarmCandidates& generators,
                        const SwarmCandidates& mutators, uint64_t seed,
                        int worker, uint64_t epoch) {
  SwarmMask mask;
  mask.epoch = epoch;
  const size_t n = generators.weights.size();
  mask.generators.factor.assign(n, 1.0);
  if (worker == 0) {
    for (size_t i = 0; i < n; i++) {
      if (!generators.core[i] && generators.weights[i] > 0) {
        mask.kept_generators++;
      }
    }
    return mask;
  }
  Random rng(SwarmSeed(seed, worker, epoch));
  // Enabled generators that are not core and that the draw removed.
  std::vector<size_t> removed;
  for (size_t i = 0; i < n; i++) {
    // Two draws for each generator, also for the core and the disabled ones.
    // Thus the mask of one generator does not depend on the others.
    const bool keep = rng.Chance(settings.keep_prob);
    const double boost = LogUniform(rng, 1.0, settings.max_boost);
    if (generators.core[i] || generators.weights[i] <= 0) {
      continue;
    }
    if (keep) {
      mask.generators.factor[i] = boost;
      mask.kept_generators++;
    } else {
      mask.generators.factor[i] = 0.0;
      removed.push_back(i);
    }
  }
  // Keep at least min_active_generators: add removed generators at random.
  while (mask.kept_generators < settings.min_active_generators &&
         !removed.empty()) {
    const size_t k = rng.Index(removed.size());
    mask.generators.factor[removed[k]] =
        LogUniform(rng, 1.0, settings.max_boost);
    removed[k] = removed.back();
    removed.pop_back();
    mask.kept_generators++;
  }
  if (!settings.mutator_variation) {
    return mask;
  }
  const size_t m = mutators.weights.size();
  mask.mutators.factor.assign(m, 1.0);
  std::vector<size_t> structural;
  for (size_t i = 0; i < m; i++) {
    const double factor = LogUniform(rng, kMinMutatorFactor, kMaxMutatorFactor);
    if (mutators.weights[i] > 0) {
      mask.mutators.factor[i] = factor;
    } else if (mutators.structural[i]) {
      structural.push_back(i);
    }
  }
  if (rng.Chance(kStructuralChance) && !structural.empty()) {
    const size_t i = structural[rng.Index(structural.size())];
    mask.mutators.enable.assign(m, 0.0);
    mask.mutators.enable[i] = kStructuralWeight;
    mask.enabled_mutator = static_cast<int>(i);
  }
  return mask;
}

void SetActiveSwarmMask(const SwarmMask* mask) { active_mask = mask; }

const SwarmMask* ActiveSwarmMask() { return active_mask; }

const WeightMask* ActiveGeneratorMask() {
  return active_mask == nullptr ? nullptr : &active_mask->generators;
}

const WeightMask* ActiveMutatorMask() {
  return active_mask == nullptr || active_mask->mutators.factor.empty()
             ? nullptr
             : &active_mask->mutators;
}

}  // namespace redline
