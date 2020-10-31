// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
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
#include <google/protobuf/stubs/stringpiece.h>

#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/testing/googletest.h>
#include <google/protobuf/stubs/hash.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace {
TEST(StringPiece, Ctor) {
  {
    // Null.
    StringPiece s10;
    EXPECT_TRUE(s10.data() == nullptr);
    EXPECT_EQ(0, s10.length());
  }

  {
    // const char* without length.
    const char* hello = "hello";
    StringPiece s20(hello);
    EXPECT_TRUE(s20.data() == hello);
    EXPECT_EQ(5, s20.length());

    // const char* with length.
    StringPiece s21(hello, 4);
    EXPECT_TRUE(s21.data() == hello);
    EXPECT_EQ(4, s21.length());

    // Not recommended, but valid C++
    StringPiece s22(hello, 6);
    EXPECT_TRUE(s22.data() == hello);
    EXPECT_EQ(6, s22.length());
  }

  {
    // std::string.
    std::string hola = "hola";
    StringPiece s30(hola);
    EXPECT_TRUE(s30.data() == hola.data());
    EXPECT_EQ(4, s30.length());

    // std::string with embedded '\0'.
    hola.push_back('\0');
    hola.append("h2");
    hola.push_back('\0');
    StringPiece s31(hola);
    EXPECT_TRUE(s31.data() == hola.data());
    EXPECT_EQ(8, s31.length());
  }

#if defined(HAS_GLOBAL_STRING)
  {
    // ::string
    string bonjour = "bonjour";
    StringPiece s40(bonjour);
    EXPECT_TRUE(s40.data() == bonjour.data());
    EXPECT_EQ(7, s40.length());
  }
#endif

  // TODO(mec): StringPiece(StringPiece x, int pos);
  // TODO(mec): StringPiece(StringPiece x, int pos, int len);
  // TODO(mec): StringPiece(const StringPiece&);
}

TEST(StringPiece, STLComparator) {
  string s1("foo");
  string s2("bar");
  string s3("baz");

  StringPiece p1(s1);
  StringPiece p2(s2);
  StringPiece p3(s3);

  typedef std::map<StringPiece, int> TestMap;
  TestMap map;

  map.insert(std::make_pair(p1, 0));
  map.insert(std::make_pair(p2, 1));
  map.insert(std::make_pair(p3, 2));
  EXPECT_EQ(map.size(), 3);

  TestMap::const_iterator iter = map.begin();
  EXPECT_EQ(iter->second, 1);
  ++iter;
  EXPECT_EQ(iter->second, 2);
  ++iter;
  EXPECT_EQ(iter->second, 0);
  ++iter;
  EXPECT_TRUE(iter == map.end());

  TestMap::iterator new_iter = map.find("zot");
  EXPECT_TRUE(new_iter == map.end());

  new_iter = map.find("bar");
  EXPECT_TRUE(new_iter != map.end());

  map.erase(new_iter);
  EXPECT_EQ(map.size(), 2);

  iter = map.begin();
  EXPECT_EQ(iter->second, 2);
  ++iter;
  EXPECT_EQ(iter->second, 0);
  ++iter;
  EXPECT_TRUE(iter == map.end());
}

TEST(StringPiece, ComparisonOperators) {
#define COMPARE(result, op, x, y) \
  EXPECT_EQ(result, StringPiece((x)) op StringPiece((y))); \
  EXPECT_EQ(result, StringPiece((x)).compare(StringPiece((y))) op 0)

  COMPARE(true, ==, "",   "");
  COMPARE(true, ==, "", nullptr);
  COMPARE(true, ==, nullptr, "");
  COMPARE(true, ==, "a",  "a");
  COMPARE(true, ==, "aa", "aa");
  COMPARE(false, ==, "a",  "");
  COMPARE(false, ==, "",   "a");
  COMPARE(false, ==, "a",  "b");
  COMPARE(false, ==, "a",  "aa");
  COMPARE(false, ==, "aa", "a");

  COMPARE(false, !=, "",   "");
  COMPARE(false, !=, "a",  "a");
  COMPARE(false, !=, "aa", "aa");
  COMPARE(true, !=, "a",  "");
  COMPARE(true, !=, "",   "a");
  COMPARE(true, !=, "a",  "b");
  COMPARE(true, !=, "a",  "aa");
  COMPARE(true, !=, "aa", "a");

  COMPARE(true, <, "a",  "b");
  COMPARE(true, <, "a",  "aa");
  COMPARE(true, <, "aa", "b");
  COMPARE(true, <, "aa", "bb");
  COMPARE(false, <, "a",  "a");
  COMPARE(false, <, "b",  "a");
  COMPARE(false, <, "aa", "a");
  COMPARE(false, <, "b",  "aa");
  COMPARE(false, <, "bb", "aa");

  COMPARE(true, <=, "a",  "a");
  COMPARE(true, <=, "a",  "b");
  COMPARE(true, <=, "a",  "aa");
  COMPARE(true, <=, "aa", "b");
  COMPARE(true, <=, "aa", "bb");
  COMPARE(false, <=, "b",  "a");
  COMPARE(false, <=, "aa", "a");
  COMPARE(false, <=, "b",  "aa");
  COMPARE(false, <=, "bb", "aa");

  COMPARE(false, >=, "a",  "b");
  COMPARE(false, >=, "a",  "aa");
  COMPARE(false, >=, "aa", "b");
  COMPARE(false, >=, "aa", "bb");
  COMPARE(true, >=, "a",  "a");
  COMPARE(true, >=, "b",  "a");
  COMPARE(true, >=, "aa", "a");
  COMPARE(true, >=, "b",  "aa");
  COMPARE(true, >=, "bb", "aa");

  COMPARE(false, >, "a",  "a");
  COMPARE(false, >, "a",  "b");
  COMPARE(false, >, "a",  "aa");
  COMPARE(false, >, "aa", "b");
  COMPARE(false, >, "aa", "bb");
  COMPARE(true, >, "b",  "a");
  COMPARE(true, >, "aa", "a");
  COMPARE(true, >, "b",  "aa");
  COMPARE(true, >, "bb", "aa");

  string x;
  for (int i = 0; i < 256; i++) {
    x += 'a';
    string y = x;
    COMPARE(true, ==, x, y);
    for (int j = 0; j < i; j++) {
      string z = x;
      z[j] = 'b';       // Differs in position 'j'
      COMPARE(false, ==, x, z);
      COMPARE(true, <, x, z);
      COMPARE(true, >, z, x);
      if (j + 1 < i) {
        z[j + 1] = 'A';  // Differs in position 'j+1' as well
        COMPARE(false, ==, x, z);
        COMPARE(true, <, x, z);
        COMPARE(true, >, z, x);
        z[j + 1] = 'z';  // Differs in position 'j+1' as well
        COMPARE(false, ==, x, z);
        COMPARE(true, <, x, z);
        COMPARE(true, >, z, x);
      }
    }
  }

#undef COMPARE
}

TEST(StringPiece, STL1) {
  const StringPiece a("abcdefghijklmnopqrstuvwxyz");
  const StringPiece b("abc");
  const StringPiece c("xyz");
  const StringPiece d("foobar");
  const StringPiece e;
  string temp("123");
  temp += '\0';
  temp += "456";
  const StringPiece f(temp);

  EXPECT_EQ(a[6], 'g');
  EXPECT_EQ(b[0], 'a');
  EXPECT_EQ(c[2], 'z');
  EXPECT_EQ(f[3], '\0');
  EXPECT_EQ(f[5], '5');

  EXPECT_EQ(*d.data(), 'f');
  EXPECT_EQ(d.data()[5], 'r');
  EXPECT_TRUE(e.data() == nullptr);

  EXPECT_EQ(*a.begin(), 'a');
  EXPECT_EQ(*(b.begin() + 2), 'c');
  EXPECT_EQ(*(c.end() - 1), 'z');

  EXPECT_EQ(*a.rbegin(), 'z');
  EXPECT_EQ(*(b.rbegin() + 2), 'a');
  EXPECT_EQ(*(c.rend() - 1), 'x');
  EXPECT_TRUE(a.rbegin() + 26 == a.rend());

  EXPECT_EQ(a.size(), 26);
  EXPECT_EQ(b.size(), 3);
  EXPECT_EQ(c.size(), 3);
  EXPECT_EQ(d.size(), 6);
  EXPECT_EQ(e.size(), 0);
  EXPECT_EQ(f.size(), 7);

  EXPECT_TRUE(!d.empty());
  EXPECT_TRUE(d.begin() != d.end());
  EXPECT_TRUE(d.begin() + 6 == d.end());

  EXPECT_TRUE(e.empty());
  EXPECT_TRUE(e.begin() == e.end());

  EXPECT_GE(a.max_size(), a.capacity());
  EXPECT_GE(a.capacity(), a.size());

  char buf[4] = { '%', '%', '%', '%' };
  EXPECT_EQ(a.copy(buf, 4), 4);
  EXPECT_EQ(buf[0], a[0]);
  EXPECT_EQ(buf[1], a[1]);
  EXPECT_EQ(buf[2], a[2]);
  EXPECT_EQ(buf[3], a[3]);
  EXPECT_EQ(a.copy(buf, 3, 7), 3);
  EXPECT_EQ(buf[0], a[7]);
  EXPECT_EQ(buf[1], a[8]);
  EXPECT_EQ(buf[2], a[9]);
  EXPECT_EQ(buf[3], a[3]);
  EXPECT_EQ(c.copy(buf, 99), 3);
  EXPECT_EQ(buf[0], c[0]);
  EXPECT_EQ(buf[1], c[1]);
  EXPECT_EQ(buf[2], c[2]);
  EXPECT_EQ(buf[3], a[3]);
}

// Separated from STL1() because some compilers produce an overly
// large stack frame for the combined function.
TEST(StringPiece, STL2) {
  const StringPiece a("abcdefghijklmnopqrstuvwxyz");
  const StringPiece b("abc");
  const StringPiece c("xyz");
  StringPiece d("foobar");
  const StringPiece e;
  const StringPiece f("123" "\0" "456", 7);

  d.clear();
  EXPECT_EQ(d.size(), 0);
  EXPECT_TRUE(d.empty());
  EXPECT_TRUE(d.data() == nullptr);
  EXPECT_TRUE(d.begin() == d.end());

  EXPECT_EQ(StringPiece::npos, string::npos);

  EXPECT_EQ(a.find(b), 0);
  EXPECT_EQ(a.find(b, 1), StringPiece::npos);
  EXPECT_EQ(a.find(c), 23);
  EXPECT_EQ(a.find(c, 9), 23);
  EXPECT_EQ(a.find(c, StringPiece::npos), StringPiece::npos);
  EXPECT_EQ(b.find(c), StringPiece::npos);
  EXPECT_EQ(b.find(c, StringPiece::npos), StringPiece::npos);
  EXPECT_EQ(a.find(d), 0);
  EXPECT_EQ(a.find(e), 0);
  EXPECT_EQ(a.find(d, 12), 12);
  EXPECT_EQ(a.find(e, 17), 17);
  StringPiece g("xx not found bb");
  EXPECT_EQ(a.find(g), StringPiece::npos);
  // empty string nonsense
  EXPECT_EQ(d.find(b), StringPiece::npos);
  EXPECT_EQ(e.find(b), StringPiece::npos);
  EXPECT_EQ(d.find(b, 4), StringPiece::npos);
  EXPECT_EQ(e.find(b, 7), StringPiece::npos);

  size_t empty_search_pos = string().find(string());
  EXPECT_EQ(d.find(d), empty_search_pos);
  EXPECT_EQ(d.find(e), empty_search_pos);
  EXPECT_EQ(e.find(d), empty_search_pos);
  EXPECT_EQ(e.find(e), empty_search_pos);
  EXPECT_EQ(d.find(d, 4), string().find(string(), 4));
  EXPECT_EQ(d.find(e, 4), string().find(string(), 4));
  EXPECT_EQ(e.find(d, 4), string().find(string(), 4));
  EXPECT_EQ(e.find(e, 4), string().find(string(), 4));

  EXPECT_EQ(a.find('a'), 0);
  EXPECT_EQ(a.find('c'), 2);
  EXPECT_EQ(a.find('z'), 25);
  EXPECT_EQ(a.find('$'), StringPiece::npos);
  EXPECT_EQ(a.find('\0'), StringPiece::npos);
  EXPECT_EQ(f.find('\0'), 3);
  EXPECT_EQ(f.find('3'), 2);
  EXPECT_EQ(f.find('5'), 5);
  EXPECT_EQ(g.find('o'), 4);
  EXPECT_EQ(g.find('o', 4), 4);
  EXPECT_EQ(g.find('o', 5), 8);
  EXPECT_EQ(a.find('b', 5), StringPiece::npos);
  // empty string nonsense
  EXPECT_EQ(d.find('\0'), StringPiece::npos);
  EXPECT_EQ(e.find('\0'), StringPiece::npos);
  EXPECT_EQ(d.find('\0', 4), StringPiece::npos);
  EXPECT_EQ(e.find('\0', 7), StringPiece::npos);
  EXPECT_EQ(d.find('x'), StringPiece::npos);
  EXPECT_EQ(e.find('x'), StringPiece::npos);
  EXPECT_EQ(d.find('x', 4), StringPiece::npos);
  EXPECT_EQ(e.find('x', 7), StringPiece::npos);

  EXPECT_EQ(a.rfind(b), 0);
  EXPECT_EQ(a.rfind(b, 1), 0);
  EXPECT_EQ(a.rfind(c), 23);
  EXPECT_EQ(a.rfind(c, 22), StringPiece::npos);
  EXPECT_EQ(a.rfind(c, 1), StringPiece::npos);
  EXPECT_EQ(a.rfind(c, 0), StringPiece::npos);
  EXPECT_EQ(b.rfind(c), StringPiece::npos);
  EXPECT_EQ(b.rfind(c, 0), StringPiece::npos);
  EXPECT_EQ(a.rfind(d), a.as_string().rfind(string()));
  EXPECT_EQ(a.rfind(e), a.as_string().rfind(string()));
  EXPECT_EQ(a.rfind(d, 12), 12);
  EXPECT_EQ(a.rfind(e, 17), 17);
  EXPECT_EQ(a.rfind(g), StringPiece::npos);
  EXPECT_EQ(d.rfind(b), StringPiece::npos);
  EXPECT_EQ(e.rfind(b), StringPiece::npos);
  EXPECT_EQ(d.rfind(b, 4), StringPiece::npos);
  EXPECT_EQ(e.rfind(b, 7), StringPiece::npos);
  // empty string nonsense
  EXPECT_EQ(d.rfind(d, 4), string().rfind(string()));
  EXPECT_EQ(e.rfind(d, 7), string().rfind(string()));
  EXPECT_EQ(d.rfind(e, 4), string().rfind(string()));
  EXPECT_EQ(e.rfind(e, 7), string().rfind(string()));
  EXPECT_EQ(d.rfind(d), string().rfind(string()));
  EXPECT_EQ(e.rfind(d), string().rfind(string()));
  EXPECT_EQ(d.rfind(e), string().rfind(string()));
  EXPECT_EQ(e.rfind(e), string().rfind(string()));

  EXPECT_EQ(g.rfind('o'), 8);
  EXPECT_EQ(g.rfind('q'), StringPiece::npos);
  EXPECT_EQ(g.rfind('o', 8), 8);
  EXPECT_EQ(g.rfind('o', 7), 4);
  EXPECT_EQ(g.rfind('o', 3), StringPiece::npos);
  EXPECT_EQ(f.rfind('\0'), 3);
  EXPECT_EQ(f.rfind('\0', 12), 3);
  EXPECT_EQ(f.rfind('3'), 2);
  EXPECT_EQ(f.rfind('5'), 5);
  // empty string nonsense
  EXPECT_EQ(d.rfind('o'), StringPiece::npos);
  EXPECT_EQ(e.rfind('o'), StringPiece::npos);
  EXPECT_EQ(d.rfind('o', 4), StringPiece::npos);
  EXPECT_EQ(e.rfind('o', 7), StringPiece::npos);

  EXPECT_EQ(a.find_first_of(b), 0);
  EXPECT_EQ(a.find_first_of(b, 0), 0);
  EXPECT_EQ(a.find_first_of(b, 1), 1);
  EXPECT_EQ(a.find_first_of(b, 2), 2);
  EXPECT_EQ(a.find_first_of(b, 3), StringPiece::npos);
  EXPECT_EQ(a.find_first_of(c), 23);
  EXPECT_EQ(a.find_first_of(c, 23), 23);
  EXPECT_EQ(a.find_first_of(c, 24), 24);
  EXPECT_EQ(a.find_first_of(c, 25), 25);
  EXPECT_EQ(a.find_first_of(c, 26), StringPiece::npos);
  EXPECT_EQ(g.find_first_of(b), 13);
  EXPECT_EQ(g.find_first_of(c), 0);
  EXPECT_EQ(a.find_first_of(f), StringPiece::npos);
  EXPECT_EQ(f.find_first_of(a), StringPiece::npos);
  // empty string nonsense
  EXPECT_EQ(a.find_first_of(d), StringPiece::npos);
  EXPECT_EQ(a.find_first_of(e), StringPiece::npos);
  EXPECT_EQ(d.find_first_of(b), StringPiece::npos);
  EXPECT_EQ(e.find_first_of(b), StringPiece::npos);
  EXPECT_EQ(d.find_first_of(d), StringPiece::npos);
  EXPECT_EQ(e.find_first_of(d), StringPiece::npos);
  EXPECT_EQ(d.find_first_of(e), StringPiece::npos);
  EXPECT_EQ(e.find_first_of(e), StringPiece::npos);

  EXPECT_EQ(a.find_first_not_of(b), 3);
  EXPECT_EQ(a.find_first_not_of(c), 0);
  EXPECT_EQ(b.find_first_not_of(a), StringPiece::npos);
  EXPECT_EQ(c.find_first_not_of(a), StringPiece::npos);
  EXPECT_EQ(f.find_first_not_of(a), 0);
  EXPECT_EQ(a.find_first_not_of(f), 0);
  EXPECT_EQ(a.find_first_not_of(d), 0);
  EXPECT_EQ(a.find_first_not_of(e), 0);
  // empty string nonsense
  EXPECT_EQ(d.find_first_not_of(a), StringPiece::npos);
  EXPECT_EQ(e.find_first_not_of(a), StringPiece::npos);
  EXPECT_EQ(d.find_first_not_of(d), StringPiece::npos);
  EXPECT_EQ(e.find_first_not_of(d), StringPiece::npos);
  EXPECT_EQ(d.find_first_not_of(e), StringPiece::npos);
  EXPECT_EQ(e.find_first_not_of(e), StringPiece::npos);

  StringPiece h("====");
  EXPECT_EQ(h.find_first_not_of('='), StringPiece::npos);
  EXPECT_EQ(h.find_first_not_of('=', 3), StringPiece::npos);
  EXPECT_EQ(h.find_first_not_of('\0'), 0);
  EXPECT_EQ(g.find_first_not_of('x'), 2);
  EXPECT_EQ(f.find_first_not_of('\0'), 0);
  EXPECT_EQ(f.find_first_not_of('\0', 3), 4);
  EXPECT_EQ(f.find_first_not_of('\0', 2), 2);
  // empty string nonsense
  EXPECT_EQ(d.find_first_not_of('x'), StringPiece::npos);
  EXPECT_EQ(e.find_first_not_of('x'), StringPiece::npos);
  EXPECT_EQ(d.find_first_not_of('\0'), StringPiece::npos);
  EXPECT_EQ(e.find_first_not_of('\0'), StringPiece::npos);

  //  StringPiece g("xx not found bb");
  StringPiece i("56");
  EXPECT_EQ(h.find_last_of(a), StringPiece::npos);
  EXPECT_EQ(g.find_last_of(a), g.size()-1);
  EXPECT_EQ(a.find_last_of(b), 2);
  EXPECT_EQ(a.find_last_of(c), a.size()-1);
  EXPECT_EQ(f.find_last_of(i), 6);
  EXPECT_EQ(a.find_last_of('a'), 0);
  EXPECT_EQ(a.find_last_of('b'), 1);
  EXPECT_EQ(a.find_last_of('z'), 25);
  EXPECT_EQ(a.find_last_of('a', 5), 0);
  EXPECT_EQ(a.find_last_of('b', 5), 1);
  EXPECT_EQ(a.find_last_of('b', 0), StringPiece::npos);
  EXPECT_EQ(a.find_last_of('z', 25), 25);
  EXPECT_EQ(a.find_last_of('z', 24), StringPiece::npos);
  EXPECT_EQ(f.find_last_of(i, 5), 5);
  EXPECT_EQ(f.find_last_of(i, 6), 6);
  EXPECT_EQ(f.find_last_of(a, 4), StringPiece::npos);
  // empty string nonsense
  EXPECT_EQ(f.find_last_of(d), StringPiece::npos);
  EXPECT_EQ(f.find_last_of(e), StringPiece::npos);
  EXPECT_EQ(f.find_last_of(d, 4), StringPiece::npos);
  EXPECT_EQ(f.find_last_of(e, 4), StringPiece::npos);
  EXPECT_EQ(d.find_last_of(d), StringPiece::npos);
  EXPECT_EQ(d.find_last_of(e), StringPiece::npos);
  EXPECT_EQ(e.find_last_of(d), StringPiece::npos);
  EXPECT_EQ(e.find_last_of(e), StringPiece::npos);
  EXPECT_EQ(d.find_last_of(f), StringPiece::npos);
  EXPECT_EQ(e.find_last_of(f), StringPiece::npos);
  EXPECT_EQ(d.find_last_of(d, 4), StringPiece::npos);
  EXPECT_EQ(d.find_last_of(e, 4), StringPiece::npos);
  EXPECT_EQ(e.find_last_of(d, 4), StringPiece::npos);
  EXPECT_EQ(e.find_last_of(e, 4), StringPiece::npos);
  EXPECT_EQ(d.find_last_of(f, 4), StringPiece::npos);
  EXPECT_EQ(e.find_last_of(f, 4), StringPiece::npos);

  EXPECT_EQ(a.find_last_not_of(b), a.size()-1);
  EXPECT_EQ(a.find_last_not_of(c), 22);
  EXPECT_EQ(b.find_last_not_of(a), StringPiece::npos);
  EXPECT_EQ(b.find_last_not_of(b), StringPiece::npos);
  EXPECT_EQ(f.find_last_not_of(i), 4);
  EXPECT_EQ(a.find_last_not_of(c, 24), 22);
  EXPECT_EQ(a.find_last_not_of(b, 3), 3);
  EXPECT_EQ(a.find_last_not_of(b, 2), StringPiece::npos);
  // empty string nonsense
  EXPECT_EQ(f.find_last_not_of(d), f.size()-1);
  EXPECT_EQ(f.find_last_not_of(e), f.size()-1);
  EXPECT_EQ(f.find_last_not_of(d, 4), 4);
  EXPECT_EQ(f.find_last_not_of(e, 4), 4);
  EXPECT_EQ(d.find_last_not_of(d), StringPiece::npos);
  EXPECT_EQ(d.find_last_not_of(e), StringPiece::npos);
  EXPECT_EQ(e.find_last_not_of(d), StringPiece::npos);
  EXPECT_EQ(e.find_last_not_of(e), StringPiece::npos);
  EXPECT_EQ(d.find_last_not_of(f), StringPiece::npos);
  EXPECT_EQ(e.find_last_not_of(f), StringPiece::npos);
  EXPECT_EQ(d.find_last_not_of(d, 4), StringPiece::npos);
  EXPECT_EQ(d.find_last_not_of(e, 4), StringPiece::npos);
  EXPECT_EQ(e.find_last_not_of(d, 4), StringPiece::npos);
  EXPECT_EQ(e.find_last_not_of(e, 4), StringPiece::npos);
  EXPECT_EQ(d.find_last_not_of(f, 4), StringPiece::npos);
  EXPECT_EQ(e.find_last_not_of(f, 4), StringPiece::npos);

  EXPECT_EQ(h.find_last_not_of('x'), h.size() - 1);
  EXPECT_EQ(h.find_last_not_of('='), StringPiece::npos);
  EXPECT_EQ(b.find_last_not_of('c'), 1);
  EXPECT_EQ(h.find_last_not_of('x', 2), 2);
  EXPECT_EQ(h.find_last_not_of('=', 2), StringPiece::npos);
  EXPECT_EQ(b.find_last_not_of('b', 1), 0);
  // empty string nonsense
  EXPECT_EQ(d.find_last_not_of('x'), StringPiece::npos);
  EXPECT_EQ(e.find_last_not_of('x'), StringPiece::npos);
  EXPECT_EQ(d.find_last_not_of('\0'), StringPiece::npos);
  EXPECT_EQ(e.find_last_not_of('\0'), StringPiece::npos);

  EXPECT_EQ(a.substr(0, 3), b);
  EXPECT_EQ(a.substr(23), c);
  EXPECT_EQ(a.substr(23, 3), c);
  EXPECT_EQ(a.substr(23, 99), c);
  EXPECT_EQ(a.substr(0), a);
  EXPECT_EQ(a.substr(3, 2), "de");
  // empty string nonsense
  EXPECT_EQ(a.substr(99, 2), e);
  EXPECT_EQ(d.substr(99), e);
  EXPECT_EQ(d.substr(0, 99), e);
  EXPECT_EQ(d.substr(99, 99), e);
  // use of npos
  EXPECT_EQ(a.substr(0, StringPiece::npos), a);
  EXPECT_EQ(a.substr(23, StringPiece::npos), c);
  EXPECT_EQ(a.substr(StringPiece::npos, 0), e);
  EXPECT_EQ(a.substr(StringPiece::npos, 1), e);
  EXPECT_EQ(a.substr(StringPiece::npos, StringPiece::npos), e);

  // Substring constructors.
  EXPECT_EQ(StringPiece(a, 0, 3), b);
  EXPECT_EQ(StringPiece(a, 23), c);
  EXPECT_EQ(StringPiece(a, 23, 3), c);
  EXPECT_EQ(StringPiece(a, 23, 99), c);
  EXPECT_EQ(StringPiece(a, 0), a);
  EXPECT_EQ(StringPiece(a, 3, 2), "de");
  // empty string nonsense
  EXPECT_EQ(StringPiece(d, 0, 99), e);
  // Verify that they work taking an actual string, not just a StringPiece.
  string a2 = a.as_string();
  EXPECT_EQ(StringPiece(a2, 0, 3), b);
  EXPECT_EQ(StringPiece(a2, 23), c);
  EXPECT_EQ(StringPiece(a2, 23, 3), c);
  EXPECT_EQ(StringPiece(a2, 23, 99), c);
  EXPECT_EQ(StringPiece(a2, 0), a);
  EXPECT_EQ(StringPiece(a2, 3, 2), "de");
}

TEST(StringPiece, Custom) {
  StringPiece a("foobar");
  string s1("123");
  s1 += '\0';
  s1 += "456";
  StringPiece b(s1);
  StringPiece e;
  string s2;

  // CopyToString
  a.CopyToString(&s2);
  EXPECT_EQ(s2.size(), 6);
  EXPECT_EQ(s2, "foobar");
  b.CopyToString(&s2);
  EXPECT_EQ(s2.size(), 7);
  EXPECT_EQ(s1, s2);
  e.CopyToString(&s2);
  EXPECT_TRUE(s2.empty());

  // AppendToString
  s2.erase();
  a.AppendToString(&s2);
  EXPECT_EQ(s2.size(), 6);
  EXPECT_EQ(s2, "foobar");
  a.AppendToString(&s2);
  EXPECT_EQ(s2.size(), 12);
  EXPECT_EQ(s2, "foobarfoobar");

  // starts_with
  EXPECT_TRUE(a.starts_with(a));
  EXPECT_TRUE(a.starts_with("foo"));
  EXPECT_TRUE(a.starts_with(e));
  EXPECT_TRUE(b.starts_with(s1));
  EXPECT_TRUE(b.starts_with(b));
  EXPECT_TRUE(b.starts_with(e));
  EXPECT_TRUE(e.starts_with(""));
  EXPECT_TRUE(!a.starts_with(b));
  EXPECT_TRUE(!b.starts_with(a));
  EXPECT_TRUE(!e.starts_with(a));

  // ends with
  EXPECT_TRUE(a.ends_with(a));
  EXPECT_TRUE(a.ends_with("bar"));
  EXPECT_TRUE(a.ends_with(e));
  EXPECT_TRUE(b.ends_with(s1));
  EXPECT_TRUE(b.ends_with(b));
  EXPECT_TRUE(b.ends_with(e));
  EXPECT_TRUE(e.ends_with(""));
  EXPECT_TRUE(!a.ends_with(b));
  EXPECT_TRUE(!b.ends_with(a));
  EXPECT_TRUE(!e.ends_with(a));

  // remove_prefix
  StringPiece c(a);
  c.remove_prefix(3);
  EXPECT_EQ(c, "bar");
  c = a;
  c.remove_prefix(0);
  EXPECT_EQ(c, a);
  c.remove_prefix(c.size());
  EXPECT_EQ(c, e);

  // remove_suffix
  c = a;
  c.remove_suffix(3);
  EXPECT_EQ(c, "foo");
  c = a;
  c.remove_suffix(0);
  EXPECT_EQ(c, a);
  c.remove_suffix(c.size());
  EXPECT_EQ(c, e);

  // set
  c.set("foobar", 6);
  EXPECT_EQ(c, a);
  c.set("foobar", 0);
  EXPECT_EQ(c, e);
  c.set("foobar", 7);
  EXPECT_NE(c, a);

  c.set("foobar");
  EXPECT_EQ(c, a);

  c.set(static_cast<const void*>("foobar"), 6);
  EXPECT_EQ(c, a);
  c.set(static_cast<const void*>("foobar"), 0);
  EXPECT_EQ(c, e);
  c.set(static_cast<const void*>("foobar"), 7);
  EXPECT_NE(c, a);

  // as_string
  string s3(a.as_string().c_str(), 7);
  EXPECT_EQ(c, s3);
  string s4(e.as_string());
  EXPECT_TRUE(s4.empty());

  // ToString
  {
    string s5(a.ToString().c_str(), 7);
    EXPECT_EQ(c, s5);
    string s6(e.ToString());
    EXPECT_TRUE(s6.empty());
  }

  // Consume
  a.set("foobar");
  EXPECT_TRUE(a.Consume("foo"));
  EXPECT_EQ(a, "bar");
  EXPECT_FALSE(a.Consume("foo"));
  EXPECT_FALSE(a.Consume("barbar"));
  EXPECT_FALSE(a.Consume("ar"));
  EXPECT_EQ(a, "bar");

  a.set("foobar");
  EXPECT_TRUE(a.ConsumeFromEnd("bar"));
  EXPECT_EQ(a, "foo");
  EXPECT_FALSE(a.ConsumeFromEnd("bar"));
  EXPECT_FALSE(a.ConsumeFromEnd("foofoo"));
  EXPECT_FALSE(a.ConsumeFromEnd("fo"));
  EXPECT_EQ(a, "foo");
}

TEST(StringPiece, Contains) {
  StringPiece a("abcdefg");
  StringPiece b("abcd");
  StringPiece c("efg");
  StringPiece d("gh");
  EXPECT_TRUE(a.contains(b));
  EXPECT_TRUE(a.contains(c));
  EXPECT_TRUE(!a.contains(d));
}

TEST(StringPiece, NullInput) {
  // we used to crash here, but now we don't.
  StringPiece s(nullptr);
  EXPECT_EQ(s.data(), (const char*)nullptr);
  EXPECT_EQ(s.size(), 0);

  s.set(nullptr);
  EXPECT_EQ(s.data(), (const char*)nullptr);
  EXPECT_EQ(s.size(), 0);

  // .ToString() on a StringPiece with nullptr should produce the empty string.
  EXPECT_EQ("", s.ToString());
  EXPECT_EQ("", s.as_string());
}

TEST(StringPiece, Comparisons2) {
  StringPiece abc("abcdefghijklmnopqrstuvwxyz");

  // check comparison operations on strings longer than 4 bytes.
  EXPECT_EQ(abc, StringPiece("abcdefghijklmnopqrstuvwxyz"));
  EXPECT_EQ(abc.compare(StringPiece("abcdefghijklmnopqrstuvwxyz")), 0);

  EXPECT_LT(abc, StringPiece("abcdefghijklmnopqrstuvwxzz"));
  EXPECT_LT(abc.compare(StringPiece("abcdefghijklmnopqrstuvwxzz")), 0);

  EXPECT_GT(abc, StringPiece("abcdefghijklmnopqrstuvwxyy"));
  EXPECT_GT(abc.compare(StringPiece("abcdefghijklmnopqrstuvwxyy")), 0);

  // starts_with
  EXPECT_TRUE(abc.starts_with(abc));
  EXPECT_TRUE(abc.starts_with("abcdefghijklm"));
  EXPECT_TRUE(!abc.starts_with("abcdefguvwxyz"));

  // ends_with
  EXPECT_TRUE(abc.ends_with(abc));
  EXPECT_TRUE(!abc.ends_with("abcdefguvwxyz"));
  EXPECT_TRUE(abc.ends_with("nopqrstuvwxyz"));
}

TEST(ComparisonOpsTest, StringCompareNotAmbiguous) {
  EXPECT_EQ("hello", string("hello"));
  EXPECT_LT("hello", string("world"));
}

TEST(ComparisonOpsTest, HeterogenousStringPieceEquals) {
  EXPECT_EQ(StringPiece("hello"), string("hello"));
  EXPECT_EQ("hello", StringPiece("hello"));
}

TEST(FindOneCharTest, EdgeCases) {
  StringPiece a("xxyyyxx");

  // Set a = "xyyyx".
  a.remove_prefix(1);
  a.remove_suffix(1);

  EXPECT_EQ(0, a.find('x'));
  EXPECT_EQ(0, a.find('x', 0));
  EXPECT_EQ(4, a.find('x', 1));
  EXPECT_EQ(4, a.find('x', 4));
  EXPECT_EQ(StringPiece::npos, a.find('x', 5));

  EXPECT_EQ(4, a.rfind('x'));
  EXPECT_EQ(4, a.rfind('x', 5));
  EXPECT_EQ(4, a.rfind('x', 4));
  EXPECT_EQ(0, a.rfind('x', 3));
  EXPECT_EQ(0, a.rfind('x', 0));

  // Set a = "yyy".
  a.remove_prefix(1);
  a.remove_suffix(1);

  EXPECT_EQ(StringPiece::npos, a.find('x'));
  EXPECT_EQ(StringPiece::npos, a.rfind('x'));
}

#ifdef PROTOBUF_HAS_DEATH_TEST
#ifndef NDEBUG
TEST(NonNegativeLenTest, NonNegativeLen) {
  EXPECT_DEATH(StringPiece("xyz", -1), "len >= 0");
}
#endif  // ndef DEBUG
#endif  // PROTOBUF_HAS_DEATH_TEST

}  // namespace
}  // namespace protobuf
}  // namespace google
