// Copyright 2025 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/base/attributes.h"

#include <cstdint>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "absl/base/config.h"

namespace {

TEST(Attributes, RequireExplicitInit) {
  struct Agg {
    int f1;
    int f2 ABSL_REQUIRE_EXPLICIT_INIT;
  };
  Agg good1 [[maybe_unused]] = {1, 2};
#if ABSL_INTERNAL_CPLUSPLUS_LANG >= 202002L
  Agg good2 [[maybe_unused]] (1, 2);
#endif
  Agg good3 [[maybe_unused]]{1, 2};
  Agg good4 [[maybe_unused]] = {1, 2};
  Agg good5 [[maybe_unused]] = Agg{1, 2};
  Agg good6 [[maybe_unused]][1] = {{1, 2}};
  Agg good7 [[maybe_unused]][1] = {Agg{1, 2}};
  union {
    Agg agg;
  } good8 [[maybe_unused]] = {{1, 2}};
  constexpr Agg good9 [[maybe_unused]] = {1, 2};
  constexpr Agg good10 [[maybe_unused]]{1, 2};
}

TEST(Attributes, MSVCBug) {
  struct ImplicitlyConstructible {
    // NOLINTNEXTLINE(google-explicit-constructor)
    ImplicitlyConstructible(const char*) {}
  };
  struct Agg {
    ImplicitlyConstructible f1 ABSL_REQUIRE_EXPLICIT_INIT;
  };
  static_assert(std::is_convertible_v<const char*, ImplicitlyConstructible>);
  Agg good1 [[maybe_unused]] = {ImplicitlyConstructible("hello")};
}

TEST(Attributes, UnsafeBuffers) {
  int buffer[10] = {0};
  int offset = 1;

  // Expression example
  int* p1 = ABSL_UNSAFE_BUFFERS(buffer + offset);
  EXPECT_EQ(p1, &buffer[1]);

  // Single-line statement example
  int* p2 = buffer;
  ABSL_UNSAFE_BUFFERS(p2 += offset;)
  EXPECT_EQ(p2, &buffer[1]);

  // Multi-line statement example
  ABSL_UNSAFE_BUFFERS(int* p3 = buffer + offset; p3[0] = 1;)
  EXPECT_EQ(buffer[1], 1);
}

ABSL_ATTRIBUTE_NOINLINE void MustTailVoidCallee(int* out, int x) { *out = x; }

ABSL_ATTRIBUTE_NOINLINE void MustTailVoidCaller(int* out, int x) {
  ABSL_INTERNAL_MUSTTAIL return MustTailVoidCallee(out, x + 2);
}

ABSL_ATTRIBUTE_NOINLINE int64_t MustTailCountdown(int64_t n, int64_t acc) {
  if (n == 0) return acc;
  ABSL_INTERNAL_MUSTTAIL return MustTailCountdown(n - 1, acc + n);
}

TEST(Attributes, MustTail) {
  int out = 0;
  MustTailVoidCaller(&out, 10);
  EXPECT_EQ(out, 12);

#if ABSL_INTERNAL_HAVE_MUSTTAIL
  // A 200,000-deep recursion would overflow the stack without tail-call.
  constexpr int64_t kDepth = 200000;
  EXPECT_EQ(MustTailCountdown(kDepth, 0), kDepth * (kDepth + 1) / 2);
#endif
}

ABSL_ATTRIBUTE_NO_SANITIZE_CFI int NoSanitizeCfiCall(int (*fn)(int), int x) {
  return fn(x);
}

TEST(Attributes, NoSanitizeCfi) {
  EXPECT_EQ(NoSanitizeCfiCall([](int x) { return x + 1; }, 41), 42);
}

// Verifies compile-time appertainment when C++11 `[[...]]` sanitizer/cold
// attributes are stacked together and before GNU `__attribute__((noinline))` on
// class member functions and member function templates.
class StackedNoSanitizeTarget {
 public:
  ABSL_ATTRIBUTE_NO_SANITIZE_ADDRESS ABSL_ATTRIBUTE_NO_SANITIZE_MEMORY int
  AddressAndMemory(int value) const {
    return value + 1;
  }

#if !defined(_MSC_VER) || defined(__clang__)
  // On native MSVC under ASan, ABSL_ATTRIBUTE_NO_SANITIZE_ADDRESS expands to
  // __declspec(no_sanitize_address) while ABSL_ATTRIBUTE_NOINLINE expands to
  // [[msvc::noinline]], which cannot follow __declspec.
  ABSL_ATTRIBUTE_NO_SANITIZE_ADDRESS ABSL_ATTRIBUTE_NOINLINE int
  AddressAndNoInline(int value) const {
    return value + 2;
  }

  template <typename T>
  ABSL_ATTRIBUTE_COLD ABSL_ATTRIBUTE_NO_SANITIZE_ADDRESS ABSL_ATTRIBUTE_NOINLINE
      T
      ColdAddressAndNoInline(T value) const {
    return value + 3;
  }
#endif
};

TEST(Attributes, StackedNoSanitizeMemberFunctions) {
  StackedNoSanitizeTarget target;
  EXPECT_EQ(target.AddressAndMemory(10), 11);
#if !defined(_MSC_VER) || defined(__clang__)
  EXPECT_EQ(target.AddressAndNoInline(20), 22);
  EXPECT_EQ(target.ColdAddressAndNoInline(30), 33);
#endif
}

}  // namespace
