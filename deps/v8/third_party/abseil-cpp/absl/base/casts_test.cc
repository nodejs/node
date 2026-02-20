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

#include "absl/base/casts.h"

#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "absl/base/options.h"

namespace {

struct BaseForImplicitCast {
  explicit BaseForImplicitCast(int value) : x(value) {}
  BaseForImplicitCast(const BaseForImplicitCast& other) = delete;
  BaseForImplicitCast& operator=(const BaseForImplicitCast& other) = delete;
  int x;
};
struct DerivedForImplicitCast : BaseForImplicitCast {
  explicit DerivedForImplicitCast(int value) : BaseForImplicitCast(value) {}
};

static_assert(std::is_same_v<decltype(absl::implicit_cast<BaseForImplicitCast&>(
                                 std::declval<DerivedForImplicitCast&>())),
                             BaseForImplicitCast&>);
static_assert(
    std::is_same_v<decltype(absl::implicit_cast<const BaseForImplicitCast&>(
                       std::declval<DerivedForImplicitCast>())),
                   const BaseForImplicitCast&>);

TEST(ImplicitCastTest, LValueReference) {
  DerivedForImplicitCast derived(5);
  EXPECT_EQ(&absl::implicit_cast<BaseForImplicitCast&>(derived), &derived);
  EXPECT_EQ(&absl::implicit_cast<const BaseForImplicitCast&>(derived),
            &derived);
}

TEST(ImplicitCastTest, RValueReference) {
  DerivedForImplicitCast derived(5);
  BaseForImplicitCast&& base =
      absl::implicit_cast<BaseForImplicitCast&&>(std::move(derived));
  EXPECT_EQ(&base, &derived);

  const DerivedForImplicitCast cderived(6);
  const BaseForImplicitCast&& cbase =
      absl::implicit_cast<const BaseForImplicitCast&&>(std::move(cderived));
  EXPECT_EQ(&cbase, &cderived);
}

class BaseForDownCast {
 public:
  virtual ~BaseForDownCast() = default;
};

class DerivedForDownCast : public BaseForDownCast {};
class Derived2ForDownCast : public BaseForDownCast {};

TEST(DownCastTest, Pointer) {
  DerivedForDownCast derived;
  BaseForDownCast* const base_ptr = &derived;

  // Tests casting a BaseForDownCast* to a DerivedForDownCast*.
  EXPECT_EQ(&derived, absl::down_cast<DerivedForDownCast*>(base_ptr));

  // Tests casting a const BaseForDownCast* to a const DerivedForDownCast*.
  const BaseForDownCast* const_base_ptr = base_ptr;
  EXPECT_EQ(&derived,
            absl::down_cast<const DerivedForDownCast*>(const_base_ptr));

  // Tests casting a BaseForDownCast* to a const DerivedForDownCast*.
  EXPECT_EQ(&derived, absl::down_cast<const DerivedForDownCast*>(base_ptr));

  // Tests casting a BaseForDownCast* to a BaseForDownCast* (an identity cast).
  EXPECT_EQ(base_ptr, absl::down_cast<BaseForDownCast*>(base_ptr));

  // Tests down casting NULL.
  EXPECT_EQ(nullptr,
            (absl::down_cast<DerivedForDownCast*, BaseForDownCast>(nullptr)));

  // Tests a bad downcast. We have to disguise the badness just enough
  // that the compiler doesn't warn about it at compile time.
  BaseForDownCast* base2 = new BaseForDownCast();
#if GTEST_HAS_DEATH_TEST && (!defined(NDEBUG) || (ABSL_OPTION_HARDENED == 1 || \
                                                  ABSL_OPTION_HARDENED == 2))
  EXPECT_DEATH(static_cast<void>(absl::down_cast<DerivedForDownCast*>(base2)),
               ".*down cast from .*BaseForDownCast.* to "
               ".*DerivedForDownCast.* failed.*");
#endif
  delete base2;
}

TEST(DownCastTest, Reference) {
  DerivedForDownCast derived;
  BaseForDownCast& base_ref = derived;

  // Tests casting a BaseForDownCast& to a DerivedForDownCast&.
  // NOLINTNEXTLINE(runtime/casting)
  EXPECT_EQ(&derived, &absl::down_cast<DerivedForDownCast&>(base_ref));

  // Tests casting a const BaseForDownCast& to a const DerivedForDownCast&.
  const BaseForDownCast& const_base_ref = base_ref;
  // NOLINTNEXTLINE(runtime/casting)
  EXPECT_EQ(&derived,
            &absl::down_cast<const DerivedForDownCast&>(const_base_ref));

  // Tests casting a BaseForDownCast& to a const DerivedForDownCast&.
  // NOLINTNEXTLINE(runtime/casting)
  EXPECT_EQ(&derived, &absl::down_cast<const DerivedForDownCast&>(base_ref));

  // Tests casting a BaseForDownCast& to a BaseForDownCast& (an identity cast).
  // NOLINTNEXTLINE(runtime/casting)
  EXPECT_EQ(&base_ref, &absl::down_cast<BaseForDownCast&>(base_ref));

  // Tests a bad downcast. We have to disguise the badness just enough
  // that the compiler doesn't warn about it at compile time.
  BaseForDownCast& base2 = *new BaseForDownCast();
#if GTEST_HAS_DEATH_TEST && (!defined(NDEBUG) || (ABSL_OPTION_HARDENED == 1 || \
                                                  ABSL_OPTION_HARDENED == 2))
  EXPECT_DEATH(static_cast<void>(absl::down_cast<DerivedForDownCast&>(base2)),
               ".*down cast from .*BaseForDownCast.* to "
               ".*DerivedForDownCast.* failed.*");
#endif
  delete &base2;
}

TEST(DownCastTest, ErrorMessage) {
  DerivedForDownCast derived;
  BaseForDownCast& base = derived;
  (void)base;

#if GTEST_HAS_DEATH_TEST && (!defined(NDEBUG) || (ABSL_OPTION_HARDENED == 1 || \
                                                  ABSL_OPTION_HARDENED == 2))
  EXPECT_DEATH(static_cast<void>(absl::down_cast<Derived2ForDownCast&>(base)),
               ".*down cast from .*DerivedForDownCast.* to "
               ".*Derived2ForDownCast.* failed.*");
#endif
}

}  // namespace
