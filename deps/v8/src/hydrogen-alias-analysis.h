// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_HYDROGEN_ALIAS_ANALYSIS_H_
#define V8_HYDROGEN_ALIAS_ANALYSIS_H_

#include "hydrogen.h"

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

  // Returns the actual value of an instruction. In the case of a chain
  // of informative definitions, return the root of the chain.
  HValue* ActualValue(HValue* obj) {
    while (obj->IsInformativeDefinition()) {  // Walk a chain of idefs.
      obj = obj->RedefinedOperand();
    }
    return obj;
  }
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_ALIAS_ANALYSIS_H_
