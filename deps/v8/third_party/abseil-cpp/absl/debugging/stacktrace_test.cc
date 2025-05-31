// Copyright 2023 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/debugging/stacktrace.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/optimization.h"
#include "absl/types/span.h"

static int g_should_fixup_calls = 0;
static int g_fixup_calls = 0;
static bool g_enable_fixup = false;

#if ABSL_HAVE_ATTRIBUTE_WEAK
bool absl::internal_stacktrace::ShouldFixUpStack() {
  ++g_should_fixup_calls;
  return g_enable_fixup;
}

void absl::internal_stacktrace::FixUpStack(void**, uintptr_t*, int*, size_t,
                                           size_t&) {
  ++g_fixup_calls;
}
#endif

namespace {

using ::testing::ContainerEq;
using ::testing::Contains;
using ::testing::internal::Cleanup;

struct StackTrace {
  static constexpr int kStackCount = 64;
  int depth;
  void* result[kStackCount];
  uintptr_t frames[kStackCount];
  int sizes[kStackCount];
};

// This test is currently only known to pass on Linux x86_64/aarch64.
#if defined(__linux__) && (defined(__x86_64__) || defined(__aarch64__))
ABSL_ATTRIBUTE_NOINLINE void Unwind(void* p) {
  ABSL_ATTRIBUTE_UNUSED static void* volatile sink = p;
  constexpr int kSize = 16;
  void* stack[kSize];
  int frames[kSize];
  absl::GetStackTrace(stack, kSize, 0);
  absl::GetStackFrames(stack, frames, kSize, 0);
}

ABSL_ATTRIBUTE_NOINLINE void HugeFrame() {
  char buffer[1 << 20];
  Unwind(buffer);
  ABSL_BLOCK_TAIL_CALL_OPTIMIZATION();
}

TEST(StackTrace, HugeFrame) {
  // Ensure that the unwinder is not confused by very large stack frames.
  HugeFrame();
  ABSL_BLOCK_TAIL_CALL_OPTIMIZATION();
}
#endif

// This is a separate function to avoid inlining.
ABSL_ATTRIBUTE_NOINLINE static void FixupNoFixupEquivalenceNoInline() {
#if defined(__riscv)
  GTEST_SKIP() << "Skipping test on RISC-V due to pre-existing failure";
#endif

#if ABSL_HAVE_ATTRIBUTE_WEAK
  // This test is known not to pass on MSVC (due to weak symbols)

  const Cleanup restore_state([enable_fixup = g_enable_fixup,
                               fixup_calls = g_fixup_calls,
                               should_fixup_calls = g_should_fixup_calls]() {
    g_enable_fixup = enable_fixup;
    g_fixup_calls = fixup_calls;
    g_should_fixup_calls = should_fixup_calls;
  });

  constexpr int kSkip = 1;  // Skip our own frame, whose return PCs won't match
  constexpr auto kStackCount = 1;

  StackTrace a;
  StackTrace b;

  // ==========================================================================

  g_fixup_calls = 0;
  g_should_fixup_calls = 0;
  a.depth = absl::GetStackTrace(a.result, kStackCount, kSkip);
  g_enable_fixup = !g_enable_fixup;
  b.depth = absl::GetStackTrace(b.result, kStackCount, kSkip);
  EXPECT_THAT(
      absl::MakeSpan(a.result, static_cast<size_t>(a.depth)),
      ContainerEq(absl::MakeSpan(b.result, static_cast<size_t>(b.depth))));
  EXPECT_GT(g_should_fixup_calls, 0);
  EXPECT_GE(g_should_fixup_calls, g_fixup_calls);

  // ==========================================================================

  g_fixup_calls = 0;
  g_should_fixup_calls = 0;
  a.depth = absl::GetStackFrames(a.result, a.sizes, kStackCount, kSkip);
  g_enable_fixup = !g_enable_fixup;
  b.depth = absl::GetStackFrames(b.result, b.sizes, kStackCount, kSkip);
  EXPECT_THAT(
      absl::MakeSpan(a.result, static_cast<size_t>(a.depth)),
      ContainerEq(absl::MakeSpan(b.result, static_cast<size_t>(b.depth))));
  EXPECT_THAT(
      absl::MakeSpan(a.sizes, static_cast<size_t>(a.depth)),
      ContainerEq(absl::MakeSpan(b.sizes, static_cast<size_t>(b.depth))));
  EXPECT_GT(g_should_fixup_calls, 0);
  EXPECT_GE(g_should_fixup_calls, g_fixup_calls);

  // ==========================================================================

  g_fixup_calls = 0;
  g_should_fixup_calls = 0;
  a.depth = absl::GetStackTraceWithContext(a.result, kStackCount, kSkip,
                                           nullptr, nullptr);
  g_enable_fixup = !g_enable_fixup;
  b.depth = absl::GetStackTraceWithContext(b.result, kStackCount, kSkip,
                                           nullptr, nullptr);
  EXPECT_THAT(
      absl::MakeSpan(a.result, static_cast<size_t>(a.depth)),
      ContainerEq(absl::MakeSpan(b.result, static_cast<size_t>(b.depth))));
  EXPECT_GT(g_should_fixup_calls, 0);
  EXPECT_GE(g_should_fixup_calls, g_fixup_calls);

  // ==========================================================================

  g_fixup_calls = 0;
  g_should_fixup_calls = 0;
  a.depth = absl::GetStackFramesWithContext(a.result, a.sizes, kStackCount,
                                            kSkip, nullptr, nullptr);
  g_enable_fixup = !g_enable_fixup;
  b.depth = absl::GetStackFramesWithContext(b.result, b.sizes, kStackCount,
                                            kSkip, nullptr, nullptr);
  EXPECT_THAT(
      absl::MakeSpan(a.result, static_cast<size_t>(a.depth)),
      ContainerEq(absl::MakeSpan(b.result, static_cast<size_t>(b.depth))));
  EXPECT_THAT(
      absl::MakeSpan(a.sizes, static_cast<size_t>(a.depth)),
      ContainerEq(absl::MakeSpan(b.sizes, static_cast<size_t>(b.depth))));
  EXPECT_GT(g_should_fixup_calls, 0);
  EXPECT_GE(g_should_fixup_calls, g_fixup_calls);

  // ==========================================================================

  g_fixup_calls = 0;
  g_should_fixup_calls = 0;
  a.depth = absl::internal_stacktrace::GetStackFrames(
      a.result, a.frames, a.sizes, kStackCount, kSkip);
  g_enable_fixup = !g_enable_fixup;
  b.depth = absl::internal_stacktrace::GetStackFrames(
      b.result, b.frames, b.sizes, kStackCount, kSkip);
  EXPECT_THAT(
      absl::MakeSpan(a.result, static_cast<size_t>(a.depth)),
      ContainerEq(absl::MakeSpan(b.result, static_cast<size_t>(b.depth))));
  EXPECT_THAT(
      absl::MakeSpan(a.sizes, static_cast<size_t>(a.depth)),
      ContainerEq(absl::MakeSpan(b.sizes, static_cast<size_t>(b.depth))));
  EXPECT_THAT(
      absl::MakeSpan(a.frames, static_cast<size_t>(a.depth)),
      ContainerEq(absl::MakeSpan(b.frames, static_cast<size_t>(b.depth))));
  EXPECT_GT(g_should_fixup_calls, 0);
  EXPECT_GE(g_should_fixup_calls, g_fixup_calls);

  // ==========================================================================

  g_fixup_calls = 0;
  g_should_fixup_calls = 0;
  a.depth = absl::internal_stacktrace::GetStackFramesWithContext(
      a.result, a.frames, a.sizes, kStackCount, kSkip, nullptr, nullptr);
  g_enable_fixup = !g_enable_fixup;
  b.depth = absl::internal_stacktrace::GetStackFramesWithContext(
      b.result, b.frames, b.sizes, kStackCount, kSkip, nullptr, nullptr);
  EXPECT_THAT(
      absl::MakeSpan(a.result, static_cast<size_t>(a.depth)),
      ContainerEq(absl::MakeSpan(b.result, static_cast<size_t>(b.depth))));
  EXPECT_THAT(
      absl::MakeSpan(a.sizes, static_cast<size_t>(a.depth)),
      ContainerEq(absl::MakeSpan(b.sizes, static_cast<size_t>(b.depth))));
  EXPECT_THAT(
      absl::MakeSpan(a.frames, static_cast<size_t>(a.depth)),
      ContainerEq(absl::MakeSpan(b.frames, static_cast<size_t>(b.depth))));
  EXPECT_GT(g_should_fixup_calls, 0);
  EXPECT_GE(g_should_fixup_calls, g_fixup_calls);

  // ==========================================================================
#else
  GTEST_SKIP() << "Need weak symbol support";
#endif
}

TEST(StackTrace, FixupNoFixupEquivalence) { FixupNoFixupEquivalenceNoInline(); }

#if ABSL_HAVE_BUILTIN(__builtin_frame_address)
struct FrameInfo {
  const void* return_address;
  uintptr_t frame_address;
};

// Returns the canonical frame address and return address for the current stack
// frame, while capturing the stack trace at the same time.
// This performs any platform-specific adjustments necessary to convert from the
// compiler built-ins to the expected API outputs.
ABSL_ATTRIBUTE_NO_SANITIZE_ADDRESS     // May read random elements from stack.
    ABSL_ATTRIBUTE_NO_SANITIZE_MEMORY  // May read random elements from stack.
        ABSL_ATTRIBUTE_NOINLINE static FrameInfo
        CaptureBacktraceNoInline(StackTrace& backtrace) {
  FrameInfo result;
  result.return_address = __builtin_return_address(0);
  // Large enough to cover all realistic slots the return address could be in
  const int kMaxReturnAddressIndex = 5;
  void* const* bfa = static_cast<void* const*>(__builtin_frame_address(0));
  backtrace.depth = absl::internal_stacktrace::GetStackFramesWithContext(
      backtrace.result, backtrace.frames, backtrace.sizes,
      StackTrace::kStackCount, /*skip_count=*/0,
      /*uc=*/nullptr, /*min_dropped_frames=*/nullptr);
  // Make sure the return address is at a reasonable location in the frame
  ptrdiff_t i;
  for (i = 0; i < kMaxReturnAddressIndex; ++i) {
    // Avoid std::find() here, since it lacks no-sanitize attributes.
    if (bfa[i] == result.return_address) {
      break;
    }
  }
  result.frame_address =
      i < kMaxReturnAddressIndex
          ? reinterpret_cast<uintptr_t>(
                bfa + i + 1 /* get the Canonical Frame Address (CFA) */)
          : 0;
  return result;
}

TEST(StackTrace, CanonicalFrameAddresses) {
  // Now capture a stack trace and verify that the return addresses and frame
  // addresses line up for one frame.
  StackTrace backtrace;
  const auto [return_address, frame_address] =
      CaptureBacktraceNoInline(backtrace);
  auto return_addresses = absl::MakeSpan(backtrace.result)
                              .subspan(0, static_cast<size_t>(backtrace.depth));
  auto frame_addresses = absl::MakeSpan(backtrace.frames)
                             .subspan(0, static_cast<size_t>(backtrace.depth));

  // Many platforms don't support this by default.
  bool support_is_expected = false;

  if (support_is_expected) {
    // If all zeros were returned, that is valid per the function's contract.
    // It just means we don't support returning frame addresses on this
    // platform.
    bool supported = static_cast<size_t>(std::count(frame_addresses.begin(),
                                                    frame_addresses.end(), 0)) <
                     frame_addresses.size();
    EXPECT_TRUE(supported);
    if (supported) {
      ASSERT_TRUE(frame_address)
          << "unable to obtain frame address corresponding to return address";
      EXPECT_THAT(return_addresses, Contains(return_address).Times(1));
      EXPECT_THAT(frame_addresses, Contains(frame_address).Times(1));
      ptrdiff_t ifound = std::find(return_addresses.begin(),
                                   return_addresses.end(), return_address) -
                         return_addresses.begin();
      // Make sure we found the frame in the first place.
      ASSERT_LT(ifound, backtrace.depth);
      // Make sure the frame address actually corresponds to the return
      // address.
      EXPECT_EQ(frame_addresses[static_cast<size_t>(ifound)], frame_address);
      // Make sure the addresses only appear once.
    }
  }
}
#endif

// This test is Linux specific.
#if defined(__linux__)
const void* g_return_address = nullptr;
bool g_sigusr2_raised = false;

void SigUsr2Handler(int, siginfo_t*, void* uc) {
  // Many platforms don't support this by default.
  bool support_is_expected = false;
  constexpr int kMaxStackDepth = 64;
  void* result[kMaxStackDepth];
  int depth =
      absl::GetStackTraceWithContext(result, kMaxStackDepth, 0, uc, nullptr);
  // Verify we can unwind past the nested signal handlers.
  if (support_is_expected) {
    EXPECT_THAT(absl::MakeSpan(result, static_cast<size_t>(depth)),
                Contains(g_return_address).Times(1));
  }
  depth = absl::GetStackTrace(result, kMaxStackDepth, 0);
  if (support_is_expected) {
    EXPECT_THAT(absl::MakeSpan(result, static_cast<size_t>(depth)),
                Contains(g_return_address).Times(1));
  }
  g_sigusr2_raised = true;
}

void SigUsr1Handler(int, siginfo_t*, void*) {
  raise(SIGUSR2);
  ABSL_BLOCK_TAIL_CALL_OPTIMIZATION();
}

ABSL_ATTRIBUTE_NOINLINE void RaiseSignal() {
  g_return_address = __builtin_return_address(0);
  raise(SIGUSR1);
  ABSL_BLOCK_TAIL_CALL_OPTIMIZATION();
}

ABSL_ATTRIBUTE_NOINLINE void TestNestedSignal() {
  constexpr size_t kAltstackSize = 1 << 14;
  // Allocate altstack on regular stack to make sure it'll have a higher
  // address than some of the regular stack frames.
  char space[kAltstackSize];
  stack_t altstack;
  stack_t old_stack;
  altstack.ss_sp = space;
  altstack.ss_size = kAltstackSize;
  altstack.ss_flags = 0;
  ASSERT_EQ(sigaltstack(&altstack, &old_stack), 0) << strerror(errno);
  struct sigaction act;
  struct sigaction oldusr1act;
  struct sigaction oldusr2act;
  act.sa_sigaction = SigUsr1Handler;
  act.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&act.sa_mask);
  ASSERT_EQ(sigaction(SIGUSR1, &act, &oldusr1act), 0) << strerror(errno);
  act.sa_sigaction = SigUsr2Handler;
  ASSERT_EQ(sigaction(SIGUSR2, &act, &oldusr2act), 0) << strerror(errno);
  RaiseSignal();
  ASSERT_EQ(sigaltstack(&old_stack, nullptr), 0) << strerror(errno);
  ASSERT_EQ(sigaction(SIGUSR1, &oldusr1act, nullptr), 0) << strerror(errno);
  ASSERT_EQ(sigaction(SIGUSR2, &oldusr2act, nullptr), 0) << strerror(errno);
  ABSL_BLOCK_TAIL_CALL_OPTIMIZATION();
}

TEST(StackTrace, NestedSignal) {
  // Verify we can unwind past the nested signal handlers.
  TestNestedSignal();
  EXPECT_TRUE(g_sigusr2_raised);
}
#endif

}  // namespace
