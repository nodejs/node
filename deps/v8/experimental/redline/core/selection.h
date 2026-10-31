// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pluggable selection strategies.
//
// Redline picks four kinds of things while fuzzing:
//
//   seeds       which corpus program a worker mutates next
//   partners    which second program Splice/Combine take material from
//   mutators    which mutator is applied
//   generators  which generator CodeGen inserts
//
// Each pick goes through a Selector<T>: "choose one eligible candidate among
// N, with the worker's RNG, knowing the candidates' static weights and live
// feedback". Which Selector is used, and with which parameters, comes from
// the configuration:
//
//   "selection": {
//     "mutators": { "strategy": "weighted", "params": { "weights": { "Splice":
//     4 } } }
//   }
//
// Strategies live in a SelectorRegistry<T> under a name, together with the
// list of parameters they accept (ParamSpec). The registry validates the
// configuration (unknown strategy, unknown parameter, wrong type -> error
// before anything runs) and builds the selector.
//
// Adding a strategy for, say, mutators:
//   1. write `class MyStrategy : public Selector<Mutator>` (mutators.cc),
//   2. add one Register() line to
//   RegisterBuiltinSelectors(SelectorRegistry<Mutator>&).
// The fuzzing loop, the mutators and the generators do not change.
//
// The default strategies reproduce Redline's historical behaviour exactly;
// `redline selftest --offline` checks that against recorded traces
// (experimental/redline/testdata/selection).

#ifndef V8_EXPERIMENTAL_REDLINE_CORE_SELECTION_H_
#define V8_EXPERIMENTAL_REDLINE_CORE_SELECTION_H_

#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/base/json.h"
#include "experimental/redline/base/json_parser.h"
#include "experimental/redline/core/random.h"

namespace redline {

// Live, read-only feedback about a candidate. Mutators and generators fill it
// from their ComponentStats, corpus programs from their pick/children
// counters. A strategy may use it to prefer or avoid candidates.
struct Feedback {
  uint64_t attempts = 0;  // times the candidate was picked / invoked
  uint64_t produced = 0;  // ... and produced something that could run
  uint64_t succeeded = 0;
  uint64_t failed = 0;
  uint64_t timed_out = 0;
  uint64_t crashed = 0;
  uint64_t interesting = 0;  // results with new deterministic edges
  uint64_t time_us = 0;      // time spent inside the candidate
};

// What happened with the last pick, reported back to the strategy.
struct SelectionOutcome {
  enum class Kind {
    kNoop,     // the candidate had nothing to do
    kInvalid,  // its result was rejected before execution
    kSucceeded,
    kFailed,
    kTimedOut,
    kCrashed
  };
  Kind kind = Kind::kNoop;
  bool interesting = false;  // new deterministic edges
  uint64_t time_us = 0;
};

// The configured strategy of one selection point.
struct SelectionConfig {
  std::string strategy;  // empty: the registry's default
  JsonValue params;      // object; null when not given
};

// A parameter a strategy accepts. Used to validate the configuration and to
// print the documentation (`redline fuzz --help-config`).
struct ParamSpec {
  enum class Type {
    kNumber,
    kBool,
    kString,
    kObject  // free-form object, validated by the strategy itself
  };
  std::string_view name;
  Type type;
  std::string_view default_value;  // as JSON text, for the documentation
  std::string_view doc;
};

// One strategy instance per selection point. T is Mutator, Generator or
// Program. Candidates are passed as a vector of pointers whose order is
// stable (registration order for mutators and generators, insertion order
// for the corpus); picks are indices into that vector.
//
// Threading: mutator and generator selectors are shared by all workers and
// must be stateless between calls or synchronize their own state. Corpus
// selectors are called with the corpus lock held.
template <typename T>
class Selector {
 public:
  using Candidates = std::vector<T*>;
  using Filter = std::function<bool(const T&)>;
  static constexpr size_t kNoChoice = std::numeric_limits<size_t>::max();

  virtual ~Selector() = default;

  virtual std::string_view name() const = 0;

  // Called once before the first pick with the candidates known at that
  // time (the corpus may still be empty); strategies validate the
  // parameters that refer to candidates here (e.g. weight names).
  virtual bool Initialize(const Candidates& candidates, std::string* error) {
    (void)candidates;
    (void)error;
    return true;
  }
  // A candidate was appended (corpus growth).
  virtual void OnAdded(const Candidates& candidates, size_t index) {
    (void)candidates;
    (void)index;
  }

  // Picks one candidate for which `eligible` is true, or kNoChoice.
  virtual size_t Pick(const Candidates& candidates, Random& rng,
                      const Filter& eligible) = 0;
  // Picks a candidate to go with `anchor` (the splice partner); an anchor
  // past the end means "exclude nothing". The default picks like Pick()
  // but never `anchor`.
  virtual size_t PickRelated(const Candidates& candidates, size_t anchor,
                             Random& rng, const Filter& eligible) {
    return Pick(candidates, rng, [&](const T& c) {
      return (anchor >= candidates.size() || &c != candidates[anchor]) &&
             eligible(c);
    });
  }
  // Feedback for the strategy; the default strategies ignore it.
  virtual void OnOutcome(T& picked, const SelectionOutcome& outcome) {
    (void)picked;
    (void)outcome;
  }

  // Corpus strategies only: true if the strategy needs every edge a new
  // program reaches deterministically (Program::hit_edges), which costs one
  // extra execution per corpus addition.
  virtual bool WantsHitEdges() const { return false; }
  // Corpus strategies only: called on the partner strategy with the seed
  // strategy, so that the two can share state (e.g. seed ages).
  virtual void AttachSeedStrategy(Selector<T>* seeds) { (void)seeds; }
  // Corpus strategies only: the edges of a crash that reproduces. Treating
  // them as already covered steers seeds away from the same crash.
  virtual void OnCrashEdges(const std::vector<uint32_t>& edges) { (void)edges; }

  // Static weights, for strategies that have them (the stats tables and
  // the --mutator-weights/--generator-weights shorthands use these).
  virtual double WeightOf(const T& candidate) const {
    (void)candidate;
    return 1.0;
  }
  virtual bool SetWeight(std::string_view candidate_name, double weight) {
    (void)candidate_name;
    (void)weight;
    return false;
  }

  // The effective parameters, for the `start` event.
  virtual JsonObject Params() const = 0;
};

// Name -> factory. One registry per candidate type; the built-in strategies
// are registered by RegisterBuiltinSelectors<T>() (defined next to the
// strategies: corpus.cc, mutators.cc, generators.cc).
template <typename T>
class SelectorRegistry {
 public:
  // Builds the strategy from validated params; may still reject them
  // (returns nullptr with *error set), e.g. a weight that is not a number.
  using Factory = std::unique_ptr<Selector<T>> (*)(const JsonValue& params,
                                                   std::string* error);

  struct Entry {
    std::string name;
    std::string doc;
    std::vector<ParamSpec> params;
    Factory factory;
  };

  static SelectorRegistry& Instance();

  void Register(Entry entry) { entries_.emplace(entry.name, std::move(entry)); }
  // The strategy used when the configuration names none.
  void SetDefault(std::string_view name) { default_ = std::string(name); }

  // Validates `config` (strategy name, parameter names and types) and
  // builds the selector. On error returns nullptr and sets *error.
  std::unique_ptr<Selector<T>> Create(const SelectionConfig& config,
                                      std::string* error) const;

  std::vector<std::string> Names() const;
  const std::string& default_name() const { return default_; }
  // Documentation of all strategies and their parameters.
  std::string Help(std::string_view point_name) const;

 private:
  std::map<std::string, Entry> entries_;
  std::string default_;
};

// Defined per candidate type next to the built-in strategies.
template <typename T>
void RegisterBuiltinSelectors(SelectorRegistry<T>& registry);

// --- shared helpers for strategies and registries (selection.cc) --------

// Checks that `params` is an object whose keys are all declared in `specs`
// with the declared types. Returns false with a message naming the offender.
bool ValidateParams(std::string_view strategy, const JsonValue& params,
                    const std::vector<ParamSpec>& specs, std::string* error);

// Reads params.weights ({"Name": number, ...}) into `out`. Returns false with
// a message for a non-object or a non-numeric weight. Names are checked
// later against the candidates (Initialize).
bool ReadWeightOverrides(const JsonValue& params,
                         std::vector<std::pair<std::string, double>>* out,
                         std::string* error);

std::string JoinNames(const std::vector<std::string>& names);

// --- template definitions ---------------------------------------------------

template <typename T>
SelectorRegistry<T>& SelectorRegistry<T>::Instance() {
  static SelectorRegistry<T>* registry = [] {
    auto* r = new SelectorRegistry<T>();
    RegisterBuiltinSelectors<T>(*r);
    return r;
  }();
  return *registry;
}

template <typename T>
std::unique_ptr<Selector<T>> SelectorRegistry<T>::Create(
    const SelectionConfig& config, std::string* error) const {
  const std::string& name =
      config.strategy.empty() ? default_ : config.strategy;
  auto found = entries_.find(name);
  if (found == entries_.end()) {
    *error = "unknown selection strategy '" + name +
             "' (known: " + JoinNames(Names()) + ")";
    return nullptr;
  }
  if (!ValidateParams(name, config.params, found->second.params, error)) {
    return nullptr;
  }
  std::unique_ptr<Selector<T>> selector =
      found->second.factory(config.params, error);
  if (selector == nullptr && !error->empty()) {
    *error = "selection strategy '" + name + "': " + *error;
  }
  return selector;
}

template <typename T>
std::vector<std::string> SelectorRegistry<T>::Names() const {
  std::vector<std::string> names;
  names.reserve(entries_.size());
  for (const auto& [name, entry] : entries_) {
    names.push_back(name);
  }
  return names;
}

template <typename T>
std::string SelectorRegistry<T>::Help(std::string_view point_name) const {
  std::string out =
      "  " + std::string(point_name) + " (default: " + default_ + ")\n";
  for (const auto& [name, entry] : entries_) {
    out += "    " + name + ": " + entry.doc + "\n";
    for (const ParamSpec& p : entry.params) {
      out += "      " + std::string(p.name) + " = " +
             std::string(p.default_value) + "  " + std::string(p.doc) + "\n";
    }
  }
  return out;
}

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORE_SELECTION_H_
