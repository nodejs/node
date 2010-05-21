// Copyright 2010 the V8 project authors. All rights reserved.
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

namespace v8 {
namespace internal {


DeferredCode::DeferredCode()
    : masm_(CodeGeneratorScope::Current()->masm()),
      statement_position_(masm_->current_statement_position()),
      position_(masm_->current_position()) {
  ASSERT(statement_position_ != RelocInfo::kNoPosition);
  ASSERT(position_ != RelocInfo::kNoPosition);

  CodeGeneratorScope::Current()->AddDeferred(this);

#ifdef DEBUG
  CodeGeneratorScope::Current()->frame()->AssertIsSpilled();
#endif
}


// -------------------------------------------------------------------------
// BreakTarget implementation.


void BreakTarget::SetExpectedHeight() {
  expected_height_ = cgen()->frame()->height();
}


void BreakTarget::Jump() {
  ASSERT(cgen()->has_valid_frame());

  int count = cgen()->frame()->height() - expected_height_;
  if (count > 0) {
    cgen()->frame()->Drop(count);
  }
  DoJump();
}


void BreakTarget::Branch(Condition cc, Hint hint) {
  if (cc == al) {
    Jump();
    return;
  }

  ASSERT(cgen()->has_valid_frame());

  int count = cgen()->frame()->height() - expected_height_;
  if (count > 0) {
    // We negate and branch here rather than using DoBranch's negate
    // and branch.  This gives us a hook to remove statement state
    // from the frame.
    JumpTarget fall_through;
    // Branch to fall through will not negate, because it is a
    // forward-only target.
    fall_through.Branch(NegateCondition(cc), NegateHint(hint));
    // Emit merge code.
    cgen()->frame()->Drop(count);
    DoJump();
    fall_through.Bind();
  } else {
    DoBranch(cc, hint);
  }
}


void BreakTarget::Bind() {
  if (cgen()->has_valid_frame()) {
    int count = cgen()->frame()->height() - expected_height_;
    if (count > 0) {
      cgen()->frame()->Drop(count);
    }
  }
  DoBind();
}

} }  // namespace v8::internal
