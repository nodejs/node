// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Swarm testing for each worker (`selection.swarm`, DESIGN.md 5.3).
//
// All workers share the mutator and generator strategies, thus all workers
// use the same weights. With swarm testing, each worker except worker 0
// multiplies these weights by its own mask (a WeightMask, core/selection.h).
// The worker draws a new mask at the start of each epoch of
// `selection.swarm.epoch_execs` fuzz executions. The mask keeps a random
// subset of the generators and multiplies the weight of each kept generator
// by a random boost. Thus the workers try different combinations of
// features. The shared strategy still gets the results of all workers.
//
// Worker 0 uses no mask, thus it keeps all generators. The mask always keeps
// the core generators (SwarmCoreGenerators(), generators/generator.h), so
// that each worker can make valid programs.

#ifndef V8_EXPERIMENTAL_REDLINE_CORE_SWARM_H_
#define V8_EXPERIMENTAL_REDLINE_CORE_SWARM_H_

#include <cstdint>
#include <vector>

#include "experimental/redline/core/selection.h"

namespace redline {

// The parameters of the mask draw. FromTunables() reads the
// `selection.swarm.*` tunables.
struct SwarmSettings {
  int epoch_execs = 5000;   // fuzz executions of a worker for each mask
  double keep_prob = 0.45;  // the chance to keep a generator that is not core
  int min_active_generators = 12;  // kept generators that are not core
  double max_boost = 4.0;  // a kept generator gets a factor in [1, max_boost]
  // Each mutator gets a factor in [0.5, 2]. Sometimes the mask also turns on
  // one structural mutator that has weight 0.
  bool mutator_variation = true;

  static SwarmSettings FromTunables();
};

// The candidates of one selection point, in the order of the selector.
struct SwarmCandidates {
  std::vector<double> weights;   // weights of the strategy, 0: disabled
  std::vector<bool> core;        // generators: the mask always keeps them
  std::vector<bool> structural;  // mutators: the mask can turn them on
};

// The mask of one worker for one epoch.
struct SwarmMask {
  uint64_t epoch = 0;
  WeightMask generators;
  WeightMask mutators;       // an empty factor list: no mutator mask
  int kept_generators = 0;   // enabled generators that are not core and kept
  int enabled_mutator = -1;  // the structural mutator turned on, or -1
};

// Draws the mask of `worker` for `epoch`. The result depends only on the
// arguments. Worker 0 gets factor 1 for all generators and no mutator mask.
SwarmMask DrawSwarmMask(const SwarmSettings& settings,
                        const SwarmCandidates& generators,
                        const SwarmCandidates& mutators, uint64_t seed,
                        int worker, uint64_t epoch);

// The mask of the worker that runs on this thread, or nullptr. The mutator
// and generator sets apply it to each pick on this thread.
void SetActiveSwarmMask(const SwarmMask* mask);
const SwarmMask* ActiveSwarmMask();
// The parts of the active mask, or nullptr.
const WeightMask* ActiveGeneratorMask();
const WeightMask* ActiveMutatorMask();

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORE_SWARM_H_
