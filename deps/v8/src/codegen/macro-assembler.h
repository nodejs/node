// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_MACRO_ASSEMBLER_H_
#define V8_CODEGEN_MACRO_ASSEMBLER_H_

#include "src/codegen/turbo-assembler.h"
#include "src/execution/frames.h"
#include "src/heap/heap.h"

// Helper types to make boolean flag easier to read at call-site.
enum InvokeFlag { CALL_FUNCTION, JUMP_FUNCTION };

// Flags used for the AllocateInNewSpace functions.
enum AllocationFlags {
  // No special flags.
  NO_ALLOCATION_FLAGS = 0,
  // The content of the result register already contains the allocation top in
  // new space.
  RESULT_CONTAINS_TOP = 1 << 0,
  // Specify that the requested size of the space to allocate is specified in
  // words instead of bytes.
  SIZE_IN_WORDS = 1 << 1,
  // Align the allocation to a multiple of kDoubleSize
  DOUBLE_ALIGNMENT = 1 << 2,
  // Directly allocate in old space
  PRETENURE = 1 << 3,
};

// This is the only place allowed to include the platform-specific headers.
#define INCLUDED_FROM_MACRO_ASSEMBLER_H
#if V8_TARGET_ARCH_IA32
#include "src/codegen/ia32/macro-assembler-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/codegen/x64/macro-assembler-x64.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/codegen/arm64/constants-arm64.h"
#include "src/codegen/arm64/macro-assembler-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/codegen/arm/constants-arm.h"
#include "src/codegen/arm/macro-assembler-arm.h"
#elif V8_TARGET_ARCH_PPC
#include "src/codegen/ppc/constants-ppc.h"
#include "src/codegen/ppc/macro-assembler-ppc.h"
#elif V8_TARGET_ARCH_MIPS
#include "src/codegen/mips/constants-mips.h"
#include "src/codegen/mips/macro-assembler-mips.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/codegen/mips64/constants-mips64.h"
#include "src/codegen/mips64/macro-assembler-mips64.h"
#elif V8_TARGET_ARCH_S390
#include "src/codegen/s390/constants-s390.h"
#include "src/codegen/s390/macro-assembler-s390.h"
#else
#error Unsupported target architecture.
#endif
#undef INCLUDED_FROM_MACRO_ASSEMBLER_H

namespace v8 {
namespace internal {

// Simulators only support C calls with up to kMaxCParameters parameters.
static constexpr int kMaxCParameters = 9;

class FrameScope {
 public:
  explicit FrameScope(TurboAssembler* tasm, StackFrame::Type type)
      : tasm_(tasm), type_(type), old_has_frame_(tasm->has_frame()) {
    tasm->set_has_frame(true);
    if (type != StackFrame::MANUAL && type_ != StackFrame::NONE) {
      tasm->EnterFrame(type);
    }
  }

  ~FrameScope() {
    if (type_ != StackFrame::MANUAL && type_ != StackFrame::NONE) {
      tasm_->LeaveFrame(type_);
    }
    tasm_->set_has_frame(old_has_frame_);
  }

  // Normally we generate the leave-frame code when this object goes
  // out of scope.  Sometimes we may need to generate the code somewhere else
  // in addition.  Calling this will achieve that, but the object stays in
  // scope, the MacroAssembler is still marked as being in a frame scope, and
  // the code will be generated again when it goes out of scope.
  void GenerateLeaveFrame() {
    DCHECK(type_ != StackFrame::MANUAL && type_ != StackFrame::NONE);
    tasm_->LeaveFrame(type_);
  }

 private:
  TurboAssembler* tasm_;
  StackFrame::Type type_;
  bool old_has_frame_;
};

class FrameAndConstantPoolScope {
 public:
  FrameAndConstantPoolScope(MacroAssembler* masm, StackFrame::Type type)
      : masm_(masm),
        type_(type),
        old_has_frame_(masm->has_frame()),
        old_constant_pool_available_(FLAG_enable_embedded_constant_pool &&
                                     masm->is_constant_pool_available()) {
    masm->set_has_frame(true);
    if (FLAG_enable_embedded_constant_pool) {
      masm->set_constant_pool_available(true);
    }
    if (type_ != StackFrame::MANUAL && type_ != StackFrame::NONE) {
      masm->EnterFrame(type, !old_constant_pool_available_);
    }
  }

  ~FrameAndConstantPoolScope() {
    masm_->LeaveFrame(type_);
    masm_->set_has_frame(old_has_frame_);
    if (FLAG_enable_embedded_constant_pool) {
      masm_->set_constant_pool_available(old_constant_pool_available_);
    }
  }

  // Normally we generate the leave-frame code when this object goes
  // out of scope.  Sometimes we may need to generate the code somewhere else
  // in addition.  Calling this will achieve that, but the object stays in
  // scope, the MacroAssembler is still marked as being in a frame scope, and
  // the code will be generated again when it goes out of scope.
  void GenerateLeaveFrame() {
    DCHECK(type_ != StackFrame::MANUAL && type_ != StackFrame::NONE);
    masm_->LeaveFrame(type_);
  }

 private:
  MacroAssembler* masm_;
  StackFrame::Type type_;
  bool old_has_frame_;
  bool old_constant_pool_available_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FrameAndConstantPoolScope);
};

// Class for scoping the the unavailability of constant pool access.
class ConstantPoolUnavailableScope {
 public:
  explicit ConstantPoolUnavailableScope(Assembler* assembler)
      : assembler_(assembler),
        old_constant_pool_available_(FLAG_enable_embedded_constant_pool &&
                                     assembler->is_constant_pool_available()) {
    if (FLAG_enable_embedded_constant_pool) {
      assembler->set_constant_pool_available(false);
    }
  }
  ~ConstantPoolUnavailableScope() {
    if (FLAG_enable_embedded_constant_pool) {
      assembler_->set_constant_pool_available(old_constant_pool_available_);
    }
  }

 private:
  Assembler* assembler_;
  int old_constant_pool_available_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ConstantPoolUnavailableScope);
};

class AllowExternalCallThatCantCauseGC : public FrameScope {
 public:
  explicit AllowExternalCallThatCantCauseGC(MacroAssembler* masm)
      : FrameScope(masm, StackFrame::NONE) {}
};

// Prevent the use of the RootArray during the lifetime of this
// scope object.
class NoRootArrayScope {
 public:
  explicit NoRootArrayScope(TurboAssembler* masm)
      : masm_(masm), old_value_(masm->root_array_available()) {
    masm->set_root_array_available(false);
  }

  ~NoRootArrayScope() { masm_->set_root_array_available(old_value_); }

 private:
  TurboAssembler* masm_;
  bool old_value_;
};

// Wrapper class for passing expected and actual parameter counts as
// either registers or immediate values. Used to make sure that the
// caller provides exactly the expected number of parameters to the
// callee.
class ParameterCount {
 public:
  explicit ParameterCount(Register reg) : reg_(reg), immediate_(0) {}
  explicit ParameterCount(uint16_t imm) : reg_(no_reg), immediate_(imm) {}

  bool is_reg() const { return reg_.is_valid(); }
  bool is_immediate() const { return !is_reg(); }

  Register reg() const {
    DCHECK(is_reg());
    return reg_;
  }
  uint16_t immediate() const {
    DCHECK(is_immediate());
    return immediate_;
  }

 private:
  const Register reg_;
  const uint16_t immediate_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ParameterCount);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_MACRO_ASSEMBLER_H_
