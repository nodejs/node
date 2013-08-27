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

#ifndef V8_HYDROGEN_MARK_DEOPTIMIZE_H_
#define V8_HYDROGEN_MARK_DEOPTIMIZE_H_

#include "hydrogen.h"

namespace v8 {
namespace internal {


// Compute DeoptimizeOnUndefined flag for phis.  Any phi that can reach a use
// with DeoptimizeOnUndefined set must have DeoptimizeOnUndefined set.
// Currently only HCompareNumericAndBranch, with double input representation,
// has this flag set.  The flag is used by HChange tagged->double, which must
// deoptimize if one of its uses has this flag set.
class HMarkDeoptimizeOnUndefinedPhase : public HPhase {
 public:
  explicit HMarkDeoptimizeOnUndefinedPhase(HGraph* graph)
      : HPhase("H_Mark deoptimize on undefined", graph),
        worklist_(16, zone()) {}

  void Run();

 private:
  void ProcessPhi(HPhi* phi);

  // Preallocated worklist used as an optimization so we don't have
  // to allocate a new ZoneList for every ProcessPhi() invocation.
  ZoneList<HPhi*> worklist_;

  DISALLOW_COPY_AND_ASSIGN(HMarkDeoptimizeOnUndefinedPhase);
};


class HComputeChangeUndefinedToNaN : public HPhase {
 public:
  explicit HComputeChangeUndefinedToNaN(HGraph* graph)
      : HPhase("H_Compute change undefined to nan", graph) {}

  void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(HComputeChangeUndefinedToNaN);
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_MARK_DEOPTIMIZE_H_
