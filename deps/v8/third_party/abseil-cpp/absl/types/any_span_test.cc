// Copyright 2026 The Abseil Authors.
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

#include "absl/types/any_span.h"

#include <cstddef>
#include <deque>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/base/internal/exception_testing.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/macros.h"
#include "absl/hash/hash_testing.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace {

using testing::ElementsAre;

static_assert(!absl::type_traits_internal::IsOwner<AnySpan<int>>::value &&
                  absl::type_traits_internal::IsView<AnySpan<int>>::value,
              "AnySpan is a view, not an owner");

bool IsHardened() {
  bool hardened = false;
  ABSL_HARDENING_ASSERT([&hardened]() {
    hardened = true;
    return true;
  }());
  return hardened;
}

// Tests that the span points to all of the elements of the given container.
template <typename T, typename Container>
void ExpectSpanEqualsContainer(AnySpan<T> span, const Container& c) {
  EXPECT_EQ(span.size(), c.size());
  typename AnySpan<const T>::size_type index = 0;
  for (const T& s : span) {
    SCOPED_TRACE(index);
    const T& expected = c[index];
    EXPECT_EQ(expected, s);
    EXPECT_EQ(expected, span[index]);
    EXPECT_EQ(&expected, &span[index]);
    ++index;
  }
  EXPECT_EQ(index, span.size());
}

// AnySpanTest covers parts of AnySpan that make sense for both const and
// non-const elements. How the given tests will run is determined by whether or
// not ElementsAreConst is true_type or false_type.
template <typename ElementsAreConst>
class AnySpanTest : public ::testing::Test {
 public:
  using IsConstTest = ElementsAreConst;

  // Const T if this test instantiation is supposed to cover const AnySpans.
  // T if this test instantiation is supposed to cover mutable AnySpans.
  template <typename T>
  using MaybeConst =
      typename std::conditional<IsConstTest::value, const T, T>::type;

  // Initalizes a AnySpan around the given container and tests for
  // equality in a variety of ways.
  template <typename T, typename Container, typename Transform>
  static void TestContainer(Container c, const Transform& t) {
    AnySpan<MaybeConst<T>> span(c, t);
    EXPECT_NO_FATAL_FAILURE(ExpectSpanEqualsContainer(span, c));
  }

  // Initializes a dereferencing AnySpan around the given container of pointers
  // and tests for equality in a variety of ways.
  template <typename T, typename Container>
  static void TestPointerContainer(Container* c) {
    AnySpan<const T> span(*c, any_span_transform::Deref());
    EXPECT_EQ(span.size(), c->size());
    int index = 0;
    for (const T& s : span) {
      const T& expected = *(*c)[index];
      EXPECT_EQ(expected, s);
      EXPECT_EQ(expected, span[index]);
      EXPECT_EQ(&expected, &span[index]);
      index++;
    }
    EXPECT_EQ(index, span.size());
  }
};

using ElementsAreConst = ::testing::Types<std::true_type, std::false_type>;

TYPED_TEST_SUITE(AnySpanTest, ElementsAreConst);

TYPED_TEST(AnySpanTest, NullAndEmpty) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  AnySpan<T> empty;
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(empty.size(), 0);
  for (const std::string& a : empty) {
    (void)a;
    ABSL_RAW_LOG(FATAL, "Empty container should not iterate.");
  }
  EXPECT_EQ(empty.begin(), empty.end());
}

TYPED_TEST(AnySpanTest, Ptr) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::vector<std::string> v = {"a", "b", "c", "d"};
  AnySpan<T> span1(v.data(), v.size());
  EXPECT_EQ(&span1[0], &v[0]);
  AnySpan<T> span2(v.data(), v.size());
  EXPECT_EQ(&span2[0], &v[0]);
}

TYPED_TEST(AnySpanTest, PtrPtr) {
  using T = typename TestFixture::template MaybeConst<int>;
  int i;
  std::vector<int*> v = {&i};
  AnySpan<T> span(v.data(), v.size(), any_span_transform::Deref());
  EXPECT_EQ(&span[0], &i);
}

TYPED_TEST(AnySpanTest, DerefOptional) {
  const std::vector<std::optional<int>> v = {17, 19};
  const AnySpan<const int> span = absl::MakeDerefAnySpan(v);
  EXPECT_THAT(span, ElementsAre(17, 19));
}

TYPED_TEST(AnySpanTest, ArrayOfKnownSize) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::string a[] = {"a", "b", "c", "d"};

  AnySpan<T> span(a);
  EXPECT_EQ(span.size(), 4);
  EXPECT_EQ(&span[0], &a[0]);
}

// This gets its own test because AnySpan(array, size) has the potential to be
// ambiguous with AnySpan(array, transform).
TYPED_TEST(AnySpanTest, ArrayWithExplicitSize) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::string a[] = {"a", "b", "c", "d"};

  AnySpan<T> span(a, 4);
  EXPECT_EQ(span.size(), 4);
  EXPECT_EQ(&span[0], &a[0]);
}

TYPED_TEST(AnySpanTest, PtrArrayOfKnownSize) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::string s;
  std::string* a[] = {&s, &s, &s, &s};

  AnySpan<T> span(a, any_span_transform::Deref());
  EXPECT_EQ(span.size(), 4);
  EXPECT_EQ(&span[0], &s);
}

TYPED_TEST(AnySpanTest, Range) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::vector<std::string> a = {"a", "b", "c", "d"};
  auto range = any_span_adaptor::MakeAdaptorFromRange(a.begin(), a.end());
  AnySpan<T> span(range);
  EXPECT_NO_FATAL_FAILURE(ExpectSpanEqualsContainer(span, a));
}

TYPED_TEST(AnySpanTest, View) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::vector<std::string> a = {"a", "b", "c", "d"};
  auto range = any_span_adaptor::MakeAdaptorFromView(a);
  AnySpan<T> span(range);
  EXPECT_NO_FATAL_FAILURE(ExpectSpanEqualsContainer(span, a));
}

TYPED_TEST(AnySpanTest, VectorString) {
  EXPECT_NO_FATAL_FAILURE(TestFixture::template TestContainer<std::string>(
      std::vector<std::string>{"a", "b", "c", "d"},
      any_span_transform::Identity()));
}

TYPED_TEST(AnySpanTest, VectorInt) {
  EXPECT_NO_FATAL_FAILURE(TestFixture::template TestContainer<int>(
      std::vector<int>{1, 2, 3, 4}, any_span_transform::Identity()));
}

TYPED_TEST(AnySpanTest, VectorStringPointer) {
  std::vector<std::string> v = {"a", "b", "c", "d"};
  std::vector<std::string*> vp;
  for (std::string& s : v) {
    vp.push_back(&s);
  }
  TestFixture::template TestPointerContainer<std::string>(&vp);
}

TYPED_TEST(AnySpanTest, VectorStringUniquePointer) {
  std::vector<std::unique_ptr<std::string>> v;
  for (const std::string& s : std::vector<std::string>{"a", "b", "c", "d"}) {
    v.emplace_back(new std::string(s));
  }
  TestFixture::template TestPointerContainer<std::string>(&v);
}

TYPED_TEST(AnySpanTest, VectorIntPointer) {
  std::vector<int> v = {1, 2, 3, 4};
  std::vector<int*> vp;
  for (int& i : v) {
    vp.push_back(&i);
  }
  TestFixture::template TestPointerContainer<int>(&vp);
}

TYPED_TEST(AnySpanTest, DequeString) {
  EXPECT_NO_FATAL_FAILURE(TestFixture::template TestContainer<std::string>(
      std::deque<std::string>{"a", "b", "c", "d"},
      any_span_transform::Identity()));
}

TYPED_TEST(AnySpanTest, DequeInt) {
  EXPECT_NO_FATAL_FAILURE(TestFixture::template TestContainer<int>(
      std::deque<int>{1, 2, 3, 4}, any_span_transform::Identity()));
}

TYPED_TEST(AnySpanTest, DequeStringPtr) {
  std::vector<std::string> v = {"a", "b", "c", "d"};
  std::deque<std::string*> dp;
  for (std::string& s : v) {
    dp.push_back(&s);
  }
  TestFixture::template TestPointerContainer<std::string>(&dp);
}

TYPED_TEST(AnySpanTest, ImplicitConversions) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::string one = "1";
  std::string two = "2";
  std::vector<std::reference_wrapper<std::string>> v(
      {std::ref(one), std::ref(two)});
  AnySpan<T> span(v);

  EXPECT_THAT(span, ElementsAre("1", "2"));
}

TYPED_TEST(AnySpanTest, TrivialSubspan) {
  EXPECT_EQ(AnySpan<int>().subspan(0).size(), 0);
}

TYPED_TEST(AnySpanTest, TrivialFirst) {
  using T = typename TestFixture::template MaybeConst<int>;
  EXPECT_EQ(AnySpan<T>().first(0).size(), 0);
}

TYPED_TEST(AnySpanTest, TrivialLast) {
  using T = typename TestFixture::template MaybeConst<int>;
  EXPECT_EQ(AnySpan<T>().last(0).size(), 0);
}

struct Base1 {
  virtual ~Base1() = default;
  bool operator==(const Base1& other) const { return this == &other; }
  int b1 = 1;
};
struct Base2 {
  virtual ~Base2() = default;
  bool operator==(const Base2& other) const { return this == &other; }
  int b2 = 2;
};
struct Child : public Base1, public Base2 {
  bool operator==(const Child& other) const { return this == &other; }
};

TYPED_TEST(AnySpanTest, VectorBaseClass) {
  std::vector<Child> v;
  v.resize(20);
  EXPECT_NO_FATAL_FAILURE(TestFixture::template TestContainer<Base1>(
      v, any_span_transform::Identity()));
  EXPECT_NO_FATAL_FAILURE(TestFixture::template TestContainer<Base2>(
      v, any_span_transform::Identity()));
}

TYPED_TEST(AnySpanTest, AnySpanVectorBaseClass) {
  std::vector<Child> v;
  v.resize(20);
  AnySpan<Child> span(v);
  EXPECT_NO_FATAL_FAILURE(TestFixture::template TestContainer<Base1>(
      span, any_span_transform::Identity()));
  EXPECT_NO_FATAL_FAILURE(TestFixture::template TestContainer<Base2>(
      span, any_span_transform::Identity()));
}

TYPED_TEST(AnySpanTest, DerefAnySpanVectorBaseClass) {
  Child c;
  std::vector<Child*> v_ptrs(20, &c);
  AnySpan<Child> span(v_ptrs, any_span_transform::Deref());
  EXPECT_NO_FATAL_FAILURE(TestFixture::template TestContainer<Base1>(
      span, any_span_transform::Identity()));
  EXPECT_NO_FATAL_FAILURE(TestFixture::template TestContainer<Base2>(
      span, any_span_transform::Identity()));
}

TYPED_TEST(AnySpanTest, AnySpanDequeBaseClass) {
  std::deque<Child> v;
  v.resize(20);
  AnySpan<Child> span(v);
  EXPECT_NO_FATAL_FAILURE(TestFixture::template TestContainer<Base1>(
      span, any_span_transform::Identity()));
  EXPECT_NO_FATAL_FAILURE(TestFixture::template TestContainer<Base2>(
      span, any_span_transform::Identity()));
}

TYPED_TEST(AnySpanTest, VectorSubspan) {
  using T = typename TestFixture::template MaybeConst<int>;
  std::vector<int> v = {1, 2, 3, 4};
  auto span = AnySpan<T>(v).subspan(1, 2);
  EXPECT_EQ(span.size(), 2);
  EXPECT_EQ(span[0], 2);
  EXPECT_EQ(span[1], 3);
  span = AnySpan<T>(v).subspan(1);
  EXPECT_EQ(span.size(), 3);
  EXPECT_EQ(span[2], 4);

  std::vector<int> zero_length_vector;
  auto zero_length_subspan = AnySpan<T>(zero_length_vector).subspan(0);
  EXPECT_EQ(zero_length_subspan.size(), 0);
}

TYPED_TEST(AnySpanTest, DequeSubspan) {
  using T = typename TestFixture::template MaybeConst<int>;
  std::deque<int> v = {1, 2, 3, 4};
  auto span = AnySpan<T>(v).subspan(1, 2);
  EXPECT_EQ(span.size(), 2);
  EXPECT_EQ(span[0], 2);
  EXPECT_EQ(span[1], 3);
}

TYPED_TEST(AnySpanTest, VectorFirst) {
  using T = typename TestFixture::template MaybeConst<int>;
  std::vector<int> v = {1, 2, 3, 4};
  auto span = AnySpan<T>(v).first(2);
  EXPECT_THAT(span, ElementsAre(1, 2));
  span = AnySpan<T>(v).first(4);
  EXPECT_THAT(span, ElementsAre(1, 2, 3, 4));

  std::vector<int> zero_length_vector;
  auto zero_length_subspan = AnySpan<T>(zero_length_vector).first(0);
  EXPECT_EQ(zero_length_subspan.size(), 0);
}

TYPED_TEST(AnySpanTest, DequeFirst) {
  using T = typename TestFixture::template MaybeConst<int>;
  std::deque<int> v = {1, 2, 3, 4};
  auto span = AnySpan<T>(v).first(2);
  EXPECT_THAT(span, ElementsAre(1, 2));
}

TYPED_TEST(AnySpanTest, VectorLast) {
  using T = typename TestFixture::template MaybeConst<int>;
  std::vector<int> v = {1, 2, 3, 4};
  auto span = AnySpan<T>(v).last(2);
  EXPECT_THAT(span, ElementsAre(3, 4));
  span = AnySpan<T>(v).last(4);
  EXPECT_THAT(span, ElementsAre(1, 2, 3, 4));

  std::vector<int> zero_length_vector;
  auto zero_length_subspan = AnySpan<T>(zero_length_vector).last(0);
  EXPECT_EQ(zero_length_subspan.size(), 0);
}

TYPED_TEST(AnySpanTest, DequeLast) {
  using T = typename TestFixture::template MaybeConst<int>;
  std::deque<int> v = {1, 2, 3, 4};
  auto span = AnySpan<T>(v).last(2);
  EXPECT_THAT(span, ElementsAre(3, 4));
}

TYPED_TEST(AnySpanTest, LambdaTransformFromVector) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::vector<std::string> v = {"b", "a", "d", "c"};
  std::vector<int> alphabetical_order = {1, 0, 3, 2};
  auto subscript_v = [&v](size_t i) -> std::string& { return v[i]; };
  AnySpan<T> span(alphabetical_order, subscript_v);
  EXPECT_EQ(span[0], "a");
  EXPECT_EQ(span[1], "b");
  EXPECT_EQ(span[2], "c");
  EXPECT_EQ(span[3], "d");
}

TYPED_TEST(AnySpanTest, LambdaTransformFromSpanOfDifferentType) {
  using T = typename TestFixture::template MaybeConst<int>;
  using U = typename TestFixture::template MaybeConst<std::string>;
  std::vector<std::string> v = {"b", "a", "d", "c"};
  std::vector<int> alphabetical_order = {1, 0, 3, 2};
  AnySpan<T> alphabetical_order_as_span(alphabetical_order);
  auto subscript_v = [&v](size_t i) -> std::string& { return v[i]; };
  AnySpan<U> span(alphabetical_order_as_span, subscript_v);
  EXPECT_EQ(span[0], "a");
  EXPECT_EQ(span[1], "b");
  EXPECT_EQ(span[2], "c");
  EXPECT_EQ(span[3], "d");
}

TYPED_TEST(AnySpanTest, LambdaTransformFromSpanOfSameType) {
  using T = typename TestFixture::template MaybeConst<int>;
  std::vector<int> v = {2, 1, 4, 3};
  std::vector<int> sorted_order = {1, 0, 3, 2};
  AnySpan<T> sorted_order_as_span(sorted_order);
  auto subscript_v = [&v](size_t i) -> int& { return v[i]; };
  AnySpan<T> span(sorted_order_as_span, subscript_v);
  EXPECT_EQ(span[0], 1);
  EXPECT_EQ(span[1], 2);
  EXPECT_EQ(span[2], 3);
  EXPECT_EQ(span[3], 4);
}

TYPED_TEST(AnySpanTest, MakeAnySpanFromVector) {
  using V = typename TestFixture::template MaybeConst<std::vector<int>>;
  using T = typename TestFixture::template MaybeConst<int>;
  V v = {1, 2, 3, 4};
  auto span = MakeAnySpan(v);
  EXPECT_TRUE((std::is_same<decltype(span), AnySpan<T>>()));
  EXPECT_EQ(span.size(), 4);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(&span[i], &v[i]);
  }
}

TYPED_TEST(AnySpanTest, MakeAnySpanFromPtrAndSize) {
  using V = typename TestFixture::template MaybeConst<std::vector<int>>;
  using T = typename TestFixture::template MaybeConst<int>;
  V v = {1, 2, 3, 4};
  auto span = MakeAnySpan(v.data(), v.size());
  EXPECT_TRUE((std::is_same<decltype(span), AnySpan<T>>()));
  EXPECT_EQ(span.size(), 4);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(&span[i], &v[i]);
  }
}

TYPED_TEST(AnySpanTest, MakeAnySpanFromArray) {
  using T = typename TestFixture::template MaybeConst<int>;
  T a[] = {1, 2, 3, 4};
  auto span = MakeAnySpan(a);
  EXPECT_TRUE((std::is_same<decltype(span), AnySpan<T>>()));
  EXPECT_EQ(span.size(), 4);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(&span[i], &a[i]);
  }
}

TYPED_TEST(AnySpanTest, MakeDerefAnySpanFromArray) {
  using T = typename TestFixture::template MaybeConst<int>;
  T v = 0;
  T* a[] = {&v, &v, &v};
  auto span = MakeDerefAnySpan(a);
  EXPECT_TRUE((std::is_same<decltype(span), AnySpan<T>>()));
  EXPECT_EQ(span.size(), 3);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(&span[i], a[i]);
  }
}

// Tests transforms that are supported by std::invoke.
TYPED_TEST(AnySpanTest, InvokableTransforms) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  struct S;
  using SRef = typename TestFixture::template MaybeConst<S>&;
  struct S {
    std::string a, b;
    static T& Fun(SRef p) { return p.a; }
    std::string& MemFun() { return a; }
    const std::string& MemFun() const { return a; }
  };

  // For const span tests, we need a pointer to constant member function. We
  // choose the correct type of member function here.
  using MemFunPtr =
      typename std::conditional<TestFixture::IsConstTest::value,
                                T& (S::*)() const, T& (S::*)()>::type;

  std::vector<S> v = {{"1", "2"}, {"3", "4"}};
  S a[] = {{"1", "2"}, {"3", "4"}};
  T& (*fun_ptr)(SRef) = &S::Fun;
  MemFunPtr mem_fun_ptr = &S::MemFun;
  T S::* mem_ptr = &S::a;

  AnySpan<T> spans[] = {
      // Function reference.
      AnySpan<T>(v, S::Fun),
      AnySpan<T>(a, S::Fun),
      AnySpan<T>(a, 2, S::Fun),

      // Function pointer prvalue.
      AnySpan<T>(v, &S::Fun),
      AnySpan<T>(a, &S::Fun),
      AnySpan<T>(a, 2, &S::Fun),

      // Function pointer lvalue.
      AnySpan<T>(v, fun_ptr),
      AnySpan<T>(a, fun_ptr),
      AnySpan<T>(a, 2, fun_ptr),

      // Dereferenced function pointer variable.
      AnySpan<T>(v, *fun_ptr),
      AnySpan<T>(a, *fun_ptr),
      AnySpan<T>(a, 2, *fun_ptr),

      // Member function pointer.
      AnySpan<T>(v, mem_fun_ptr),
      AnySpan<T>(a, mem_fun_ptr),
      AnySpan<T>(a, 2, mem_fun_ptr),

      // Data member.
      AnySpan<T>(v, mem_ptr),
      AnySpan<T>(a, mem_ptr),
      AnySpan<T>(a, 2, mem_ptr),
  };

  for (const auto& span : spans) {
    EXPECT_EQ(span[0], "1");
    EXPECT_EQ(span[1], "3");
  }
}

TYPED_TEST(AnySpanTest, VectorPtrSubspan) {
  using T = typename TestFixture::template MaybeConst<int>;
  std::vector<int> v = {1, 2, 3, 4};
  std::vector<int*> vp;
  for (int& i : v) {
    vp.push_back(&i);
  }
  AnySpan<T> span = AnySpan<T>(vp, any_span_transform::Deref()).subspan(1, 2);
  EXPECT_EQ(span.size(), 2);
  EXPECT_EQ(span[0], 2);
  EXPECT_EQ(span[1], 3);
}

TYPED_TEST(AnySpanTest, VectorPtrFirst) {
  using T = typename TestFixture::template MaybeConst<int>;
  std::vector<int> v = {1, 2, 3, 4};
  std::vector<int*> vp;
  for (int& i : v) {
    vp.push_back(&i);
  }
  AnySpan<T> span = AnySpan<T>(vp, any_span_transform::Deref()).first(2);
  EXPECT_THAT(span, ElementsAre(1, 2));
}

TYPED_TEST(AnySpanTest, VectorPtrLast) {
  using T = typename TestFixture::template MaybeConst<int>;
  std::vector<int> v = {1, 2, 3, 4};
  std::vector<int*> vp;
  for (int& i : v) {
    vp.push_back(&i);
  }
  AnySpan<T> span = AnySpan<T>(vp, any_span_transform::Deref()).last(2);
  EXPECT_THAT(span, ElementsAre(3, 4));
}

TYPED_TEST(AnySpanTest, Iterator) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::vector<std::string> v = {"0", "1", "2", "3"};
  AnySpan<T> span(v);

  // Dereference.
  auto it = span.begin();
  EXPECT_EQ("0", *it);

  // Postfix increment.
  EXPECT_EQ("0", *(it++));
  EXPECT_EQ("1", *(it++));

  // Postfix decrement.
  EXPECT_EQ("2", *(it--));
  EXPECT_EQ("1", *(it--));

  // Prefix increment.
  EXPECT_EQ(it, span.begin());
  EXPECT_EQ("1", *(++it));
  EXPECT_EQ("2", *(++it));

  // Postfix decrement.
  EXPECT_EQ("1", *(--it));
  EXPECT_EQ("0", *(--it));

  // Increment by integer.
  EXPECT_EQ(it, span.begin());
  it += 3;
  EXPECT_EQ("3", *it);

  // Decrement by integer.
  it = span.end();
  it -= 4;
  EXPECT_EQ("0", *it);

  // Add and subtract.
  EXPECT_EQ(v.begin() + 2, v.end() - 2);
  EXPECT_EQ(2 + v.begin(), v.end() - 2);

  // Index.
  it = span.begin();
  EXPECT_EQ("3", v[3]);

  // Relational operators.
  EXPECT_TRUE(span.begin() != span.end());
  EXPECT_TRUE(span.begin() == span.begin());
  EXPECT_TRUE(span.begin() < span.end());
  EXPECT_TRUE(span.begin() <= span.end());
  EXPECT_TRUE(span.end() > span.begin());
  EXPECT_TRUE(span.end() >= span.begin());
}

TYPED_TEST(AnySpanTest, ConstIterator) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::vector<std::string> v = {"0", "1", "2", "3"};
  AnySpan<T> span(v);

  // Dereference.
  auto it = span.cbegin();
  EXPECT_EQ("0", *it);

  // Postfix increment.
  EXPECT_EQ("0", *(it++));
  EXPECT_EQ("1", *(it++));

  // Postfix decrement.
  EXPECT_EQ("2", *(it--));
  EXPECT_EQ("1", *(it--));

  // Prefix increment.
  EXPECT_EQ(it, span.cbegin());
  EXPECT_EQ("1", *(++it));
  EXPECT_EQ("2", *(++it));

  // Postfix decrement.
  EXPECT_EQ("1", *(--it));
  EXPECT_EQ("0", *(--it));

  // Increment by integer.
  EXPECT_EQ(it, span.cbegin());
  it += 3;
  EXPECT_EQ("3", *it);

  // Decrement by integer.
  it = span.cend();
  it -= 4;
  EXPECT_EQ("0", *it);

  // Add and subtract.
  EXPECT_EQ(v.cbegin() + 2, v.cend() - 2);
  EXPECT_EQ(2 + v.cbegin(), v.cend() - 2);

  // Index.
  it = span.cbegin();
  EXPECT_EQ("3", v[3]);

  // Relational operators.
  EXPECT_TRUE(span.cbegin() != span.cend());
  EXPECT_TRUE(span.cbegin() == span.cbegin());
  EXPECT_TRUE(span.cbegin() < span.cend());
  EXPECT_TRUE(span.cbegin() <= span.cend());
  EXPECT_TRUE(span.cend() > span.cbegin());
  EXPECT_TRUE(span.cend() >= span.cbegin());
}

TYPED_TEST(AnySpanTest, ReverseIterator) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::vector<std::string> v = {"a", "b", "c", "d"};
  AnySpan<T> span(v);
  auto v_it = v.rbegin();
  for (auto it = span.rbegin(); it != span.rend(); ++it, ++v_it) {
    EXPECT_EQ(*v_it, *it);
  }
}

TYPED_TEST(AnySpanTest, IndexingAt) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::vector<std::string> v = {"a", "b", "c", "d"};
  AnySpan<T> span(v);
  EXPECT_EQ(&v.at(0), &v.at(0));
  EXPECT_EQ(&v.at(1), &v.at(1));
  EXPECT_EQ(&v.at(3), &v.at(3));
}

TYPED_TEST(AnySpanTest, AtThrows) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::vector<std::string> v = {"a", "b", "c", "d"};
  AnySpan<T> span(v);
  ABSL_BASE_INTERNAL_EXPECT_FAIL(span.at(4), std::out_of_range,
                                 "failed bounds check");
}

TYPED_TEST(AnySpanTest, FrontBack) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  std::vector<std::string> v = {"a", "b", "c", "d"};
  AnySpan<T> span(v);
  EXPECT_EQ(&v.front(), &span.front());
  EXPECT_EQ("a", span.front());
  EXPECT_EQ(&v.back(), &span.back());
  EXPECT_EQ("d", span.back());
}

// Returns a reference to a default constructed instance of the given type.
template <typename Container>
Container& DefaultInstance() {
  static auto* c = new Container;
  return *c;
}

// This test reaches into the implementation a bit, but it buys a lot of
// confidence that the flat-array optimizations are working correctly.
TYPED_TEST(AnySpanTest, CheapTypes) {
  using T = typename TestFixture::template MaybeConst<int>;
  using any_span_internal::IsCheap;
  EXPECT_FALSE(IsCheap(AnySpan<T>(DefaultInstance<std::deque<int>>())));
  EXPECT_FALSE(
      IsCheap(AnySpan<T>(DefaultInstance<std::vector<std::unique_ptr<int>>>(),
                         any_span_transform::Deref())));
  EXPECT_TRUE(IsCheap(AnySpan<T>(DefaultInstance<std::vector<int>>())));
  EXPECT_TRUE(IsCheap(AnySpan<T>(DefaultInstance<absl::Span<int>>())));
  EXPECT_TRUE(IsCheap(AnySpan<T>(DefaultInstance<std::vector<int>>())));
  EXPECT_TRUE(IsCheap(AnySpan<T>(DefaultInstance<std::vector<int*>>(),
                                 any_span_transform::Deref())));
  EXPECT_TRUE(IsCheap(AnySpan<T>(DefaultInstance<std::vector<int*>>(),
                                 any_span_transform::Deref())));
  struct SomeContainer {
    int& operator[](std::size_t) { ABSL_RAW_LOG(FATAL, "Not implemented"); }
    const int& operator[](std::size_t) const {
      ABSL_RAW_LOG(FATAL, "Not implemented");
    }
    std::size_t size() const { return 1; }
  };
  EXPECT_FALSE(IsCheap(AnySpan<T>(DefaultInstance<SomeContainer>())));
  struct SomeCheapMutableContainer {
    int& operator[](std::size_t) const {
      ABSL_RAW_LOG(FATAL, "Not implemented");
    }
    int* data() const { return nullptr; }
    std::size_t size() const { return 1; }
  };
  EXPECT_TRUE(
      IsCheap(AnySpan<T>(DefaultInstance<SomeCheapMutableContainer>())));

  struct SomeWonkyContainer {
    int& operator[](std::size_t) const {
      ABSL_RAW_LOG(FATAL, "Not implemented");
    }
    const int** data() const { return nullptr; }
    std::size_t size() const { return 1; }
  };
  EXPECT_FALSE(IsCheap(AnySpan<T>(DefaultInstance<SomeWonkyContainer>())));
}

TYPED_TEST(AnySpanTest, TriviallyCopyable) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  // Note: we could use is_trivially_copyable, but it is not implemented in
  // Crosstool v18 which is the current version at the time of writing.
  EXPECT_TRUE(std::is_trivially_copy_constructible<AnySpan<T>>::value);
  EXPECT_TRUE(std::is_trivially_copy_assignable<AnySpan<T>>::value);
  EXPECT_TRUE(std::is_trivially_destructible<AnySpan<T>>::value);
}

TYPED_TEST(AnySpanTest, IsConstructible) {
  using T = typename TestFixture::template MaybeConst<std::string>;
  using U = typename TestFixture::template MaybeConst<int>;
  using VectorT =
      typename TestFixture::template MaybeConst<std::vector<std::string>>;
  using VectorU = typename TestFixture::template MaybeConst<std::vector<int>>;
  EXPECT_FALSE((std::is_constructible<AnySpan<T>, T>::value));
  EXPECT_FALSE((std::is_constructible<AnySpan<T>, T*>::value));
  EXPECT_FALSE((std::is_constructible<AnySpan<T>, U>::value));
  EXPECT_FALSE((std::is_constructible<AnySpan<T>, U(&)[5]>::value));
  EXPECT_FALSE((std::is_constructible<AnySpan<T>, U*, std::size_t>::value));
  EXPECT_FALSE((std::is_constructible<AnySpan<T>, VectorU&>::value));

  EXPECT_TRUE((std::is_constructible<AnySpan<T>, VectorT&>::value));
  EXPECT_TRUE((std::is_constructible<AnySpan<T>, T(&)[5]>::value));
  EXPECT_TRUE((std::is_constructible<AnySpan<T>, T*, std::size_t>::value));
}

//
// ConstAnySpanTest covers parts of AnySpan interface that are only valid for
// AnySpans with const elements.
//

TEST(ConstAnySpanTest, ImplicitConversion) {
  std::vector<int> v = {1, 2, 3};
  ExpectSpanEqualsContainer<const int>(v, v);
}

TEST(ConstAnySpanTest, TemporaryArrayOfKnownSize) {
  const int i = 5;
  EXPECT_THAT(AnySpan<const int>({&i, &i, &i}, any_span_transform::Deref()),
              testing::ElementsAre(5, 5, 5));
}

TEST(ConstAnySpanTest, CheapTypes) {
  using any_span_internal::IsCheap;
  EXPECT_TRUE(IsCheap(AnySpan<const int>(std::vector<const int*>(),
                                         any_span_transform::Deref())));
  EXPECT_TRUE(IsCheap(AnySpan<const int>(std::vector<const int*>(),
                                         any_span_transform::Deref())));
  EXPECT_TRUE(
      IsCheap(AnySpan<const int>(DefaultInstance<absl::Span<const int>>())));
  struct SomeCheapConstContainer {
    const int& operator[](std::size_t) const {
      ABSL_RAW_LOG(FATAL, "Not implemented");
    }
    const int* data() const { return nullptr; }
    std::size_t size() const { return 1; }
  };
  EXPECT_TRUE(IsCheap(AnySpan<const int>(SomeCheapConstContainer())));
}

TEST(ConstAnySpanTest, InitializerListInt) {
  auto test = [](AnySpan<const int> span) {
    int expected = 1;
    for (int i : span) {
      EXPECT_EQ(expected++, i);
    }
    EXPECT_EQ(expected, 5);
  };
  test({1, 2, 3, 4});
}

TEST(ConstAnySpanTest, InitializerListString) {
  auto test = [](AnySpan<const std::string> span) {
    int expected = 1;
    for (const std::string& i : span) {
      EXPECT_EQ(absl::StrCat(expected++), i);
    }
    EXPECT_EQ(expected, 5);
  };
  test({"1", "2", "3", "4"});
}

TEST(ConstAnySpanTest, InitializerListDerivedToBase) {
  // Derived to base, implicit conversion.
  [](AnySpan<const Base2> bases) {
    for (const Base2& b : bases) {
      EXPECT_EQ(b.b2, 2);
      // Make sure the value was not sliced.
      EXPECT_EQ(typeid(b), typeid(Child));
    }
  }({Child{}, Child{}, Child{}});

  // Derived to base with explicit transform.
  Child children[3];
  [](AnySpan<const Base2> bases) {
    for (const Base2& b : bases) {
      EXPECT_EQ(b.b2, 2);
      // Make sure the value was not sliced.
      EXPECT_EQ(typeid(b), typeid(Child));
    }
  }({{Child{}, Child{}, Child{}},
     [](auto& obj) -> const Base2& { return obj; }});
}

TEST(ConstAnySpanTest,
     InitializerListUsesValueTypeWhenConversionNeedsTemporaries) {
  // If this were to call the initializer_list<Element> overload it would fail
  // when it tries to make an lvalue transform from std::string to
  // std::string_view.
  // Instead, it must be calling the initializer_list<value_type> overload,
  // doing the conversion on the call site.
  [](AnySpan<const std::string_view> s) {
    EXPECT_THAT(s, ElementsAre("A", "B", "C"));
  }({"A", "B", "C"});
}

TEST(ConstAnySpanTest, MakeAnySpanFromVector) {
  std::vector<int> v = {1, 2, 3, 4};
  auto span = MakeConstAnySpan(v);
  EXPECT_TRUE((std::is_same<decltype(span), AnySpan<const int>>()));
  EXPECT_EQ(span.size(), 4);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(&span[i], &v[i]);
  }
}

TEST(ConstAnySpanTest, MakeAnySpanFromPtrAndSize) {
  std::vector<int> v = {1, 2, 3, 4};
  auto span = MakeConstAnySpan(v.data(), v.size());
  EXPECT_TRUE((std::is_same<decltype(span), AnySpan<const int>>()));
  EXPECT_EQ(span.size(), 4);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(&span[i], &v[i]);
  }
}

TEST(ConstAnySpanTest, MakeAnySpanFromArray) {
  int a[] = {1, 2, 3, 4};
  auto span = MakeConstAnySpan(a);
  EXPECT_TRUE((std::is_same<decltype(span), AnySpan<const int>>()));
  EXPECT_EQ(span.size(), 4);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(&span[i], &a[i]);
  }
}

TEST(ConstAnySpanTest, MakeDerefAnySpanFromArray) {
  int v = 0;
  int* a[] = {&v, &v, &v};
  auto span = MakeConstDerefAnySpan(a);
  EXPECT_TRUE((std::is_same<decltype(span), AnySpan<const int>>()));
  EXPECT_EQ(span.size(), 3);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(&span[i], a[i]);
  }
}

//
// MutableAnySpanTest covers parts of AnySpan interface that are only valid for
// AnySpans with mutable elements.
//

TEST(MutableAnySpanTest, MutableVectorString) {
  std::vector<std::string> v = {"a", "b", "c", "d"};
  AnySpan<std::string> span(v);
  span[0] = "e";
  EXPECT_EQ(v[0], "e");
}

TEST(MutableAnySpanTest, MutableToConstConversion) {
  std::vector<std::string> v = {"a", "b", "c", "d"};
  AnySpan<std::string> mutable_span(v);
  AnySpan<const std::string> const_span(mutable_span);
  EXPECT_TRUE(any_span_internal::IsCheap(const_span));
  mutable_span[0] = "e";
  EXPECT_EQ(const_span[0], "e");
  mutable_span.subspan(1)[0] = "f";
  EXPECT_EQ(const_span[1], "f");

  std::vector<std::string> v2 = {"a"};
  mutable_span = AnySpan<std::string>(v2);
  EXPECT_EQ(const_span[0], "e");
}

TEST(MutableAnySpanTest, DerefMutableToConstConversion) {
  std::vector<std::string> v = {"a", "b", "c", "d"};
  std::vector<std::string*> v_ptrs;
  for (std::string& s : v) {
    v_ptrs.push_back(&s);
  }
  AnySpan<std::string> mutable_span(v_ptrs, any_span_transform::Deref());
  AnySpan<const std::string> const_span(mutable_span);
  EXPECT_TRUE(any_span_internal::IsCheap(const_span));
  mutable_span[0] = "e";
  EXPECT_EQ(const_span[0], "e");
  EXPECT_EQ(v[0], "e");
  mutable_span.subspan(1)[0] = "f";
  EXPECT_EQ(const_span[1], "f");
}

TEST(MutableAnySpanTest, MutableToConstDeque) {
  std::deque<std::string> d = {"a", "b", "c", "d"};
  AnySpan<std::string> mutable_span(d);
  AnySpan<const std::string> const_span(mutable_span);
  EXPECT_FALSE(any_span_internal::IsCheap(const_span));
  mutable_span[0] = "e";
  EXPECT_EQ(const_span[0], "e");
  mutable_span.subspan(1)[0] = "f";
  EXPECT_EQ(const_span[1], "f");
}

TEST(MutableAnySpanTest, Deref) {
  std::string s = "a";
  AnySpan<std::string>({&s}, any_span_transform::Deref())[0] = "b";
  EXPECT_EQ(s, "b");
}

// Checks that recommended usage works.
TEST(MutableAnySpanTest, AsFunctionArgument) {
  std::vector<std::string> v = {"a", "b", "c", "d"};
  auto set_all_to_e = [](AnySpan<std::string> span) {
    for (std::string& s : span) {
      s = "e";
    }
  };
  set_all_to_e(AnySpan<std::string>(v));
  for (const std::string& e : v) {
    EXPECT_EQ(e, "e");
  }
}

TEST(MutableAnySpanTest, MutateThroughIterator) {
  std::vector<std::string> v = {"a", "b", "c", "d"};
  AnySpan<std::string> span(v);
  AnySpan<std::string>::iterator it = span.begin();
  it[2] = "e";
  EXPECT_EQ(v[2], "e");
  ++it;
  *it = "f";
  EXPECT_EQ(v[1], "f");
  for (std::string& s : span) {
    s = "g";
  }
  for (const std::string& e : v) {
    EXPECT_EQ(e, "g");
  }
}

TEST(MutableAnySpanTest, MutateThroughSubSpan) {
  std::vector<int> v = {1, 2, 3, 4};
  AnySpan<int> span(v);
  span = span.subspan(1, 2);
  for (int& i : span) {
    i = 42;
  }
  EXPECT_THAT(v, ElementsAre(1, 42, 42, 4));
}

TYPED_TEST(AnySpanTest, SubspanToEnd) {
  int arr[] = {0, 1, 2};
  AnySpan<int> span(arr);
  EXPECT_THAT(span.subspan(0, AnySpan<int>::npos), ElementsAre(0, 1, 2));
  EXPECT_THAT(span.subspan(1, AnySpan<int>::npos), ElementsAre(1, 2));
  EXPECT_THAT(span.subspan(2, AnySpan<int>::npos), ElementsAre(2));
  EXPECT_THAT(span.subspan(3, AnySpan<int>::npos), ElementsAre());
#if GTEST_HAS_DEATH_TEST
  if (IsHardened()) {
    EXPECT_DEATH(span.subspan(4, AnySpan<int>::npos), "");
    EXPECT_DEATH(span.subspan(AnySpan<int>::npos, AnySpan<int>::npos), "");
  }
#endif
}

TEST(MutableAnySpanTest, NonTruncatingSubspan) {
  std::vector<int> v = {1, 2, 3, 4};
  AnySpan<int> span(v);
#if GTEST_HAS_DEATH_TEST
  if (IsHardened()) {
    EXPECT_DEATH(span.subspan(5, 0), "");
    EXPECT_DEATH(span.subspan(5, 1), "");
    EXPECT_DEATH(span.subspan(AnySpan<int>::npos, 0), "");
    EXPECT_DEATH(span.subspan(AnySpan<int>::npos, 1), "");
      EXPECT_DEATH(span.subspan(0, 5), "");
      EXPECT_DEATH(span.first(5), "");
      EXPECT_DEATH(span.first(AnySpan<int>::npos), "");
  }
#endif
}

TEST(MutableAnySpanTest, IteratorToConstIteratorConversion) {
  std::vector<int> v = {1};
  AnySpan<int> span(v);
  AnySpan<int>::iterator it = span.begin();
  AnySpan<int>::const_iterator cit = it;
  EXPECT_EQ(&*it, &*cit);
}

// Checks that implicit conversion from a mutable view works.
TEST(MutableAnySpanTest, ImplicitConversionFromMutableSpan) {
  std::vector<std::string> v = {"a", "b", "c", "d"};
  absl::Span<std::string> mutable_slice(v);
  auto set_all_to_e = [](AnySpan<std::string> span) {
    for (std::string& s : span) {
      s = "e";
    }
  };
  set_all_to_e(mutable_slice);
  for (const std::string& s : v) {
    EXPECT_EQ(s, "e");
  }
}

bool WantsStringOrAnySpan(absl::string_view /* ignored */) { return false; }
bool WantsStringOrAnySpan(AnySpan<std::string> /* ignored */) { return true; }
bool WantsStringOrConstAnySpan(absl::string_view /* ignored */) {
  return false;
}
bool WantsStringOrConstAnySpan(AnySpan<const std::string> /* ignored */) {
  return true;
}
// Checks that AnySpan(char (&array)[N]) is not used in the overload
// resolution when building an AnySpan<string> (it would not compile anyway).
TEST(MutableAnySpanTest, ResolveAmbiguityWithAnySpanOfChars) {
  std::string foo;
  AnySpan<std::string> bar;
  EXPECT_FALSE(WantsStringOrAnySpan("abc"));
  EXPECT_FALSE(WantsStringOrAnySpan(foo));
  EXPECT_TRUE(WantsStringOrAnySpan(bar));
  EXPECT_FALSE(WantsStringOrConstAnySpan("abc"));
  EXPECT_FALSE(WantsStringOrConstAnySpan(foo));
  EXPECT_TRUE(WantsStringOrConstAnySpan(bar));
}

TEST(AnySpanTest, SupportsAbslHash) {
  std::vector<size_t> v = {1, 2, 3};
  std::vector<std::string> vs = {"1", "2", "3"};
  std::vector<std::unique_ptr<size_t>> vu;
  vu.push_back(std::make_unique<size_t>(1));
  vu.push_back(std::make_unique<size_t>(2));
  vu.push_back(std::make_unique<size_t>(3));

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      // {1, 2, 3}
      AnySpan<const size_t>(v),
      AnySpan<const size_t>(vu, any_span_transform::Deref()),
      // {1, 2}
      AnySpan<const size_t>(std::vector<size_t>({1, 2})),
      AnySpan<const size_t>(std::deque<size_t>({1, 2})),
      // {1, 2, 4}
      AnySpan<const size_t>(std::vector<size_t>({1, 2, 4})),
      AnySpan<const size_t>(std::deque<size_t>({1, 2, 4})),
      // Empty.
      AnySpan<const size_t>(),
      AnySpan<const size_t>(std::vector<size_t>()),
  }));
}

// This is necessary to avoid a bunch of lint warnings suggesting that we use
// EXPECT_EQ/etc.
bool Identity(bool b) { return b; }

template <typename T, typename U, typename V>
void TestEquality(T&& orig, U&& eq, V&& diff) {
  EXPECT_TRUE(Identity(std::forward<T>(orig) == std::forward<U>(eq)));
  EXPECT_TRUE(Identity(std::forward<U>(eq) == std::forward<T>(orig)));
  EXPECT_FALSE(Identity(std::forward<T>(orig) != std::forward<U>(eq)));
  EXPECT_FALSE(Identity(std::forward<U>(eq) != std::forward<T>(orig)));

  EXPECT_FALSE(Identity(std::forward<T>(orig) == std::forward<V>(diff)));
  EXPECT_FALSE(Identity(std::forward<V>(diff) == std::forward<T>(orig)));
  EXPECT_TRUE(Identity(std::forward<T>(orig) != std::forward<V>(diff)));
  EXPECT_TRUE(Identity(std::forward<V>(diff) != std::forward<T>(orig)));
}

TEST(AnySpanTest, Equals) {
  int arr[] = {1, 2, 3};
  std::vector<int> eq(std::begin(arr), std::end(arr));
  int diff[] = {1, 2, 4};
  TestEquality(AnySpan<int>(arr), AnySpan<int>(eq), AnySpan<int>(diff));
  TestEquality(AnySpan<const int>(arr), AnySpan<int>(eq), AnySpan<int>(diff));
  TestEquality(AnySpan<int>(arr), AnySpan<const int>(eq),
               AnySpan<const int>(diff));
  TestEquality(AnySpan<const int>(arr), AnySpan<const int>(eq),
               AnySpan<const int>(diff));

  TestEquality(AnySpan<const int>(arr), eq, diff);
  TestEquality(AnySpan<int>(arr), eq, diff);
}

// If an object is copied, mark the bool pointed by `copied` true.
class MarkIfCopied {
 public:
  explicit MarkIfCopied(bool* copied) : copied_(copied) {}

  // NOLINT_NEXT_LINE(google-explicit-constructor)
  MarkIfCopied(const MarkIfCopied& other) { *this = other; }

  MarkIfCopied& operator=(const MarkIfCopied& other) {
    copied_ = other.copied_;
    *copied_ = true;
    return *this;
  }

 private:
  bool* copied_;
};

TEST(MarkIfCopied, EnsureWorks) {
  bool copied = false;
  MarkIfCopied item(&copied);
  ASSERT_FALSE(copied);
  MarkIfCopied _unused_copied_item(item);
  EXPECT_TRUE(copied);
}

TEST(AnySpanTest, ReferenceWrapperNotCopied) {
  bool copied1 = false;
  bool copied2 = false;
  MarkIfCopied item1(&copied1);
  MarkIfCopied item2(&copied2);
  std::vector<std::reference_wrapper<const MarkIfCopied>> items;
  items.push_back(std::cref(item1));
  items.push_back(std::cref(item2));
  AnySpan<const MarkIfCopied> span(items);
  EXPECT_EQ(2, span.size());
  EXPECT_FALSE(copied1);
  EXPECT_FALSE(copied2);
}

}  // namespace
ABSL_NAMESPACE_END
}  // namespace absl
