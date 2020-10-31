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

#include "perfetto/ext/base/string_splitter.h"

#include <vector>

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace base {
namespace {

using testing::ElementsAreArray;

TEST(StringSplitterTest, StdString) {
  {
    StringSplitter ss("", 'x');
    EXPECT_EQ(nullptr, ss.cur_token());
    EXPECT_EQ(0u, ss.cur_token_size());
    EXPECT_FALSE(ss.Next());
    EXPECT_EQ(nullptr, ss.cur_token());
    EXPECT_EQ(0u, ss.cur_token_size());
  }
  {
    StringSplitter ss(std::string(), 'x');
    EXPECT_EQ(nullptr, ss.cur_token());
    EXPECT_EQ(0u, ss.cur_token_size());
    EXPECT_FALSE(ss.Next());
    EXPECT_EQ(nullptr, ss.cur_token());
    EXPECT_EQ(0u, ss.cur_token_size());
  }
  {
    StringSplitter ss("a", 'x');
    EXPECT_EQ(nullptr, ss.cur_token());
    EXPECT_EQ(0u, ss.cur_token_size());
    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("a", ss.cur_token());
    EXPECT_EQ(1u, ss.cur_token_size());
    EXPECT_FALSE(ss.Next());
    EXPECT_EQ(0u, ss.cur_token_size());
  }
  {
    StringSplitter ss("abc", 'x');
    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("abc", ss.cur_token());
    EXPECT_EQ(3u, ss.cur_token_size());
    EXPECT_FALSE(ss.Next());
  }
  {
    StringSplitter ss("ab,", ',');
    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("ab", ss.cur_token());
    EXPECT_EQ(2u, ss.cur_token_size());
    EXPECT_FALSE(ss.Next());
  }
  {
    StringSplitter ss(",ab,", ',');
    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("ab", ss.cur_token());
    EXPECT_EQ(2u, ss.cur_token_size());
    EXPECT_FALSE(ss.Next());
  }
  {
    StringSplitter ss("a,b,c", ',');
    EXPECT_TRUE(ss.Next());
    EXPECT_EQ(1u, ss.cur_token_size());
    EXPECT_STREQ("a", ss.cur_token());

    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("b", ss.cur_token());
    EXPECT_EQ(1u, ss.cur_token_size());

    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("c", ss.cur_token());
    EXPECT_EQ(1u, ss.cur_token_size());

    EXPECT_FALSE(ss.Next());
    EXPECT_EQ(nullptr, ss.cur_token());
    EXPECT_EQ(0u, ss.cur_token_size());
  }
  {
    StringSplitter ss("a,b,c,", ',');
    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("a", ss.cur_token());

    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("b", ss.cur_token());

    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("c", ss.cur_token());

    EXPECT_FALSE(ss.Next());
    EXPECT_EQ(nullptr, ss.cur_token());
  }
  {
    StringSplitter ss(",,a,,b,,,,c,,,", ',');
    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("a", ss.cur_token());
    EXPECT_EQ(1u, ss.cur_token_size());

    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("b", ss.cur_token());
    EXPECT_EQ(1u, ss.cur_token_size());

    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("c", ss.cur_token());
    EXPECT_EQ(1u, ss.cur_token_size());

    for (int i = 0; i < 3; i++) {
      EXPECT_FALSE(ss.Next());
      EXPECT_EQ(0u, ss.cur_token_size());
    }
  }
  {
    StringSplitter ss(",,", ',');
    for (int i = 0; i < 3; i++) {
      EXPECT_FALSE(ss.Next());
      EXPECT_EQ(nullptr, ss.cur_token());
      EXPECT_EQ(0u, ss.cur_token_size());
    }
  }
  {
    StringSplitter ss(",,foo", ',');
    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("foo", ss.cur_token());
    EXPECT_EQ(3u, ss.cur_token_size());
    EXPECT_FALSE(ss.Next());
  }
}

TEST(StringSplitterTest, CString) {
  {
    char buf[] = "\0x\0";
    StringSplitter ss(buf, sizeof(buf), ',');
    EXPECT_FALSE(ss.Next());
    EXPECT_EQ(nullptr, ss.cur_token());
  }
  {
    char buf[] = "foo\nbar\n\nbaz\n";
    StringSplitter ss(buf, sizeof(buf), '\n');
    EXPECT_TRUE(ss.Next());
    EXPECT_EQ(3u, ss.cur_token_size());
    EXPECT_STREQ("foo", ss.cur_token());

    EXPECT_TRUE(ss.Next());
    EXPECT_EQ(3u, ss.cur_token_size());
    EXPECT_STREQ("bar", ss.cur_token());

    EXPECT_TRUE(ss.Next());
    EXPECT_EQ(3u, ss.cur_token_size());
    EXPECT_STREQ("baz", ss.cur_token());

    EXPECT_FALSE(ss.Next());
    EXPECT_EQ(0u, ss.cur_token_size());
  }
  {
    char buf[] = "";
    StringSplitter ss(buf, 0, ',');
    EXPECT_FALSE(ss.Next());
    EXPECT_EQ(nullptr, ss.cur_token());
    EXPECT_EQ(0u, ss.cur_token_size());
  }
  {
    char buf[] = "\0";
    StringSplitter ss(buf, 1, ',');
    EXPECT_FALSE(ss.Next());
    EXPECT_EQ(nullptr, ss.cur_token());
    EXPECT_EQ(0u, ss.cur_token_size());
  }
  {
    char buf[] = ",,foo,bar\0,baz";
    StringSplitter ss(buf, sizeof(buf), ',');

    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("foo", ss.cur_token());
    EXPECT_EQ(3u, ss.cur_token_size());

    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("bar", ss.cur_token());
    EXPECT_EQ(3u, ss.cur_token_size());

    for (int i = 0; i < 3; i++) {
      EXPECT_FALSE(ss.Next());
      EXPECT_EQ(0u, ss.cur_token_size());
    }
  }
  {
    char buf[] = ",,a\0,b,";
    StringSplitter ss(buf, sizeof(buf), ',');
    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("a", ss.cur_token());
    EXPECT_EQ(1u, ss.cur_token_size());
    for (int i = 0; i < 3; i++) {
      EXPECT_FALSE(ss.Next());
      EXPECT_EQ(nullptr, ss.cur_token());
      EXPECT_EQ(0u, ss.cur_token_size());
    }
  }
  {
    char buf[] = ",a,\0b";
    StringSplitter ss(buf, sizeof(buf), ',');
    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("a", ss.cur_token());
    EXPECT_EQ(1u, ss.cur_token_size());
    for (int i = 0; i < 3; i++) {
      EXPECT_FALSE(ss.Next());
      EXPECT_EQ(0u, ss.cur_token_size());
      EXPECT_EQ(nullptr, ss.cur_token());
    }
  }
  {
    char buf[] = ",a\0\0,x\0\0b";
    StringSplitter ss(buf, sizeof(buf), ',');
    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("a", ss.cur_token());
    EXPECT_EQ(1u, ss.cur_token_size());
    for (int i = 0; i < 3; i++) {
      EXPECT_FALSE(ss.Next());
      EXPECT_EQ(0u, ss.cur_token_size());
      EXPECT_EQ(nullptr, ss.cur_token());
    }
  }
}

TEST(StringSplitterTest, SplitOnNUL) {
  {
    StringSplitter ss(std::string(""), '\0');
    EXPECT_FALSE(ss.Next());
    EXPECT_EQ(nullptr, ss.cur_token());
  }
  {
    std::string str;
    str.resize(48);
    memcpy(&str[0], "foo\0", 4);
    memcpy(&str[4], "bar\0", 4);
    memcpy(&str[20], "baz", 3);
    StringSplitter ss(std::move(str), '\0');
    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("foo", ss.cur_token());
    EXPECT_EQ(3u, ss.cur_token_size());

    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("bar", ss.cur_token());
    EXPECT_EQ(3u, ss.cur_token_size());

    EXPECT_TRUE(ss.Next());
    EXPECT_STREQ("baz", ss.cur_token());
    EXPECT_EQ(3u, ss.cur_token_size());

    for (int i = 0; i < 3; i++) {
      EXPECT_FALSE(ss.Next());
      EXPECT_EQ(0u, ss.cur_token_size());
      EXPECT_EQ(nullptr, ss.cur_token());
    }
  }
  {
    char buf[] = "foo\0bar\0baz\0";
    StringSplitter ss(buf, sizeof(buf), '\0');
    EXPECT_TRUE(ss.Next());
    EXPECT_EQ(3u, ss.cur_token_size());
    EXPECT_STREQ("foo", ss.cur_token());

    EXPECT_TRUE(ss.Next());
    EXPECT_EQ(3u, ss.cur_token_size());
    EXPECT_STREQ("bar", ss.cur_token());

    EXPECT_TRUE(ss.Next());
    EXPECT_EQ(3u, ss.cur_token_size());
    EXPECT_STREQ("baz", ss.cur_token());

    for (int i = 0; i < 3; i++) {
      EXPECT_FALSE(ss.Next());
      EXPECT_EQ(0u, ss.cur_token_size());
      EXPECT_EQ(nullptr, ss.cur_token());
    }
  }
  {
    char buf[] = "\0\0foo\0\0\0\0bar\0baz\0\0";
    StringSplitter ss(buf, sizeof(buf), '\0');
    EXPECT_TRUE(ss.Next());
    EXPECT_EQ(3u, ss.cur_token_size());
    EXPECT_STREQ("foo", ss.cur_token());

    EXPECT_TRUE(ss.Next());
    EXPECT_EQ(3u, ss.cur_token_size());
    EXPECT_STREQ("bar", ss.cur_token());

    EXPECT_TRUE(ss.Next());
    EXPECT_EQ(3u, ss.cur_token_size());
    EXPECT_STREQ("baz", ss.cur_token());

    for (int i = 0; i < 3; i++) {
      EXPECT_FALSE(ss.Next());
      EXPECT_EQ(0u, ss.cur_token_size());
      EXPECT_EQ(nullptr, ss.cur_token());
    }
  }
  {
    char buf[] = "";
    StringSplitter ss(buf, 0, '\0');
    for (int i = 0; i < 3; i++) {
      EXPECT_FALSE(ss.Next());
      EXPECT_EQ(0u, ss.cur_token_size());
      EXPECT_EQ(nullptr, ss.cur_token());
    }
  }
  {
    char buf[] = "\0";
    StringSplitter ss(buf, 1, '\0');
    for (int i = 0; i < 3; i++) {
      EXPECT_FALSE(ss.Next());
      EXPECT_EQ(0u, ss.cur_token_size());
      EXPECT_EQ(nullptr, ss.cur_token());
    }
  }
  {
    char buf[] = "\0\0";
    StringSplitter ss(buf, 2, '\0');
    for (int i = 0; i < 3; i++) {
      EXPECT_FALSE(ss.Next());
      EXPECT_EQ(0u, ss.cur_token_size());
      EXPECT_EQ(nullptr, ss.cur_token());
    }
  }
}

TEST(StringSplitterTest, NestedUsage) {
  char text[] = R"(
l1w1 l1w2 l1w3

,l,2,w,1   l,2,,w,,2,,
)";
  std::vector<std::string> all_lines;
  std::vector<std::string> all_words;
  std::vector<std::string> all_tokens;
  for (StringSplitter lines(text, sizeof(text), '\n'); lines.Next();) {
    all_lines.push_back(lines.cur_token());
    for (StringSplitter words(&lines, ' '); words.Next();) {
      all_words.push_back(words.cur_token());
      for (StringSplitter tokens(&words, ','); tokens.Next();) {
        all_tokens.push_back(tokens.cur_token());
      }
    }
  }
  EXPECT_THAT(all_lines,
              ElementsAreArray({"l1w1 l1w2 l1w3", ",l,2,w,1   l,2,,w,,2,,"}));
  EXPECT_THAT(all_words, ElementsAreArray({"l1w1", "l1w2", "l1w3", ",l,2,w,1",
                                           "l,2,,w,,2,,"}));
  EXPECT_THAT(all_tokens, ElementsAreArray({"l1w1", "l1w2", "l1w3", "l", "2",
                                            "w", "1", "l", "2", "w", "2"}));
}  // namespace

}  // namespace
}  // namespace base
}  // namespace perfetto
