// Copyright 2025 The Abseil Authors.
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

#include "absl/meta/internal/constexpr_testing.h"

#include <map>
#include <string_view>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

TEST(ConstexprTesting, Basic) {
  using absl::meta_internal::HasConstexprEvaluation;

  EXPECT_TRUE(HasConstexprEvaluation([] {}));
  static constexpr int const_global = 7;
  EXPECT_TRUE(HasConstexprEvaluation([] { return const_global; }));
  EXPECT_TRUE(HasConstexprEvaluation([] { return 0; }));
  EXPECT_TRUE(HasConstexprEvaluation([] { return std::string_view{}; }));

  static int nonconst_global;
  EXPECT_FALSE(HasConstexprEvaluation([] { return nonconst_global; }));
  EXPECT_FALSE(HasConstexprEvaluation([] { std::abort(); }));
  EXPECT_FALSE(HasConstexprEvaluation([] { return std::map<int, int>(); }));
}

}  // namespace
