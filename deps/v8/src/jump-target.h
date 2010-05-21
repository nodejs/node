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

#ifndef V8_JUMP_TARGET_H_
#define V8_JUMP_TARGET_H_

#if V8_TARGET_ARCH_IA32
#include "jump-target-heavy.h"
#elif V8_TARGET_ARCH_X64
#include "jump-target-heavy.h"
#elif V8_TARGET_ARCH_ARM
#include "jump-target-light.h"
#elif V8_TARGET_ARCH_MIPS
#include "jump-target-light.h"
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// Shadow break targets
//
// A shadow break target represents a break target that is temporarily
// shadowed by another one (represented by the original during
// shadowing).  They are used to catch jumps to labels in certain
// contexts, e.g. try blocks.  After shadowing ends, the formerly
// shadowed target is again represented by the original and the
// ShadowTarget can be used as a jump target in its own right,
// representing the formerly shadowing target.

class ShadowTarget : public BreakTarget {
 public:
  // Construct a shadow jump target.  After construction the shadow
  // target object holds the state of the original target, and the
  // original target is actually a fresh one that intercepts control
  // flow intended for the shadowed one.
  explicit ShadowTarget(BreakTarget* shadowed);

  virtual ~ShadowTarget() {}

  // End shadowing.  After shadowing ends, the original jump target
  // again gives access to the formerly shadowed target and the shadow
  // target object gives access to the formerly shadowing target.
  void StopShadowing();

  // During shadowing, the currently shadowing target.  After
  // shadowing, the target that was shadowed.
  BreakTarget* other_target() const { return other_target_; }

 private:
  // During shadowing, the currently shadowing target.  After
  // shadowing, the target that was shadowed.
  BreakTarget* other_target_;

#ifdef DEBUG
  bool is_shadowing_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ShadowTarget);
};

} }  // namespace v8::internal

#endif  // V8_JUMP_TARGET_H_
