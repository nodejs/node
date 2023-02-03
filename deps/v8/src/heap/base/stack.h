// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_BASE_STACK_H_
#define V8_HEAP_BASE_STACK_H_

#include "src/base/macros.h"
#include "src/base/platform/platform.h"

namespace heap::base {

class StackVisitor {
 public:
  virtual ~StackVisitor() = default;
  virtual void VisitPointer(const void* address) = 0;
};

// Abstraction over the stack. Supports handling of:
// - native stack;
// - ASAN/MSAN;
// - SafeStack: https://releases.llvm.org/10.0.0/tools/clang/docs/SafeStack.html
class V8_EXPORT_PRIVATE Stack final {
 public:
  // The following constant is architecture-specific. The size of the buffer
  // for storing the callee-saved registers is going to be equal to
  // NumberOfCalleeSavedRegisters * sizeof(intptr_t).

#if V8_HOST_ARCH_IA32
  // Must be consistent with heap/base/asm/ia32/.
  static constexpr int NumberOfCalleeSavedRegisters = 3;
#elif V8_HOST_ARCH_X64
#ifdef _WIN64
  // Must be consistent with heap/base/asm/x64/.
  static constexpr int NumberOfCalleeSavedRegisters = 28;
#else   // !_WIN64
  // Must be consistent with heap/base/asm/x64/.
  static constexpr int NumberOfCalleeSavedRegisters = 5;
#endif  // !_WIN64
#elif V8_HOST_ARCH_ARM64
  // Must be consistent with heap/base/asm/arm64/.
  static constexpr int NumberOfCalleeSavedRegisters = 11;
#elif V8_HOST_ARCH_ARM
  // Must be consistent with heap/base/asm/arm/.
  static constexpr int NumberOfCalleeSavedRegisters = 8;
#elif V8_HOST_ARCH_PPC64
  // Must be consistent with heap/base/asm/ppc/.
  static constexpr int NumberOfCalleeSavedRegisters = 20;
#elif V8_HOST_ARCH_PPC
  // Must be consistent with heap/base/asm/ppc/.
  static constexpr int NumberOfCalleeSavedRegisters = 20;
#elif V8_HOST_ARCH_MIPS64
  // Must be consistent with heap/base/asm/mips64el/.
  static constexpr int NumberOfCalleeSavedRegisters = 9;
#elif V8_HOST_ARCH_LOONG64
  // Must be consistent with heap/base/asm/loong64/.
  static constexpr int NumberOfCalleeSavedRegisters = 11;
#elif V8_HOST_ARCH_S390
  // Must be consistent with heap/base/asm/s390/.
  static constexpr int NumberOfCalleeSavedRegisters = 10;
#elif V8_HOST_ARCH_RISCV32
  // Must be consistent with heap/base/asm/riscv/.
  static constexpr int NumberOfCalleeSavedRegisters = 12;
#elif V8_HOST_ARCH_RISCV64
  // Must be consistent with heap/base/asm/riscv/.
  static constexpr int NumberOfCalleeSavedRegisters = 12;
#else
#error Unknown architecture.
#endif

  explicit Stack(const void* stack_start = nullptr);

  // Sets the start of the stack.
  void SetStackStart(const void* stack_start);

  // Returns true if |slot| is part of the stack and false otherwise.
  bool IsOnStack(const void* slot) const;

  // Word-aligned iteration of the stack. Callee-saved registers are pushed to
  // the stack before iterating pointers. Slot values are passed on to
  // `visitor`.
  void IteratePointers(StackVisitor* visitor) const;

  // Word-aligned iteration of the stack, starting at `stack_end`. Slot values
  // are passed on to `visitor`. This is intended to be used with verifiers that
  // only visit a subset of the stack of IteratePointers().
  //
  // **Ignores:**
  // - Callee-saved registers.
  // - SafeStack.
  void IteratePointersUnsafe(StackVisitor* visitor,
                             const void* stack_end) const;

  // Returns the start of the stack.
  const void* stack_start() const { return stack_start_; }

  // Sets, clears and gets the stack marker.
  void set_marker(const void* stack_marker);
  void clear_marker();
  const void* get_marker() const;

  // Mechanism for saving the callee-saved registers, required for conservative
  // stack scanning.

  struct CalleeSavedRegisters {
    // We always double-align this buffer, to support for longer registers,
    // e.g., 128-bit registers in WIN64.
    alignas(2 * sizeof(intptr_t))
        std::array<intptr_t, NumberOfCalleeSavedRegisters> buffer;
  };

  using Callback = void (*)(StackVisitor*, const void*, const void*,
                            const CalleeSavedRegisters* registers);

  static V8_NOINLINE void PushAllRegistersAndInvokeCallback(
      StackVisitor* visitor, const void* stack_start, Callback callback);

 private:
  const void* stack_start_;
  const void* stack_marker_ = nullptr;
};

}  // namespace heap::base

#endif  // V8_HEAP_BASE_STACK_H_
