// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/numbers/conversions.h"

#include "src/codegen/source-position.h"
#include "src/init/v8.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class ConversionsTest : public ::testing::Test {
 public:
  ConversionsTest() = default;
  ~ConversionsTest() override = default;

  SourcePosition toPos(int offset) {
    return SourcePosition(offset, offset % 10 - 1);
  }
};

// Some random offsets, mostly at 'suspicious' bit boundaries.

struct IntStringPair {
  int integer;
  std::string string;
};

static IntStringPair int_pairs[] = {{0, "0"},
                                    {101, "101"},
                                    {-1, "-1"},
                                    {1024, "1024"},
                                    {200000, "200000"},
                                    {-1024, "-1024"},
                                    {-200000, "-200000"},
                                    {kMinInt, "-2147483648"},
                                    {kMaxInt, "2147483647"}};

TEST_F(ConversionsTest, IntToCString) {
  std::unique_ptr<char[]> buf(new char[4096]);

  for (size_t i = 0; i < arraysize(int_pairs); i++) {
    ASSERT_STREQ(IntToCString(int_pairs[i].integer, {buf.get(), 4096}),
                 int_pairs[i].string.c_str());
  }
}

struct DoubleStringPair {
  double number;
  std::string string;
};

static DoubleStringPair double_pairs[] = {
    {0.0, "0"},
    {kMinInt, "-2147483648"},
    {kMaxInt, "2147483647"},
    // ES section 7.1.12.1 #sec-tostring-applied-to-the-number-type:
    // -0.0 is stringified to "0".
    {-0.0, "0"},
    {1.1, "1.1"},
    {0.1, "0.1"}};

TEST_F(ConversionsTest, DoubleToCString) {
  std::unique_ptr<char[]> buf(new char[4096]);

  for (size_t i = 0; i < arraysize(double_pairs); i++) {
    ASSERT_STREQ(DoubleToCString(double_pairs[i].number, {buf.get(), 4096}),
                 double_pairs[i].string.c_str());
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
