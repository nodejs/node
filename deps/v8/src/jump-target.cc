// Copyright 2009 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "codegen-inl.h"
#include "jump-target-inl.h"
#include "register-allocator-inl.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// JumpTarget implementation.

void JumpTarget::Jump() {
  DoJump();
}


void JumpTarget::Branch(Condition cc, Hint hint) {
  DoBranch(cc, hint);
}


void JumpTarget::Bind() {
  DoBind();
}


// -------------------------------------------------------------------------
// ShadowTarget implementation.

ShadowTarget::ShadowTarget(BreakTarget* shadowed) {
  ASSERT(shadowed != NULL);
  other_target_ = shadowed;

#ifdef DEBUG
  is_shadowing_ = true;
#endif
  // While shadowing this shadow target saves the state of the original.
  shadowed->CopyTo(this);

  // The original's state is reset.
  shadowed->Unuse();
  ASSERT(cgen()->has_valid_frame());
  shadowed->set_expected_height(cgen()->frame()->height());
}


void ShadowTarget::StopShadowing() {
  ASSERT(is_shadowing_);

  // The states of this target, which was shadowed, and the original
  // target, which was shadowing, are swapped.
  BreakTarget temp;
  other_target_->CopyTo(&temp);
  CopyTo(other_target_);
  temp.CopyTo(this);
  temp.Unuse();

#ifdef DEBUG
  is_shadowing_ = false;
#endif
}

} }  // namespace v8::internal
