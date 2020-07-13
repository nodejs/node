// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/stack.h"

#include <memory>
#include <ostream>

#include "include/v8config.h"
#include "src/base/platform/platform.h"
#include "testing/gtest/include/gtest/gtest.h"

#if V8_OS_LINUX && (V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64)
#include <xmmintrin.h>
#endif

namespace cppgc {
namespace internal {

namespace {

class GCStackTest : public ::testing::Test {
 protected:
  void SetUp() override {
    stack_.reset(new Stack(v8::base::Stack::GetStackStart()));
  }

  void TearDown() override { stack_.reset(); }

  Stack* GetStack() const { return stack_.get(); }

 private:
  std::unique_ptr<Stack> stack_;
};

}  // namespace

TEST_F(GCStackTest, IsOnStackForStackValue) {
  void* dummy;
  EXPECT_TRUE(GetStack()->IsOnStack(&dummy));
}

TEST_F(GCStackTest, IsOnStackForHeapValue) {
  auto dummy = std::make_unique<int>();
  EXPECT_FALSE(GetStack()->IsOnStack(dummy.get()));
}

namespace {

class StackScanner final : public StackVisitor {
 public:
  struct Container {
    std::unique_ptr<int> value;
  };

  StackScanner() : container_(new Container{}) {
    container_->value = std::make_unique<int>();
  }

  void VisitPointer(const void* address) final {
    if (address == container_->value.get()) found_ = true;
  }

  void Reset() { found_ = false; }
  bool found() const { return found_; }
  int* needle() const { return container_->value.get(); }

 private:
  std::unique_ptr<Container> container_;
  bool found_ = false;
};

}  // namespace

TEST_F(GCStackTest, IteratePointersFindsOnStackValue) {
  auto scanner = std::make_unique<StackScanner>();

  // No check that the needle is initially not found as on some platforms it
  // may be part of  temporaries after setting it up through StackScanner.
  {
    int* volatile tmp = scanner->needle();
    USE(tmp);
    GetStack()->IteratePointers(scanner.get());
    EXPECT_TRUE(scanner->found());
  }
}

TEST_F(GCStackTest, IteratePointersFindsOnStackValuePotentiallyUnaligned) {
  auto scanner = std::make_unique<StackScanner>();

  // No check that the needle is initially not found as on some platforms it
  // may be part of  temporaries after setting it up through StackScanner.
  {
    char a = 'c';
    USE(a);
    int* volatile tmp = scanner->needle();
    USE(tmp);
    GetStack()->IteratePointers(scanner.get());
    EXPECT_TRUE(scanner->found());
  }
}

namespace {

// Prevent inlining as that would allow the compiler to prove that the parameter
// must not actually be materialized.
//
// Parameter positiosn are explicit to test various calling conventions.
V8_NOINLINE void* RecursivelyPassOnParameterImpl(void* p1, void* p2, void* p3,
                                                 void* p4, void* p5, void* p6,
                                                 void* p7, void* p8,
                                                 Stack* stack,
                                                 StackVisitor* visitor) {
  if (p1) {
    return RecursivelyPassOnParameterImpl(nullptr, p1, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          stack, visitor);
  } else if (p2) {
    return RecursivelyPassOnParameterImpl(nullptr, nullptr, p2, nullptr,
                                          nullptr, nullptr, nullptr, nullptr,
                                          stack, visitor);
  } else if (p3) {
    return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, p3,
                                          nullptr, nullptr, nullptr, nullptr,
                                          stack, visitor);
  } else if (p4) {
    return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                          p4, nullptr, nullptr, nullptr, stack,
                                          visitor);
  } else if (p5) {
    return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                          nullptr, p5, nullptr, nullptr, stack,
                                          visitor);
  } else if (p6) {
    return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, p6, nullptr, stack,
                                          visitor);
  } else if (p7) {
    return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr, p7, stack,
                                          visitor);
  } else if (p8) {
    stack->IteratePointers(visitor);
    return p8;
  }
  return nullptr;
}

V8_NOINLINE void* RecursivelyPassOnParameter(size_t num, void* parameter,
                                             Stack* stack,
                                             StackVisitor* visitor) {
  switch (num) {
    case 0:
      stack->IteratePointers(visitor);
      return parameter;
    case 1:
      return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                            nullptr, nullptr, nullptr,
                                            parameter, stack, visitor);
    case 2:
      return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                            nullptr, nullptr, parameter,
                                            nullptr, stack, visitor);
    case 3:
      return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                            nullptr, parameter, nullptr,
                                            nullptr, stack, visitor);
    case 4:
      return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr, nullptr,
                                            parameter, nullptr, nullptr,
                                            nullptr, stack, visitor);
    case 5:
      return RecursivelyPassOnParameterImpl(nullptr, nullptr, nullptr,
                                            parameter, nullptr, nullptr,
                                            nullptr, nullptr, stack, visitor);
    case 6:
      return RecursivelyPassOnParameterImpl(nullptr, nullptr, parameter,
                                            nullptr, nullptr, nullptr, nullptr,
                                            nullptr, stack, visitor);
    case 7:
      return RecursivelyPassOnParameterImpl(nullptr, parameter, nullptr,
                                            nullptr, nullptr, nullptr, nullptr,
                                            nullptr, stack, visitor);
    case 8:
      return RecursivelyPassOnParameterImpl(parameter, nullptr, nullptr,
                                            nullptr, nullptr, nullptr, nullptr,
                                            nullptr, stack, visitor);
    default:
      UNREACHABLE();
  }
  UNREACHABLE();
}

}  // namespace

TEST_F(GCStackTest, IteratePointersFindsParameterNesting0) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(0, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(GCStackTest, IteratePointersFindsParameterNesting1) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(1, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(GCStackTest, IteratePointersFindsParameterNesting2) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(2, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(GCStackTest, IteratePointersFindsParameterNesting3) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(3, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(GCStackTest, IteratePointersFindsParameterNesting4) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(4, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(GCStackTest, IteratePointersFindsParameterNesting5) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(5, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(GCStackTest, IteratePointersFindsParameterNesting6) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(6, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(GCStackTest, IteratePointersFindsParameterNesting7) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(7, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

TEST_F(GCStackTest, IteratePointersFindsParameterNesting8) {
  auto scanner = std::make_unique<StackScanner>();
  void* needle = RecursivelyPassOnParameter(8, scanner->needle(), GetStack(),
                                            scanner.get());
  EXPECT_EQ(scanner->needle(), needle);
  EXPECT_TRUE(scanner->found());
}

// The following test uses inline assembly and has been checked to work on clang
// to verify that the stack-scanning trampoline pushes callee-saved registers.
//
// The test uses a macro loop as asm() can only be passed string literals.
#ifdef __clang__
#ifdef V8_TARGET_ARCH_X64
#ifdef V8_OS_WIN

// Excluded from test: rbp
#define FOR_ALL_CALLEE_SAVED_REGS(V) \
  V("rdi")                           \
  V("rsi")                           \
  V("rbx")                           \
  V("r12")                           \
  V("r13")                           \
  V("r14")                           \
  V("r15")

#else  // !V8_OS_WIN

// Excluded from test: rbp
#define FOR_ALL_CALLEE_SAVED_REGS(V) \
  V("rbx")                           \
  V("r12")                           \
  V("r13")                           \
  V("r14")                           \
  V("r15")

#endif  // !V8_OS_WIN
#endif  // V8_TARGET_ARCH_X64
#endif  // __clang__

#ifdef FOR_ALL_CALLEE_SAVED_REGS

TEST_F(GCStackTest, IteratePointersFindsCalleeSavedRegisters) {
  auto scanner = std::make_unique<StackScanner>();

  // No check that the needle is initially not found as on some platforms it
  // may be part of  temporaries after setting it up through StackScanner.

// First, clear all callee-saved registers.
#define CLEAR_REGISTER(reg) asm("mov $0, %%" reg : : : reg);

  FOR_ALL_CALLEE_SAVED_REGS(CLEAR_REGISTER)
#undef CLEAR_REGISTER

  // Keep local raw pointers to keep instruction sequences small below.
  auto* local_stack = GetStack();
  auto* local_scanner = scanner.get();

// Moves |local_scanner->needle()| into a callee-saved register, leaving the
// callee-saved register as the only register referencing the needle.
// (Ignoring implementation-dependent dirty registers/stack.)
#define KEEP_ALIVE_FROM_CALLEE_SAVED(reg)                                \
  local_scanner->Reset();                                                \
  /* This moves the temporary into the calee-saved register. */          \
  asm("mov %0, %%" reg : : "r"(local_scanner->needle()) : reg);          \
  /* Register is unprotected from here till the actual invocation. */    \
  local_stack->IteratePointers(local_scanner);                           \
  EXPECT_TRUE(local_scanner->found())                                    \
      << "pointer in callee-saved register not found. register: " << reg \
      << std::endl;                                                      \
  /* Clear out the register again */                                     \
  asm("mov $0, %%" reg : : : reg);

  FOR_ALL_CALLEE_SAVED_REGS(KEEP_ALIVE_FROM_CALLEE_SAVED)
#undef KEEP_ALIVE_FROM_CALLEE_SAVED
#undef FOR_ALL_CALLEE_SAVED_REGS
}
#endif  // FOR_ALL_CALLEE_SAVED_REGS

#if V8_OS_LINUX && (V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64)
class CheckStackAlignmentVisitor final : public StackVisitor {
 public:
  void VisitPointer(const void*) final {
    float f[4] = {0.};
    volatile auto xmm = ::_mm_load_ps(f);
    USE(xmm);
  }
};

TEST_F(GCStackTest, StackAlignment) {
  auto checker = std::make_unique<CheckStackAlignmentVisitor>();
  GetStack()->IteratePointers(checker.get());
}
#endif  // V8_OS_LINUX && (V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64)

}  // namespace internal
}  // namespace cppgc
