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

#include "absl/strings/internal/stringify_stream.h"

#include <cstddef>
#include <iomanip>
#include <ostream>
#include <sstream>

#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace strings_internal {
namespace {

// Exercises the Append(size_t, char) overload
struct AppendNCharsTest {
  size_t count;
  char ch;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const AppendNCharsTest& t) {
    sink.Append(t.count, t.ch);
  }
};
TEST(StringifyStreamTest, AppendNChars) {
  std::ostringstream os;
  StringifyStream(os) << AppendNCharsTest{5, 'a'};
  EXPECT_EQ(os.str(), "aaaaa");
}

// Exercises the Append(absl::string_view) overload
struct AppendStringViewTest {
  absl::string_view v;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const AppendStringViewTest& t) {
    sink.Append(t.v);
  }
};
TEST(StringifyStreamTest, AppendStringView) {
  std::ostringstream os;
  StringifyStream(os) << AppendStringViewTest{"abc"};
  EXPECT_EQ(os.str(), "abc");
}

// Exercises AbslFormatFlush(OStringStreamSink*, absl::string_view v)
struct AbslFormatFlushTest {
  absl::string_view a, b, c;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const AbslFormatFlushTest& t) {
    absl::Format(&sink, "%s, %s, %s", t.a, t.b, t.c);
  }
};
TEST(StringifyStreamTest, AbslFormatFlush) {
  std::ostringstream os;
  StringifyStream(os) << AbslFormatFlushTest{"a", "b", "c"};
  EXPECT_EQ(os.str(), "a, b, c");
}

// If overloads of both AbslStringify and operator<< are defined for the type,
// the operator<< overload should take precedence.
struct PreferStreamInsertionOverAbslStringifyTest {
  friend std::ostream& operator<<(  // NOLINT(clang-diagnostic-unused-function)
      std::ostream& os, const PreferStreamInsertionOverAbslStringifyTest&) {
    return os << "good";
  }

  template <typename Sink>
  friend void AbslStringify  // NOLINT(clang-diagnostic-unused-function)
      (Sink& sink, const PreferStreamInsertionOverAbslStringifyTest&) {
    sink.Append("bad");
  }
};
TEST(StringifyStreamTest, PreferStreamInsertionOverAbslStringify) {
  std::ostringstream os;
  StringifyStream(os) << PreferStreamInsertionOverAbslStringifyTest{};
  EXPECT_EQ(os.str(), "good");
}
TEST(StringifyStreamTest, SupportEndl) {
  std::ostringstream os;
  StringifyStream(os) << std::endl;
  EXPECT_EQ(os.str(), "\n");
}
TEST(StringifyStreamTest, SupportSetbase) {
  std::ostringstream os;
  StringifyStream(os) << std::setbase(16) << 255;
  EXPECT_EQ(os.str(), "ff");
}

}  // namespace
}  // namespace strings_internal
ABSL_NAMESPACE_END
}  // namespace absl
