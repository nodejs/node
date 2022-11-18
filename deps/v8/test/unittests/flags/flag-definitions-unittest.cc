// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#include <stdlib.h>

#include "src/flags/flags.h"
#include "src/init/v8.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class FlagDefinitionsTest : public ::testing::Test {
 public:
  void SetUp() override { FlagList::EnforceFlagImplications(); }
};

void TestDefault() {
  CHECK(v8_flags.testing_bool_flag);
  CHECK_EQ(13, v8_flags.testing_int_flag);
  CHECK_EQ(2.5, v8_flags.testing_float_flag);
  CHECK_EQ(0, strcmp(v8_flags.testing_string_flag, "Hello, world!"));
}

// This test must be executed first!
TEST_F(FlagDefinitionsTest, Default) { TestDefault(); }

TEST_F(FlagDefinitionsTest, Flags1) { FlagList::PrintHelp(); }

TEST_F(FlagDefinitionsTest, Flags2) {
  int argc = 8;
  const char* argv[] = {"Test2",
                        "-notesting-bool-flag",
                        "--notesting-maybe-bool-flag",
                        "notaflag",
                        "--testing_int_flag=77",
                        "-testing_float_flag=.25",
                        "--testing_string_flag",
                        "no way!"};
  CHECK_EQ(0, FlagList::SetFlagsFromCommandLine(&argc, const_cast<char**>(argv),
                                                false));
  CHECK_EQ(8, argc);
  CHECK(!v8_flags.testing_bool_flag);
  CHECK(v8_flags.testing_maybe_bool_flag.value().has_value());
  CHECK(!v8_flags.testing_maybe_bool_flag.value().value());
  CHECK_EQ(77, v8_flags.testing_int_flag);
  CHECK_EQ(.25, v8_flags.testing_float_flag);
  CHECK_EQ(0, strcmp(v8_flags.testing_string_flag, "no way!"));
}

TEST_F(FlagDefinitionsTest, Flags2b) {
  const char* str =
      " -notesting-bool-flag notaflag   --testing_int_flag=77 "
      "-notesting-maybe-bool-flag   "
      "-testing_float_flag=.25  "
      "--testing_string_flag   no_way!  ";
  CHECK_EQ(0, FlagList::SetFlagsFromString(str, strlen(str)));
  CHECK(!v8_flags.testing_bool_flag);
  CHECK(v8_flags.testing_maybe_bool_flag.value().has_value());
  CHECK(!v8_flags.testing_maybe_bool_flag.value().value());
  CHECK_EQ(77, v8_flags.testing_int_flag);
  CHECK_EQ(.25, v8_flags.testing_float_flag);
  CHECK_EQ(0, strcmp(v8_flags.testing_string_flag, "no_way!"));
}

TEST_F(FlagDefinitionsTest, Flags3) {
  int argc = 9;
  const char* argv[] = {"Test3",
                        "--testing_bool_flag",
                        "--testing-maybe-bool-flag",
                        "notaflag",
                        "--testing_int_flag",
                        "-666",
                        "--testing_float_flag",
                        "-12E10",
                        "-testing-string-flag=foo-bar"};
  CHECK_EQ(0, FlagList::SetFlagsFromCommandLine(&argc, const_cast<char**>(argv),
                                                true));
  CHECK_EQ(2, argc);
  CHECK(v8_flags.testing_bool_flag);
  CHECK(v8_flags.testing_maybe_bool_flag.value().has_value());
  CHECK(v8_flags.testing_maybe_bool_flag.value().value());
  CHECK_EQ(-666, v8_flags.testing_int_flag);
  CHECK_EQ(-12E10, v8_flags.testing_float_flag);
  CHECK_EQ(0, strcmp(v8_flags.testing_string_flag, "foo-bar"));
}

TEST_F(FlagDefinitionsTest, Flags3b) {
  const char* str =
      "--testing_bool_flag --testing-maybe-bool-flag notaflag "
      "--testing_int_flag -666 "
      "--testing_float_flag -12E10 "
      "-testing-string-flag=foo-bar";
  CHECK_EQ(0, FlagList::SetFlagsFromString(str, strlen(str)));
  CHECK(v8_flags.testing_bool_flag);
  CHECK(v8_flags.testing_maybe_bool_flag.value().has_value());
  CHECK(v8_flags.testing_maybe_bool_flag.value().value());
  CHECK_EQ(-666, v8_flags.testing_int_flag);
  CHECK_EQ(-12E10, v8_flags.testing_float_flag);
  CHECK_EQ(0, strcmp(v8_flags.testing_string_flag, "foo-bar"));
}

TEST_F(FlagDefinitionsTest, Flags4) {
  int argc = 3;
  const char* argv[] = {"Test4", "--testing_bool_flag", "--foo"};
  CHECK_EQ(0, FlagList::SetFlagsFromCommandLine(&argc, const_cast<char**>(argv),
                                                true));
  CHECK_EQ(2, argc);
  CHECK(!v8_flags.testing_maybe_bool_flag.value().has_value());
}

TEST_F(FlagDefinitionsTest, Flags4b) {
  const char* str = "--testing_bool_flag --foo";
  CHECK_EQ(2, FlagList::SetFlagsFromString(str, strlen(str)));
  CHECK(!v8_flags.testing_maybe_bool_flag.value().has_value());
}

TEST_F(FlagDefinitionsTest, Flags5) {
  int argc = 2;
  const char* argv[] = {"Test5", "--testing_int_flag=\"foobar\""};
  CHECK_EQ(1, FlagList::SetFlagsFromCommandLine(&argc, const_cast<char**>(argv),
                                                true));
  CHECK_EQ(2, argc);
}

TEST_F(FlagDefinitionsTest, Flags5b) {
  const char* str = "                     --testing_int_flag=\"foobar\"";
  CHECK_EQ(1, FlagList::SetFlagsFromString(str, strlen(str)));
}

TEST_F(FlagDefinitionsTest, Flags6) {
  int argc = 4;
  const char* argv[] = {"Test5", "--testing-int-flag", "0",
                        "--testing_float_flag"};
  CHECK_EQ(3, FlagList::SetFlagsFromCommandLine(&argc, const_cast<char**>(argv),
                                                true));
  CHECK_EQ(2, argc);
}

TEST_F(FlagDefinitionsTest, Flags6b) {
  const char* str = "       --testing-int-flag 0      --testing_float_flag    ";
  CHECK_EQ(3, FlagList::SetFlagsFromString(str, strlen(str)));
}

TEST_F(FlagDefinitionsTest, FlagsRemoveIncomplete) {
  // Test that processed command line arguments are removed, even
  // if the list of arguments ends unexpectedly.
  int argc = 3;
  const char* argv[] = {"", "--testing-bool-flag", "--expose-gc-as"};
  CHECK_EQ(2, FlagList::SetFlagsFromCommandLine(&argc, const_cast<char**>(argv),
                                                true));
  CHECK(argv[1]);
  CHECK_EQ(2, argc);
}

TEST_F(FlagDefinitionsTest, FlagsJitlessImplications) {
  if (v8_flags.jitless) {
    // Double-check implications work as expected. Our implication system is
    // fairly primitive and can break easily depending on the implication
    // definition order in flag-definitions.h.
    CHECK(!v8_flags.turbofan);
    CHECK(!v8_flags.maglev);
    CHECK(!v8_flags.sparkplug);
#if V8_ENABLE_WEBASSEMBLY
    CHECK(!v8_flags.validate_asm);
    CHECK(!v8_flags.asm_wasm_lazy_compilation);
    CHECK(!v8_flags.wasm_lazy_compilation);
#endif  // V8_ENABLE_WEBASSEMBLY
  }
}

TEST_F(FlagDefinitionsTest, FreezeFlags) {
  // Before freezing, we can arbitrarily change values.
  CHECK_EQ(13, v8_flags.testing_int_flag);  // Initial (default) value.
  v8_flags.testing_int_flag = 27;
  CHECK_EQ(27, v8_flags.testing_int_flag);

  // Get a direct pointer to the flag storage.
  static_assert(sizeof(v8_flags.testing_int_flag) == sizeof(int));
  int* direct_testing_int_ptr =
      reinterpret_cast<int*>(&v8_flags.testing_int_flag);
  CHECK_EQ(27, *direct_testing_int_ptr);
  *direct_testing_int_ptr = 42;
  CHECK_EQ(42, v8_flags.testing_int_flag);

  // Now freeze flags. Accesses via the API and via the direct pointer should
  // both crash.
  FlagList::FreezeFlags();
  // Accessing via the API fails with a CHECK.
  ASSERT_DEATH_IF_SUPPORTED(v8_flags.testing_int_flag = 41,
                            "Check failed: !IsFrozen\\(\\)");
  // Writing to the memory directly results in a segfault.
  ASSERT_DEATH_IF_SUPPORTED(*direct_testing_int_ptr = 41, "");
  // We can still read the old value.
  CHECK_EQ(42, v8_flags.testing_int_flag);
  CHECK_EQ(42, *direct_testing_int_ptr);
}

}  // namespace internal
}  // namespace v8
