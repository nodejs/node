/*
 * Copyright (C) 2017 The Android Open Source Project
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
#include "src/traced/probes/ftrace/format_parser.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

using testing::ElementsAre;
using testing::Eq;

TEST(FtraceEventParserTest, HappyPath) {
  const std::string input = R"(name: the_name
ID: 42
format:
	field:unsigned short common_type;	offset:0;	size:2;	signed:0;
	field:unsigned char common_flags;	offset:2;	size:1;	signed:0;
	field:unsigned char common_preempt_count;	offset:3;	size:1;	signed:0;
	field:int common_pid;	offset:4;	size:4;	signed:1;

	field:char client_name[64];	offset:8;	size:64;	signed:0;
	field:const char * heap_name;	offset:72;	size:4;	signed:1;

print fmt: "client_name=%s heap_name=%s len=%zu mask=0x%x flags=0x%x", REC->client_name, REC->heap_name, REC->len, REC->mask, REC->flags
)";

  FtraceEvent output;
  EXPECT_TRUE(ParseFtraceEvent(input));
  EXPECT_TRUE(ParseFtraceEvent(input, &output));
  EXPECT_EQ(output.name, "the_name");
  EXPECT_EQ(output.id, 42u);
  EXPECT_THAT(
      output.fields,
      ElementsAre(
          Eq(FtraceEvent::Field{"char client_name[64]", 8, 64, false}),
          Eq(FtraceEvent::Field{"const char * heap_name", 72, 4, true})));
  EXPECT_THAT(
      output.common_fields,
      ElementsAre(
          Eq(FtraceEvent::Field{"unsigned short common_type", 0, 2, false}),
          Eq(FtraceEvent::Field{"unsigned char common_flags", 2, 1, false}),
          Eq(FtraceEvent::Field{"unsigned char common_preempt_count", 3, 1,
                                false}),
          Eq(FtraceEvent::Field{"int common_pid", 4, 4, true})));
}

TEST(FtraceEventParserTest, MissingName) {
  const std::string input = R"(ID: 42
format:
	field:unsigned short common_type;	offset:0;	size:2;	signed:0;
	field:unsigned char common_flags;	offset:2;	size:1;	signed:0;
	field:unsigned char common_preempt_count;	offset:3;	size:1;	signed:0;
	field:int common_pid;	offset:4;	size:4;	signed:1;

print fmt: "client_name=%s heap_name=%s len=%zu mask=0x%x flags=0x%x", REC->client_name, REC->heap_name, REC->len, REC->mask, REC->flags
)";

  EXPECT_FALSE(ParseFtraceEvent(input));
}

TEST(FtraceEventParserTest, MissingID) {
  const std::string input = R"(name: the_name
format:
	field:unsigned short common_type;	offset:0;	size:2;	signed:0;
	field:unsigned char common_flags;	offset:2;	size:1;	signed:0;
	field:unsigned char common_preempt_count;	offset:3;	size:1;	signed:0;
	field:int common_pid;	offset:4;	size:4;	signed:1;

print fmt: "client_name=%s heap_name=%s len=%zu mask=0x%x flags=0x%x", REC->client_name, REC->heap_name, REC->len, REC->mask, REC->flags
)";

  EXPECT_FALSE(ParseFtraceEvent(input));
}

TEST(FtraceEventParserTest, NoFields) {
  const std::string input = R"(name: the_name
ID: 10
print fmt: "client_name=%s heap_name=%s len=%zu mask=0x%x flags=0x%x", REC->client_name, REC->heap_name, REC->len, REC->mask, REC->flags
)";

  EXPECT_FALSE(ParseFtraceEvent(input));
}

TEST(FtraceEventParserTest, BasicFuzzing) {
  const std::string input = R"(name: the_name
ID: 42
format:
	field:unsigned short common_type;	offset:0;	size:2;	signed:0;
	field:unsigned char common_flags;	offset:2;	size:1;	signed:0;
	field:unsigned char common_preempt_count;	offset:3;	size:1;	signed:0;
	field:int common_pid;	offset:4;	size:4;	signed:1;

	field:char client_name[64];	offset:8;	size:64;	signed:0;
	field:const char * heap_name;	offset:72;	size:4;	signed:0;
	field:size_t len;	offset:76;	size:4;	signed:0;
	field:unsigned int mask;	offset:80;	size:4;	signed:0;
	field:unsigned int flags;	offset:84;	size:4;	signed:0;

print fmt: "client_name=%s heap_name=%s len=%zu mask=0x%x flags=0x%x", REC->client_name, REC->heap_name, REC->len, REC->mask, REC->flags
)";

  for (size_t i = 0; i < input.length(); i++) {
    for (size_t j = 1; j < 10 && i + j < input.length(); j++) {
      std::string copy = input;
      copy.erase(i, j);
      ParseFtraceEvent(copy);
    }
  }
}

TEST(FtraceEventParserTest, GetNameFromTypeAndName) {
  EXPECT_EQ(GetNameFromTypeAndName("int foo"), "foo");
  EXPECT_EQ(GetNameFromTypeAndName("int foo_bar"), "foo_bar");
  EXPECT_EQ(GetNameFromTypeAndName("const char * foo"), "foo");
  EXPECT_EQ(GetNameFromTypeAndName("const char foo[64]"), "foo");
  EXPECT_EQ(GetNameFromTypeAndName("char[] foo[16]"), "foo");
  EXPECT_EQ(GetNameFromTypeAndName("u8 foo[(int)sizeof(struct blah)]"), "foo");

  EXPECT_EQ(GetNameFromTypeAndName(""), "");
  EXPECT_EQ(GetNameFromTypeAndName("]"), "");
  EXPECT_EQ(GetNameFromTypeAndName("["), "");
  EXPECT_EQ(GetNameFromTypeAndName(" "), "");
  EXPECT_EQ(GetNameFromTypeAndName(" []"), "");
  EXPECT_EQ(GetNameFromTypeAndName(" ]["), "");
  EXPECT_EQ(GetNameFromTypeAndName("char"), "");
  EXPECT_EQ(GetNameFromTypeAndName("char *"), "");
  EXPECT_EQ(GetNameFromTypeAndName("char 42"), "");
}

}  // namespace
}  // namespace perfetto
