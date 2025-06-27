// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Macros and functions for implementing parameterized tests
// in Google C++ Testing and Mocking Framework (Google Test)

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_GTEST_PARAM_TEST_H_
#define GOOGLETEST_INCLUDE_GTEST_GTEST_PARAM_TEST_H_

// Value-parameterized tests allow you to test your code with different
// parameters without writing multiple copies of the same test.
//
// Here is how you use value-parameterized tests:

#if 0

// To write value-parameterized tests, first you should define a fixture
// class. It is usually derived from testing::TestWithParam<T> (see below for
// another inheritance scheme that's sometimes useful in more complicated
// class hierarchies), where the type of your parameter values.
// TestWithParam<T> is itself derived from testing::Test. T can be any
// copyable type. If it's a raw pointer, you are responsible for managing the
// lifespan of the pointed values.

class FooTest : public ::testing::TestWithParam<const char*> {
  // You can implement all the usual class fixture members here.
};

// Then, use the TEST_P macro to define as many parameterized tests
// for this fixture as you want. The _P suffix is for "parameterized"
// or "pattern", whichever you prefer to think.

TEST_P(FooTest, DoesBlah) {
  // Inside a test, access the test parameter with the GetParam() method
  // of the TestWithParam<T> class:
  EXPECT_TRUE(foo.Blah(GetParam()));
  ...
}

TEST_P(FooTest, HasBlahBlah) {
  ...
}

// Finally, you can use INSTANTIATE_TEST_SUITE_P to instantiate the test
// case with any set of parameters you want. Google Test defines a number
// of functions for generating test parameters. They return what we call
// (surprise!) parameter generators. Here is a summary of them, which
// are all in the testing namespace:
//
//
//  Range(begin, end [, step]) - Yields values {begin, begin+step,
//                               begin+step+step, ...}. The values do not
//                               include end. step defaults to 1.
//  Values(v1, v2, ..., vN)    - Yields values {v1, v2, ..., vN}.
//  ValuesIn(container)        - Yields values from a C-style array, an STL
//  ValuesIn(begin,end)          container, or an iterator range [begin, end).
//  Bool()                     - Yields sequence {false, true}.
//  Combine(g1, g2, ..., gN)   - Yields all combinations (the Cartesian product
//                               for the math savvy) of the values generated
//                               by the N generators.
//
// For more details, see comments at the definitions of these functions below
// in this file.
//
// The following statement will instantiate tests from the FooTest test suite
// each with parameter values "meeny", "miny", and "moe".

INSTANTIATE_TEST_SUITE_P(InstantiationName,
                         FooTest,
                         Values("meeny", "miny", "moe"));

// To distinguish different instances of the pattern, (yes, you
// can instantiate it more than once) the first argument to the
// INSTANTIATE_TEST_SUITE_P macro is a prefix that will be added to the
// actual test suite name. Remember to pick unique prefixes for different
// instantiations. The tests from the instantiation above will have
// these names:
//
//    * InstantiationName/FooTest.DoesBlah/0 for "meeny"
//    * InstantiationName/FooTest.DoesBlah/1 for "miny"
//    * InstantiationName/FooTest.DoesBlah/2 for "moe"
//    * InstantiationName/FooTest.HasBlahBlah/0 for "meeny"
//    * InstantiationName/FooTest.HasBlahBlah/1 for "miny"
//    * InstantiationName/FooTest.HasBlahBlah/2 for "moe"
//
// You can use these names in --gtest_filter.
//
// This statement will instantiate all tests from FooTest again, each
// with parameter values "cat" and "dog":

const char* pets[] = {"cat", "dog"};
INSTANTIATE_TEST_SUITE_P(AnotherInstantiationName, FooTest, ValuesIn(pets));

// The tests from the instantiation above will have these names:
//
//    * AnotherInstantiationName/FooTest.DoesBlah/0 for "cat"
//    * AnotherInstantiationName/FooTest.DoesBlah/1 for "dog"
//    * AnotherInstantiationName/FooTest.HasBlahBlah/0 for "cat"
//    * AnotherInstantiationName/FooTest.HasBlahBlah/1 for "dog"
//
// Please note that INSTANTIATE_TEST_SUITE_P will instantiate all tests
// in the given test suite, whether their definitions come before or
// AFTER the INSTANTIATE_TEST_SUITE_P statement.
//
// Please also note that generator expressions (including parameters to the
// generators) are evaluated in InitGoogleTest(), after main() has started.
// This allows the user on one hand, to adjust generator parameters in order
// to dynamically determine a set of tests to run and on the other hand,
// give the user a chance to inspect the generated tests with Google Test
// reflection API before RUN_ALL_TESTS() is executed.
//
// You can see samples/sample7_unittest.cc and samples/sample8_unittest.cc
// for more examples.
//
// In the future, we plan to publish the API for defining new parameter
// generators. But for now this interface remains part of the internal
// implementation and is subject to change.
//
//
// A parameterized test fixture must be derived from testing::Test and from
// testing::WithParamInterface<T>, where T is the type of the parameter
// values. Inheriting from TestWithParam<T> satisfies that requirement because
// TestWithParam<T> inherits from both Test and WithParamInterface. In more
// complicated hierarchies, however, it is occasionally useful to inherit
// separately from Test and WithParamInterface. For example:

class BaseTest : public ::testing::Test {
  // You can inherit all the usual members for a non-parameterized test
  // fixture here.
};

class DerivedTest : public BaseTest, public ::testing::WithParamInterface<int> {
  // The usual test fixture members go here too.
};

TEST_F(BaseTest, HasFoo) {
  // This is an ordinary non-parameterized test.
}

TEST_P(DerivedTest, DoesBlah) {
  // GetParam works just the same here as if you inherit from TestWithParam.
  EXPECT_TRUE(foo.Blah(GetParam()));
}

#endif  // 0

#include <functional>
#include <iterator>
#include <utility>

#include "gtest/internal/gtest-internal.h"
#include "gtest/internal/gtest-param-util.h"  // IWYU pragma: export
#include "gtest/internal/gtest-port.h"

namespace testing {

// Functions producing parameter generators.
//
// Google Test uses these generators to produce parameters for value-
// parameterized tests. When a parameterized test suite is instantiated
// with a particular generator, Google Test creates and runs tests
// for each element in the sequence produced by the generator.
//
// In the following sample, tests from test suite FooTest are instantiated
// each three times with parameter values 3, 5, and 8:
//
// class FooTest : public TestWithParam<int> { ... };
//
// TEST_P(FooTest, TestThis) {
// }
// TEST_P(FooTest, TestThat) {
// }
// INSTANTIATE_TEST_SUITE_P(TestSequence, FooTest, Values(3, 5, 8));
//

// Range() returns generators providing sequences of values in a range.
//
// Synopsis:
// Range(start, end)
//   - returns a generator producing a sequence of values {start, start+1,
//     start+2, ..., }.
// Range(start, end, step)
//   - returns a generator producing a sequence of values {start, start+step,
//     start+step+step, ..., }.
// Notes:
//   * The generated sequences never include end. For example, Range(1, 5)
//     returns a generator producing a sequence {1, 2, 3, 4}. Range(1, 9, 2)
//     returns a generator producing {1, 3, 5, 7}.
//   * start and end must have the same type. That type may be any integral or
//     floating-point type or a user defined type satisfying these conditions:
//     * It must be assignable (have operator=() defined).
//     * It must have operator+() (operator+(int-compatible type) for
//       two-operand version).
//     * It must have operator<() defined.
//     Elements in the resulting sequences will also have that type.
//   * Condition start < end must be satisfied in order for resulting sequences
//     to contain any elements.
//
template <typename T, typename IncrementT>
internal::ParamGenerator<T> Range(T start, T end, IncrementT step) {
  return internal::ParamGenerator<T>(
      new internal::RangeGenerator<T, IncrementT>(start, end, step));
}

template <typename T>
internal::ParamGenerator<T> Range(T start, T end) {
  return Range(start, end, 1);
}

// ValuesIn() function allows generation of tests with parameters coming from
// a container.
//
// Synopsis:
// ValuesIn(const T (&array)[N])
//   - returns a generator producing sequences with elements from
//     a C-style array.
// ValuesIn(const Container& container)
//   - returns a generator producing sequences with elements from
//     an STL-style container.
// ValuesIn(Iterator begin, Iterator end)
//   - returns a generator producing sequences with elements from
//     a range [begin, end) defined by a pair of STL-style iterators. These
//     iterators can also be plain C pointers.
//
// Please note that ValuesIn copies the values from the containers
// passed in and keeps them to generate tests in RUN_ALL_TESTS().
//
// Examples:
//
// This instantiates tests from test suite StringTest
// each with C-string values of "foo", "bar", and "baz":
//
// const char* strings[] = {"foo", "bar", "baz"};
// INSTANTIATE_TEST_SUITE_P(StringSequence, StringTest, ValuesIn(strings));
//
// This instantiates tests from test suite StlStringTest
// each with STL strings with values "a" and "b":
//
// ::std::vector< ::std::string> GetParameterStrings() {
//   ::std::vector< ::std::string> v;
//   v.push_back("a");
//   v.push_back("b");
//   return v;
// }
//
// INSTANTIATE_TEST_SUITE_P(CharSequence,
//                          StlStringTest,
//                          ValuesIn(GetParameterStrings()));
//
//
// This will also instantiate tests from CharTest
// each with parameter values 'a' and 'b':
//
// ::std::list<char> GetParameterChars() {
//   ::std::list<char> list;
//   list.push_back('a');
//   list.push_back('b');
//   return list;
// }
// ::std::list<char> l = GetParameterChars();
// INSTANTIATE_TEST_SUITE_P(CharSequence2,
//                          CharTest,
//                          ValuesIn(l.begin(), l.end()));
//
template <typename ForwardIterator>
internal::ParamGenerator<
    typename std::iterator_traits<ForwardIterator>::value_type>
ValuesIn(ForwardIterator begin, ForwardIterator end) {
  typedef typename std::iterator_traits<ForwardIterator>::value_type ParamType;
  return internal::ParamGenerator<ParamType>(
      new internal::ValuesInIteratorRangeGenerator<ParamType>(begin, end));
}

template <typename T, size_t N>
internal::ParamGenerator<T> ValuesIn(const T (&array)[N]) {
  return ValuesIn(array, array + N);
}

template <class Container>
internal::ParamGenerator<typename Container::value_type> ValuesIn(
    const Container& container) {
  return ValuesIn(container.begin(), container.end());
}

// Values() allows generating tests from explicitly specified list of
// parameters.
//
// Synopsis:
// Values(T v1, T v2, ..., T vN)
//   - returns a generator producing sequences with elements v1, v2, ..., vN.
//
// For example, this instantiates tests from test suite BarTest each
// with values "one", "two", and "three":
//
// INSTANTIATE_TEST_SUITE_P(NumSequence,
//                          BarTest,
//                          Values("one", "two", "three"));
//
// This instantiates tests from test suite BazTest each with values 1, 2, 3.5.
// The exact type of values will depend on the type of parameter in BazTest.
//
// INSTANTIATE_TEST_SUITE_P(FloatingNumbers, BazTest, Values(1, 2, 3.5));
//
//
template <typename... T>
internal::ValueArray<T...> Values(T... v) {
  return internal::ValueArray<T...>(std::move(v)...);
}

// Bool() allows generating tests with parameters in a set of (false, true).
//
// Synopsis:
// Bool()
//   - returns a generator producing sequences with elements {false, true}.
//
// It is useful when testing code that depends on Boolean flags. Combinations
// of multiple flags can be tested when several Bool()'s are combined using
// Combine() function.
//
// In the following example all tests in the test suite FlagDependentTest
// will be instantiated twice with parameters false and true.
//
// class FlagDependentTest : public testing::TestWithParam<bool> {
//   virtual void SetUp() {
//     external_flag = GetParam();
//   }
// }
// INSTANTIATE_TEST_SUITE_P(BoolSequence, FlagDependentTest, Bool());
//
inline internal::ParamGenerator<bool> Bool() { return Values(false, true); }

// Combine() allows the user to combine two or more sequences to produce
// values of a Cartesian product of those sequences' elements.
//
// Synopsis:
// Combine(gen1, gen2, ..., genN)
//   - returns a generator producing sequences with elements coming from
//     the Cartesian product of elements from the sequences generated by
//     gen1, gen2, ..., genN. The sequence elements will have a type of
//     std::tuple<T1, T2, ..., TN> where T1, T2, ..., TN are the types
//     of elements from sequences produces by gen1, gen2, ..., genN.
//
// Example:
//
// This will instantiate tests in test suite AnimalTest each one with
// the parameter values tuple("cat", BLACK), tuple("cat", WHITE),
// tuple("dog", BLACK), and tuple("dog", WHITE):
//
// enum Color { BLACK, GRAY, WHITE };
// class AnimalTest
//     : public testing::TestWithParam<std::tuple<const char*, Color> > {...};
//
// TEST_P(AnimalTest, AnimalLooksNice) {...}
//
// INSTANTIATE_TEST_SUITE_P(AnimalVariations, AnimalTest,
//                          Combine(Values("cat", "dog"),
//                                  Values(BLACK, WHITE)));
//
// This will instantiate tests in FlagDependentTest with all variations of two
// Boolean flags:
//
// class FlagDependentTest
//     : public testing::TestWithParam<std::tuple<bool, bool> > {
//   virtual void SetUp() {
//     // Assigns external_flag_1 and external_flag_2 values from the tuple.
//     std::tie(external_flag_1, external_flag_2) = GetParam();
//   }
// };
//
// TEST_P(FlagDependentTest, TestFeature1) {
//   // Test your code using external_flag_1 and external_flag_2 here.
// }
// INSTANTIATE_TEST_SUITE_P(TwoBoolSequence, FlagDependentTest,
//                          Combine(Bool(), Bool()));
//
template <typename... Generator>
internal::CartesianProductHolder<Generator...> Combine(const Generator&... g) {
  return internal::CartesianProductHolder<Generator...>(g...);
}

// ConvertGenerator() wraps a parameter generator in order to cast each produced
// value through a known type before supplying it to the test suite
//
// Synopsis:
// ConvertGenerator<T>(gen)
//   - returns a generator producing the same elements as generated by gen, but
//     each T-typed element is static_cast to a type deduced from the interface
//     that accepts this generator, and then returned
//
// It is useful when using the Combine() function to get the generated
// parameters in a custom type instead of std::tuple
//
// Example:
//
// This will instantiate tests in test suite AnimalTest each one with
// the parameter values tuple("cat", BLACK), tuple("cat", WHITE),
// tuple("dog", BLACK), and tuple("dog", WHITE):
//
// enum Color { BLACK, GRAY, WHITE };
// struct ParamType {
//   using TupleT = std::tuple<const char*, Color>;
//   std::string animal;
//   Color color;
//   ParamType(TupleT t) : animal(std::get<0>(t)), color(std::get<1>(t)) {}
// };
// class AnimalTest
//     : public testing::TestWithParam<ParamType> {...};
//
// TEST_P(AnimalTest, AnimalLooksNice) {...}
//
// INSTANTIATE_TEST_SUITE_P(AnimalVariations, AnimalTest,
//                          ConvertGenerator<ParamType::TupleT>(
//                              Combine(Values("cat", "dog"),
//                                      Values(BLACK, WHITE))));
//
template <typename RequestedT>
internal::ParamConverterGenerator<RequestedT> ConvertGenerator(
    internal::ParamGenerator<RequestedT> gen) {
  return internal::ParamConverterGenerator<RequestedT>(std::move(gen));
}

// As above, but takes a callable as a second argument. The callable converts
// the generated parameter to the test fixture's parameter type. This allows you
// to use a parameter type that does not have a converting constructor from the
// generated type.
//
// Example:
//
// This will instantiate tests in test suite AnimalTest each one with
// the parameter values tuple("cat", BLACK), tuple("cat", WHITE),
// tuple("dog", BLACK), and tuple("dog", WHITE):
//
// enum Color { BLACK, GRAY, WHITE };
// struct ParamType {
//   std::string animal;
//   Color color;
// };
// class AnimalTest
//     : public testing::TestWithParam<ParamType> {...};
//
// TEST_P(AnimalTest, AnimalLooksNice) {...}
//
// INSTANTIATE_TEST_SUITE_P(
//     AnimalVariations, AnimalTest,
//     ConvertGenerator(Combine(Values("cat", "dog"), Values(BLACK, WHITE)),
//                      [](std::tuple<std::string, Color> t) {
//                        return ParamType{.animal = std::get<0>(t),
//                                         .color = std::get<1>(t)};
//                      }));
//
template <typename T, int&... ExplicitArgumentBarrier, typename Gen,
          typename Func,
          typename StdFunction = decltype(std::function(std::declval<Func>()))>
internal::ParamConverterGenerator<T, StdFunction> ConvertGenerator(Gen&& gen,
                                                                   Func&& f) {
  return internal::ParamConverterGenerator<T, StdFunction>(
      std::forward<Gen>(gen), std::forward<Func>(f));
}

// As above, but infers the T from the supplied std::function instead of
// having the caller specify it.
template <int&... ExplicitArgumentBarrier, typename Gen, typename Func,
          typename StdFunction = decltype(std::function(std::declval<Func>()))>
auto ConvertGenerator(Gen&& gen, Func&& f) {
  constexpr bool is_single_arg_std_function =
      internal::IsSingleArgStdFunction<StdFunction>::value;
  if constexpr (is_single_arg_std_function) {
    return ConvertGenerator<
        typename internal::FuncSingleParamType<StdFunction>::type>(
        std::forward<Gen>(gen), std::forward<Func>(f));
  } else {
    static_assert(is_single_arg_std_function,
                  "The call signature must contain a single argument.");
  }
}

#define TEST_P(test_suite_name, test_name)                                     \
  class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                     \
      : public test_suite_name,                                                \
        private ::testing::internal::GTestNonCopyable {                        \
   public:                                                                     \
    GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)() {}                    \
    void TestBody() override;                                                  \
                                                                               \
   private:                                                                    \
    static int AddToRegistry() {                                               \
      ::testing::UnitTest::GetInstance()                                       \
          ->parameterized_test_registry()                                      \
          .GetTestSuitePatternHolder<test_suite_name>(                         \
              GTEST_STRINGIFY_(test_suite_name),                               \
              ::testing::internal::CodeLocation(__FILE__, __LINE__))           \
          ->AddTestPattern(                                                    \
              GTEST_STRINGIFY_(test_suite_name), GTEST_STRINGIFY_(test_name),  \
              new ::testing::internal::TestMetaFactory<GTEST_TEST_CLASS_NAME_( \
                  test_suite_name, test_name)>(),                              \
              ::testing::internal::CodeLocation(__FILE__, __LINE__));          \
      return 0;                                                                \
    }                                                                          \
    [[maybe_unused]] static int gtest_registering_dummy_;                      \
  };                                                                           \
  int GTEST_TEST_CLASS_NAME_(test_suite_name,                                  \
                             test_name)::gtest_registering_dummy_ =            \
      GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::AddToRegistry();     \
  void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBody()

// The last argument to INSTANTIATE_TEST_SUITE_P allows the user to specify
// generator and an optional function or functor that generates custom test name
// suffixes based on the test parameters. Such a function or functor should
// accept one argument of type testing::TestParamInfo<class ParamType>, and
// return std::string.
//
// testing::PrintToStringParamName is a builtin test suffix generator that
// returns the value of testing::PrintToString(GetParam()).
//
// Note: test names must be non-empty, unique, and may only contain ASCII
// alphanumeric characters or underscore. Because PrintToString adds quotes
// to std::string and C strings, it won't work for these types.

#define GTEST_EXPAND_(arg) arg
#define GTEST_GET_FIRST_(first, ...) first
#define GTEST_GET_SECOND_(first, second, ...) second

#define INSTANTIATE_TEST_SUITE_P(prefix, test_suite_name, ...)                \
  static ::testing::internal::ParamGenerator<test_suite_name::ParamType>      \
      gtest_##prefix##test_suite_name##_EvalGenerator_() {                    \
    return GTEST_EXPAND_(GTEST_GET_FIRST_(__VA_ARGS__, DUMMY_PARAM_));        \
  }                                                                           \
  static ::std::string gtest_##prefix##test_suite_name##_EvalGenerateName_(   \
      const ::testing::TestParamInfo<test_suite_name::ParamType>& info) {     \
    if (::testing::internal::AlwaysFalse()) {                                 \
      ::testing::internal::TestNotEmpty(GTEST_EXPAND_(GTEST_GET_SECOND_(      \
          __VA_ARGS__,                                                        \
          ::testing::internal::DefaultParamName<test_suite_name::ParamType>,  \
          DUMMY_PARAM_)));                                                    \
      auto t = std::make_tuple(__VA_ARGS__);                                  \
      static_assert(std::tuple_size<decltype(t)>::value <= 2,                 \
                    "Too Many Args!");                                        \
    }                                                                         \
    return ((GTEST_EXPAND_(GTEST_GET_SECOND_(                                 \
        __VA_ARGS__,                                                          \
        ::testing::internal::DefaultParamName<test_suite_name::ParamType>,    \
        DUMMY_PARAM_))))(info);                                               \
  }                                                                           \
  [[maybe_unused]] static int gtest_##prefix##test_suite_name##_dummy_ =      \
      ::testing::UnitTest::GetInstance()                                      \
          ->parameterized_test_registry()                                     \
          .GetTestSuitePatternHolder<test_suite_name>(                        \
              GTEST_STRINGIFY_(test_suite_name),                              \
              ::testing::internal::CodeLocation(__FILE__, __LINE__))          \
          ->AddTestSuiteInstantiation(                                        \
              GTEST_STRINGIFY_(prefix),                                       \
              &gtest_##prefix##test_suite_name##_EvalGenerator_,              \
              &gtest_##prefix##test_suite_name##_EvalGenerateName_, __FILE__, \
              __LINE__)

// Allow Marking a Parameterized test class as not needing to be instantiated.
#define GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(T)                  \
  namespace gtest_do_not_use_outside_namespace_scope {}                   \
  static const ::testing::internal::MarkAsIgnored gtest_allow_ignore_##T( \
      GTEST_STRINGIFY_(T))

// Legacy API is deprecated but still available
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
#define INSTANTIATE_TEST_CASE_P                                            \
  static_assert(::testing::internal::InstantiateTestCase_P_IsDeprecated(), \
                "");                                                       \
  INSTANTIATE_TEST_SUITE_P
#endif  // GTEST_REMOVE_LEGACY_TEST_CASEAPI_

}  // namespace testing

#endif  // GOOGLETEST_INCLUDE_GTEST_GTEST_PARAM_TEST_H_
