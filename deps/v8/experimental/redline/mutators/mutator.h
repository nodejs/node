// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_MUTATORS_MUTATOR_H_
#define V8_EXPERIMENTAL_REDLINE_MUTATORS_MUTATOR_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/core/builder.h"
#include "experimental/redline/core/component.h"
#include "experimental/redline/core/environment.h"
#include "experimental/redline/core/program.h"
#include "experimental/redline/core/selection.h"
#include "experimental/redline/generators/generator.h"

namespace redline {

struct ExploreProbe;

struct MutationContext {
  // The program being mutated (its tree is NOT the one in the Builder; the
  // Builder works on a copy). May be null for ad-hoc trees.
  ProgramPtr parent;
  // A second program for splicing/combining; null if none available.
  ProgramPtr partner;
  const Environment* env = nullptr;
  const GeneratorSet* generators = nullptr;
  // Out: a short human-readable description of what was changed.
  std::string detail;
  // Out, for runtime-assisted mutators: the probes they inserted. The
  // worker executes the instrumented program and completes the mutation
  // from the target's answers (see mutators/explore.h).
  std::vector<ExploreProbe> probes;

  MutationContext();
  ~MutationContext();
  MutationContext(const MutationContext&) = delete;
  MutationContext& operator=(const MutationContext&) = delete;
};

class Mutator : public Component {
 public:
  // `max_simultaneous` is the largest number of sub-mutations one
  // application may perform (Fuzzilli's maxSimultaneousMutations: 7 for
  // instruction-level mutators, 3 for code generation, 1 for everything
  // else); the engine draws the actual number uniformly from 1..max.
  Mutator(std::string name, std::string_view description, double weight,
          bool needs_partner, int max_simultaneous = 1)
      : Component(std::move(name)),
        description_(description),
        weight_(weight),
        needs_partner_(needs_partner),
        max_simultaneous_(max_simultaneous) {}

  std::string_view description() const override { return description_; }
  double weight() const { return weight_; }
  bool needs_partner() const { return needs_partner_; }
  int max_simultaneous() const { return max_simultaneous_; }

  // Mutates the Builder's tree in place. Returns false if the mutator could
  // not find anything to do (the tree is then unchanged).
  virtual bool Mutate(Builder& builder, MutationContext& ctx) = 0;
  // True if Mutate() only instruments the program and the worker must run
  // it and apply the target's answers to obtain the real mutation.
  virtual bool runtime_assisted() const { return false; }
  // For mutators that insert generated code: how many generator outputs
  // one application inserts (min_count plus 0..extra_count). Others
  // return false.
  virtual bool SetInsertCounts(int min_count, int extra_count) {
    (void)min_count;
    (void)extra_count;
    return false;
  }

 private:
  std::string_view description_;
  double weight_;
  bool needs_partner_;
  int max_simultaneous_;
};

// All mutators, and the strategy that picks among them (core/selection.h).
// CreateDefault() installs the built-in mutators with the default `weighted`
// strategy; a fuzzing session replaces the strategy with the configured one
// through SetSelector().
class MutatorSet {
 public:
  static std::unique_ptr<MutatorSet> CreateDefault();

  const std::vector<std::unique_ptr<Mutator>>& all() const { return mutators_; }
  // The same mutators as plain pointers, in the same order: what the
  // selector sees.
  const std::vector<Mutator*>& candidates() const { return candidates_; }
  Mutator* Find(std::string_view name) const;
  std::vector<std::string> Names() const;

  // Installs a selection strategy and initializes it with the mutators
  // (weight names are validated here). Returns false with *error set.
  bool SetSelector(std::unique_ptr<Selector<Mutator>> selector,
                   std::string* error);
  // Number of fresh values the Prefix mutator declares: min_values plus
  // 0..extra_values (FuzzConfig::prefix_min_values/prefix_extra_values).
  bool ConfigurePrefix(int min_values, int extra_values);
  Selector<Mutator>& selector() const { return *selector_; }
  // Shorthands for the strategy's static weights (0 disables a mutator).
  bool SetWeight(std::string_view name, double weight) {
    return selector_->SetWeight(name, weight);
  }
  double WeightOf(const Mutator* mutator) const {
    return selector_->WeightOf(*mutator);
  }

  // Picks a mutator with the strategy. Mutators needing a partner are only
  // eligible when `has_partner`.
  Mutator* Pick(Random& rng, bool has_partner) const;

  std::vector<const Component*> Components() const;

 private:
  std::vector<std::unique_ptr<Mutator>> mutators_;
  std::vector<Mutator*> candidates_;
  std::unique_ptr<Selector<Mutator>> selector_;
};

// Applies `mutator` to a copy of `parent`'s tree. On success returns the
// mutated tree; on failure returns nullptr. `rounds` sub-mutations are
// attempted (at least one must succeed).
// Applies `mutator` `rounds` times to a copy of `parent`'s tree. Returns the
// mutated tree, or nullptr if no round changed anything. For runtime-assisted
// mutators `probes` (if given) receives the inserted probes.
std::unique_ptr<fast::Tree> ApplyMutation(
    Mutator& mutator, const Program& parent, const ProgramPtr& partner,
    Random& rng, const Environment& env, const GeneratorSet& generators,
    int rounds, std::string* detail,
    std::vector<ExploreProbe>* probes = nullptr);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_MUTATORS_MUTATOR_H_
