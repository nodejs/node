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

#include "absl/base/nullability.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"

namespace {
void funcWithNonnullArg(int* absl_nonnull /*arg*/) {}
template <typename T>
void funcWithDeducedNonnullArg(T* absl_nonnull /*arg*/) {}

TEST(NonnullTest, NonnullArgument) {
  int var = 0;
  funcWithNonnullArg(&var);
  funcWithDeducedNonnullArg(&var);
}

int* absl_nonnull funcWithNonnullReturn() {
  static int var = 0;
  return &var;
}

TEST(NonnullTest, NonnullReturn) {
  auto var = funcWithNonnullReturn();
  (void)var;
}

TEST(PassThroughTest, PassesThroughRawPointerToInt) {
  EXPECT_TRUE((std::is_same_v<int* absl_nonnull, int*>));
  EXPECT_TRUE((std::is_same_v<int* absl_nullable, int*>));
  EXPECT_TRUE((std::is_same_v<int* absl_nullability_unknown, int*>));
}

TEST(PassThroughTest, PassesThroughRawPointerToVoid) {
  EXPECT_TRUE((std::is_same_v<void* absl_nonnull, void*>));
  EXPECT_TRUE((std::is_same_v<void* absl_nullable, void*>));
  EXPECT_TRUE((std::is_same_v<void* absl_nullability_unknown, void*>));
}

TEST(PassThroughTest, PassesThroughUniquePointerToInt) {
  using T = std::unique_ptr<int>;
  EXPECT_TRUE((std::is_same_v<absl_nonnull T, T>));
  EXPECT_TRUE((std::is_same_v<absl_nullable T, T>));
  EXPECT_TRUE((std::is_same_v<absl_nullability_unknown T, T>));
}

TEST(PassThroughTest, PassesThroughSharedPointerToInt) {
  using T = std::shared_ptr<int>;
  EXPECT_TRUE((std::is_same_v<absl_nonnull T, T>));
  EXPECT_TRUE((std::is_same_v<absl_nullable T, T>));
  EXPECT_TRUE((std::is_same_v<absl_nullability_unknown T, T>));
}

TEST(PassThroughTest, PassesThroughSharedPointerToVoid) {
  using T = std::shared_ptr<void>;
  EXPECT_TRUE((std::is_same_v<absl_nonnull T, T>));
  EXPECT_TRUE((std::is_same_v<absl_nullable T, T>));
  EXPECT_TRUE((std::is_same_v<absl_nullability_unknown T, T>));
}

TEST(PassThroughTest, PassesThroughPointerToMemberObject) {
  using T = decltype(&std::pair<int, int>::first);
  EXPECT_TRUE((std::is_same_v<absl_nonnull T, T>));
  EXPECT_TRUE((std::is_same_v<absl_nullable T, T>));
  EXPECT_TRUE((std::is_same_v<absl_nullability_unknown T, T>));
}

TEST(PassThroughTest, PassesThroughPointerToMemberFunction) {
  using T = decltype(&std::unique_ptr<int>::reset);
  EXPECT_TRUE((std::is_same_v<absl_nonnull T, T>));
  EXPECT_TRUE((std::is_same_v<absl_nullable T, T>));
  EXPECT_TRUE((std::is_same_v<absl_nullability_unknown T, T>));
}
}  // namespace
