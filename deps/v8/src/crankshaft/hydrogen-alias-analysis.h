// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_HYDROGEN_ALIAS_ANALYSIS_H_
#define V8_CRANKSHAFT_HYDROGEN_ALIAS_ANALYSIS_H_

#include "src/crankshaft/hydrogen.h"

namespace v8 {
namespace internal {

enum HAliasing {
  kMustAlias,
  kMayAlias,
  kNoAlias
};


// Defines the interface to alias analysis for the rest of the compiler.
// A simple implementation can use only local reasoning, but a more powerful
// analysis might employ points-to analysis.
class HAliasAnalyzer : public ZoneObject {
 public:
  // Simple alias analysis distinguishes allocations, parameters,
  // and constants using only local reasoning.
  HAliasing Query(HValue* a, HValue* b) {
    // The same SSA value always references the same object.
    if (a == b) return kMustAlias;

    if (a->IsAllocate() || a->IsInnerAllocatedObject()) {
      // Two non-identical allocations can never be aliases.
      if (b->IsAllocate()) return kNoAlias;
      if (b->IsInnerAllocatedObject()) return kNoAlias;
      // An allocation can never alias a parameter or a constant.
      if (b->IsParameter()) return kNoAlias;
      if (b->IsConstant()) return kNoAlias;
    }
    if (b->IsAllocate() || b->IsInnerAllocatedObject()) {
      // An allocation can never alias a parameter or a constant.
      if (a->IsParameter()) return kNoAlias;
      if (a->IsConstant()) return kNoAlias;
    }

    // Constant objects can be distinguished statically.
    if (a->IsConstant()) {
      // TODO(titzer): DataEquals() is more efficient, but that's protected.
      return a->Equals(b) ? kMustAlias : kNoAlias;
    }
    return kMayAlias;
  }

  // Checks whether the objects referred to by the given instructions may
  // ever be aliases. Note that this is more conservative than checking
  // {Query(a, b) == kMayAlias}, since this method considers kMustAlias
  // objects to also be may-aliasing.
  inline bool MayAlias(HValue* a, HValue* b) {
    return Query(a, b) != kNoAlias;
  }

  inline bool MustAlias(HValue* a, HValue* b) {
    return Query(a, b) == kMustAlias;
  }

  inline bool NoAlias(HValue* a, HValue* b) {
    return Query(a, b) == kNoAlias;
  }
};


}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_HYDROGEN_ALIAS_ANALYSIS_H_
