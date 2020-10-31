/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/ext/base/string_writer.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace base {
namespace {

TEST(StringWriterTest, BasicCases) {
  char buffer[128];
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendChar('0');
    ASSERT_EQ(writer.GetStringView().ToStdString(), "0");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendInt(132545);
    ASSERT_EQ(writer.GetStringView().ToStdString(), "132545");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendUnsignedInt(523);
    ASSERT_EQ(writer.GetStringView().ToStdString(), "523");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendPaddedInt<'0', 3>(0);
    ASSERT_EQ(writer.GetStringView().ToStdString(), "000");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendPaddedInt<'0', 1>(1);
    ASSERT_EQ(writer.GetStringView().ToStdString(), "1");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendPaddedInt<'0', 3>(1);
    ASSERT_EQ(writer.GetStringView().ToStdString(), "001");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendPaddedInt<'0', 0>(1);
    ASSERT_EQ(writer.GetStringView().ToStdString(), "1");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendPaddedInt<' ', 5>(123);
    ASSERT_EQ(writer.GetStringView().ToStdString(), "  123");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendPaddedUnsignedInt<' ', 5>(123);
    ASSERT_EQ(writer.GetStringView().ToStdString(), "  123");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendDouble(123.25);
    ASSERT_EQ(writer.GetStringView().ToStdString(), "123.250000");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendInt(std::numeric_limits<int64_t>::min());
    ASSERT_EQ(writer.GetStringView().ToStdString(), "-9223372036854775808");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendInt(std::numeric_limits<int64_t>::max());
    ASSERT_EQ(writer.GetStringView().ToStdString(), "9223372036854775807");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendUnsignedInt(std::numeric_limits<uint64_t>::max());
    ASSERT_EQ(writer.GetStringView().ToStdString(), "18446744073709551615");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendBool(true);
    ASSERT_EQ(writer.GetStringView().ToStdString(), "true");
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendBool(false);
    ASSERT_EQ(writer.GetStringView().ToStdString(), "false");
  }

  constexpr char kTestStr[] = "test";
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendLiteral(kTestStr);
    ASSERT_EQ(writer.GetStringView().ToStdString(), kTestStr);
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendString(kTestStr, sizeof(kTestStr) - 1);
    ASSERT_EQ(writer.GetStringView().ToStdString(), kTestStr);
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendString(kTestStr);
    ASSERT_EQ(writer.GetStringView().ToStdString(), kTestStr);
  }
  {
    base::StringWriter writer(buffer, sizeof(buffer));
    writer.AppendChar('x', sizeof(buffer));
    ASSERT_EQ(writer.GetStringView().ToStdString(),
              std::string(sizeof(buffer), 'x').c_str());
  }
}

TEST(StringWriterTest, WriteAllTypes) {
  char buffer[128];
  base::StringWriter writer(buffer, sizeof(buffer));
  writer.AppendChar('0');
  writer.AppendInt(132545);
  writer.AppendUnsignedInt(523);
  writer.AppendPaddedInt<'0', 0>(1);
  writer.AppendPaddedInt<'0', 3>(0);
  writer.AppendPaddedInt<'0', 1>(1);
  writer.AppendPaddedInt<'0', 2>(1);
  writer.AppendPaddedInt<'0', 3>(1);
  writer.AppendPaddedInt<' ', 5>(123);
  writer.AppendPaddedUnsignedInt<' ', 5>(456);
  writer.AppendDouble(123.25);
  writer.AppendBool(true);

  constexpr char kTestStr[] = "test";
  writer.AppendLiteral(kTestStr);
  writer.AppendString(kTestStr, sizeof(kTestStr) - 1);
  writer.AppendString(kTestStr);

  ASSERT_EQ(writer.GetStringView().ToStdString(),
            "01325455231000101001  123  456123.250000truetesttesttest");
}

}  // namespace
}  // namespace base
}  // namespace perfetto
