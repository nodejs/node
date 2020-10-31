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

#include "perfetto/ext/base/string_view.h"

#include <forward_list>
#include <unordered_map>
#include <unordered_set>

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace base {
namespace {

TEST(StringViewTest, BasicCases) {
  EXPECT_EQ(StringView(), StringView(""));
  EXPECT_EQ(StringView(""), StringView(""));
  EXPECT_EQ(StringView(""), StringView("", 0));
  EXPECT_EQ(StringView("ab"), StringView("ab", 2));
  EXPECT_EQ(StringView("ax", 1), StringView("ay", 1));
  EXPECT_EQ(StringView("ax", 1), StringView("a"));
  EXPECT_EQ(StringView("ax", 1), "a");
  EXPECT_EQ(StringView("foo|", 3).ToStdString(), std::string("foo"));
  EXPECT_TRUE(StringView("x") != StringView(""));
  EXPECT_TRUE(StringView("") != StringView("y"));
  EXPECT_TRUE(StringView("a") != StringView("b"));
  EXPECT_EQ(StringView().size(), 0ul);
  EXPECT_EQ(StringView().data(), nullptr);
  EXPECT_EQ(StringView("").size(), 0ul);
  EXPECT_NE(StringView("").data(), nullptr);
  EXPECT_TRUE(StringView("").empty());
  EXPECT_FALSE(StringView("x").empty());

  {
    StringView x("abc");
    EXPECT_EQ(x.size(), 3u);
    EXPECT_EQ(x.data()[0], 'a');
    EXPECT_EQ(x.data()[2], 'c');
    EXPECT_TRUE(x == "abc");
    EXPECT_TRUE(x == StringView("abc"));
    EXPECT_TRUE(x != StringView("abcd"));
    EXPECT_FALSE(x == StringView("aBc"));
    EXPECT_TRUE(x.CaseInsensitiveEq("aBc"));
    EXPECT_TRUE(x.CaseInsensitiveEq("AbC"));
    EXPECT_FALSE(x.CaseInsensitiveEq("AbCd"));
    EXPECT_FALSE(x.CaseInsensitiveEq("ab"));
    EXPECT_FALSE(x.CaseInsensitiveEq("abcd"));
    EXPECT_FALSE(x.CaseInsensitiveEq(""));
  }

  // Test find(char).
  EXPECT_EQ(StringView().find('x'), StringView::npos);
  EXPECT_EQ(StringView("").find('x'), StringView::npos);
  EXPECT_EQ(StringView("foo").find('x'), StringView::npos);
  EXPECT_EQ(StringView("foo").find('f'), 0u);
  EXPECT_EQ(StringView("foo").find('o'), 1u);

  // Test rfind(char).
  EXPECT_EQ(StringView().rfind('x'), StringView::npos);
  EXPECT_EQ(StringView("").rfind('x'), StringView::npos);
  EXPECT_EQ(StringView("foo").rfind('x'), StringView::npos);
  EXPECT_EQ(StringView("foo").rfind('f'), 0u);
  EXPECT_EQ(StringView("foo").rfind('o'), 2u);

  // Test find(const char*).
  EXPECT_EQ(StringView().find("x"), StringView::npos);
  EXPECT_EQ(StringView("").find("x"), StringView::npos);
  EXPECT_EQ(StringView("foo").find("x"), StringView::npos);
  EXPECT_EQ(StringView("foo").find("foobar"), StringView::npos);
  EXPECT_EQ(StringView("foo").find("f", 1), StringView::npos);
  EXPECT_EQ(StringView("foo").find("f"), 0u);
  EXPECT_EQ(StringView("foo").find("fo"), 0u);
  EXPECT_EQ(StringView("foo").find("oo"), 1u);
  EXPECT_EQ(StringView("foo").find("o"), 1u);
  EXPECT_EQ(StringView("foo").find("o", 2), 2u);
  EXPECT_EQ(StringView("foo").find("o", 3), StringView::npos);
  EXPECT_EQ(StringView("foo").find("o", 10), StringView::npos);
  EXPECT_EQ(StringView("foobar").find("bar", 3), 3u);
  EXPECT_EQ(StringView("foobar").find("bartender"), StringView::npos);
  EXPECT_EQ(StringView("foobar").find("bartender", 3), StringView::npos);
  EXPECT_EQ(StringView("foo").find(""), 0u);  // std::string behaves the same.

  // Test substr().
  EXPECT_EQ(StringView().substr(0, 0).ToStdString(), "");
  EXPECT_EQ(StringView().substr(3, 1).ToStdString(), "");
  EXPECT_EQ(StringView("foo").substr(3, 1).ToStdString(), "");
  EXPECT_EQ(StringView("foo").substr(4, 0).ToStdString(), "");
  EXPECT_EQ(StringView("foo").substr(4, 1).ToStdString(), "");
  EXPECT_EQ(StringView("foo").substr(0, 1).ToStdString(), "f");
  EXPECT_EQ(StringView("foo").substr(0, 3).ToStdString(), "foo");
  EXPECT_EQ(StringView("foo").substr(0, 99).ToStdString(), "foo");
  EXPECT_EQ(StringView("foo").substr(1, 2).ToStdString(), "oo");
  EXPECT_EQ(StringView("foo").substr(1, 3).ToStdString(), "oo");
  EXPECT_EQ(StringView("foo").substr(1, 99).ToStdString(), "oo");
  EXPECT_EQ(StringView("xyz").substr(0).ToStdString(), "xyz");
  EXPECT_EQ(StringView("xyz").substr(2).ToStdString(), "z");
  EXPECT_EQ(StringView("xyz").substr(3).ToStdString(), "");

  // Test the < operator.
  EXPECT_FALSE(StringView() < StringView());
  EXPECT_FALSE(StringView() < StringView(""));
  EXPECT_TRUE(StringView() < StringView("foo"));
  EXPECT_TRUE(StringView("") < StringView("foo"));
  EXPECT_FALSE(StringView() < StringView("foo", 0));
  EXPECT_FALSE(StringView("foo") < StringView("foo"));
  EXPECT_TRUE(StringView("foo") < StringView("fooo"));
  EXPECT_FALSE(StringView("fooo") < StringView("foo"));
  EXPECT_TRUE(StringView("bar") < StringView("foo"));

  // Test the <= operator.
  EXPECT_TRUE(StringView() <= StringView());
  EXPECT_TRUE(StringView() <= StringView(""));
  EXPECT_TRUE(StringView() <= StringView("foo"));
  EXPECT_TRUE(StringView("") <= StringView("foo"));
  EXPECT_TRUE(StringView() <= StringView("foo", 0));
  EXPECT_TRUE(StringView("foo") <= StringView("foo"));
  EXPECT_TRUE(StringView("foo") <= StringView("fooo"));
  EXPECT_FALSE(StringView("fooo") <= StringView("foo"));
  EXPECT_TRUE(StringView("bar") <= StringView("foo"));

  // Test the > operator.
  EXPECT_FALSE(StringView() > StringView());
  EXPECT_FALSE(StringView() > StringView(""));
  EXPECT_FALSE(StringView() > StringView("foo"));
  EXPECT_FALSE(StringView("") > StringView("foo"));
  EXPECT_FALSE(StringView() > StringView("foo", 0));
  EXPECT_FALSE(StringView("foo") > StringView("foo"));
  EXPECT_FALSE(StringView("foo") > StringView("fooo"));
  EXPECT_TRUE(StringView("fooo") > StringView("foo"));
  EXPECT_FALSE(StringView("bar") > StringView("foo"));

  // Test the >= operator.
  EXPECT_TRUE(StringView() >= StringView());
  EXPECT_TRUE(StringView() >= StringView(""));
  EXPECT_FALSE(StringView() >= StringView("foo"));
  EXPECT_FALSE(StringView("") >= StringView("foo"));
  EXPECT_TRUE(StringView() >= StringView("foo", 0));
  EXPECT_TRUE(StringView("foo") >= StringView("foo"));
  EXPECT_FALSE(StringView("foo") >= StringView("fooo"));
  EXPECT_TRUE(StringView("fooo") >= StringView("foo"));
  EXPECT_FALSE(StringView("bar") >= StringView("foo"));
}

TEST(StringViewTest, HashCollisions) {
  std::unordered_map<uint64_t, StringView> hashes;
  std::unordered_set<StringView> sv_set;
  auto insert_sv = [&hashes, &sv_set](StringView sv) {
    hashes.emplace(sv.Hash(), sv);
    size_t prev_set_size = sv_set.size();
    sv_set.insert(sv);
    ASSERT_EQ(sv_set.size(), prev_set_size + 1);
  };

  insert_sv("");
  EXPECT_EQ(hashes.size(), 1u);
  size_t last_size = 1;
  std::forward_list<std::string> strings;
  for (uint8_t c = 0; c < 0x80; c++) {
    char buf[500];
    memset(buf, static_cast<char>(c), sizeof(buf));
    for (size_t i = 1; i <= sizeof(buf); i++) {
      strings.emplace_front(buf, i);
      StringView sv(strings.front());
      auto other = hashes.find(sv.Hash());
      if (other == hashes.end()) {
        insert_sv(sv);
        ++last_size;
        ASSERT_EQ(hashes.size(), last_size);
        continue;
      }
      EXPECT_TRUE(false) << "H(" << sv.ToStdString() << ") = "
                         << "H(" << other->second.ToStdString() << ")";
    }
  }
}

}  // namespace
}  // namespace base
}  // namespace perfetto
