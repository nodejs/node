// Copyright 2012 the V8 project authors. All rights reserved.
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

#include <cstdlib>
#include <memory>
#include <sstream>

#include "include/v8.h"
#include "src/api/api-inl.h"
#include "src/ast/ast.h"
#include "src/codegen/assembler-arch.h"
#include "src/codegen/macro-assembler.h"
#include "src/init/v8.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/objects-inl.h"
#include "src/regexp/regexp-bytecode-generator.h"
#include "src/regexp/regexp-bytecodes.h"
#include "src/regexp/regexp-compiler.h"
#include "src/regexp/regexp-interpreter.h"
#include "src/regexp/regexp-macro-assembler-arch.h"
#include "src/regexp/regexp-parser.h"
#include "src/regexp/regexp.h"
#include "src/strings/char-predicates-inl.h"
#include "src/strings/string-stream.h"
#include "src/strings/unicode-inl.h"
#include "src/utils/ostreams.h"
#include "src/zone/zone-list-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace test_regexp {

static bool CheckParse(const char* input) {
  v8::HandleScope scope(CcTest::isolate());
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  FlatStringReader reader(CcTest::i_isolate(), CStrVector(input));
  RegExpCompileData result;
  return v8::internal::RegExpParser::ParseRegExp(
      CcTest::i_isolate(), &zone, &reader, JSRegExp::kNone, &result);
}


static void CheckParseEq(const char* input, const char* expected,
                         bool unicode = false) {
  v8::HandleScope scope(CcTest::isolate());
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  FlatStringReader reader(CcTest::i_isolate(), CStrVector(input));
  RegExpCompileData result;
  JSRegExp::Flags flags = JSRegExp::kNone;
  if (unicode) flags |= JSRegExp::kUnicode;
  CHECK(v8::internal::RegExpParser::ParseRegExp(CcTest::i_isolate(), &zone,
                                                &reader, flags, &result));
  CHECK_NOT_NULL(result.tree);
  CHECK(result.error == RegExpError::kNone);
  std::ostringstream os;
  result.tree->Print(os, &zone);
  if (strcmp(expected, os.str().c_str()) != 0) {
    printf("%s | %s\n", expected, os.str().c_str());
  }
  CHECK_EQ(0, strcmp(expected, os.str().c_str()));
}


static bool CheckSimple(const char* input) {
  v8::HandleScope scope(CcTest::isolate());
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  FlatStringReader reader(CcTest::i_isolate(), CStrVector(input));
  RegExpCompileData result;
  CHECK(v8::internal::RegExpParser::ParseRegExp(
      CcTest::i_isolate(), &zone, &reader, JSRegExp::kNone, &result));
  CHECK_NOT_NULL(result.tree);
  CHECK(result.error == RegExpError::kNone);
  return result.simple;
}

struct MinMaxPair {
  int min_match;
  int max_match;
};


static MinMaxPair CheckMinMaxMatch(const char* input) {
  v8::HandleScope scope(CcTest::isolate());
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  FlatStringReader reader(CcTest::i_isolate(), CStrVector(input));
  RegExpCompileData result;
  CHECK(v8::internal::RegExpParser::ParseRegExp(
      CcTest::i_isolate(), &zone, &reader, JSRegExp::kNone, &result));
  CHECK_NOT_NULL(result.tree);
  CHECK(result.error == RegExpError::kNone);
  int min_match = result.tree->min_match();
  int max_match = result.tree->max_match();
  MinMaxPair pair = { min_match, max_match };
  return pair;
}


#define CHECK_PARSE_ERROR(input) CHECK(!CheckParse(input))
#define CHECK_SIMPLE(input, simple) CHECK_EQ(simple, CheckSimple(input));
#define CHECK_MIN_MAX(input, min, max)                                         \
  { MinMaxPair min_max = CheckMinMaxMatch(input);                              \
    CHECK_EQ(min, min_max.min_match);                                          \
    CHECK_EQ(max, min_max.max_match);                                          \
  }

TEST(RegExpParser) {
  CHECK_PARSE_ERROR("?");

  CheckParseEq("abc", "'abc'");
  CheckParseEq("", "%");
  CheckParseEq("abc|def", "(| 'abc' 'def')");
  CheckParseEq("abc|def|ghi", "(| 'abc' 'def' 'ghi')");
  CheckParseEq("^xxx$", "(: @^i 'xxx' @$i)");
  CheckParseEq("ab\\b\\d\\bcd", "(: 'ab' @b [0-9] @b 'cd')");
  CheckParseEq("\\w|\\d", "(| [0-9 A-Z _ a-z] [0-9])");
  CheckParseEq("a*", "(# 0 - g 'a')");
  CheckParseEq("a*?", "(# 0 - n 'a')");
  CheckParseEq("abc+", "(: 'ab' (# 1 - g 'c'))");
  CheckParseEq("abc+?", "(: 'ab' (# 1 - n 'c'))");
  CheckParseEq("xyz?", "(: 'xy' (# 0 1 g 'z'))");
  CheckParseEq("xyz??", "(: 'xy' (# 0 1 n 'z'))");
  CheckParseEq("xyz{0,1}", "(: 'xy' (# 0 1 g 'z'))");
  CheckParseEq("xyz{0,1}?", "(: 'xy' (# 0 1 n 'z'))");
  CheckParseEq("xyz{93}", "(: 'xy' (# 93 93 g 'z'))");
  CheckParseEq("xyz{93}?", "(: 'xy' (# 93 93 n 'z'))");
  CheckParseEq("xyz{1,32}", "(: 'xy' (# 1 32 g 'z'))");
  CheckParseEq("xyz{1,32}?", "(: 'xy' (# 1 32 n 'z'))");
  CheckParseEq("xyz{1,}", "(: 'xy' (# 1 - g 'z'))");
  CheckParseEq("xyz{1,}?", "(: 'xy' (# 1 - n 'z'))");
  CheckParseEq("a\\fb\\nc\\rd\\te\\vf", "'a\\x0cb\\x0ac\\x0dd\\x09e\\x0bf'");
  CheckParseEq("a\\nb\\bc", "(: 'a\\x0ab' @b 'c')");
  CheckParseEq("(?:foo)", "(?: 'foo')");
  CheckParseEq("(?: foo )", "(?: ' foo ')");
  CheckParseEq("(foo|bar|baz)", "(^ (| 'foo' 'bar' 'baz'))");
  CheckParseEq("foo|(bar|baz)|quux", "(| 'foo' (^ (| 'bar' 'baz')) 'quux')");
  CheckParseEq("foo(?=bar)baz", "(: 'foo' (-> + 'bar') 'baz')");
  CheckParseEq("foo(?!bar)baz", "(: 'foo' (-> - 'bar') 'baz')");
  CheckParseEq("foo(?<=bar)baz", "(: 'foo' (<- + 'bar') 'baz')");
  CheckParseEq("foo(?<!bar)baz", "(: 'foo' (<- - 'bar') 'baz')");
  CheckParseEq("()", "(^ %)");
  CheckParseEq("(?=)", "(-> + %)");
  CheckParseEq("[]", "^[\\x00-\\u{10ffff}]");  // Doesn't compile on windows
  CheckParseEq("[^]", "[\\x00-\\u{10ffff}]");  // \uffff isn't in codepage 1252
  CheckParseEq("[x]", "[x]");
  CheckParseEq("[xyz]", "[x y z]");
  CheckParseEq("[a-zA-Z0-9]", "[a-z A-Z 0-9]");
  CheckParseEq("[-123]", "[- 1 2 3]");
  CheckParseEq("[^123]", "^[1 2 3]");
  CheckParseEq("]", "']'");
  CheckParseEq("}", "'}'");
  CheckParseEq("[a-b-c]", "[a-b - c]");
  CheckParseEq("[\\d]", "[0-9]");
  CheckParseEq("[x\\dz]", "[x 0-9 z]");
  CheckParseEq("[\\d-z]", "[0-9 - z]");
  CheckParseEq("[\\d-\\d]", "[0-9 0-9 -]");
  CheckParseEq("[z-\\d]", "[0-9 z -]");
  // Control character outside character class.
  CheckParseEq("\\cj\\cJ\\ci\\cI\\ck\\cK", "'\\x0a\\x0a\\x09\\x09\\x0b\\x0b'");
  CheckParseEq("\\c!", "'\\c!'");
  CheckParseEq("\\c_", "'\\c_'");
  CheckParseEq("\\c~", "'\\c~'");
  CheckParseEq("\\c1", "'\\c1'");
  // Control character inside character class.
  CheckParseEq("[\\c!]", "[\\ c !]");
  CheckParseEq("[\\c_]", "[\\x1f]");
  CheckParseEq("[\\c~]", "[\\ c ~]");
  CheckParseEq("[\\ca]", "[\\x01]");
  CheckParseEq("[\\cz]", "[\\x1a]");
  CheckParseEq("[\\cA]", "[\\x01]");
  CheckParseEq("[\\cZ]", "[\\x1a]");
  CheckParseEq("[\\c1]", "[\\x11]");

  CheckParseEq("[a\\]c]", "[a ] c]");
  CheckParseEq("\\[\\]\\{\\}\\(\\)\\%\\^\\#\\ ", "'[]{}()%^# '");
  CheckParseEq("[\\[\\]\\{\\}\\(\\)\\%\\^\\#\\ ]", "[[ ] { } ( ) % ^ #  ]");
  CheckParseEq("\\0", "'\\x00'");
  CheckParseEq("\\8", "'8'");
  CheckParseEq("\\9", "'9'");
  CheckParseEq("\\11", "'\\x09'");
  CheckParseEq("\\11a", "'\\x09a'");
  CheckParseEq("\\011", "'\\x09'");
  CheckParseEq("\\00011", "'\\x0011'");
  CheckParseEq("\\118", "'\\x098'");
  CheckParseEq("\\111", "'I'");
  CheckParseEq("\\1111", "'I1'");
  CheckParseEq("(x)(x)(x)\\1", "(: (^ 'x') (^ 'x') (^ 'x') (<- 1))");
  CheckParseEq("(x)(x)(x)\\2", "(: (^ 'x') (^ 'x') (^ 'x') (<- 2))");
  CheckParseEq("(x)(x)(x)\\3", "(: (^ 'x') (^ 'x') (^ 'x') (<- 3))");
  CheckParseEq("(x)(x)(x)\\4", "(: (^ 'x') (^ 'x') (^ 'x') '\\x04')");
  CheckParseEq("(x)(x)(x)\\1*",
               "(: (^ 'x') (^ 'x') (^ 'x')"
               " (# 0 - g (<- 1)))");
  CheckParseEq("(x)(x)(x)\\2*",
               "(: (^ 'x') (^ 'x') (^ 'x')"
               " (# 0 - g (<- 2)))");
  CheckParseEq("(x)(x)(x)\\3*",
               "(: (^ 'x') (^ 'x') (^ 'x')"
               " (# 0 - g (<- 3)))");
  CheckParseEq("(x)(x)(x)\\4*",
               "(: (^ 'x') (^ 'x') (^ 'x')"
               " (# 0 - g '\\x04'))");
  CheckParseEq("(x)(x)(x)(x)(x)(x)(x)(x)(x)(x)\\10",
               "(: (^ 'x') (^ 'x') (^ 'x') (^ 'x') (^ 'x') (^ 'x')"
               " (^ 'x') (^ 'x') (^ 'x') (^ 'x') (<- 10))");
  CheckParseEq("(x)(x)(x)(x)(x)(x)(x)(x)(x)(x)\\11",
               "(: (^ 'x') (^ 'x') (^ 'x') (^ 'x') (^ 'x') (^ 'x')"
               " (^ 'x') (^ 'x') (^ 'x') (^ 'x') '\\x09')");
  CheckParseEq("(a)\\1", "(: (^ 'a') (<- 1))");
  CheckParseEq("(a\\1)", "(^ 'a')");
  CheckParseEq("(\\1a)", "(^ 'a')");
  CheckParseEq("(\\2)(\\1)", "(: (^ (<- 2)) (^ (<- 1)))");
  CheckParseEq("(?=a)?a", "'a'");
  CheckParseEq("(?=a){0,10}a", "'a'");
  CheckParseEq("(?=a){1,10}a", "(: (-> + 'a') 'a')");
  CheckParseEq("(?=a){9,10}a", "(: (-> + 'a') 'a')");
  CheckParseEq("(?!a)?a", "'a'");
  CheckParseEq("\\1(a)", "(: (<- 1) (^ 'a'))");
  CheckParseEq("(?!(a))\\1", "(: (-> - (^ 'a')) (<- 1))");
  CheckParseEq("(?!\\1(a\\1)\\1)\\1",
               "(: (-> - (: (<- 1) (^ 'a') (<- 1))) (<- 1))");
  CheckParseEq("\\1\\2(a(?:\\1(b\\1\\2))\\2)\\1",
               "(: (<- 1) (<- 2) (^ (: 'a' (?: (^ 'b')) (<- 2))) (<- 1))");
  CheckParseEq("\\1\\2(a(?<=\\1(b\\1\\2))\\2)\\1",
               "(: (<- 1) (<- 2) (^ (: 'a' (<- + (^ 'b')) (<- 2))) (<- 1))");
  CheckParseEq("[\\0]", "[\\x00]");
  CheckParseEq("[\\11]", "[\\x09]");
  CheckParseEq("[\\11a]", "[\\x09 a]");
  CheckParseEq("[\\011]", "[\\x09]");
  CheckParseEq("[\\00011]", "[\\x00 1 1]");
  CheckParseEq("[\\118]", "[\\x09 8]");
  CheckParseEq("[\\111]", "[I]");
  CheckParseEq("[\\1111]", "[I 1]");
  CheckParseEq("\\x34", "'\x34'");
  CheckParseEq("\\x60", "'\x60'");
  CheckParseEq("\\x3z", "'x3z'");
  CheckParseEq("\\c", "'\\c'");
  CheckParseEq("\\u0034", "'\x34'");
  CheckParseEq("\\u003z", "'u003z'");
  CheckParseEq("foo[z]*", "(: 'foo' (# 0 - g [z]))");
  CheckParseEq("^^^$$$\\b\\b\\b\\b", "(: @^i @^i @^i @$i @$i @$i @b @b @b @b)");
  CheckParseEq("\\b\\b\\b\\b\\B\\B\\B\\B\\b\\b\\b\\b",
               "(: @b @b @b @b @B @B @B @B @b @b @b @b)");
  CheckParseEq("\\b\\B\\b", "(: @b @B @b)");

  // Unicode regexps
  CheckParseEq("\\u{12345}", "'\\ud808\\udf45'", true);
  CheckParseEq("\\u{12345}\\u{23456}", "(! '\\ud808\\udf45' '\\ud84d\\udc56')",
               true);
  CheckParseEq("\\u{12345}|\\u{23456}", "(| '\\ud808\\udf45' '\\ud84d\\udc56')",
               true);
  CheckParseEq("\\u{12345}{3}", "(# 3 3 g '\\ud808\\udf45')", true);
  CheckParseEq("\\u{12345}*", "(# 0 - g '\\ud808\\udf45')", true);

  CheckParseEq("\\ud808\\udf45*", "(# 0 - g '\\ud808\\udf45')", true);
  CheckParseEq("[\\ud808\\udf45-\\ud809\\udccc]", "[\\u{012345}-\\u{0124cc}]",
               true);

  CHECK_SIMPLE("", false);
  CHECK_SIMPLE("a", true);
  CHECK_SIMPLE("a|b", false);
  CHECK_SIMPLE("a\\n", false);
  CHECK_SIMPLE("^a", false);
  CHECK_SIMPLE("a$", false);
  CHECK_SIMPLE("a\\b!", false);
  CHECK_SIMPLE("a\\Bb", false);
  CHECK_SIMPLE("a*", false);
  CHECK_SIMPLE("a*?", false);
  CHECK_SIMPLE("a?", false);
  CHECK_SIMPLE("a??", false);
  CHECK_SIMPLE("a{0,1}?", false);
  CHECK_SIMPLE("a{1,1}?", false);
  CHECK_SIMPLE("a{1,2}?", false);
  CHECK_SIMPLE("a+?", false);
  CHECK_SIMPLE("(a)", false);
  CHECK_SIMPLE("(a)\\1", false);
  CHECK_SIMPLE("(\\1a)", false);
  CHECK_SIMPLE("\\1(a)", false);
  CHECK_SIMPLE("a\\s", false);
  CHECK_SIMPLE("a\\S", false);
  CHECK_SIMPLE("a\\d", false);
  CHECK_SIMPLE("a\\D", false);
  CHECK_SIMPLE("a\\w", false);
  CHECK_SIMPLE("a\\W", false);
  CHECK_SIMPLE("a.", false);
  CHECK_SIMPLE("a\\q", false);
  CHECK_SIMPLE("a[a]", false);
  CHECK_SIMPLE("a[^a]", false);
  CHECK_SIMPLE("a[a-z]", false);
  CHECK_SIMPLE("a[\\q]", false);
  CHECK_SIMPLE("a(?:b)", false);
  CHECK_SIMPLE("a(?=b)", false);
  CHECK_SIMPLE("a(?!b)", false);
  CHECK_SIMPLE("\\x60", false);
  CHECK_SIMPLE("\\u0060", false);
  CHECK_SIMPLE("\\cA", false);
  CHECK_SIMPLE("\\q", false);
  CHECK_SIMPLE("\\1112", false);
  CHECK_SIMPLE("\\0", false);
  CHECK_SIMPLE("(a)\\1", false);
  CHECK_SIMPLE("(?=a)?a", false);
  CHECK_SIMPLE("(?!a)?a\\1", false);
  CHECK_SIMPLE("(?:(?=a))a\\1", false);

  CheckParseEq("a{}", "'a{}'");
  CheckParseEq("a{,}", "'a{,}'");
  CheckParseEq("a{", "'a{'");
  CheckParseEq("a{z}", "'a{z}'");
  CheckParseEq("a{1z}", "'a{1z}'");
  CheckParseEq("a{12z}", "'a{12z}'");
  CheckParseEq("a{12,", "'a{12,'");
  CheckParseEq("a{12,3b", "'a{12,3b'");
  CheckParseEq("{}", "'{}'");
  CheckParseEq("{,}", "'{,}'");
  CheckParseEq("{", "'{'");
  CheckParseEq("{z}", "'{z}'");
  CheckParseEq("{1z}", "'{1z}'");
  CheckParseEq("{12z}", "'{12z}'");
  CheckParseEq("{12,", "'{12,'");
  CheckParseEq("{12,3b", "'{12,3b'");

  CHECK_MIN_MAX("a", 1, 1);
  CHECK_MIN_MAX("abc", 3, 3);
  CHECK_MIN_MAX("a[bc]d", 3, 3);
  CHECK_MIN_MAX("a|bc", 1, 2);
  CHECK_MIN_MAX("ab|c", 1, 2);
  CHECK_MIN_MAX("a||bc", 0, 2);
  CHECK_MIN_MAX("|", 0, 0);
  CHECK_MIN_MAX("(?:ab)", 2, 2);
  CHECK_MIN_MAX("(?:ab|cde)", 2, 3);
  CHECK_MIN_MAX("(?:ab)|cde", 2, 3);
  CHECK_MIN_MAX("(ab)", 2, 2);
  CHECK_MIN_MAX("(ab|cde)", 2, 3);
  CHECK_MIN_MAX("(ab)\\1", 2, RegExpTree::kInfinity);
  CHECK_MIN_MAX("(ab|cde)\\1", 2, RegExpTree::kInfinity);
  CHECK_MIN_MAX("(?:ab)?", 0, 2);
  CHECK_MIN_MAX("(?:ab)*", 0, RegExpTree::kInfinity);
  CHECK_MIN_MAX("(?:ab)+", 2, RegExpTree::kInfinity);
  CHECK_MIN_MAX("a?", 0, 1);
  CHECK_MIN_MAX("a*", 0, RegExpTree::kInfinity);
  CHECK_MIN_MAX("a+", 1, RegExpTree::kInfinity);
  CHECK_MIN_MAX("a??", 0, 1);
  CHECK_MIN_MAX("a*?", 0, RegExpTree::kInfinity);
  CHECK_MIN_MAX("a+?", 1, RegExpTree::kInfinity);
  CHECK_MIN_MAX("(?:a?)?", 0, 1);
  CHECK_MIN_MAX("(?:a*)?", 0, RegExpTree::kInfinity);
  CHECK_MIN_MAX("(?:a+)?", 0, RegExpTree::kInfinity);
  CHECK_MIN_MAX("(?:a?)+", 0, RegExpTree::kInfinity);
  CHECK_MIN_MAX("(?:a*)+", 0, RegExpTree::kInfinity);
  CHECK_MIN_MAX("(?:a+)+", 1, RegExpTree::kInfinity);
  CHECK_MIN_MAX("(?:a?)*", 0, RegExpTree::kInfinity);
  CHECK_MIN_MAX("(?:a*)*", 0, RegExpTree::kInfinity);
  CHECK_MIN_MAX("(?:a+)*", 0, RegExpTree::kInfinity);
  CHECK_MIN_MAX("a{0}", 0, 0);
  CHECK_MIN_MAX("(?:a+){0}", 0, 0);
  CHECK_MIN_MAX("(?:a+){0,0}", 0, 0);
  CHECK_MIN_MAX("a*b", 1, RegExpTree::kInfinity);
  CHECK_MIN_MAX("a+b", 2, RegExpTree::kInfinity);
  CHECK_MIN_MAX("a*b|c", 1, RegExpTree::kInfinity);
  CHECK_MIN_MAX("a+b|c", 1, RegExpTree::kInfinity);
  CHECK_MIN_MAX("(?:a{5,1000000}){3,1000000}", 15, RegExpTree::kInfinity);
  CHECK_MIN_MAX("(?:ab){4,7}", 8, 14);
  CHECK_MIN_MAX("a\\bc", 2, 2);
  CHECK_MIN_MAX("a\\Bc", 2, 2);
  CHECK_MIN_MAX("a\\sc", 3, 3);
  CHECK_MIN_MAX("a\\Sc", 3, 3);
  CHECK_MIN_MAX("a(?=b)c", 2, 2);
  CHECK_MIN_MAX("a(?=bbb|bb)c", 2, 2);
  CHECK_MIN_MAX("a(?!bbb|bb)c", 2, 2);

  CheckParseEq("(?<a>x)(?<b>x)(?<c>x)\\k<a>",
               "(: (^ 'x') (^ 'x') (^ 'x') (<- 1))", true);
  CheckParseEq("(?<a>x)(?<b>x)(?<c>x)\\k<b>",
               "(: (^ 'x') (^ 'x') (^ 'x') (<- 2))", true);
  CheckParseEq("(?<a>x)(?<b>x)(?<c>x)\\k<c>",
               "(: (^ 'x') (^ 'x') (^ 'x') (<- 3))", true);
  CheckParseEq("(?<a>a)\\k<a>", "(: (^ 'a') (<- 1))", true);
  CheckParseEq("(?<a>a\\k<a>)", "(^ 'a')", true);
  CheckParseEq("(?<a>\\k<a>a)", "(^ 'a')", true);
  CheckParseEq("(?<a>\\k<b>)(?<b>\\k<a>)", "(: (^ (<- 2)) (^ (<- 1)))", true);
  CheckParseEq("\\k<a>(?<a>a)", "(: (<- 1) (^ 'a'))", true);

  CheckParseEq("(?<\\u{03C0}>a)", "(^ 'a')", true);
  CheckParseEq("(?<\\u03C0>a)", "(^ 'a')", true);
}

TEST(ParserRegression) {
  CheckParseEq("[A-Z$-][x]", "(! [A-Z $ -] [x])");
  CheckParseEq("a{3,4*}", "(: 'a{3,' (# 0 - g '4') '}')");
  CheckParseEq("{", "'{'");
  CheckParseEq("a|", "(| 'a' %)");
}

static void ExpectError(const char* input, const char* expected,
                        bool unicode = false) {
  Isolate* isolate = CcTest::i_isolate();

  v8::HandleScope scope(CcTest::isolate());
  Zone zone(isolate->allocator(), ZONE_NAME);
  FlatStringReader reader(isolate, CStrVector(input));
  RegExpCompileData result;
  JSRegExp::Flags flags = JSRegExp::kNone;
  if (unicode) flags |= JSRegExp::kUnicode;
  CHECK(!v8::internal::RegExpParser::ParseRegExp(isolate, &zone, &reader, flags,
                                                 &result));
  CHECK_NULL(result.tree);
  CHECK(result.error != RegExpError::kNone);
  CHECK_EQ(0, strcmp(expected, RegExpErrorString(result.error)));
}


TEST(Errors) {
  const char* kEndBackslash = "\\ at end of pattern";
  ExpectError("\\", kEndBackslash);
  const char* kUnterminatedGroup = "Unterminated group";
  ExpectError("(foo", kUnterminatedGroup);
  const char* kInvalidGroup = "Invalid group";
  ExpectError("(?", kInvalidGroup);
  const char* kUnterminatedCharacterClass = "Unterminated character class";
  ExpectError("[", kUnterminatedCharacterClass);
  ExpectError("[a-", kUnterminatedCharacterClass);
  const char* kNothingToRepeat = "Nothing to repeat";
  ExpectError("*", kNothingToRepeat);
  ExpectError("?", kNothingToRepeat);
  ExpectError("+", kNothingToRepeat);
  ExpectError("{1}", kNothingToRepeat);
  ExpectError("{1,2}", kNothingToRepeat);
  ExpectError("{1,}", kNothingToRepeat);

  // Check that we don't allow more than kMaxCapture captures
  const int kMaxCaptures = 1 << 16;  // Must match RegExpParser::kMaxCaptures.
  const char* kTooManyCaptures = "Too many captures";
  std::ostringstream os;
  for (int i = 0; i <= kMaxCaptures; i++) {
    os << "()";
  }
  ExpectError(os.str().c_str(), kTooManyCaptures);

  const char* kInvalidCaptureName = "Invalid capture group name";
  ExpectError("(?<>.)", kInvalidCaptureName, true);
  ExpectError("(?<1>.)", kInvalidCaptureName, true);
  ExpectError("(?<_%>.)", kInvalidCaptureName, true);
  ExpectError("\\k<a", kInvalidCaptureName, true);
  const char* kDuplicateCaptureName = "Duplicate capture group name";
  ExpectError("(?<a>.)(?<a>.)", kDuplicateCaptureName, true);
  const char* kInvalidUnicodeEscape = "Invalid Unicode escape";
  ExpectError("(?<\\u{FISK}", kInvalidUnicodeEscape, true);
  const char* kInvalidCaptureReferenced = "Invalid named capture referenced";
  ExpectError("\\k<a>", kInvalidCaptureReferenced, true);
  ExpectError("(?<b>)\\k<a>", kInvalidCaptureReferenced, true);
  const char* kInvalidNamedReference = "Invalid named reference";
  ExpectError("\\ka", kInvalidNamedReference, true);
}


static bool IsDigit(uc16 c) {
  return ('0' <= c && c <= '9');
}


static bool NotDigit(uc16 c) {
  return !IsDigit(c);
}


static bool IsWhiteSpaceOrLineTerminator(uc16 c) {
  // According to ECMA 5.1, 15.10.2.12 the CharacterClassEscape \s includes
  // WhiteSpace (7.2) and LineTerminator (7.3) values.
  return v8::internal::IsWhiteSpaceOrLineTerminator(c);
}


static bool NotWhiteSpaceNorLineTermiantor(uc16 c) {
  return !IsWhiteSpaceOrLineTerminator(c);
}


static bool NotWord(uc16 c) {
  return !IsRegExpWord(c);
}


static void TestCharacterClassEscapes(uc16 c, bool (pred)(uc16 c)) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  ZoneList<CharacterRange>* ranges =
      new(&zone) ZoneList<CharacterRange>(2, &zone);
  CharacterRange::AddClassEscape(c, ranges, &zone);
  for (uc32 i = 0; i < (1 << 16); i++) {
    bool in_class = false;
    for (int j = 0; !in_class && j < ranges->length(); j++) {
      CharacterRange& range = ranges->at(j);
      in_class = (range.from() <= i && i <= range.to());
    }
    CHECK_EQ(pred(i), in_class);
  }
}


TEST(CharacterClassEscapes) {
  TestCharacterClassEscapes('.', IsRegExpNewline);
  TestCharacterClassEscapes('d', IsDigit);
  TestCharacterClassEscapes('D', NotDigit);
  TestCharacterClassEscapes('s', IsWhiteSpaceOrLineTerminator);
  TestCharacterClassEscapes('S', NotWhiteSpaceNorLineTermiantor);
  TestCharacterClassEscapes('w', IsRegExpWord);
  TestCharacterClassEscapes('W', NotWord);
}


static RegExpNode* Compile(const char* input, bool multiline, bool unicode,
                           bool is_one_byte, Zone* zone) {
  Isolate* isolate = CcTest::i_isolate();
  FlatStringReader reader(isolate, CStrVector(input));
  RegExpCompileData compile_data;
  compile_data.compilation_target = RegExpCompilationTarget::kNative;
  JSRegExp::Flags flags = JSRegExp::kNone;
  if (multiline) flags = JSRegExp::kMultiline;
  if (unicode) flags = JSRegExp::kUnicode;
  if (!v8::internal::RegExpParser::ParseRegExp(CcTest::i_isolate(), zone,
                                               &reader, flags, &compile_data))
    return nullptr;
  Handle<String> pattern = isolate->factory()
                               ->NewStringFromUtf8(CStrVector(input))
                               .ToHandleChecked();
  Handle<String> sample_subject =
      isolate->factory()->NewStringFromUtf8(CStrVector("")).ToHandleChecked();
  RegExp::CompileForTesting(isolate, zone, &compile_data, flags, pattern,
                            sample_subject, is_one_byte);
  return compile_data.node;
}


static void Execute(const char* input, bool multiline, bool unicode,
                    bool is_one_byte, bool dot_output = false) {
  v8::HandleScope scope(CcTest::isolate());
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  RegExpNode* node = Compile(input, multiline, unicode, is_one_byte, &zone);
  USE(node);
#ifdef DEBUG
  if (dot_output) RegExp::DotPrintForTesting(input, node);
#endif  // DEBUG
}

// Test of debug-only syntax.
#ifdef DEBUG

TEST(ParsePossessiveRepetition) {
  bool old_flag_value = FLAG_regexp_possessive_quantifier;

  // Enable possessive quantifier syntax.
  FLAG_regexp_possessive_quantifier = true;

  CheckParseEq("a*+", "(# 0 - p 'a')");
  CheckParseEq("a++", "(# 1 - p 'a')");
  CheckParseEq("a?+", "(# 0 1 p 'a')");
  CheckParseEq("a{10,20}+", "(# 10 20 p 'a')");
  CheckParseEq("za{10,20}+b", "(: 'z' (# 10 20 p 'a') 'b')");

  // Disable possessive quantifier syntax.
  FLAG_regexp_possessive_quantifier = false;

  CHECK_PARSE_ERROR("a*+");
  CHECK_PARSE_ERROR("a++");
  CHECK_PARSE_ERROR("a?+");
  CHECK_PARSE_ERROR("a{10,20}+");
  CHECK_PARSE_ERROR("a{10,20}+b");

  FLAG_regexp_possessive_quantifier = old_flag_value;
}

#endif

// Tests of interpreter.

#if V8_TARGET_ARCH_IA32
using ArchRegExpMacroAssembler = RegExpMacroAssemblerIA32;
#elif V8_TARGET_ARCH_X64
using ArchRegExpMacroAssembler = RegExpMacroAssemblerX64;
#elif V8_TARGET_ARCH_ARM
using ArchRegExpMacroAssembler = RegExpMacroAssemblerARM;
#elif V8_TARGET_ARCH_ARM64
using ArchRegExpMacroAssembler = RegExpMacroAssemblerARM64;
#elif V8_TARGET_ARCH_S390
using ArchRegExpMacroAssembler = RegExpMacroAssemblerS390;
#elif V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64
using ArchRegExpMacroAssembler = RegExpMacroAssemblerPPC;
#elif V8_TARGET_ARCH_MIPS
using ArchRegExpMacroAssembler = RegExpMacroAssemblerMIPS;
#elif V8_TARGET_ARCH_MIPS64
using ArchRegExpMacroAssembler = RegExpMacroAssemblerMIPS;
#elif V8_TARGET_ARCH_X87
using ArchRegExpMacroAssembler = RegExpMacroAssemblerX87;
#endif

class ContextInitializer {
 public:
  ContextInitializer()
      : scope_(CcTest::isolate()),
        env_(v8::Context::New(CcTest::isolate())) {
    env_->Enter();
  }
  ~ContextInitializer() {
    env_->Exit();
  }
 private:
  v8::HandleScope scope_;
  v8::Local<v8::Context> env_;
};

// Create new JSRegExp object with only necessary fields (for this tests)
// initialized.
static Handle<JSRegExp> CreateJSRegExp(Handle<String> source, Handle<Code> code,
                                       bool is_unicode = false) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Handle<JSFunction> constructor = isolate->regexp_function();
  Handle<JSRegExp> regexp =
      Handle<JSRegExp>::cast(factory->NewJSObject(constructor));

  factory->SetRegExpIrregexpData(regexp, JSRegExp::IRREGEXP, source,
                                 JSRegExp::kNone, 0,
                                 JSRegExp::kNoBacktrackLimit);
  regexp->SetDataAt(is_unicode ? JSRegExp::kIrregexpUC16CodeIndex
                               : JSRegExp::kIrregexpLatin1CodeIndex,
                    *code);

  return regexp;
}

static ArchRegExpMacroAssembler::Result Execute(JSRegExp regexp, String input,
                                                int start_offset,
                                                Address input_start,
                                                Address input_end,
                                                int* captures) {
  return static_cast<NativeRegExpMacroAssembler::Result>(
      NativeRegExpMacroAssembler::Execute(
          input, start_offset, reinterpret_cast<byte*>(input_start),
          reinterpret_cast<byte*>(input_end), captures, 0, CcTest::i_isolate(),
          regexp));
}

TEST(MacroAssemblerNativeSuccess) {
  v8::V8::Initialize();
  ContextInitializer initializer;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);

  ArchRegExpMacroAssembler m(isolate, &zone, NativeRegExpMacroAssembler::LATIN1,
                             4);

  m.Succeed();

  Handle<String> source = factory->NewStringFromStaticChars("");
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);
  Handle<JSRegExp> regexp = CreateJSRegExp(source, code);

  int captures[4] = {42, 37, 87, 117};
  Handle<String> input = factory->NewStringFromStaticChars("foofoo");
  Handle<SeqOneByteString> seq_input = Handle<SeqOneByteString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  NativeRegExpMacroAssembler::Result result = Execute(
      *regexp, *input, 0, start_adr, start_adr + seq_input->length(), captures);

  CHECK_EQ(NativeRegExpMacroAssembler::SUCCESS, result);
  CHECK_EQ(-1, captures[0]);
  CHECK_EQ(-1, captures[1]);
  CHECK_EQ(-1, captures[2]);
  CHECK_EQ(-1, captures[3]);
}


TEST(MacroAssemblerNativeSimple) {
  v8::V8::Initialize();
  ContextInitializer initializer;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);

  ArchRegExpMacroAssembler m(isolate, &zone, NativeRegExpMacroAssembler::LATIN1,
                             4);

  Label fail, backtrack;
  m.PushBacktrack(&fail);
  m.CheckNotAtStart(0, nullptr);
  m.LoadCurrentCharacter(2, nullptr);
  m.CheckNotCharacter('o', nullptr);
  m.LoadCurrentCharacter(1, nullptr, false);
  m.CheckNotCharacter('o', nullptr);
  m.LoadCurrentCharacter(0, nullptr, false);
  m.CheckNotCharacter('f', nullptr);
  m.WriteCurrentPositionToRegister(0, 0);
  m.WriteCurrentPositionToRegister(1, 3);
  m.AdvanceCurrentPosition(3);
  m.PushBacktrack(&backtrack);
  m.Succeed();
  m.BindJumpTarget(&backtrack);
  m.Backtrack();
  m.BindJumpTarget(&fail);
  m.Fail();

  Handle<String> source = factory->NewStringFromStaticChars("^foo");
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);
  Handle<JSRegExp> regexp = CreateJSRegExp(source, code);

  int captures[4] = {42, 37, 87, 117};
  Handle<String> input = factory->NewStringFromStaticChars("foofoo");
  Handle<SeqOneByteString> seq_input = Handle<SeqOneByteString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  NativeRegExpMacroAssembler::Result result = Execute(
      *regexp, *input, 0, start_adr, start_adr + input->length(), captures);

  CHECK_EQ(NativeRegExpMacroAssembler::SUCCESS, result);
  CHECK_EQ(0, captures[0]);
  CHECK_EQ(3, captures[1]);
  CHECK_EQ(-1, captures[2]);
  CHECK_EQ(-1, captures[3]);

  input = factory->NewStringFromStaticChars("barbarbar");
  seq_input = Handle<SeqOneByteString>::cast(input);
  start_adr = seq_input->GetCharsAddress();

  result = Execute(*regexp, *input, 0, start_adr, start_adr + input->length(),
                   captures);

  CHECK_EQ(NativeRegExpMacroAssembler::FAILURE, result);
}


TEST(MacroAssemblerNativeSimpleUC16) {
  v8::V8::Initialize();
  ContextInitializer initializer;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);

  ArchRegExpMacroAssembler m(isolate, &zone, NativeRegExpMacroAssembler::UC16,
                             4);

  Label fail, backtrack;
  m.PushBacktrack(&fail);
  m.CheckNotAtStart(0, nullptr);
  m.LoadCurrentCharacter(2, nullptr);
  m.CheckNotCharacter('o', nullptr);
  m.LoadCurrentCharacter(1, nullptr, false);
  m.CheckNotCharacter('o', nullptr);
  m.LoadCurrentCharacter(0, nullptr, false);
  m.CheckNotCharacter('f', nullptr);
  m.WriteCurrentPositionToRegister(0, 0);
  m.WriteCurrentPositionToRegister(1, 3);
  m.AdvanceCurrentPosition(3);
  m.PushBacktrack(&backtrack);
  m.Succeed();
  m.BindJumpTarget(&backtrack);
  m.Backtrack();
  m.BindJumpTarget(&fail);
  m.Fail();

  Handle<String> source = factory->NewStringFromStaticChars("^foo");
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);
  Handle<JSRegExp> regexp = CreateJSRegExp(source, code, true);

  int captures[4] = {42, 37, 87, 117};
  const uc16 input_data[6] = {'f', 'o', 'o', 'f', 'o',
                              static_cast<uc16>(0x2603)};
  Handle<String> input = factory->NewStringFromTwoByte(
      Vector<const uc16>(input_data, 6)).ToHandleChecked();
  Handle<SeqTwoByteString> seq_input = Handle<SeqTwoByteString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  NativeRegExpMacroAssembler::Result result = Execute(
      *regexp, *input, 0, start_adr, start_adr + input->length(), captures);

  CHECK_EQ(NativeRegExpMacroAssembler::SUCCESS, result);
  CHECK_EQ(0, captures[0]);
  CHECK_EQ(3, captures[1]);
  CHECK_EQ(-1, captures[2]);
  CHECK_EQ(-1, captures[3]);

  const uc16 input_data2[9] = {'b', 'a', 'r', 'b', 'a', 'r', 'b', 'a',
                               static_cast<uc16>(0x2603)};
  input = factory->NewStringFromTwoByte(
      Vector<const uc16>(input_data2, 9)).ToHandleChecked();
  seq_input = Handle<SeqTwoByteString>::cast(input);
  start_adr = seq_input->GetCharsAddress();

  result = Execute(*regexp, *input, 0, start_adr,
                   start_adr + input->length() * 2, captures);

  CHECK_EQ(NativeRegExpMacroAssembler::FAILURE, result);
}


TEST(MacroAssemblerNativeBacktrack) {
  v8::V8::Initialize();
  ContextInitializer initializer;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);

  ArchRegExpMacroAssembler m(isolate, &zone, NativeRegExpMacroAssembler::LATIN1,
                             0);

  Label fail;
  Label backtrack;
  m.LoadCurrentCharacter(10, &fail);
  m.Succeed();
  m.BindJumpTarget(&fail);
  m.PushBacktrack(&backtrack);
  m.LoadCurrentCharacter(10, nullptr);
  m.Succeed();
  m.BindJumpTarget(&backtrack);
  m.Fail();

  Handle<String> source = factory->NewStringFromStaticChars("..........");
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);
  Handle<JSRegExp> regexp = CreateJSRegExp(source, code);

  Handle<String> input = factory->NewStringFromStaticChars("foofoo");
  Handle<SeqOneByteString> seq_input = Handle<SeqOneByteString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  NativeRegExpMacroAssembler::Result result = Execute(
      *regexp, *input, 0, start_adr, start_adr + input->length(), nullptr);

  CHECK_EQ(NativeRegExpMacroAssembler::FAILURE, result);
}


TEST(MacroAssemblerNativeBackReferenceLATIN1) {
  v8::V8::Initialize();
  ContextInitializer initializer;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);

  ArchRegExpMacroAssembler m(isolate, &zone, NativeRegExpMacroAssembler::LATIN1,
                             4);

  m.WriteCurrentPositionToRegister(0, 0);
  m.AdvanceCurrentPosition(2);
  m.WriteCurrentPositionToRegister(1, 0);
  Label nomatch;
  m.CheckNotBackReference(0, false, &nomatch);
  m.Fail();
  m.Bind(&nomatch);
  m.AdvanceCurrentPosition(2);
  Label missing_match;
  m.CheckNotBackReference(0, false, &missing_match);
  m.WriteCurrentPositionToRegister(2, 0);
  m.Succeed();
  m.Bind(&missing_match);
  m.Fail();

  Handle<String> source = factory->NewStringFromStaticChars("^(..)..\1");
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);
  Handle<JSRegExp> regexp = CreateJSRegExp(source, code);

  Handle<String> input = factory->NewStringFromStaticChars("fooofo");
  Handle<SeqOneByteString> seq_input = Handle<SeqOneByteString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  int output[4];
  NativeRegExpMacroAssembler::Result result = Execute(
      *regexp, *input, 0, start_adr, start_adr + input->length(), output);

  CHECK_EQ(NativeRegExpMacroAssembler::SUCCESS, result);
  CHECK_EQ(0, output[0]);
  CHECK_EQ(2, output[1]);
  CHECK_EQ(6, output[2]);
  CHECK_EQ(-1, output[3]);
}


TEST(MacroAssemblerNativeBackReferenceUC16) {
  v8::V8::Initialize();
  ContextInitializer initializer;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);

  ArchRegExpMacroAssembler m(isolate, &zone, NativeRegExpMacroAssembler::UC16,
                             4);

  m.WriteCurrentPositionToRegister(0, 0);
  m.AdvanceCurrentPosition(2);
  m.WriteCurrentPositionToRegister(1, 0);
  Label nomatch;
  m.CheckNotBackReference(0, false, &nomatch);
  m.Fail();
  m.Bind(&nomatch);
  m.AdvanceCurrentPosition(2);
  Label missing_match;
  m.CheckNotBackReference(0, false, &missing_match);
  m.WriteCurrentPositionToRegister(2, 0);
  m.Succeed();
  m.Bind(&missing_match);
  m.Fail();

  Handle<String> source = factory->NewStringFromStaticChars("^(..)..\1");
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);
  Handle<JSRegExp> regexp = CreateJSRegExp(source, code, true);

  const uc16 input_data[6] = {'f', 0x2028, 'o', 'o', 'f', 0x2028};
  Handle<String> input = factory->NewStringFromTwoByte(
      Vector<const uc16>(input_data, 6)).ToHandleChecked();
  Handle<SeqTwoByteString> seq_input = Handle<SeqTwoByteString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  int output[4];
  NativeRegExpMacroAssembler::Result result = Execute(
      *regexp, *input, 0, start_adr, start_adr + input->length() * 2, output);

  CHECK_EQ(NativeRegExpMacroAssembler::SUCCESS, result);
  CHECK_EQ(0, output[0]);
  CHECK_EQ(2, output[1]);
  CHECK_EQ(6, output[2]);
  CHECK_EQ(-1, output[3]);
}



TEST(MacroAssemblernativeAtStart) {
  v8::V8::Initialize();
  ContextInitializer initializer;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);

  ArchRegExpMacroAssembler m(isolate, &zone, NativeRegExpMacroAssembler::LATIN1,
                             0);

  Label not_at_start, newline, fail;
  m.CheckNotAtStart(0, &not_at_start);
  // Check that prevchar = '\n' and current = 'f'.
  m.CheckCharacter('\n', &newline);
  m.BindJumpTarget(&fail);
  m.Fail();
  m.Bind(&newline);
  m.LoadCurrentCharacter(0, &fail);
  m.CheckNotCharacter('f', &fail);
  m.Succeed();

  m.Bind(&not_at_start);
  // Check that prevchar = 'o' and current = 'b'.
  Label prevo;
  m.CheckCharacter('o', &prevo);
  m.Fail();
  m.Bind(&prevo);
  m.LoadCurrentCharacter(0, &fail);
  m.CheckNotCharacter('b', &fail);
  m.Succeed();

  Handle<String> source = factory->NewStringFromStaticChars("(^f|ob)");
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);
  Handle<JSRegExp> regexp = CreateJSRegExp(source, code);

  Handle<String> input = factory->NewStringFromStaticChars("foobar");
  Handle<SeqOneByteString> seq_input = Handle<SeqOneByteString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  NativeRegExpMacroAssembler::Result result = Execute(
      *regexp, *input, 0, start_adr, start_adr + input->length(), nullptr);

  CHECK_EQ(NativeRegExpMacroAssembler::SUCCESS, result);

  result = Execute(*regexp, *input, 3, start_adr + 3,
                   start_adr + input->length(), nullptr);

  CHECK_EQ(NativeRegExpMacroAssembler::SUCCESS, result);
}


TEST(MacroAssemblerNativeBackRefNoCase) {
  v8::V8::Initialize();
  ContextInitializer initializer;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);

  ArchRegExpMacroAssembler m(isolate, &zone, NativeRegExpMacroAssembler::LATIN1,
                             4);

  Label fail, succ;

  m.WriteCurrentPositionToRegister(0, 0);
  m.WriteCurrentPositionToRegister(2, 0);
  m.AdvanceCurrentPosition(3);
  m.WriteCurrentPositionToRegister(3, 0);
  m.CheckNotBackReferenceIgnoreCase(2, false, &fail);  // Match "AbC".
  m.CheckNotBackReferenceIgnoreCase(2, false, &fail);  // Match "ABC".
  Label expected_fail;
  m.CheckNotBackReferenceIgnoreCase(2, false, &expected_fail);
  m.BindJumpTarget(&fail);
  m.Fail();

  m.Bind(&expected_fail);
  m.AdvanceCurrentPosition(3);  // Skip "xYz"
  m.CheckNotBackReferenceIgnoreCase(2, false, &succ);
  m.Fail();

  m.Bind(&succ);
  m.WriteCurrentPositionToRegister(1, 0);
  m.Succeed();

  Handle<String> source =
      factory->NewStringFromStaticChars("^(abc)\1\1(?!\1)...(?!\1)");
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);
  Handle<JSRegExp> regexp = CreateJSRegExp(source, code);

  Handle<String> input = factory->NewStringFromStaticChars("aBcAbCABCxYzab");
  Handle<SeqOneByteString> seq_input = Handle<SeqOneByteString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  int output[4];
  NativeRegExpMacroAssembler::Result result = Execute(
      *regexp, *input, 0, start_adr, start_adr + input->length(), output);

  CHECK_EQ(NativeRegExpMacroAssembler::SUCCESS, result);
  CHECK_EQ(0, output[0]);
  CHECK_EQ(12, output[1]);
  CHECK_EQ(0, output[2]);
  CHECK_EQ(3, output[3]);
}



TEST(MacroAssemblerNativeRegisters) {
  v8::V8::Initialize();
  ContextInitializer initializer;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);

  ArchRegExpMacroAssembler m(isolate, &zone, NativeRegExpMacroAssembler::LATIN1,
                             6);

  uc16 foo_chars[3] = {'f', 'o', 'o'};
  Vector<const uc16> foo(foo_chars, 3);

  enum registers { out1, out2, out3, out4, out5, out6, sp, loop_cnt };
  Label fail;
  Label backtrack;
  m.WriteCurrentPositionToRegister(out1, 0);  // Output: [0]
  m.PushRegister(out1, RegExpMacroAssembler::kNoStackLimitCheck);
  m.PushBacktrack(&backtrack);
  m.WriteStackPointerToRegister(sp);
  // Fill stack and registers
  m.AdvanceCurrentPosition(2);
  m.WriteCurrentPositionToRegister(out1, 0);
  m.PushRegister(out1, RegExpMacroAssembler::kNoStackLimitCheck);
  m.PushBacktrack(&fail);
  // Drop backtrack stack frames.
  m.ReadStackPointerFromRegister(sp);
  // And take the first backtrack (to &backtrack)
  m.Backtrack();

  m.PushCurrentPosition();
  m.AdvanceCurrentPosition(2);
  m.PopCurrentPosition();

  m.BindJumpTarget(&backtrack);
  m.PopRegister(out1);
  m.ReadCurrentPositionFromRegister(out1);
  m.AdvanceCurrentPosition(3);
  m.WriteCurrentPositionToRegister(out2, 0);  // [0,3]

  Label loop;
  m.SetRegister(loop_cnt, 0);  // loop counter
  m.Bind(&loop);
  m.AdvanceRegister(loop_cnt, 1);
  m.AdvanceCurrentPosition(1);
  m.IfRegisterLT(loop_cnt, 3, &loop);
  m.WriteCurrentPositionToRegister(out3, 0);  // [0,3,6]

  Label loop2;
  m.SetRegister(loop_cnt, 2);  // loop counter
  m.Bind(&loop2);
  m.AdvanceRegister(loop_cnt, -1);
  m.AdvanceCurrentPosition(1);
  m.IfRegisterGE(loop_cnt, 0, &loop2);
  m.WriteCurrentPositionToRegister(out4, 0);  // [0,3,6,9]

  Label loop3;
  Label exit_loop3;
  m.PushRegister(out4, RegExpMacroAssembler::kNoStackLimitCheck);
  m.PushRegister(out4, RegExpMacroAssembler::kNoStackLimitCheck);
  m.ReadCurrentPositionFromRegister(out3);
  m.Bind(&loop3);
  m.AdvanceCurrentPosition(1);
  m.CheckGreedyLoop(&exit_loop3);
  m.GoTo(&loop3);
  m.Bind(&exit_loop3);
  m.PopCurrentPosition();
  m.WriteCurrentPositionToRegister(out5, 0);  // [0,3,6,9,9,-1]

  m.Succeed();

  m.BindJumpTarget(&fail);
  m.Fail();

  Handle<String> source = factory->NewStringFromStaticChars("<loop test>");
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);
  Handle<JSRegExp> regexp = CreateJSRegExp(source, code);

  // String long enough for test (content doesn't matter).
  Handle<String> input = factory->NewStringFromStaticChars("foofoofoofoofoo");
  Handle<SeqOneByteString> seq_input = Handle<SeqOneByteString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  int output[6];
  NativeRegExpMacroAssembler::Result result = Execute(
      *regexp, *input, 0, start_adr, start_adr + input->length(), output);

  CHECK_EQ(NativeRegExpMacroAssembler::SUCCESS, result);
  CHECK_EQ(0, output[0]);
  CHECK_EQ(3, output[1]);
  CHECK_EQ(6, output[2]);
  CHECK_EQ(9, output[3]);
  CHECK_EQ(9, output[4]);
  CHECK_EQ(-1, output[5]);
}


TEST(MacroAssemblerStackOverflow) {
  v8::V8::Initialize();
  ContextInitializer initializer;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);

  ArchRegExpMacroAssembler m(isolate, &zone, NativeRegExpMacroAssembler::LATIN1,
                             0);

  Label loop;
  m.Bind(&loop);
  m.PushBacktrack(&loop);
  m.GoTo(&loop);

  Handle<String> source =
      factory->NewStringFromStaticChars("<stack overflow test>");
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);
  Handle<JSRegExp> regexp = CreateJSRegExp(source, code);

  // String long enough for test (content doesn't matter).
  Handle<String> input = factory->NewStringFromStaticChars("dummy");
  Handle<SeqOneByteString> seq_input = Handle<SeqOneByteString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  NativeRegExpMacroAssembler::Result result = Execute(
      *regexp, *input, 0, start_adr, start_adr + input->length(), nullptr);

  CHECK_EQ(NativeRegExpMacroAssembler::EXCEPTION, result);
  CHECK(isolate->has_pending_exception());
  isolate->clear_pending_exception();
}


TEST(MacroAssemblerNativeLotsOfRegisters) {
  v8::V8::Initialize();
  ContextInitializer initializer;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);

  ArchRegExpMacroAssembler m(isolate, &zone, NativeRegExpMacroAssembler::LATIN1,
                             2);

  // At least 2048, to ensure the allocated space for registers
  // span one full page.
  const int large_number = 8000;
  m.WriteCurrentPositionToRegister(large_number, 42);
  m.WriteCurrentPositionToRegister(0, 0);
  m.WriteCurrentPositionToRegister(1, 1);
  Label done;
  m.CheckNotBackReference(0, false, &done);  // Performs a system-stack push.
  m.Bind(&done);
  m.PushRegister(large_number, RegExpMacroAssembler::kNoStackLimitCheck);
  m.PopRegister(1);
  m.Succeed();

  Handle<String> source =
      factory->NewStringFromStaticChars("<huge register space test>");
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);
  Handle<JSRegExp> regexp = CreateJSRegExp(source, code);

  // String long enough for test (content doesn't matter).
  Handle<String> input = factory->NewStringFromStaticChars("sample text");
  Handle<SeqOneByteString> seq_input = Handle<SeqOneByteString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  int captures[2];
  NativeRegExpMacroAssembler::Result result = Execute(
      *regexp, *input, 0, start_adr, start_adr + input->length(), captures);

  CHECK_EQ(NativeRegExpMacroAssembler::SUCCESS, result);
  CHECK_EQ(0, captures[0]);
  CHECK_EQ(42, captures[1]);

  isolate->clear_pending_exception();
}

TEST(MacroAssembler) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  RegExpBytecodeGenerator m(CcTest::i_isolate(), &zone);
  // ^f(o)o.
  Label start, fail, backtrack;

  m.SetRegister(4, 42);
  m.PushRegister(4, RegExpMacroAssembler::kNoStackLimitCheck);
  m.AdvanceRegister(4, 42);
  m.GoTo(&start);
  m.Fail();
  m.Bind(&start);
  m.PushBacktrack(&fail);
  m.CheckNotAtStart(0, nullptr);
  m.LoadCurrentCharacter(0, nullptr);
  m.CheckNotCharacter('f', nullptr);
  m.LoadCurrentCharacter(1, nullptr);
  m.CheckNotCharacter('o', nullptr);
  m.LoadCurrentCharacter(2, nullptr);
  m.CheckNotCharacter('o', nullptr);
  m.WriteCurrentPositionToRegister(0, 0);
  m.WriteCurrentPositionToRegister(1, 3);
  m.WriteCurrentPositionToRegister(2, 1);
  m.WriteCurrentPositionToRegister(3, 2);
  m.AdvanceCurrentPosition(3);
  m.PushBacktrack(&backtrack);
  m.Succeed();
  m.BindJumpTarget(&backtrack);
  m.ClearRegisters(2, 3);
  m.Backtrack();
  m.BindJumpTarget(&fail);
  m.PopRegister(0);
  m.Fail();

  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  Handle<String> source = factory->NewStringFromStaticChars("^f(o)o");
  Handle<ByteArray> array = Handle<ByteArray>::cast(m.GetCode(source));
  int captures[5];

  const uc16 str1[] = {'f', 'o', 'o', 'b', 'a', 'r'};
  Handle<String> f1_16 = factory->NewStringFromTwoByte(
      Vector<const uc16>(str1, 6)).ToHandleChecked();

  CHECK(IrregexpInterpreter::MatchInternal(isolate, *array, *f1_16, captures, 5,
                                           0, RegExp::CallOrigin::kFromRuntime,
                                           JSRegExp::kNoBacktrackLimit));
  CHECK_EQ(0, captures[0]);
  CHECK_EQ(3, captures[1]);
  CHECK_EQ(1, captures[2]);
  CHECK_EQ(2, captures[3]);
  CHECK_EQ(84, captures[4]);

  const uc16 str2[] = {'b', 'a', 'r', 'f', 'o', 'o'};
  Handle<String> f2_16 = factory->NewStringFromTwoByte(
      Vector<const uc16>(str2, 6)).ToHandleChecked();

  CHECK(!IrregexpInterpreter::MatchInternal(
      isolate, *array, *f2_16, captures, 5, 0, RegExp::CallOrigin::kFromRuntime,
      JSRegExp::kNoBacktrackLimit));
  CHECK_EQ(42, captures[0]);
}

#ifndef V8_INTL_SUPPORT
static uc32 canonicalize(uc32 c) {
  unibrow::uchar canon[unibrow::Ecma262Canonicalize::kMaxWidth];
  int count = unibrow::Ecma262Canonicalize::Convert(c, '\0', canon, nullptr);
  if (count == 0) {
    return c;
  } else {
    CHECK_EQ(1, count);
    return canon[0];
  }
}

TEST(LatinCanonicalize) {
  unibrow::Mapping<unibrow::Ecma262UnCanonicalize> un_canonicalize;
  for (unibrow::uchar lower = 'a'; lower <= 'z'; lower++) {
    unibrow::uchar upper = lower + ('A' - 'a');
    CHECK_EQ(canonicalize(lower), canonicalize(upper));
    unibrow::uchar uncanon[unibrow::Ecma262UnCanonicalize::kMaxWidth];
    int length = un_canonicalize.get(lower, '\0', uncanon);
    CHECK_EQ(2, length);
    CHECK_EQ(upper, uncanon[0]);
    CHECK_EQ(lower, uncanon[1]);
  }
  for (uc32 c = 128; c < (1 << 21); c++)
    CHECK_GE(canonicalize(c), 128);
  unibrow::Mapping<unibrow::ToUppercase> to_upper;
  // Canonicalization is only defined for the Basic Multilingual Plane.
  for (uc32 c = 0; c < (1 << 16); c++) {
    unibrow::uchar upper[unibrow::ToUppercase::kMaxWidth];
    int length = to_upper.get(c, '\0', upper);
    if (length == 0) {
      length = 1;
      upper[0] = c;
    }
    uc32 u = upper[0];
    if (length > 1 || (c >= 128 && u < 128))
      u = c;
    CHECK_EQ(u, canonicalize(c));
  }
}

static uc32 CanonRangeEnd(uc32 c) {
  unibrow::uchar canon[unibrow::CanonicalizationRange::kMaxWidth];
  int count = unibrow::CanonicalizationRange::Convert(c, '\0', canon, nullptr);
  if (count == 0) {
    return c;
  } else {
    CHECK_EQ(1, count);
    return canon[0];
  }
}


TEST(RangeCanonicalization) {
  // Check that we arrive at the same result when using the basic
  // range canonicalization primitives as when using immediate
  // canonicalization.
  unibrow::Mapping<unibrow::Ecma262UnCanonicalize> un_canonicalize;
  int block_start = 0;
  while (block_start <= 0xFFFF) {
    uc32 block_end = CanonRangeEnd(block_start);
    unsigned block_length = block_end - block_start + 1;
    if (block_length > 1) {
      unibrow::uchar first[unibrow::Ecma262UnCanonicalize::kMaxWidth];
      int first_length = un_canonicalize.get(block_start, '\0', first);
      for (unsigned i = 1; i < block_length; i++) {
        unibrow::uchar succ[unibrow::Ecma262UnCanonicalize::kMaxWidth];
        int succ_length = un_canonicalize.get(block_start + i, '\0', succ);
        CHECK_EQ(first_length, succ_length);
        for (int j = 0; j < succ_length; j++) {
          int calc = first[j] + i;
          int found = succ[j];
          CHECK_EQ(calc, found);
        }
      }
    }
    block_start = block_start + block_length;
  }
}


TEST(UncanonicalizeEquivalence) {
  unibrow::Mapping<unibrow::Ecma262UnCanonicalize> un_canonicalize;
  unibrow::uchar chars[unibrow::Ecma262UnCanonicalize::kMaxWidth];
  for (int i = 0; i < (1 << 16); i++) {
    int length = un_canonicalize.get(i, '\0', chars);
    for (int j = 0; j < length; j++) {
      unibrow::uchar chars2[unibrow::Ecma262UnCanonicalize::kMaxWidth];
      int length2 = un_canonicalize.get(chars[j], '\0', chars2);
      CHECK_EQ(length, length2);
      for (int k = 0; k < length; k++)
        CHECK_EQ(static_cast<int>(chars[k]), static_cast<int>(chars2[k]));
    }
  }
}

#endif

static void TestRangeCaseIndependence(Isolate* isolate, CharacterRange input,
                                      Vector<CharacterRange> expected) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  int count = expected.length();
  ZoneList<CharacterRange>* list =
      new(&zone) ZoneList<CharacterRange>(count, &zone);
  list->Add(input, &zone);
  CharacterRange::AddCaseEquivalents(isolate, &zone, list, false);
  list->Remove(0);  // Remove the input before checking results.
  CHECK_EQ(count, list->length());
  for (int i = 0; i < list->length(); i++) {
    CHECK_EQ(expected[i].from(), list->at(i).from());
    CHECK_EQ(expected[i].to(), list->at(i).to());
  }
}


static void TestSimpleRangeCaseIndependence(Isolate* isolate,
                                            CharacterRange input,
                                            CharacterRange expected) {
  EmbeddedVector<CharacterRange, 1> vector;
  vector[0] = expected;
  TestRangeCaseIndependence(isolate, input, vector);
}


TEST(CharacterRangeCaseIndependence) {
  Isolate* isolate = CcTest::i_isolate();
  TestSimpleRangeCaseIndependence(isolate, CharacterRange::Singleton('a'),
                                  CharacterRange::Singleton('A'));
  TestSimpleRangeCaseIndependence(isolate, CharacterRange::Singleton('z'),
                                  CharacterRange::Singleton('Z'));
#ifndef V8_INTL_SUPPORT
  TestSimpleRangeCaseIndependence(isolate, CharacterRange::Range('a', 'z'),
                                  CharacterRange::Range('A', 'Z'));
#endif  // !V8_INTL_SUPPORT
  TestSimpleRangeCaseIndependence(isolate, CharacterRange::Range('c', 'f'),
                                  CharacterRange::Range('C', 'F'));
  TestSimpleRangeCaseIndependence(isolate, CharacterRange::Range('a', 'b'),
                                  CharacterRange::Range('A', 'B'));
  TestSimpleRangeCaseIndependence(isolate, CharacterRange::Range('y', 'z'),
                                  CharacterRange::Range('Y', 'Z'));
#ifndef V8_INTL_SUPPORT
  TestSimpleRangeCaseIndependence(isolate,
                                  CharacterRange::Range('a' - 1, 'z' + 1),
                                  CharacterRange::Range('A', 'Z'));
  TestSimpleRangeCaseIndependence(isolate, CharacterRange::Range('A', 'Z'),
                                  CharacterRange::Range('a', 'z'));
#endif  // !V8_INTL_SUPPORT
  TestSimpleRangeCaseIndependence(isolate, CharacterRange::Range('C', 'F'),
                                  CharacterRange::Range('c', 'f'));
#ifndef V8_INTL_SUPPORT
  TestSimpleRangeCaseIndependence(isolate,
                                  CharacterRange::Range('A' - 1, 'Z' + 1),
                                  CharacterRange::Range('a', 'z'));
  // Here we need to add [l-z] to complete the case independence of
  // [A-Za-z] but we expect [a-z] to be added since we always add a
  // whole block at a time.
  TestSimpleRangeCaseIndependence(isolate, CharacterRange::Range('A', 'k'),
                                  CharacterRange::Range('a', 'z'));
#endif  // !V8_INTL_SUPPORT
}

static bool InClass(uc32 c,
                    const UnicodeRangeSplitter::CharacterRangeVector* ranges) {
  if (ranges == nullptr) return false;
  for (size_t i = 0; i < ranges->size(); i++) {
    CharacterRange range = ranges->at(i);
    if (range.from() <= c && c <= range.to())
      return true;
  }
  return false;
}

TEST(UnicodeRangeSplitter) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  ZoneList<CharacterRange>* base =
      new(&zone) ZoneList<CharacterRange>(1, &zone);
  base->Add(CharacterRange::Everything(), &zone);
  UnicodeRangeSplitter splitter(base);
  // BMP
  for (uc32 c = 0; c < 0xD800; c++) {
    CHECK(InClass(c, splitter.bmp()));
    CHECK(!InClass(c, splitter.lead_surrogates()));
    CHECK(!InClass(c, splitter.trail_surrogates()));
    CHECK(!InClass(c, splitter.non_bmp()));
  }
  // Lead surrogates
  for (uc32 c = 0xD800; c < 0xDBFF; c++) {
    CHECK(!InClass(c, splitter.bmp()));
    CHECK(InClass(c, splitter.lead_surrogates()));
    CHECK(!InClass(c, splitter.trail_surrogates()));
    CHECK(!InClass(c, splitter.non_bmp()));
  }
  // Trail surrogates
  for (uc32 c = 0xDC00; c < 0xDFFF; c++) {
    CHECK(!InClass(c, splitter.bmp()));
    CHECK(!InClass(c, splitter.lead_surrogates()));
    CHECK(InClass(c, splitter.trail_surrogates()));
    CHECK(!InClass(c, splitter.non_bmp()));
  }
  // BMP
  for (uc32 c = 0xE000; c < 0xFFFF; c++) {
    CHECK(InClass(c, splitter.bmp()));
    CHECK(!InClass(c, splitter.lead_surrogates()));
    CHECK(!InClass(c, splitter.trail_surrogates()));
    CHECK(!InClass(c, splitter.non_bmp()));
  }
  // Non-BMP
  for (uc32 c = 0x10000; c < 0x10FFFF; c++) {
    CHECK(!InClass(c, splitter.bmp()));
    CHECK(!InClass(c, splitter.lead_surrogates()));
    CHECK(!InClass(c, splitter.trail_surrogates()));
    CHECK(InClass(c, splitter.non_bmp()));
  }
}


TEST(CanonicalizeCharacterSets) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  ZoneList<CharacterRange>* list =
      new(&zone) ZoneList<CharacterRange>(4, &zone);
  CharacterSet set(list);

  list->Add(CharacterRange::Range(10, 20), &zone);
  list->Add(CharacterRange::Range(30, 40), &zone);
  list->Add(CharacterRange::Range(50, 60), &zone);
  set.Canonicalize();
  CHECK_EQ(3, list->length());
  CHECK_EQ(10, list->at(0).from());
  CHECK_EQ(20, list->at(0).to());
  CHECK_EQ(30, list->at(1).from());
  CHECK_EQ(40, list->at(1).to());
  CHECK_EQ(50, list->at(2).from());
  CHECK_EQ(60, list->at(2).to());

  list->Rewind(0);
  list->Add(CharacterRange::Range(10, 20), &zone);
  list->Add(CharacterRange::Range(50, 60), &zone);
  list->Add(CharacterRange::Range(30, 40), &zone);
  set.Canonicalize();
  CHECK_EQ(3, list->length());
  CHECK_EQ(10, list->at(0).from());
  CHECK_EQ(20, list->at(0).to());
  CHECK_EQ(30, list->at(1).from());
  CHECK_EQ(40, list->at(1).to());
  CHECK_EQ(50, list->at(2).from());
  CHECK_EQ(60, list->at(2).to());

  list->Rewind(0);
  list->Add(CharacterRange::Range(30, 40), &zone);
  list->Add(CharacterRange::Range(10, 20), &zone);
  list->Add(CharacterRange::Range(25, 25), &zone);
  list->Add(CharacterRange::Range(100, 100), &zone);
  list->Add(CharacterRange::Range(1, 1), &zone);
  set.Canonicalize();
  CHECK_EQ(5, list->length());
  CHECK_EQ(1, list->at(0).from());
  CHECK_EQ(1, list->at(0).to());
  CHECK_EQ(10, list->at(1).from());
  CHECK_EQ(20, list->at(1).to());
  CHECK_EQ(25, list->at(2).from());
  CHECK_EQ(25, list->at(2).to());
  CHECK_EQ(30, list->at(3).from());
  CHECK_EQ(40, list->at(3).to());
  CHECK_EQ(100, list->at(4).from());
  CHECK_EQ(100, list->at(4).to());

  list->Rewind(0);
  list->Add(CharacterRange::Range(10, 19), &zone);
  list->Add(CharacterRange::Range(21, 30), &zone);
  list->Add(CharacterRange::Range(20, 20), &zone);
  set.Canonicalize();
  CHECK_EQ(1, list->length());
  CHECK_EQ(10, list->at(0).from());
  CHECK_EQ(30, list->at(0).to());
}


TEST(CharacterRangeMerge) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  ZoneList<CharacterRange> l1(4, &zone);
  ZoneList<CharacterRange> l2(4, &zone);
  // Create all combinations of intersections of ranges, both singletons and
  // longer.

  int offset = 0;

  // The five kinds of singleton intersections:
  //     X
  //   Y      - outside before
  //    Y     - outside touching start
  //     Y    - overlap
  //      Y   - outside touching end
  //       Y  - outside after

  for (int i = 0; i < 5; i++) {
    l1.Add(CharacterRange::Singleton(offset + 2), &zone);
    l2.Add(CharacterRange::Singleton(offset + i), &zone);
    offset += 6;
  }

  // The seven kinds of singleton/non-singleton intersections:
  //    XXX
  //  Y        - outside before
  //   Y       - outside touching start
  //    Y      - inside touching start
  //     Y     - entirely inside
  //      Y    - inside touching end
  //       Y   - outside touching end
  //        Y  - disjoint after

  for (int i = 0; i < 7; i++) {
    l1.Add(CharacterRange::Range(offset + 2, offset + 4), &zone);
    l2.Add(CharacterRange::Singleton(offset + i), &zone);
    offset += 8;
  }

  // The eleven kinds of non-singleton intersections:
  //
  //       XXXXXXXX
  // YYYY                  - outside before.
  //   YYYY                - outside touching start.
  //     YYYY              - overlapping start
  //       YYYY            - inside touching start
  //         YYYY          - entirely inside
  //           YYYY        - inside touching end
  //             YYYY      - overlapping end
  //               YYYY    - outside touching end
  //                 YYYY  - outside after
  //       YYYYYYYY        - identical
  //     YYYYYYYYYYYY      - containing entirely.

  for (int i = 0; i < 9; i++) {
    l1.Add(CharacterRange::Range(offset + 6, offset + 15), &zone);  // Length 8.
    l2.Add(CharacterRange::Range(offset + 2 * i, offset + 2 * i + 3), &zone);
    offset += 22;
  }
  l1.Add(CharacterRange::Range(offset + 6, offset + 15), &zone);
  l2.Add(CharacterRange::Range(offset + 6, offset + 15), &zone);
  offset += 22;
  l1.Add(CharacterRange::Range(offset + 6, offset + 15), &zone);
  l2.Add(CharacterRange::Range(offset + 4, offset + 17), &zone);
  offset += 22;

  // Different kinds of multi-range overlap:
  // XXXXXXXXXXXXXXXXXXXXXX         XXXXXXXXXXXXXXXXXXXXXX
  //   YYYY  Y  YYYY  Y  YYYY  Y  YYYY  Y  YYYY  Y  YYYY  Y

  l1.Add(CharacterRange::Range(offset, offset + 21), &zone);
  l1.Add(CharacterRange::Range(offset + 31, offset + 52), &zone);
  for (int i = 0; i < 6; i++) {
    l2.Add(CharacterRange::Range(offset + 2, offset + 5), &zone);
    l2.Add(CharacterRange::Singleton(offset + 8), &zone);
    offset += 9;
  }

  CHECK(CharacterRange::IsCanonical(&l1));
  CHECK(CharacterRange::IsCanonical(&l2));

  ZoneList<CharacterRange> first_only(4, &zone);
  ZoneList<CharacterRange> second_only(4, &zone);
  ZoneList<CharacterRange> both(4, &zone);
}


TEST(Graph) {
  Execute("\\b\\w+\\b", false, true, true);
}


namespace {

int* global_use_counts = nullptr;

void MockUseCounterCallback(v8::Isolate* isolate,
                            v8::Isolate::UseCounterFeature feature) {
  ++global_use_counts[feature];
}
}

// Test that ES2015+ RegExp compatibility fixes are in place, that they
// are not overly broad, and the appropriate UseCounters are incremented
TEST(UseCountRegExp) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  int use_counts[v8::Isolate::kUseCounterFeatureCount] = {};
  global_use_counts = use_counts;
  CcTest::isolate()->SetUseCounterCallback(MockUseCounterCallback);

  // Compat fix: RegExp.prototype.sticky == undefined; UseCounter tracks it
  v8::Local<v8::Value> resultSticky = CompileRun("RegExp.prototype.sticky");
  CHECK_EQ(1, use_counts[v8::Isolate::kRegExpPrototypeStickyGetter]);
  CHECK_EQ(0, use_counts[v8::Isolate::kRegExpPrototypeToString]);
  CHECK(resultSticky->IsUndefined());

  // re.sticky has approriate value and doesn't touch UseCounter
  v8::Local<v8::Value> resultReSticky = CompileRun("/a/.sticky");
  CHECK_EQ(1, use_counts[v8::Isolate::kRegExpPrototypeStickyGetter]);
  CHECK_EQ(0, use_counts[v8::Isolate::kRegExpPrototypeToString]);
  CHECK(resultReSticky->IsFalse());

  // When the getter is called on another object, throw an exception
  // and don't increment the UseCounter
  v8::Local<v8::Value> resultStickyError = CompileRun(
      "var exception;"
      "try { "
      "  Object.getOwnPropertyDescriptor(RegExp.prototype, 'sticky')"
      "      .get.call(null);"
      "} catch (e) {"
      "  exception = e;"
      "}"
      "exception");
  CHECK_EQ(1, use_counts[v8::Isolate::kRegExpPrototypeStickyGetter]);
  CHECK_EQ(0, use_counts[v8::Isolate::kRegExpPrototypeToString]);
  CHECK(resultStickyError->IsObject());

  // RegExp.prototype.toString() returns '/(?:)/' as a compatibility fix;
  // a UseCounter is incremented to track it.
  v8::Local<v8::Value> resultToString =
      CompileRun("RegExp.prototype.toString().length");
  CHECK_EQ(2, use_counts[v8::Isolate::kRegExpPrototypeStickyGetter]);
  CHECK_EQ(1, use_counts[v8::Isolate::kRegExpPrototypeToString]);
  CHECK(resultToString->IsInt32());
  CHECK_EQ(6,
           resultToString->Int32Value(isolate->GetCurrentContext()).FromJust());

  // .toString() works on normal RegExps
  v8::Local<v8::Value> resultReToString = CompileRun("/a/.toString().length");
  CHECK_EQ(2, use_counts[v8::Isolate::kRegExpPrototypeStickyGetter]);
  CHECK_EQ(1, use_counts[v8::Isolate::kRegExpPrototypeToString]);
  CHECK(resultReToString->IsInt32());
  CHECK_EQ(
      3, resultReToString->Int32Value(isolate->GetCurrentContext()).FromJust());

  // .toString() throws on non-RegExps that aren't RegExp.prototype
  v8::Local<v8::Value> resultToStringError = CompileRun(
      "var exception;"
      "try { RegExp.prototype.toString.call(null) }"
      "catch (e) { exception = e; }"
      "exception");
  CHECK_EQ(2, use_counts[v8::Isolate::kRegExpPrototypeStickyGetter]);
  CHECK_EQ(1, use_counts[v8::Isolate::kRegExpPrototypeToString]);
  CHECK(resultToStringError->IsObject());
}

class UncachedExternalString
    : public v8::String::ExternalOneByteStringResource {
 public:
  const char* data() const override { return "abcdefghijklmnopqrstuvwxyz"; }
  size_t length() const override { return 26; }
  bool IsCacheable() const override { return false; }
};

TEST(UncachedExternalString) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  v8::Local<v8::String> external =
      v8::String::NewExternalOneByte(isolate, new UncachedExternalString())
          .ToLocalChecked();
  CHECK(v8::Utils::OpenHandle(*external)->map() ==
        ReadOnlyRoots(CcTest::i_isolate())
            .uncached_external_one_byte_string_map());
  v8::Local<v8::Object> global = env->Global();
  global->Set(env.local(), v8_str("external"), external).FromJust();
  CompileRun("var re = /y(.)/; re.test('ab');");
  ExpectString("external.substring(1).match(re)[1]", "z");
}

// Test bytecode peephole optimization

void CreatePeepholeNoChangeBytecode(RegExpMacroAssembler* m) {
  Label fail, backtrack;
  m->PushBacktrack(&fail);
  m->CheckNotAtStart(0, nullptr);
  m->LoadCurrentCharacter(2, nullptr);
  m->CheckNotCharacter('o', nullptr);
  m->LoadCurrentCharacter(1, nullptr, false);
  m->CheckNotCharacter('o', nullptr);
  m->LoadCurrentCharacter(0, nullptr, false);
  m->CheckNotCharacter('f', nullptr);
  m->WriteCurrentPositionToRegister(0, 0);
  m->WriteCurrentPositionToRegister(1, 3);
  m->AdvanceCurrentPosition(3);
  m->PushBacktrack(&backtrack);
  m->Succeed();
  m->Bind(&backtrack);
  m->Backtrack();
  m->Bind(&fail);
  m->Fail();
}

TEST(PeepholeNoChange) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  RegExpBytecodeGenerator orig(CcTest::i_isolate(), &zone);
  RegExpBytecodeGenerator opt(CcTest::i_isolate(), &zone);

  CreatePeepholeNoChangeBytecode(&orig);
  CreatePeepholeNoChangeBytecode(&opt);

  Handle<String> source = factory->NewStringFromStaticChars("^foo");

  i::FLAG_regexp_peephole_optimization = false;
  Handle<ByteArray> array = Handle<ByteArray>::cast(orig.GetCode(source));
  int length = array->length();
  byte* byte_array = array->GetDataStartAddress();

  i::FLAG_regexp_peephole_optimization = true;
  Handle<ByteArray> array_optimized =
      Handle<ByteArray>::cast(opt.GetCode(source));
  byte* byte_array_optimized = array_optimized->GetDataStartAddress();

  CHECK_EQ(0, memcmp(byte_array, byte_array_optimized, length));
}

void CreatePeepholeSkipUntilCharBytecode(RegExpMacroAssembler* m) {
  Label start;
  m->Bind(&start);
  m->LoadCurrentCharacter(0, nullptr, true);
  m->CheckCharacter('x', nullptr);
  m->AdvanceCurrentPosition(1);
  m->GoTo(&start);
}

TEST(PeepholeSkipUntilChar) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  RegExpBytecodeGenerator orig(CcTest::i_isolate(), &zone);
  RegExpBytecodeGenerator opt(CcTest::i_isolate(), &zone);

  CreatePeepholeSkipUntilCharBytecode(&orig);
  CreatePeepholeSkipUntilCharBytecode(&opt);

  Handle<String> source = factory->NewStringFromStaticChars("dummy");

  i::FLAG_regexp_peephole_optimization = false;
  Handle<ByteArray> array = Handle<ByteArray>::cast(orig.GetCode(source));
  int length = array->length();

  i::FLAG_regexp_peephole_optimization = true;
  Handle<ByteArray> array_optimized =
      Handle<ByteArray>::cast(opt.GetCode(source));
  int length_optimized = array_optimized->length();

  int length_expected = RegExpBytecodeLength(BC_LOAD_CURRENT_CHAR) +
                        RegExpBytecodeLength(BC_CHECK_CHAR) +
                        RegExpBytecodeLength(BC_ADVANCE_CP_AND_GOTO) +
                        RegExpBytecodeLength(BC_POP_BT);
  int length_optimized_expected = RegExpBytecodeLength(BC_SKIP_UNTIL_CHAR) +
                                  RegExpBytecodeLength(BC_POP_BT);

  CHECK_EQ(length, length_expected);
  CHECK_EQ(length_optimized, length_optimized_expected);

  CHECK_EQ(BC_SKIP_UNTIL_CHAR, array_optimized->get(0));
  CHECK_EQ(BC_POP_BT,
           array_optimized->get(RegExpBytecodeLength(BC_SKIP_UNTIL_CHAR)));
}

void CreatePeepholeSkipUntilBitInTableBytecode(RegExpMacroAssembler* m,
                                               Factory* factory) {
  Handle<ByteArray> bit_table = factory->NewByteArray(
      RegExpMacroAssembler::kTableSize, AllocationType::kOld);
  for (uint32_t i = 0; i < RegExpMacroAssembler::kTableSize; i++) {
    bit_table->set(i, 0);
  }

  Label start;
  m->Bind(&start);
  m->LoadCurrentCharacter(0, nullptr, true);
  m->CheckBitInTable(bit_table, nullptr);
  m->AdvanceCurrentPosition(1);
  m->GoTo(&start);
}

TEST(PeepholeSkipUntilBitInTable) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  RegExpBytecodeGenerator orig(CcTest::i_isolate(), &zone);
  RegExpBytecodeGenerator opt(CcTest::i_isolate(), &zone);

  CreatePeepholeSkipUntilBitInTableBytecode(&orig, factory);
  CreatePeepholeSkipUntilBitInTableBytecode(&opt, factory);

  Handle<String> source = factory->NewStringFromStaticChars("dummy");

  i::FLAG_regexp_peephole_optimization = false;
  Handle<ByteArray> array = Handle<ByteArray>::cast(orig.GetCode(source));
  int length = array->length();

  i::FLAG_regexp_peephole_optimization = true;
  Handle<ByteArray> array_optimized =
      Handle<ByteArray>::cast(opt.GetCode(source));
  int length_optimized = array_optimized->length();

  int length_expected = RegExpBytecodeLength(BC_LOAD_CURRENT_CHAR) +
                        RegExpBytecodeLength(BC_CHECK_BIT_IN_TABLE) +
                        RegExpBytecodeLength(BC_ADVANCE_CP_AND_GOTO) +
                        RegExpBytecodeLength(BC_POP_BT);
  int length_optimized_expected =
      RegExpBytecodeLength(BC_SKIP_UNTIL_BIT_IN_TABLE) +
      RegExpBytecodeLength(BC_POP_BT);

  CHECK_EQ(length, length_expected);
  CHECK_EQ(length_optimized, length_optimized_expected);

  CHECK_EQ(BC_SKIP_UNTIL_BIT_IN_TABLE, array_optimized->get(0));
  CHECK_EQ(BC_POP_BT, array_optimized->get(
                          RegExpBytecodeLength(BC_SKIP_UNTIL_BIT_IN_TABLE)));
}

void CreatePeepholeSkipUntilCharPosCheckedBytecode(RegExpMacroAssembler* m) {
  Label start;
  m->Bind(&start);
  m->LoadCurrentCharacter(0, nullptr, true, 1, 2);
  m->CheckCharacter('x', nullptr);
  m->AdvanceCurrentPosition(1);
  m->GoTo(&start);
}

TEST(PeepholeSkipUntilCharPosChecked) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  RegExpBytecodeGenerator orig(CcTest::i_isolate(), &zone);
  RegExpBytecodeGenerator opt(CcTest::i_isolate(), &zone);

  CreatePeepholeSkipUntilCharPosCheckedBytecode(&orig);
  CreatePeepholeSkipUntilCharPosCheckedBytecode(&opt);

  Handle<String> source = factory->NewStringFromStaticChars("dummy");

  i::FLAG_regexp_peephole_optimization = false;
  Handle<ByteArray> array = Handle<ByteArray>::cast(orig.GetCode(source));
  int length = array->length();

  i::FLAG_regexp_peephole_optimization = true;
  Handle<ByteArray> array_optimized =
      Handle<ByteArray>::cast(opt.GetCode(source));
  int length_optimized = array_optimized->length();

  int length_expected = RegExpBytecodeLength(BC_CHECK_CURRENT_POSITION) +
                        RegExpBytecodeLength(BC_LOAD_CURRENT_CHAR_UNCHECKED) +
                        RegExpBytecodeLength(BC_CHECK_CHAR) +
                        RegExpBytecodeLength(BC_ADVANCE_CP_AND_GOTO) +
                        RegExpBytecodeLength(BC_POP_BT);
  int length_optimized_expected =
      RegExpBytecodeLength(BC_SKIP_UNTIL_CHAR_POS_CHECKED) +
      RegExpBytecodeLength(BC_POP_BT);

  CHECK_EQ(length, length_expected);
  CHECK_EQ(length_optimized, length_optimized_expected);

  CHECK_EQ(BC_SKIP_UNTIL_CHAR_POS_CHECKED, array_optimized->get(0));
  CHECK_EQ(BC_POP_BT, array_optimized->get(RegExpBytecodeLength(
                          BC_SKIP_UNTIL_CHAR_POS_CHECKED)));
}

void CreatePeepholeSkipUntilCharAndBytecode(RegExpMacroAssembler* m) {
  Label start;
  m->Bind(&start);
  m->LoadCurrentCharacter(0, nullptr, true, 1, 2);
  m->CheckCharacterAfterAnd('x', 0xFF, nullptr);
  m->AdvanceCurrentPosition(1);
  m->GoTo(&start);
}

TEST(PeepholeSkipUntilCharAnd) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  RegExpBytecodeGenerator orig(CcTest::i_isolate(), &zone);
  RegExpBytecodeGenerator opt(CcTest::i_isolate(), &zone);

  CreatePeepholeSkipUntilCharAndBytecode(&orig);
  CreatePeepholeSkipUntilCharAndBytecode(&opt);

  Handle<String> source = factory->NewStringFromStaticChars("dummy");

  i::FLAG_regexp_peephole_optimization = false;
  Handle<ByteArray> array = Handle<ByteArray>::cast(orig.GetCode(source));
  int length = array->length();

  i::FLAG_regexp_peephole_optimization = true;
  Handle<ByteArray> array_optimized =
      Handle<ByteArray>::cast(opt.GetCode(source));
  int length_optimized = array_optimized->length();

  int length_expected = RegExpBytecodeLength(BC_CHECK_CURRENT_POSITION) +
                        RegExpBytecodeLength(BC_LOAD_CURRENT_CHAR_UNCHECKED) +
                        RegExpBytecodeLength(BC_AND_CHECK_CHAR) +
                        RegExpBytecodeLength(BC_ADVANCE_CP_AND_GOTO) +
                        RegExpBytecodeLength(BC_POP_BT);
  int length_optimized_expected = RegExpBytecodeLength(BC_SKIP_UNTIL_CHAR_AND) +
                                  RegExpBytecodeLength(BC_POP_BT);

  CHECK_EQ(length, length_expected);
  CHECK_EQ(length_optimized, length_optimized_expected);

  CHECK_EQ(BC_SKIP_UNTIL_CHAR_AND, array_optimized->get(0));
  CHECK_EQ(BC_POP_BT,
           array_optimized->get(RegExpBytecodeLength(BC_SKIP_UNTIL_CHAR_AND)));
}

void CreatePeepholeSkipUntilCharOrCharBytecode(RegExpMacroAssembler* m) {
  Label start;
  m->Bind(&start);
  m->LoadCurrentCharacter(0, nullptr, true);
  m->CheckCharacter('x', nullptr);
  m->CheckCharacter('y', nullptr);
  m->AdvanceCurrentPosition(1);
  m->GoTo(&start);
}

TEST(PeepholeSkipUntilCharOrChar) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  RegExpBytecodeGenerator orig(CcTest::i_isolate(), &zone);
  RegExpBytecodeGenerator opt(CcTest::i_isolate(), &zone);

  CreatePeepholeSkipUntilCharOrCharBytecode(&orig);
  CreatePeepholeSkipUntilCharOrCharBytecode(&opt);

  Handle<String> source = factory->NewStringFromStaticChars("dummy");

  i::FLAG_regexp_peephole_optimization = false;
  Handle<ByteArray> array = Handle<ByteArray>::cast(orig.GetCode(source));
  int length = array->length();

  i::FLAG_regexp_peephole_optimization = true;
  Handle<ByteArray> array_optimized =
      Handle<ByteArray>::cast(opt.GetCode(source));
  int length_optimized = array_optimized->length();

  int length_expected = RegExpBytecodeLength(BC_LOAD_CURRENT_CHAR) +
                        RegExpBytecodeLength(BC_CHECK_CHAR) +
                        RegExpBytecodeLength(BC_CHECK_CHAR) +
                        RegExpBytecodeLength(BC_ADVANCE_CP_AND_GOTO) +
                        RegExpBytecodeLength(BC_POP_BT);
  int length_optimized_expected =
      RegExpBytecodeLength(BC_SKIP_UNTIL_CHAR_OR_CHAR) +
      RegExpBytecodeLength(BC_POP_BT);

  CHECK_EQ(length, length_expected);
  CHECK_EQ(length_optimized, length_optimized_expected);

  CHECK_EQ(BC_SKIP_UNTIL_CHAR_OR_CHAR, array_optimized->get(0));
  CHECK_EQ(BC_POP_BT, array_optimized->get(
                          RegExpBytecodeLength(BC_SKIP_UNTIL_CHAR_OR_CHAR)));
}

void CreatePeepholeSkipUntilGtOrNotBitInTableBytecode(RegExpMacroAssembler* m,
                                                      Factory* factory) {
  Handle<ByteArray> bit_table = factory->NewByteArray(
      RegExpMacroAssembler::kTableSize, AllocationType::kOld);
  for (uint32_t i = 0; i < RegExpMacroAssembler::kTableSize; i++) {
    bit_table->set(i, 0);
  }

  Label start, end, advance;
  m->Bind(&start);
  m->LoadCurrentCharacter(0, nullptr, true);
  m->CheckCharacterGT('x', nullptr);
  m->CheckBitInTable(bit_table, &advance);
  m->GoTo(&end);
  m->Bind(&advance);
  m->AdvanceCurrentPosition(1);
  m->GoTo(&start);
  m->Bind(&end);
}

TEST(PeepholeSkipUntilGtOrNotBitInTable) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  RegExpBytecodeGenerator orig(CcTest::i_isolate(), &zone);
  RegExpBytecodeGenerator opt(CcTest::i_isolate(), &zone);

  CreatePeepholeSkipUntilGtOrNotBitInTableBytecode(&orig, factory);
  CreatePeepholeSkipUntilGtOrNotBitInTableBytecode(&opt, factory);

  Handle<String> source = factory->NewStringFromStaticChars("dummy");

  i::FLAG_regexp_peephole_optimization = false;
  Handle<ByteArray> array = Handle<ByteArray>::cast(orig.GetCode(source));
  int length = array->length();

  i::FLAG_regexp_peephole_optimization = true;
  Handle<ByteArray> array_optimized =
      Handle<ByteArray>::cast(opt.GetCode(source));
  int length_optimized = array_optimized->length();

  int length_expected = RegExpBytecodeLength(BC_LOAD_CURRENT_CHAR) +
                        RegExpBytecodeLength(BC_CHECK_GT) +
                        RegExpBytecodeLength(BC_CHECK_BIT_IN_TABLE) +
                        RegExpBytecodeLength(BC_GOTO) +
                        RegExpBytecodeLength(BC_ADVANCE_CP_AND_GOTO) +
                        RegExpBytecodeLength(BC_POP_BT);
  int length_optimized_expected =
      RegExpBytecodeLength(BC_SKIP_UNTIL_GT_OR_NOT_BIT_IN_TABLE) +
      RegExpBytecodeLength(BC_POP_BT);

  CHECK_EQ(length, length_expected);
  CHECK_EQ(length_optimized, length_optimized_expected);

  CHECK_EQ(BC_SKIP_UNTIL_GT_OR_NOT_BIT_IN_TABLE, array_optimized->get(0));
  CHECK_EQ(BC_POP_BT, array_optimized->get(RegExpBytecodeLength(
                          BC_SKIP_UNTIL_GT_OR_NOT_BIT_IN_TABLE)));
}

void CreatePeepholeLabelFixupsInsideBytecode(RegExpMacroAssembler* m,
                                             Label* dummy_before,
                                             Label* dummy_after,
                                             Label* dummy_inside) {
  Label loop;
  m->Bind(dummy_before);
  m->LoadCurrentCharacter(0, dummy_before);
  m->CheckCharacter('a', dummy_after);
  m->CheckCharacter('b', dummy_inside);
  m->Bind(&loop);
  m->LoadCurrentCharacter(0, nullptr, true);
  m->CheckCharacter('x', nullptr);
  m->Bind(dummy_inside);
  m->CheckCharacter('y', nullptr);
  m->AdvanceCurrentPosition(1);
  m->GoTo(&loop);
  m->Bind(dummy_after);
  m->LoadCurrentCharacter(0, dummy_before);
  m->CheckCharacter('a', dummy_after);
  m->CheckCharacter('b', dummy_inside);
}

TEST(PeepholeLabelFixupsInside) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  RegExpBytecodeGenerator orig(CcTest::i_isolate(), &zone);
  RegExpBytecodeGenerator opt(CcTest::i_isolate(), &zone);

  {
    Label dummy_before, dummy_after, dummy_inside;
    CreatePeepholeLabelFixupsInsideBytecode(&opt, &dummy_before, &dummy_after,
                                            &dummy_inside);
  }
  Label dummy_before, dummy_after, dummy_inside;
  CreatePeepholeLabelFixupsInsideBytecode(&orig, &dummy_before, &dummy_after,
                                          &dummy_inside);

  CHECK_EQ(0x00, dummy_before.pos());
  CHECK_EQ(0x28, dummy_inside.pos());
  CHECK_EQ(0x38, dummy_after.pos());

  const Label* labels[] = {&dummy_before, &dummy_after, &dummy_inside};
  const int label_positions[4][3] = {
      {0x04, 0x3C},  // dummy_before
      {0x0C, 0x44},  // dummy after
      {0x14, 0x4C}   // dummy inside
  };

  Handle<String> source = factory->NewStringFromStaticChars("dummy");

  i::FLAG_regexp_peephole_optimization = false;
  Handle<ByteArray> array = Handle<ByteArray>::cast(orig.GetCode(source));

  for (int label_idx = 0; label_idx < 3; label_idx++) {
    for (int pos_idx = 0; pos_idx < 2; pos_idx++) {
      CHECK_EQ(labels[label_idx]->pos(),
               array->get(label_positions[label_idx][pos_idx]));
    }
  }

  i::FLAG_regexp_peephole_optimization = true;
  Handle<ByteArray> array_optimized =
      Handle<ByteArray>::cast(opt.GetCode(source));

  const int pos_fixups[] = {
      0,  // Position before optimization should be unchanged.
      4,  // Position after first replacement should be 4 (optimized size (20) -
          // original size (32) + preserve length (16)).
  };
  const int target_fixups[] = {
      0,  // dummy_before should be unchanged
      4,  // dummy_inside should be 4
      4   // dummy_after should be 4
  };

  for (int label_idx = 0; label_idx < 3; label_idx++) {
    for (int pos_idx = 0; pos_idx < 2; pos_idx++) {
      int label_pos = label_positions[label_idx][pos_idx] + pos_fixups[pos_idx];
      int jump_address = *reinterpret_cast<uint32_t*>(
          array_optimized->GetDataStartAddress() + label_pos);
      int expected_jump_address =
          labels[label_idx]->pos() + target_fixups[label_idx];
      CHECK_EQ(expected_jump_address, jump_address);
    }
  }
}

void CreatePeepholeLabelFixupsComplexBytecode(RegExpMacroAssembler* m,
                                              Label* dummy_before,
                                              Label* dummy_between,
                                              Label* dummy_after,
                                              Label* dummy_inside) {
  Label loop1, loop2;
  m->Bind(dummy_before);
  m->LoadCurrentCharacter(0, dummy_before);
  m->CheckCharacter('a', dummy_between);
  m->CheckCharacter('b', dummy_after);
  m->CheckCharacter('c', dummy_inside);
  m->Bind(&loop1);
  m->LoadCurrentCharacter(0, nullptr, true);
  m->CheckCharacter('x', nullptr);
  m->CheckCharacter('y', nullptr);
  m->AdvanceCurrentPosition(1);
  m->GoTo(&loop1);
  m->Bind(dummy_between);
  m->LoadCurrentCharacter(0, dummy_before);
  m->CheckCharacter('a', dummy_between);
  m->CheckCharacter('b', dummy_after);
  m->CheckCharacter('c', dummy_inside);
  m->Bind(&loop2);
  m->LoadCurrentCharacter(0, nullptr, true);
  m->CheckCharacter('x', nullptr);
  m->Bind(dummy_inside);
  m->CheckCharacter('y', nullptr);
  m->AdvanceCurrentPosition(1);
  m->GoTo(&loop2);
  m->Bind(dummy_after);
  m->LoadCurrentCharacter(0, dummy_before);
  m->CheckCharacter('a', dummy_between);
  m->CheckCharacter('b', dummy_after);
  m->CheckCharacter('c', dummy_inside);
}

TEST(PeepholeLabelFixupsComplex) {
  Zone zone(CcTest::i_isolate()->allocator(), ZONE_NAME);
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  RegExpBytecodeGenerator orig(CcTest::i_isolate(), &zone);
  RegExpBytecodeGenerator opt(CcTest::i_isolate(), &zone);

  {
    Label dummy_before, dummy_between, dummy_after, dummy_inside;
    CreatePeepholeLabelFixupsComplexBytecode(
        &opt, &dummy_before, &dummy_between, &dummy_after, &dummy_inside);
  }
  Label dummy_before, dummy_between, dummy_after, dummy_inside;
  CreatePeepholeLabelFixupsComplexBytecode(&orig, &dummy_before, &dummy_between,
                                           &dummy_after, &dummy_inside);

  CHECK_EQ(0x00, dummy_before.pos());
  CHECK_EQ(0x40, dummy_between.pos());
  CHECK_EQ(0x70, dummy_inside.pos());
  CHECK_EQ(0x80, dummy_after.pos());

  const Label* labels[] = {&dummy_before, &dummy_between, &dummy_after,
                           &dummy_inside};
  const int label_positions[4][3] = {
      {0x04, 0x44, 0x84},  // dummy_before
      {0x0C, 0x4C, 0x8C},  // dummy between
      {0x14, 0x54, 0x94},  // dummy after
      {0x1C, 0x5C, 0x9C}   // dummy inside
  };

  Handle<String> source = factory->NewStringFromStaticChars("dummy");

  i::FLAG_regexp_peephole_optimization = false;
  Handle<ByteArray> array = Handle<ByteArray>::cast(orig.GetCode(source));

  for (int label_idx = 0; label_idx < 4; label_idx++) {
    for (int pos_idx = 0; pos_idx < 3; pos_idx++) {
      CHECK_EQ(labels[label_idx]->pos(),
               array->get(label_positions[label_idx][pos_idx]));
    }
  }

  i::FLAG_regexp_peephole_optimization = true;
  Handle<ByteArray> array_optimized =
      Handle<ByteArray>::cast(opt.GetCode(source));

  const int pos_fixups[] = {
      0,    // Position before optimization should be unchanged.
      -12,  // Position after first replacement should be -12 (optimized size =
            // 20 - 32 = original size).
      -8    // Position after second replacement should be -8 (-12 from first
            // optimization -12 from second optimization + 16 preserved
            // bytecodes).
  };
  const int target_fixups[] = {
      0,    // dummy_before should be unchanged
      -12,  // dummy_between should be -12
      -8,   // dummy_inside should be -8
      -8    // dummy_after should be -8
  };

  for (int label_idx = 0; label_idx < 4; label_idx++) {
    for (int pos_idx = 0; pos_idx < 3; pos_idx++) {
      int label_pos = label_positions[label_idx][pos_idx] + pos_fixups[pos_idx];
      int jump_address = *reinterpret_cast<uint32_t*>(
          array_optimized->GetDataStartAddress() + label_pos);
      int expected_jump_address =
          labels[label_idx]->pos() + target_fixups[label_idx];
      CHECK_EQ(expected_jump_address, jump_address);
    }
  }
}

#undef CHECK_PARSE_ERROR
#undef CHECK_SIMPLE
#undef CHECK_MIN_MAX

}  // namespace test_regexp
}  // namespace internal
}  // namespace v8
