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

#include "absl/base/internal/nullability_traits.h"

#include <memory>
#include <type_traits>

#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/base/nullability.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {
namespace {

struct NotSmartPtr {
  int* x;
};

class ABSL_NULLABILITY_COMPATIBLE MySmartPtr : std::unique_ptr<int> {};

// The IsNullabilityCompatibleType trait value can only be true when we define
// `absl_nullable` (isn't a no-op).
#if defined(__clang__) && !defined(__OBJC__) && \
    ABSL_HAVE_FEATURE(nullability_on_classes)
#define EXPECT_TRUE_IF_SUPPORTED EXPECT_TRUE
#else
#define EXPECT_TRUE_IF_SUPPORTED EXPECT_FALSE
#endif

TEST(NullabilityTraitsTest, IsNullabilityEligibleTypePrimitives) {
  EXPECT_FALSE(IsNullabilityCompatibleType<int>::value);
  EXPECT_FALSE(IsNullabilityCompatibleType<int*&>::value);

  EXPECT_TRUE_IF_SUPPORTED(IsNullabilityCompatibleType<int*>::value);
  EXPECT_TRUE_IF_SUPPORTED(IsNullabilityCompatibleType<int**>::value);
  EXPECT_TRUE_IF_SUPPORTED(IsNullabilityCompatibleType<int* const>::value);
  EXPECT_TRUE_IF_SUPPORTED(IsNullabilityCompatibleType<const int*>::value);
  EXPECT_TRUE_IF_SUPPORTED(IsNullabilityCompatibleType<void (*)(int)>::value);
}

TEST(NullabilityTraitsTest, IsNullabilityCompatibleTypeAliases) {
  using MyInt = int;
  using MyIntPtr = int*;
  EXPECT_FALSE(IsNullabilityCompatibleType<MyInt>::value);
  EXPECT_TRUE_IF_SUPPORTED(IsNullabilityCompatibleType<MyIntPtr>::value);
}

TEST(NullabilityTraitsTest, IsNullabilityCompatibleTypeSmartPointers) {
  EXPECT_TRUE_IF_SUPPORTED(
      IsNullabilityCompatibleType<std::unique_ptr<int>>::value);
  EXPECT_TRUE_IF_SUPPORTED(
      IsNullabilityCompatibleType<std::shared_ptr<int>>::value);

  EXPECT_FALSE(IsNullabilityCompatibleType<NotSmartPtr>::value);
  EXPECT_TRUE_IF_SUPPORTED(IsNullabilityCompatibleType<NotSmartPtr*>::value);
  EXPECT_TRUE_IF_SUPPORTED(IsNullabilityCompatibleType<MySmartPtr>::value);
}

#undef EXPECT_TRUE_IF_SUPPORTED

TEST(NullabilityTraitsTest, AddNonnullIfCompatiblePassThroughPrimitives) {
  EXPECT_TRUE((std::is_same_v<AddNonnullIfCompatible<int>::type, int>));
  EXPECT_TRUE((std::is_same_v<AddNonnullIfCompatible<int*>::type, int*>));
  EXPECT_TRUE(
      (std::is_same_v<AddNonnullIfCompatible<int* const>::type, int* const>));
}

TEST(NullabilityTraitsTest, AddNonnullIfCompatiblePassThroughSmartPointers) {
  EXPECT_TRUE(
      (std::is_same_v<AddNonnullIfCompatible<std::unique_ptr<int>>::type,
                      std::unique_ptr<int>>));
  EXPECT_TRUE(
      (std::is_same_v<AddNonnullIfCompatible<std::shared_ptr<int>>::type,
                      std::shared_ptr<int>>));
  EXPECT_TRUE(
      (std::is_same_v<AddNonnullIfCompatible<NotSmartPtr>::type, NotSmartPtr>));
  EXPECT_TRUE(
      (std::is_same_v<AddNonnullIfCompatible<MySmartPtr>::type, MySmartPtr>));
}

}  // namespace
}  // namespace base_internal
ABSL_NAMESPACE_END
}  // namespace absl
