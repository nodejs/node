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

#include "absl/container/internal/container_memory.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/base/no_destructor.h"
#include "absl/container/internal/test_instance_tracker.h"
#include "absl/functional/overload.h"
#include "absl/hash/hash.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {
namespace {

using ::absl::test_internal::CopyableMovableInstance;
using ::absl::test_internal::InstanceTracker;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Gt;
using ::testing::Pair;

#if ABSL_HAVE_BUILTIN(__builtin_infer_alloc_token)
TEST(Memory, AlignedTypeAllocToken) {
#if defined(__wasm__)
  GTEST_SKIP() << "Fails on wasm due to lack of heap partitioning support.";
#endif
  EXPECT_GT(__builtin_infer_alloc_token(sizeof(AlignedType<alignof(void*)>)),
            __builtin_infer_alloc_token(sizeof(int)));
}
#endif

TEST(Memory, AlignmentLargerThanBase) {
  std::allocator<int8_t> alloc;
  void* mem = Allocate<2>(&alloc, 3);
  EXPECT_EQ(0, reinterpret_cast<uintptr_t>(mem) % 2);
  memcpy(mem, "abc", 3);
  Deallocate<2>(&alloc, mem, 3);
}

TEST(Memory, AlignmentSmallerThanBase) {
  std::allocator<int64_t> alloc;
  void* mem = Allocate<2>(&alloc, 3);
  EXPECT_EQ(0, reinterpret_cast<uintptr_t>(mem) % 2);
  memcpy(mem, "abc", 3);
  Deallocate<2>(&alloc, mem, 3);
}

std::map<std::type_index, int>& AllocationMap() {
  static absl::NoDestructor<std::map<std::type_index, int>> map;
  return *map;
}

template <typename T>
struct TypeCountingAllocator {
  TypeCountingAllocator() = default;
  template <typename U>
  TypeCountingAllocator(const TypeCountingAllocator<U>&) {}  // NOLINT

  using value_type = T;

  T* allocate(size_t n, const void* = nullptr) {
    AllocationMap()[typeid(T)] += n;
    return std::allocator<T>().allocate(n);
  }
  void deallocate(T* p, std::size_t n) {
    AllocationMap()[typeid(T)] -= n;
    return std::allocator<T>().deallocate(p, n);
  }
};

TEST(Memory, AllocateDeallocateMatchType) {
  TypeCountingAllocator<int> alloc;
  void* mem = Allocate<1>(&alloc, 1);
  // Verify that it was allocated
  EXPECT_THAT(AllocationMap(), ElementsAre(Pair(_, Gt(0))));
  Deallocate<1>(&alloc, mem, 1);
  // Verify that the deallocation matched.
  EXPECT_THAT(AllocationMap(), ElementsAre(Pair(_, 0)));
}

class Fixture : public ::testing::Test {
  using Alloc = std::allocator<std::string>;

 public:
  Fixture() { ptr_ = std::allocator_traits<Alloc>::allocate(*alloc(), 1); }
  ~Fixture() override {
    std::allocator_traits<Alloc>::destroy(*alloc(), ptr_);
    std::allocator_traits<Alloc>::deallocate(*alloc(), ptr_, 1);
  }
  std::string* ptr() { return ptr_; }
  Alloc* alloc() { return &alloc_; }

 private:
  Alloc alloc_;
  std::string* ptr_;
};

TEST_F(Fixture, ConstructNoArgs) {
  ConstructFromTuple(alloc(), ptr(), std::forward_as_tuple());
  EXPECT_EQ(*ptr(), "");
}

TEST_F(Fixture, ConstructOneArg) {
  ConstructFromTuple(alloc(), ptr(), std::forward_as_tuple("abcde"));
  EXPECT_EQ(*ptr(), "abcde");
}

TEST_F(Fixture, ConstructTwoArg) {
  ConstructFromTuple(alloc(), ptr(), std::forward_as_tuple(5, 'a'));
  EXPECT_EQ(*ptr(), "aaaaa");
}

TEST(PairArgs, NoArgs) {
  EXPECT_THAT(PairArgs(),
              Pair(std::forward_as_tuple(), std::forward_as_tuple()));
}

TEST(PairArgs, TwoArgs) {
  EXPECT_EQ(
      std::make_pair(std::forward_as_tuple(1), std::forward_as_tuple('A')),
      PairArgs(1, 'A'));
}

TEST(PairArgs, Pair) {
  EXPECT_EQ(
      std::make_pair(std::forward_as_tuple(1), std::forward_as_tuple('A')),
      PairArgs(std::make_pair(1, 'A')));
}

TEST(PairArgs, Piecewise) {
  EXPECT_EQ(
      std::make_pair(std::forward_as_tuple(1), std::forward_as_tuple('A')),
      PairArgs(std::piecewise_construct, std::forward_as_tuple(1),
               std::forward_as_tuple('A')));
}

TEST(WithConstructed, Simple) {
  EXPECT_EQ(1, WithConstructed<absl::string_view>(
                   std::make_tuple(std::string("a")),
                   [](absl::string_view str) { return str.size(); }));
}

template <class F, class Arg>
decltype(DecomposeValue(std::declval<F>(), std::declval<Arg>()))
DecomposeValueImpl(int, F&& f, Arg&& arg) {
  return DecomposeValue(std::forward<F>(f), std::forward<Arg>(arg));
}

template <class F, class Arg>
const char* DecomposeValueImpl(char, F&&, Arg&&) {
  return "not decomposable";
}

template <class F, class Arg>
decltype(DecomposeValueImpl(0, std::declval<F>(), std::declval<Arg>()))
TryDecomposeValue(F&& f, Arg&& arg) {
  return DecomposeValueImpl(0, std::forward<F>(f), std::forward<Arg>(arg));
}

TEST(DecomposeValue, Decomposable) {
  auto f = [](const int& x, int&& y) {  // NOLINT
    EXPECT_EQ(&x, &y);
    EXPECT_EQ(42, x);
    return 'A';
  };
  EXPECT_EQ('A', TryDecomposeValue(f, 42));
}

TEST(DecomposeValue, NotDecomposable) {
  auto f = [](void*) {
    ADD_FAILURE() << "Must not be called";
    return 'A';
  };
  EXPECT_STREQ("not decomposable", TryDecomposeValue(f, 42));
}

template <class F, class... Args>
decltype(DecomposePair(std::declval<F>(), std::declval<Args>()...))
DecomposePairImpl(int, F&& f, Args&&... args) {
  return DecomposePair(std::forward<F>(f), std::forward<Args>(args)...);
}

template <class F, class... Args>
const char* DecomposePairImpl(char, F&&, Args&&...) {
  return "not decomposable";
}

template <class F, class... Args>
decltype(DecomposePairImpl(0, std::declval<F>(), std::declval<Args>()...))
TryDecomposePair(F&& f, Args&&... args) {
  return DecomposePairImpl(0, std::forward<F>(f), std::forward<Args>(args)...);
}

TEST(DecomposePair, Decomposable) {
  static constexpr auto f0 = [](const int& x,  // NOLINT
                                int&& k, double&& v) {
    EXPECT_EQ(&x, &k);
    EXPECT_EQ(42, x);
    EXPECT_EQ(0.5, v);
    return 'A';
  };
  static constexpr auto f = absl::Overload(
      [](const int& x,  // NOLINT
         std::piecewise_construct_t, std::tuple<int&&> k,
         std::tuple<double>&& v) {
        return f0(x, std::get<0>(std::move(k)), std::get<0>(std::move(v)));
      },
      f0);
  EXPECT_EQ('A', TryDecomposePair(f, std::pair<int&&, double&&>(42, 0.5)));
  EXPECT_EQ('A', TryDecomposePair(f, 42, 0.5));
  EXPECT_EQ('A', TryDecomposePair(f, std::make_pair(42, 0.5)));
  EXPECT_EQ('A', TryDecomposePair(f, std::piecewise_construct,
                                  std::make_tuple(42), std::make_tuple(0.5)));
}

TEST(DecomposePair, NotDecomposable) {
  auto f = [](...) {
    ADD_FAILURE() << "Must not be called";
    return 'A';
  };
  EXPECT_STREQ("not decomposable", TryDecomposePair(f));
  EXPECT_STREQ("not decomposable",
               TryDecomposePair(f, std::piecewise_construct, std::make_tuple(),
                                std::make_tuple(0.5)));
}

TEST(MapSlotPolicy, ConstKeyAndValue) {
  using slot_policy = map_slot_policy<const CopyableMovableInstance,
                                      const CopyableMovableInstance>;
  using slot_type = typename slot_policy::slot_type;

  union Slots {
    Slots() {}
    ~Slots() {}
    slot_type slots[100];
  } slots;

  std::allocator<
      std::pair<const CopyableMovableInstance, const CopyableMovableInstance>>
      alloc;
  InstanceTracker tracker;
  slot_policy::construct(&alloc, &slots.slots[0], CopyableMovableInstance(1),
                         CopyableMovableInstance(1));
  for (int i = 0; i < 99; ++i) {
    slot_policy::transfer(&alloc, &slots.slots[i + 1], &slots.slots[i]);
  }
  slot_policy::destroy(&alloc, &slots.slots[99]);

  EXPECT_EQ(tracker.copies(), 0);
}

TEST(MapSlotPolicy, TransferReturnsTrue) {
  {
    using slot_policy = map_slot_policy<int, float>;
    EXPECT_TRUE(
        (std::is_same_v<decltype(slot_policy::transfer<std::allocator<char>>(
                            nullptr, nullptr, nullptr)),
                        std::true_type>));
  }
  {
    struct NonRelocatable {
      NonRelocatable() = default;
      NonRelocatable(NonRelocatable&&) {}
      NonRelocatable& operator=(NonRelocatable&&) { return *this; }
      void* self = nullptr;
    };

    EXPECT_FALSE(absl::is_trivially_relocatable<NonRelocatable>::value);
    using slot_policy = map_slot_policy<int, NonRelocatable>;
    EXPECT_TRUE(
        (std::is_same_v<decltype(slot_policy::transfer<std::allocator<char>>(
                            nullptr, nullptr, nullptr)),
                        std::false_type>));
  }
}

TEST(MapSlotPolicy, DestroyReturnsTrue) {
  {
    using slot_policy = map_slot_policy<int, float>;
    EXPECT_TRUE(
        (std::is_same_v<decltype(slot_policy::destroy<std::allocator<char>>(
                            nullptr, nullptr)),
                        std::true_type>));
  }
  {
    EXPECT_FALSE(std::is_trivially_destructible_v<std::unique_ptr<int>>);
    using slot_policy = map_slot_policy<int, std::unique_ptr<int>>;
    EXPECT_TRUE(
        (std::is_same_v<decltype(slot_policy::destroy<std::allocator<char>>(
                            nullptr, nullptr)),
                        std::false_type>));
  }
}

TEST(ApplyTest, TypeErasedApplyToSlotFn) {
  size_t x = 7;
  size_t seed = 100;
  auto fn = [](size_t v) { return v * 2; };
  EXPECT_EQ((TypeErasedApplyToSlotFn<decltype(fn), size_t, /*kIsAbsl=*/false,
                                     /*kSeedShift=*/0>(&fn, &x, seed)),
            (HashElement<decltype(fn), /*kIsAbsl=*/false, /*kSeedShift=*/0>(
                fn, seed)(x)));
}

TEST(ApplyTest, GetTypeErasedApplyToSlotFnCanonicalizesTypes) {
  enum class MyEnum : uint32_t { kOne = 1, kTwo = 2 };
  HashSlotFn fn = GetTypeErasedApplyToSlotFn<absl::Hash<int32_t>, int32_t,
                                             /*kIsAbsl=*/true, 0>();
  EXPECT_EQ(fn, (GetTypeErasedApplyToSlotFn<absl::Hash<uint32_t>, uint32_t,
                                            /*kIsAbsl=*/true, 0>()));
  EXPECT_EQ(fn, (GetTypeErasedApplyToSlotFn<absl::Hash<MyEnum>, MyEnum,
                                            /*kIsAbsl=*/true, 0>()));
  EXPECT_EQ(fn,
            (GetTypeErasedApplyToSlotFn<absl::TransparentHash<int32_t, int64_t>,
                                        int32_t,
                                        /*kIsAbsl=*/true, 0>()));
  EXPECT_NE(fn, (GetTypeErasedApplyToSlotFn<absl::Hash<int32_t>, int32_t,
                                            /*kIsAbsl=*/false, 0>()));
  MyEnum c = MyEnum::kTwo;
  size_t seed = 100;
  EXPECT_EQ(fn(nullptr, &c, seed),
            (HashElement<absl::Hash<MyEnum>, /*kIsAbsl=*/true, 0>(
                absl::Hash<MyEnum>{}, seed)(c)));
}

TEST(ApplyTest, TypeErasedDerefAndApplyToSlotFn) {
  size_t x = 7;
  size_t seed = 100;
  auto fn = [](size_t v) { return v * 2; };
  size_t* x_ptr = &x;
  EXPECT_EQ(
      (TypeErasedDerefAndApplyToSlotFn<decltype(fn), size_t,
                                       /*kIsAbsl=*/false,
                                       /*kSeedShift=*/0>(&fn, &x_ptr, seed)),
      (HashElement<decltype(fn), /*kIsAbsl=*/false, /*kSeedShift=*/0>(
          fn, seed)(x)));
}

TEST(ApplyTest, GetTypeErasedDerefAndApplyToSlotFnCanonicalizesTypes) {
  enum class MyEnum : uint32_t { kOne = 1, kTwo = 2 };
  HashSlotFn fn =
      GetTypeErasedDerefAndApplyToSlotFn<absl::Hash<int32_t>, int32_t,
                                         /*kIsAbsl=*/true, 0>();
  EXPECT_EQ(fn,
            (GetTypeErasedDerefAndApplyToSlotFn<absl::Hash<uint32_t>, uint32_t,
                                                /*kIsAbsl=*/true, 0>()));
  EXPECT_EQ(fn, (GetTypeErasedDerefAndApplyToSlotFn<absl::Hash<MyEnum>, MyEnum,
                                                    /*kIsAbsl=*/true, 0>()));
  EXPECT_EQ(fn, (GetTypeErasedDerefAndApplyToSlotFn<
                    absl::TransparentHash<int32_t, int64_t>, int32_t,
                    /*kIsAbsl=*/true, 0>()));
  EXPECT_NE(fn,
            (GetTypeErasedDerefAndApplyToSlotFn<absl::Hash<int32_t>, int32_t,
                                                /*kIsAbsl=*/false, 0>()));
  MyEnum c = MyEnum::kTwo;
  MyEnum* c_ptr = &c;
  size_t seed = 100;
  EXPECT_EQ(fn(nullptr, &c_ptr, seed),
            (HashElement<absl::Hash<MyEnum>, /*kIsAbsl=*/true, 0>(
                absl::Hash<MyEnum>{}, seed)(c)));
}

TEST(HashKeyTest, NonCanonicalUsesHasher) {
  auto hash = [](int32_t v) { return static_cast<size_t>(v + 42); };
  using HashKeyCustom = HashKey<decltype(hash), int32_t, /*kIsAbsl=*/false,
                                /*kSeedShift=*/0, /*kUseCanonical=*/false>;
  int32_t a = 1;
  size_t seed = 100;
  HashKeyCustom hk{hash, a};
  EXPECT_EQ(hk.data().ToPtr<HashKeyCustom>(), &hk);
  EXPECT_EQ(HashKeyCustom::get_hash_fn()(hk.data(), seed),
            (HashElement<decltype(hash), /*kIsAbsl=*/false, 0>(hash, seed)(a)));
}

TEST(EqSlotFnTest, SlotEqFnWorks) {
  auto test_eq = [](auto eq) {
    std::string key = "a";
    SlotEqFn<std::string, /*kDerefSlot=*/false> eq_slot;
    static_assert(std::is_empty_v<decltype(eq_slot)>);
    static_assert(
        std::is_same_v<typename decltype(eq_slot)::SlotKeyType,
                       const std::string>);
    std::string slot_match = "a";
    std::string slot_mismatch = "b";
    EXPECT_TRUE(eq_slot(eq, &slot_match, key));
    EXPECT_FALSE(eq_slot(eq, &slot_mismatch, key));

    SlotEqFn<std::string, /*kDerefSlot=*/true> eq_deref;
    static_assert(std::is_empty_v<decltype(eq_deref)>);
    static_assert(
        std::is_same_v<typename decltype(eq_deref)::SlotKeyType,
                       const std::string>);
    std::string* slot_match_ptr = &slot_match;
    std::string* slot_mismatch_ptr = &slot_mismatch;
    EXPECT_TRUE(eq_deref(eq, &slot_match_ptr, key));
    EXPECT_FALSE(eq_deref(eq, &slot_mismatch_ptr, key));
  };

  test_eq(std::equal_to<std::string>{});

  struct StatefulEq {
    int dummy = 0;
    bool operator()(const std::string& lhs, const std::string& rhs) const {
      return lhs == rhs;
    }
  };
  test_eq(StatefulEq{1});
}

TEST(HashElement, DefaultHash) {
  size_t x = 7;
  size_t seed = 100;
  struct HashWithSeed {
    size_t operator()(size_t v) const { return v * 2; }
    size_t hash_with_seed(size_t v, size_t seed) const {
      return v * 2 + seed * 3;
    }
  } hash;
  EXPECT_EQ((HashElement<HashWithSeed, /*kIsAbsl=*/true,
                         /*kSeedShift=*/0>(hash, seed)(x)),
            hash.hash_with_seed(x, seed));
}

TEST(HashElement, NonDefaultHash) {
  size_t x = 7;
  size_t seed = 100;
  auto fn = [](size_t v) { return v * 2; };
  EXPECT_EQ(
      (HashElement<decltype(fn), /*kIsAbsl=*/false, /*kSeedShift=*/0>(
          fn, seed)(x)),
      fn(x) ^ seed);
}

TEST(HashElement, NonDefaultHashWithSeedShift) {
  size_t x = 7;
  size_t seed = 100;
  auto fn = [](size_t v) { return v * 2; };
  EXPECT_EQ(
      (HashElement<decltype(fn), /*kIsAbsl=*/false, /*kSeedShift=*/1>(
          fn, seed)(x)),
      fn(x) ^ (seed >> 1));
}

}  // namespace
}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl
