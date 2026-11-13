// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_MUTATORS_PROBING_H_
#define V8_EXPERIMENTAL_REDLINE_MUTATORS_PROBING_H_

#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/mutators/explore.h"

namespace redline {

// The runtime-assisted "Probing" mutator, after Fuzzilli's ProbingMutator.
//
// Where Explore asks "what can be done with this value", Probing asks "what
// does the program try to do with it that does not exist yet":
//
//  1. Mutate(): turns a few object-valued variables into probes with
//     `__redline_probe("pN", v)`: the probe swaps the object's prototype for
//     a Proxy around the original one, which sees every load, store and
//     `in` test of a property the object does not have.
//  2. The worker executes the instrumented program with kProbeRuntime
//     prepended; each distinct missing property is reported once as
//     "PROBE_HIT: {"id","kind","key"}" on fuzzout (string keys and
//     well-known symbols only).
//  3. ApplyProbeActions() replaces each probe by code that installs one or
//     two of the reported properties on the object: a plain value, an
//     accessor pair via Object.defineProperty, or a well-known-symbol
//     method (Symbol.toPrimitive, Symbol.iterator, ...). Probes without
//     hits are removed.
//
// Installing exactly the properties the program looks for exercises the
// engine's property lookup, IC and prototype-chain machinery along paths
// that generated code rarely reaches by accident.
class ProbingMutator final : public RuntimeAssistedMutator {
 public:
  ProbingMutator();
  bool Mutate(Builder& builder, MutationContext& ctx) override;
  std::string_view runtime() const override;
  int Apply(Builder& builder, const std::vector<ExploreProbe>& probes,
            const std::string& fuzzout, MutationContext& ctx,
            std::string* detail) override;

  static constexpr std::string_view kProbeFunction = "__redline_probe";
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_MUTATORS_PROBING_H_
