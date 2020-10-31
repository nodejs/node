/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "tools/ftrace_proto_gen/ftrace_proto_gen.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

TEST(FtraceEventParserTest, InferProtoType) {
  using Field = FtraceEvent::Field;
  EXPECT_EQ(InferProtoType(Field{"char foo[16]", 0, 16, false}).ToString(),
            "string");
  EXPECT_EQ(InferProtoType(Field{"char bar_42[64]", 0, 64, false}).ToString(),
            "string");
  EXPECT_EQ(
      InferProtoType(Field{"__data_loc char[] foo", 0, 4, false}).ToString(),
      "string");
  EXPECT_EQ(InferProtoType(Field{"char[] foo", 0, 8, false}).ToString(),
            "string");
  EXPECT_EQ(InferProtoType(Field{"char * foo", 0, 8, false}).ToString(),
            "string");

  EXPECT_EQ(InferProtoType(Field{"int foo", 0, 4, true}).ToString(), "int32");
  EXPECT_EQ(InferProtoType(Field{"s32 signal", 50, 4, true}).ToString(),
            "int32");

  EXPECT_EQ(InferProtoType(Field{"unsigned int foo", 0, 4, false}).ToString(),
            "uint32");
  EXPECT_EQ(InferProtoType(Field{"u32 control_freq", 44, 4, false}).ToString(),
            "uint32");

  EXPECT_EQ(InferProtoType(Field{"ino_t foo", 0, 4, false}).ToString(),
            "uint64");
  EXPECT_EQ(InferProtoType(Field{"ino_t foo", 0, 8, false}).ToString(),
            "uint64");

  EXPECT_EQ(InferProtoType(Field{"dev_t foo", 0, 4, false}).ToString(),
            "uint64");
  EXPECT_EQ(InferProtoType(Field{"dev_t foo", 0, 8, false}).ToString(),
            "uint64");

  EXPECT_EQ(InferProtoType(Field{"char foo", 0, 0, false}).ToString(),
            "string");
}

TEST(FtraceEventParserTest, GenerateProtoName) {
  FtraceEvent input;
  Proto output;
  input.name = "the_snake_case_name";

  GenerateProto("group", input, &output);

  EXPECT_EQ(output.name, "TheSnakeCaseNameFtraceEvent");
}

}  // namespace
}  // namespace perfetto
