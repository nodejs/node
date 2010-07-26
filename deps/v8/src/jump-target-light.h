// Copyright 2008 the V8 project authors. All rights reserved.
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

#ifndef V8_JUMP_TARGET_LIGHT_H_
#define V8_JUMP_TARGET_LIGHT_H_

#include "macro-assembler.h"
#include "zone-inl.h"
#include "virtual-frame.h"

namespace v8 {
namespace internal {

// Forward declarations.
class FrameElement;
class Result;

// -------------------------------------------------------------------------
// Jump targets
//
// A jump target is an abstraction of a basic-block entry in generated
// code.  It collects all the virtual frames reaching the block by
// forward jumps and pairs them with labels for the merge code along
// all forward-reaching paths.  When bound, an expected frame for the
// block is determined and code is generated to merge to the expected
// frame.  For backward jumps, the merge code is generated at the edge
// leaving the predecessor block.
//
// A jump target must have been reached via control flow (either by
// jumping, branching, or falling through) at the time it is bound.
// In particular, this means that at least one of the control-flow
// graph edges reaching the target must be a forward edge.

class JumpTarget : public ZoneObject {  // Shadows are dynamically allocated.
 public:
  // Forward-only jump targets can only be reached by forward CFG edges.
  enum Directionality { FORWARD_ONLY, BIDIRECTIONAL };

  // Construct a jump target.
  explicit inline JumpTarget(Directionality direction);

  inline JumpTarget();

  virtual ~JumpTarget() {}

  void Unuse() {
    entry_frame_set_ = false;
    entry_label_.Unuse();
  }

  inline CodeGenerator* cgen();

  Label* entry_label() { return &entry_label_; }

  const VirtualFrame* entry_frame() const {
    return entry_frame_set_ ? &entry_frame_ : NULL;
  }

  void set_entry_frame(VirtualFrame* frame) {
    entry_frame_ = *frame;
    entry_frame_set_ = true;
  }

  // Predicates testing the state of the encapsulated label.
  bool is_bound() const { return entry_label_.is_bound(); }
  bool is_linked() const { return entry_label_.is_linked(); }
  bool is_unused() const { return entry_label_.is_unused(); }

  // Copy the state of this jump target to the destination.
  inline void CopyTo(JumpTarget* destination) {
    *destination = *this;
  }

  // Emit a jump to the target.  There must be a current frame at the
  // jump and there will be no current frame after the jump.
  virtual void Jump();

  // Emit a conditional branch to the target.  There must be a current
  // frame at the branch.  The current frame will fall through to the
  // code after the branch.
  virtual void Branch(Condition cc, Hint hint = no_hint);

  // Bind a jump target.  If there is no current frame at the binding
  // site, there must be at least one frame reaching via a forward
  // jump.
  virtual void Bind();

  // Emit a call to a jump target.  There must be a current frame at
  // the call.  The frame at the target is the same as the current
  // frame except for an extra return address on top of it.  The frame
  // after the call is the same as the frame before the call.
  void Call();

 protected:
  // Has an entry frame been found?
  bool entry_frame_set_;

  // Can we branch backwards to this label?
  Directionality direction_;

  // The frame used on entry to the block and expected at backward
  // jumps to the block.  Set the first time something branches to this
  // jump target.
  VirtualFrame entry_frame_;

  // The actual entry label of the block.
  Label entry_label_;

  // Implementations of Jump, Branch, and Bind with all arguments and
  // return values using the virtual frame.
  void DoJump();
  void DoBranch(Condition cc, Hint hint);
  void DoBind();
};


// -------------------------------------------------------------------------
// Break targets
//
// A break target is a jump target that can be used to break out of a
// statement that keeps extra state on the stack (eg, for/in or
// try/finally).  They know the expected stack height at the target
// and will drop state from nested statements as part of merging.
//
// Break targets are used for return, break, and continue targets.

class BreakTarget : public JumpTarget {
 public:
  // Construct a break target.
  inline BreakTarget();
  inline BreakTarget(JumpTarget::Directionality direction);

  virtual ~BreakTarget() {}

  // Copy the state of this jump target to the destination.
  inline void CopyTo(BreakTarget* destination) {
    *destination = *this;
  }

  // Emit a jump to the target.  There must be a current frame at the
  // jump and there will be no current frame after the jump.
  virtual void Jump();

  // Emit a conditional branch to the target.  There must be a current
  // frame at the branch.  The current frame will fall through to the
  // code after the branch.
  virtual void Branch(Condition cc, Hint hint = no_hint);

  // Bind a break target.  If there is no current frame at the binding
  // site, there must be at least one frame reaching via a forward
  // jump.
  virtual void Bind();

  // Setter for expected height.
  void set_expected_height(int expected) { expected_height_ = expected; }

  // Uses the current frame to set the expected height.
  void SetExpectedHeight();

 private:
  // The expected height of the expression stack where the target will
  // be bound, statically known at initialization time.
  int expected_height_;
};

} }  // namespace v8::internal

#endif  // V8_JUMP_TARGET_LIGHT_H_
