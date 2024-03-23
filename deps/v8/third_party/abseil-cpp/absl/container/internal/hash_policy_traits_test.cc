// Copyright 2018 The Abseil Authors.
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

#include "absl/container/internal/hash_policy_traits.h"

#include <functional>
#include <memory>
#include <new>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {
namespace {

using ::testing::MockFunction;
using ::testing::Return;
using ::testing::ReturnRef;

using Alloc = std::allocator<int>;
using Slot = int;

struct PolicyWithoutOptionalOps {
  using slot_type = Slot;
  using key_type = Slot;
  using init_type = Slot;

  static std::function<Slot&(Slot*)> element;
  static int apply(int v) { return apply_impl(v); }
  static std::function<int(int)> apply_impl;
  static std::function<Slot&(Slot*)> value;
};

std::function<int(int)> PolicyWithoutOptionalOps::apply_impl;
std::function<Slot&(Slot*)> PolicyWithoutOptionalOps::value;

struct Test : ::testing::Test {
  Test() {
    PolicyWithoutOptionalOps::apply_impl = [&](int a1) -> int {
      return apply.Call(a1);
    };
    PolicyWithoutOptionalOps::value = [&](Slot* a1) -> Slot& {
      return value.Call(a1);
    };
  }

  std::allocator<int> alloc;
  int a = 53;
  MockFunction<int(int)> apply;
  MockFunction<Slot&(Slot*)> value;
};

TEST_F(Test, apply) {
  EXPECT_CALL(apply, Call(42)).WillOnce(Return(1337));
  EXPECT_EQ(1337, (hash_policy_traits<PolicyWithoutOptionalOps>::apply(42)));
}

TEST_F(Test, value) {
  int b = 0;
  EXPECT_CALL(value, Call(&a)).WillOnce(ReturnRef(b));
  EXPECT_EQ(&b, &hash_policy_traits<PolicyWithoutOptionalOps>::value(&a));
}

}  // namespace
}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl
