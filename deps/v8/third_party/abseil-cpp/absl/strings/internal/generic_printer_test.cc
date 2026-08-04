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

#include "absl/strings/internal/generic_printer.h"

#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"

namespace generic_logging_test {
struct NotStreamable {};
}  // namespace generic_logging_test

static std::ostream& operator<<(std::ostream& os,
                                const generic_logging_test::NotStreamable&) {
  return os << "This overload should NOT be found by GenericPrint.";
}

// Types to test selection logic for streamable and non-streamable types.
namespace generic_logging_test {
struct Streamable {
  int x;
  friend std::ostream& operator<<(std::ostream& os, const Streamable& l) {
    return os << "Streamable{" << l.x << "}";
  }
};
}  // namespace generic_logging_test

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace strings_internal {
namespace {

using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ContainsRegex;
using ::testing::EndsWith;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::MatchesRegex;

struct AbslStringifiable {
  template <typename S>
  friend void AbslStringify(S& sink, const AbslStringifiable&) {
    sink.Append("AbslStringifiable!");
  }
};

auto IsUnprintable() {
#ifdef GTEST_USES_SIMPLE_RE
  return HasSubstr("unprintable value of size");
#else
  return ContainsRegex(
      "\\[unprintable value of size [0-9]+ @(0x)?[0-9a-fA-F]+\\]");
#endif
}

auto HasExactlyNInstancesOf(int n, absl::string_view me) {
#ifdef GTEST_USES_SIMPLE_RE
  (void)n;
  return HasSubstr(me);
#else
  absl::string_view value_m_times = "(.*$0){$1}.*";

  return AllOf(MatchesRegex(absl::Substitute(value_m_times, me, n)),
               Not(MatchesRegex(absl::Substitute(value_m_times, me, n + 1))));
#endif
}

template <typename T>
std::string GenericPrintToString(const T& v) {
  std::stringstream ss;
  ss << GenericPrint(v);
  {
    std::stringstream ss2;
    ss2 << GenericPrint() << v;
    EXPECT_EQ(ss.str(), ss2.str());
  }
  return ss.str();
}

TEST(GenericPrinterTest, Bool) {
  EXPECT_EQ("true", GenericPrintToString(true));
  EXPECT_EQ("false", GenericPrintToString(false));
}

TEST(GenericPrinterTest, VectorOfBool) {
  std::vector<bool> v{true, false, true};
  const auto& cv = v;
  EXPECT_EQ("[true, false, true]", GenericPrintToString(v));
  EXPECT_EQ("true", GenericPrintToString(v[0]));
  EXPECT_EQ("true", GenericPrintToString(cv[0]));
}

TEST(GenericPrinterTest, CharLiterals) {
  EXPECT_EQ(R"(a"\b)", GenericPrintToString(R"(a"\b)"));
}

TEST(GenericPrinterTest, Builtin) {
  EXPECT_EQ("123", GenericPrintToString(123));
}

TEST(GenericPrinterTest, AbslStringifiable) {
  EXPECT_EQ("AbslStringifiable!", GenericPrintToString(AbslStringifiable{}));
}

TEST(GenericPrinterTest, Nullptr) {
  EXPECT_EQ("nullptr", GenericPrintToString(nullptr));
}

TEST(GenericPrinterTest, Chars) {
  EXPECT_EQ(R"('\x0a' (0x0a 10))", GenericPrintToString('\x0a'));
  EXPECT_EQ(R"(' ' (0x20 32))", GenericPrintToString(' '));
  EXPECT_EQ(R"('~' (0x7e 126))", GenericPrintToString('~'));
  EXPECT_EQ(R"('\'' (0x27 39))", GenericPrintToString('\''));
}

TEST(GenericPrinterTest, SignedChars) {
  EXPECT_EQ(R"('\x0a' (0x0a 10))",
            GenericPrintToString(static_cast<signed char>('\x0a')));
  EXPECT_EQ(R"(' ' (0x20 32))",
            GenericPrintToString(static_cast<signed char>(' ')));
  EXPECT_EQ(R"('~' (0x7e 126))",
            GenericPrintToString(static_cast<signed char>('~')));
  EXPECT_EQ(R"('\'' (0x27 39))",
            GenericPrintToString(static_cast<signed char>('\'')));
}

TEST(GenericPrinterTest, UnsignedChars) {
  EXPECT_EQ(R"('\x0a' (0x0a 10))",
            GenericPrintToString(static_cast<unsigned char>('\x0a')));
  EXPECT_EQ(R"(' ' (0x20 32))",
            GenericPrintToString(static_cast<unsigned char>(' ')));
  EXPECT_EQ(R"('~' (0x7e 126))",
            GenericPrintToString(static_cast<unsigned char>('~')));
  EXPECT_EQ(R"('\'' (0x27 39))",
            GenericPrintToString(static_cast<unsigned char>('\'')));
}

TEST(GenericPrinterTest, Bytes) {
  EXPECT_EQ("0x00", GenericPrintToString(static_cast<std::byte>(0)));
  EXPECT_EQ("0x7f", GenericPrintToString(static_cast<std::byte>(0x7F)));
  EXPECT_EQ("0xff", GenericPrintToString(static_cast<std::byte>(0xFF)));
}

TEST(GenericPrinterTest, Strings) {
  const std::string expected_quotes = R"("a\"\\b")";
  EXPECT_EQ(expected_quotes, GenericPrintToString(std::string(R"(a"\b)")));
  const std::string expected_nonprintable = R"("\x00\xcd\n\xab")";
  EXPECT_EQ(expected_nonprintable,
            GenericPrintToString(absl::string_view("\0\315\n\xAB", 4)));
}

TEST(GenericPrinterTest, PreciseFloat) {
  // Instead of testing exactly how the values are formatted, just check that
  // they are distinct.

  // Ensure concise output for exact values:
  EXPECT_EQ("1f", GenericPrintToString(1.f));
  EXPECT_EQ("1.1f", GenericPrintToString(1.1f));

  // Plausible real-world values:
  float f = 10.0000095f;
  EXPECT_NE(GenericPrintToString(f), GenericPrintToString(10.0000105f));
  // Smallest increment for a real-world value:
  EXPECT_NE(GenericPrintToString(f),
            GenericPrintToString(std::nextafter(f, 11)));
  // The two smallest (finite) values possible:
  EXPECT_NE(GenericPrintToString(std::numeric_limits<float>::lowest()),
            GenericPrintToString(
                std::nextafter(std::numeric_limits<float>::lowest(), 1)));
  // Ensure the value has the correct type suffix:
  EXPECT_THAT(GenericPrintToString(0.f), EndsWith("f"));
}

TEST(GenericPrinterTest, PreciseDouble) {
  EXPECT_EQ("1", GenericPrintToString(1.));
  EXPECT_EQ("1.1", GenericPrintToString(1.1));
  double d = 10.000000000000002;
  EXPECT_NE(GenericPrintToString(d), GenericPrintToString(10.000000000000004));
  EXPECT_NE(GenericPrintToString(d),
            GenericPrintToString(std::nextafter(d, 11)));
  EXPECT_NE(GenericPrintToString(std::numeric_limits<double>::lowest()),
            GenericPrintToString(
                std::nextafter(std::numeric_limits<double>::lowest(), 1)));
  EXPECT_THAT(GenericPrintToString(0.), EndsWith("0"));
}

TEST(GenericPrinterTest, PreciseLongDouble) {
  EXPECT_EQ("1L", GenericPrintToString(1.L));
  EXPECT_EQ("1.1L", GenericPrintToString(1.1L));
  long double ld = 10.0000000000000000000000000000002;
  EXPECT_NE(GenericPrintToString(ld),
            GenericPrintToString(10.0000000000000000000000000000004));
  EXPECT_NE(GenericPrintToString(ld),
            GenericPrintToString(std::nextafter(ld, 11)));
  EXPECT_NE(GenericPrintToString(std::numeric_limits<long double>::lowest()),
            GenericPrintToString(
                std::nextafter(std::numeric_limits<long double>::lowest(), 1)));
  EXPECT_THAT(GenericPrintToString(0.L), EndsWith("L"));
}

TEST(GenericPrinterTest, StreamableLvalue) {
  generic_logging_test::Streamable x{234};
  EXPECT_EQ("Streamable{234}", GenericPrintToString(x));
}

TEST(GenericPrinterTest, StreamableXvalue) {
  EXPECT_EQ("Streamable{345}",
            GenericPrintToString(generic_logging_test::Streamable{345}));
}

TEST(GenericPrinterTest, NotStreamableWithoutGenericPrint) {
  ::generic_logging_test::NotStreamable x;
  std::stringstream ss;
  ::operator<<(ss, x);
  EXPECT_EQ(ss.str(), "This overload should NOT be found by GenericPrint.");
}

TEST(GenericPrinterTest, NotStreamableLvalue) {
  generic_logging_test::NotStreamable x;
  EXPECT_THAT(GenericPrintToString(x), IsUnprintable());
}

TEST(GenericPrinterTest, NotStreamableXvalue) {
  EXPECT_THAT(GenericPrintToString(generic_logging_test::NotStreamable{}),
              IsUnprintable());
}

TEST(GenericPrinterTest, DebugString) {
  struct WithDebugString {
    std::string val;
    std::string DebugString() const {
      return absl::StrCat("WithDebugString{", val, "}");
    }
  };
  EXPECT_EQ("WithDebugString{foo}",
            GenericPrintToString(WithDebugString{"foo"}));
}

TEST(GenericPrinterTest, Vector) {
  std::vector<int> v = {4, 5, 6};
  EXPECT_THAT(GenericPrintToString(v), MatchesRegex(".*4,? 5,? 6.*"));
}

TEST(GenericPrinterTest, StreamableVector) {
  std::vector<generic_logging_test::Streamable> v = {{7}, {8}, {9}};
  EXPECT_THAT(GenericPrintToString(v),
              MatchesRegex(".*Streamable.7.,? Streamable.8.,? Streamable.9.*"));
}

TEST(GenericPrinterTest, Map) {
  absl::flat_hash_map<
      std::string, absl::flat_hash_map<std::string, std::pair<double, double>>>
      v = {{"A", {{"B", {.5, .25}}}}};

  EXPECT_THAT(GenericPrintToString(v), R"([<"A", [<"B", <0.5, 0.25>>]>])");

  std::map<std::string, std::map<std::string, std::pair<double, double>>> v2 = {
      {"A", {{"B", {.5, .25}}}}};

  EXPECT_THAT(GenericPrintToString(v2), R"([<"A", [<"B", <0.5, 0.25>>]>])");
}

TEST(GenericPrinterTest, StreamAdapter) {
  std::stringstream ss;
  static_assert(
      std::is_same<
          typename std::remove_reference<decltype(ss << GenericPrint())>::type,
          internal_generic_printer::GenericPrintStreamAdapter::Impl<
              std::stringstream>>::value,
      "expected ostream << GenericPrint() to yield adapter impl");

  ss << GenericPrint() << "again, " << "back-up, " << "cue, "
     << "double-u, " << "eye, "
     << "four: " << generic_logging_test::NotStreamable{};
  EXPECT_THAT(
      ss.str(),
      MatchesRegex(
          "again, back-up, cue, double-u, eye, four: .unprintable value.*"));
}

TEST(GenericPrinterTest, NotStreamableVector) {
  std::vector<generic_logging_test::NotStreamable> v = {{}, {}, {}};
#ifdef GTEST_USES_SIMPLE_RE
  EXPECT_THAT(GenericPrintToString(v), HasSubstr("unprintable"));
#else
  EXPECT_THAT(GenericPrintToString(v), MatchesRegex(".*(unprintable.*){3}.*"));
#endif
}

struct CustomContainer : public std::array<int, 4> {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const CustomContainer& c) {
    absl::Format(&sink, "%d %d", c[0], c[1]);
  }
};

// Checks that AbslStringify (go/totw/215) is respected for container-like
// types.
TEST(GenericPrinterTest, ContainerLikeCustomLogging) {
  CustomContainer c = {1, 2, 3, 4};
  EXPECT_EQ(GenericPrintToString(c), "1 2");
}

// Test helper: this function demonstrates customizable printing logic:
// 'GenericPrinter<T>' can be nominated as a default template argument.
template <typename T, typename Printer = GenericPrinter<T>>
std::string SpecializablePrint(const T& v) {
  std::stringstream ss;
  ss << Printer{v};
  return ss.str();
}

TEST(GenericPrinterTest, DefaultPrinter) {
  EXPECT_EQ("123", SpecializablePrint(123));
}

// Example of custom printing logic. This doesn't actually test anything in
// GenericPrinter, but it's a working example of customizing printing logic (as
// opposed to the comments in generic_printer.h).
struct CustomPrinter {
  explicit CustomPrinter(int) {}
  friend std::ostream& operator<<(std::ostream& os, CustomPrinter&&) {
    return os << "custom printer";
  }
};

TEST(GenericPrinterTest, CustomPrinter) {
  EXPECT_EQ("custom printer", (SpecializablePrint<int, CustomPrinter>(123)));
}

TEST(GenricPrinterTest, Nullopt) {
  EXPECT_EQ("nullopt", GenericPrintToString(std::nullopt));
}

TEST(GenericPrinterTest, Optional) {
  EXPECT_EQ("nullopt", GenericPrintToString(std::optional<int>()));
  EXPECT_EQ("nullopt", GenericPrintToString(std::optional<int>(std::nullopt)));
  EXPECT_EQ("<3>", GenericPrintToString(std::make_optional(3)));
  EXPECT_EQ("<Streamable{3}>", GenericPrintToString(std::make_optional(
                                   generic_logging_test::Streamable{3})));
}

TEST(GenericPrinterTest, Tuple) {
  EXPECT_EQ("<1, two, 3>", GenericPrintToString(std::make_tuple(1, "two", 3)));
}

TEST(GenericPrinterTest, EmptyTuple) {
  EXPECT_EQ("<>", GenericPrintToString(std::make_tuple()));
}

TEST(GenericPrinterTest, TupleWithStreamableMember) {
  EXPECT_EQ("<1, two, Streamable{3}>",
            GenericPrintToString(std::make_tuple(
                1, "two", generic_logging_test::Streamable{3})));
}

TEST(GenericPrinterTest, Variant) {
  EXPECT_EQ(R"(('(index = 0)' "cow"))",
            GenericPrintToString(std::variant<std::string, float>("cow")));

  EXPECT_EQ("('(index = 1)' 1.1f)",
            GenericPrintToString(std::variant<std::string, float>(1.1F)));
}

TEST(GenericPrinterTest, VariantMonostate) {
  EXPECT_THAT(GenericPrintToString(std::variant<std::monostate, std::string>()),
              IsUnprintable());
}

TEST(GenericPrinterTest, VariantNonStreamable) {
  EXPECT_EQ(R"(('(index = 0)' "cow"))",
            GenericPrintToString(
                std::variant<std::string, generic_logging_test::NotStreamable>(
                    "cow")));

  EXPECT_THAT(
      GenericPrintToString(
          std::variant<std::string, generic_logging_test::NotStreamable>(
              generic_logging_test::NotStreamable{})),
      IsUnprintable());
}

TEST(GenericPrinterTest, VariantNestedVariant) {
  EXPECT_EQ(
      "('(index = 1)' ('(index = 1)' 1.1f))",
      GenericPrintToString(std::variant<std::string, std::variant<int, float>>(
          std::variant<int, float>(1.1F))));
}

TEST(GenericPrinterTest, VariantInPlace) {
  EXPECT_EQ("('(index = 0)' 17)", GenericPrintToString(std::variant<int, int>(
                                      std::in_place_index<0>, 17)));

  EXPECT_EQ("('(index = 1)' 17)", GenericPrintToString(std::variant<int, int>(
                                      std::in_place_index<1>, 17)));
}

TEST(GenericPrinterTest, StatusOrLikeOkPrintsValue) {
  EXPECT_EQ(R"(<OK: "cow">)",
            GenericPrintToString(absl::StatusOr<std::string>("cow")));

  EXPECT_EQ(R"(<OK: 1.1f>)", GenericPrintToString(absl::StatusOr<float>(1.1F)));
}

TEST(GenericPrinterTest, StatusOrLikeNonOkPrintsStatus) {
  EXPECT_THAT(
      GenericPrintToString(absl::StatusOr<float>(
          absl::InvalidArgumentError("my error message"))),
      AllOf(HasSubstr("my error message"), HasSubstr("INVALID_ARGUMENT")));

  EXPECT_THAT(GenericPrintToString(
                  absl::StatusOr<int>(absl::AbortedError("other message"))),
              AllOf(HasSubstr("other message"), HasSubstr("ABORTED")));
}

TEST(GenericPrinterTest, StatusOrLikeNonStreamableValueUnprintable) {
  EXPECT_THAT(
      GenericPrintToString(absl::StatusOr<generic_logging_test::NotStreamable>(
          generic_logging_test::NotStreamable{})),
      IsUnprintable());
}

TEST(GenericPrinterTest, StatusOrLikeNonStreamableErrorStillPrintable) {
  EXPECT_THAT(
      GenericPrintToString(absl::StatusOr<generic_logging_test::NotStreamable>(
          absl::AbortedError("other message"))),
      AllOf(HasSubstr("other message"), HasSubstr("ABORTED")));
}

TEST(GenericPrinterTest, IsSupportedPointer) {
  using internal_generic_printer::is_supported_ptr;

  EXPECT_TRUE(is_supported_ptr<std::unique_ptr<std::string>>);
  EXPECT_TRUE(is_supported_ptr<std::unique_ptr<int[]>>);
  EXPECT_TRUE((is_supported_ptr<std::unique_ptr<void, void (*)(void*)>>));

  EXPECT_FALSE(is_supported_ptr<int*>);
  EXPECT_FALSE(is_supported_ptr<std::shared_ptr<int>>);
  EXPECT_FALSE(is_supported_ptr<std::weak_ptr<int>>);
}

TEST(GenericPrinterTest, SmartPointerPrintsNullptrForAllNullptrs) {
  std::unique_ptr<std::string> up;

  EXPECT_EQ("<nullptr>", GenericPrintToString(up));
}

TEST(GenericPrinterTest, SmartPointerPrintsValueIfNonNull) {
  EXPECT_THAT(GenericPrintToString(std::make_unique<int>(5)),
              HasSubstr("pointing to 5"));
}

TEST(GenericPrinterTest, SmartPointerPrintsAddressOfPointee) {
  auto i = std::make_unique<int>(5);
  auto c = std::make_unique<char>('z');
  char memory[] = "abcdefg";
  auto cp = std::make_unique<char*>(memory);

  EXPECT_THAT(GenericPrintToString(i),
              AnyOf(Eq(absl::StrFormat("<%016X pointing to 5>",
                                       reinterpret_cast<intptr_t>(&*i))),
                    Eq(absl::StrFormat("<%#x pointing to 5>",
                                       reinterpret_cast<intptr_t>(&*i)))));

  EXPECT_THAT(
      GenericPrintToString(c),
      AnyOf(HasSubstr(absl::StrFormat("<%016X pointing to 'z'",
                                      reinterpret_cast<intptr_t>(&*c))),
            HasSubstr(absl::StrFormat("<%#x pointing to 'z'",
                                      reinterpret_cast<intptr_t>(&*c)))));

  EXPECT_THAT(GenericPrintToString(cp),
              AnyOf(Eq(absl::StrFormat("<%016X pointing to abcdefg>",
                                       reinterpret_cast<intptr_t>(&*cp))),
                    Eq(absl::StrFormat("<%#x pointing to abcdefg>",
                                       reinterpret_cast<intptr_t>(&*cp)))));
}

TEST(GenericPrinterTest, SmartPointerToArrayOnlyPrintsAddressAndHelpText) {
  auto empty = std::make_unique<int[]>(0);
  auto nonempty = std::make_unique<int[]>(5);
  nonempty[0] = 12345;
  nonempty[4] = 54321;
  // NOTE: ArenaSafeUniquePtr is not meant to support array-type template
  // parameters, so we skip testing that here.
  // http://g/c-users/J-AEFrFHssY/UMMFzCkdBAAJ, b/265984185.

  EXPECT_THAT(
      GenericPrintToString(nonempty),
      AllOf(AnyOf(HasSubstr(absl::StrFormat(
                      "%016X", reinterpret_cast<intptr_t>(nonempty.get()))),
                  HasSubstr(absl::StrFormat(
                      "%#x", reinterpret_cast<intptr_t>(nonempty.get())))),
            HasSubstr("array"), Not(HasSubstr("to 54321")),
            Not(HasSubstr("to 12345"))));

  EXPECT_THAT(
      GenericPrintToString(empty),
      AllOf(AnyOf(HasSubstr(absl::StrFormat(
                      "%016X", reinterpret_cast<intptr_t>(empty.get()))),
                  HasSubstr(absl::StrFormat(
                      "%#x", reinterpret_cast<intptr_t>(empty.get())))),
            HasSubstr("array")));
}

TEST(GenericPrinterTest, SmartPointerToNonObjectType) {
  auto int_ptr_deleter = [](void* data) {
    int* p = static_cast<int*>(data);
    delete p;
  };

  std::unique_ptr<void, decltype(int_ptr_deleter)> void_ptr(new int(959),
                                                            int_ptr_deleter);

  EXPECT_THAT(GenericPrintToString(void_ptr),
              HasSubstr("pointing to a non-object type"));
}

TEST(GenericPrinterTest, PrintsCustomDeleterSmartPointer) {
  // Delete `p` (if not nullptr) only on the 4th time the deleter is used.
  auto four_deleter = [](std::string* p) {
    static int counter = 0;
    if (p == nullptr) return;  // skip calls to moved-from destructors.
    if (++counter >= 4) delete p;
  };

  // Have four `unique_ptr`s "manage" the same string-pointer, with only the
  // final (4th) call to the deleter deleting the string pointer.
  auto* unique_string = new std::string("unique string");
  std::vector<std::unique_ptr<std::string, decltype(four_deleter)>> test_ptrs;
  for (int i = 0; i < 4; ++i) {
    test_ptrs.emplace_back(unique_string, four_deleter);
  }

  EXPECT_THAT(GenericPrintToString(test_ptrs),
              HasExactlyNInstancesOf(4, "unique string"));
}

// Ensure that GenericPrint is robust to recursion when a type's operator<<
// calls into GenericPrint internally.
struct CustomRecursive {
  std::unique_ptr<CustomRecursive> next;
  int val = 0;

  friend std::ostream& operator<<(std::ostream& os, const CustomRecursive& cr) {
    return os << "custom print: next = " << GenericPrintToString(cr.next);
  }
};

TEST(GenericPrinterTest, DISABLED_CustomPrintOverloadRecursionDetected) {
  auto r1 = std::make_unique<CustomRecursive>();
  r1->val = 1;
  auto& r2 = r1->next = std::make_unique<CustomRecursive>();
  r2->val = 2;
  r2->next = std::move(r1);

  EXPECT_THAT(GenericPrintToString(*r2),
              AllOf(HasExactlyNInstancesOf(2, "custom print"),
                    HasExactlyNInstancesOf(1, "<recursive>")));

  r2->next = nullptr;  // break the cycle
}
// <end DISABLED_ test section>

enum CStyleEnum { kValue0, kValue1 };
TEST(GenericPrinterTest, Enum) {
  EXPECT_EQ("1", GenericPrintToString(kValue1));
}

enum class CppStyleEnum { kValue0, kValue1, kValue2 };
TEST(GenericPrinterTest, EnumClass) {
  EXPECT_EQ("2", GenericPrintToString(CppStyleEnum::kValue2));
}

enum class CharBasedEnum : char { kValueA = 'A', kValue1 = '\x01' };
TEST(GenericPrinterTest, CharBasedEnum) {
  EXPECT_EQ("'A' (0x41 65)", GenericPrintToString(CharBasedEnum::kValueA));
  EXPECT_EQ("'\\x01' (0x01 1)", GenericPrintToString(CharBasedEnum::kValue1));
}

enum class WideBasedEnum : uint64_t {
  kValue = std::numeric_limits<uint64_t>::max()
};
TEST(GenericPrinterTest, WideBasedEnum) {
  EXPECT_EQ(absl::StrCat(std::numeric_limits<uint64_t>::max()),
            GenericPrintToString(WideBasedEnum::kValue));
}

enum CStyleEnumWithStringify { kValueA = 0, kValueB = 2 };
template <typename Sink>
void AbslStringify(Sink& sink, CStyleEnumWithStringify e) {
  switch (e) {
    case CStyleEnumWithStringify::kValueA:
      sink.Append("A");
      return;
    case CStyleEnumWithStringify::kValueB:
      sink.Append("B");
      return;
  }
  sink.Append("??");
}
TEST(GenericPrinterTest, CStyleEnumWithStringify) {
  EXPECT_EQ("A", GenericPrintToString(CStyleEnumWithStringify::kValueA));
  EXPECT_EQ("??",
            GenericPrintToString(static_cast<CStyleEnumWithStringify>(1)));
}

enum class CppStyleEnumWithStringify { kValueA, kValueB, kValueC };
template <typename Sink>
void AbslStringify(Sink& sink, CppStyleEnumWithStringify e) {
  switch (e) {
    case CppStyleEnumWithStringify::kValueA:
      sink.Append("A");
      return;
    case CppStyleEnumWithStringify::kValueB:
      sink.Append("B");
      return;
    case CppStyleEnumWithStringify::kValueC:
      sink.Append("C");
      return;
  }
  sink.Append("??");
}
TEST(GenericPrinterTest, CppStyleEnumWithStringify) {
  EXPECT_EQ("A", GenericPrintToString(CppStyleEnumWithStringify::kValueA));
  EXPECT_EQ("??",
            GenericPrintToString(static_cast<CppStyleEnumWithStringify>(17)));
}

enum class CharBasedEnumWithStringify : char { kValueA = 'A', kValueB = 'B' };
template <typename Sink>
void AbslStringify(Sink& sink, CharBasedEnumWithStringify e) {
  switch (e) {
    case CharBasedEnumWithStringify::kValueA:
      sink.Append("charA");
      return;
    case CharBasedEnumWithStringify::kValueB:
      sink.Append("charB");
      return;
  }
  sink.Append("??");
}
TEST(GenericPrinterTest, CharBasedEnumWithStringify) {
  EXPECT_EQ("charA", GenericPrintToString(CharBasedEnumWithStringify::kValueA));
  EXPECT_EQ("??",
            GenericPrintToString(static_cast<CharBasedEnumWithStringify>('W')));
}

}  // namespace
}  // namespace strings_internal
ABSL_NAMESPACE_END
}  // namespace absl
