// Copyright 2020 The Abseil Authors.
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

#include "absl/base/fast_type_id.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "gtest/gtest.h"
#include "absl/base/macros.h"

namespace {

// NOLINTBEGIN(runtime/int)
#define PRIM_TYPES(A)   \
  A(bool)               \
  A(short)              \
  A(unsigned short)     \
  A(int)                \
  A(unsigned int)       \
  A(long)               \
  A(unsigned long)      \
  A(long long)          \
  A(unsigned long long) \
  A(float)              \
  A(double)             \
  A(long double)
// NOLINTEND(runtime/int)

TEST(FastTypeIdTest, PrimitiveTypes) {
  // clang-format off
  constexpr absl::FastTypeIdType kTypeIds[] = {
#define A(T) absl::FastTypeId<T>(),
    PRIM_TYPES(A)
#undef A
#define A(T) absl::FastTypeId<const T>(),
    PRIM_TYPES(A)
#undef A
#define A(T) absl::FastTypeId<volatile T>(),
    PRIM_TYPES(A)
#undef A
#define A(T) absl::FastTypeId<const volatile T>(),
    PRIM_TYPES(A)
#undef A
  };
  // clang-format on

  for (size_t i = 0; i < ABSL_ARRAYSIZE(kTypeIds); ++i) {
    EXPECT_EQ(kTypeIds[i], kTypeIds[i]);
    for (size_t j = 0; j < i; ++j) {
      EXPECT_NE(kTypeIds[i], kTypeIds[j]);
    }
  }
}

#define FIXED_WIDTH_TYPES(A) \
  A(int8_t)                  \
  A(uint8_t)                 \
  A(int16_t)                 \
  A(uint16_t)                \
  A(int32_t)                 \
  A(uint32_t)                \
  A(int64_t)                 \
  A(uint64_t)

TEST(FastTypeIdTest, FixedWidthTypes) {
  // clang-format off
  constexpr absl::FastTypeIdType kTypeIds[] = {
#define A(T) absl::FastTypeId<T>(),
    FIXED_WIDTH_TYPES(A)
#undef A
#define A(T) absl::FastTypeId<const T>(),
    FIXED_WIDTH_TYPES(A)
#undef A
#define A(T) absl::FastTypeId<volatile T>(),
    FIXED_WIDTH_TYPES(A)
#undef A
#define A(T) absl::FastTypeId<const volatile T>(),
    FIXED_WIDTH_TYPES(A)
#undef A
  };
  // clang-format on

  for (size_t i = 0; i < ABSL_ARRAYSIZE(kTypeIds); ++i) {
    EXPECT_EQ(kTypeIds[i], kTypeIds[i]);
    for (size_t j = 0; j < i; ++j) {
      EXPECT_NE(kTypeIds[i], kTypeIds[j]);
    }
  }
}

TEST(FastTypeIdTest, AliasTypes) {
  using int_alias = int;
  EXPECT_EQ(absl::FastTypeId<int_alias>(), absl::FastTypeId<int>());
}

TEST(FastTypeIdTest, TemplateSpecializations) {
  EXPECT_NE(absl::FastTypeId<std::vector<int>>(),
            absl::FastTypeId<std::vector<long>>());  // NOLINT(runtime/int)

  EXPECT_NE((absl::FastTypeId<std::map<int, float>>()),
            (absl::FastTypeId<std::map<int, double>>()));
}

struct Base {};
struct Derived : Base {};
struct PDerived : private Base {};

TEST(FastTypeIdTest, Inheritance) {
  EXPECT_NE(absl::FastTypeId<Base>(), absl::FastTypeId<Derived>());
  EXPECT_NE(absl::FastTypeId<Base>(), absl::FastTypeId<PDerived>());
}

}  // namespace
