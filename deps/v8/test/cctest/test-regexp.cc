// Copyright 2008 the V8 project authors. All rights reserved.
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
#include <set>

#include "v8.h"

#include "string-stream.h"
#include "cctest.h"
#include "zone-inl.h"
#include "parser.h"
#include "ast.h"
#include "jsregexp-inl.h"
#include "regexp-macro-assembler.h"
#include "regexp-macro-assembler-irregexp.h"
#ifdef ARM
#include "regexp-macro-assembler-arm.h"
#else  // IA32
#include "macro-assembler-ia32.h"
#include "regexp-macro-assembler-ia32.h"
#endif
#include "interpreter-irregexp.h"


using namespace v8::internal;


static SmartPointer<const char> Parse(const char* input) {
  V8::Initialize(NULL);
  v8::HandleScope scope;
  ZoneScope zone_scope(DELETE_ON_EXIT);
  FlatStringReader reader(CStrVector(input));
  RegExpCompileData result;
  CHECK(v8::internal::ParseRegExp(&reader, false, &result));
  CHECK(result.tree != NULL);
  CHECK(result.error.is_null());
  SmartPointer<const char> output = result.tree->ToString();
  return output;
}

static bool CheckSimple(const char* input) {
  V8::Initialize(NULL);
  v8::HandleScope scope;
  unibrow::Utf8InputBuffer<> buffer(input, strlen(input));
  ZoneScope zone_scope(DELETE_ON_EXIT);
  FlatStringReader reader(CStrVector(input));
  RegExpCompileData result;
  CHECK(v8::internal::ParseRegExp(&reader, false, &result));
  CHECK(result.tree != NULL);
  CHECK(result.error.is_null());
  return result.simple;
}

struct MinMaxPair {
  int min_match;
  int max_match;
};

static MinMaxPair CheckMinMaxMatch(const char* input) {
  V8::Initialize(NULL);
  v8::HandleScope scope;
  unibrow::Utf8InputBuffer<> buffer(input, strlen(input));
  ZoneScope zone_scope(DELETE_ON_EXIT);
  FlatStringReader reader(CStrVector(input));
  RegExpCompileData result;
  CHECK(v8::internal::ParseRegExp(&reader, false, &result));
  CHECK(result.tree != NULL);
  CHECK(result.error.is_null());
  int min_match = result.tree->min_match();
  int max_match = result.tree->max_match();
  MinMaxPair pair = { min_match, max_match };
  return pair;
}



#define CHECK_PARSE_EQ(input, expected) CHECK_EQ(expected, *Parse(input))
#define CHECK_SIMPLE(input, simple) CHECK_EQ(simple, CheckSimple(input));
#define CHECK_MIN_MAX(input, min, max)                                         \
  { MinMaxPair min_max = CheckMinMaxMatch(input);                              \
    CHECK_EQ(min, min_max.min_match);                                          \
    CHECK_EQ(max, min_max.max_match);                                          \
  }

TEST(Parser) {
  V8::Initialize(NULL);
  CHECK_PARSE_EQ("abc", "'abc'");
  CHECK_PARSE_EQ("", "%");
  CHECK_PARSE_EQ("abc|def", "(| 'abc' 'def')");
  CHECK_PARSE_EQ("abc|def|ghi", "(| 'abc' 'def' 'ghi')");
  CHECK_PARSE_EQ("^xxx$", "(: @^i 'xxx' @$i)");
  CHECK_PARSE_EQ("ab\\b\\d\\bcd", "(: 'ab' @b [0-9] @b 'cd')");
  CHECK_PARSE_EQ("\\w|\\d", "(| [0-9 A-Z _ a-z] [0-9])");
  CHECK_PARSE_EQ("a*", "(# 0 - g 'a')");
  CHECK_PARSE_EQ("a*?", "(# 0 - n 'a')");
  CHECK_PARSE_EQ("abc+", "(: 'ab' (# 1 - g 'c'))");
  CHECK_PARSE_EQ("abc+?", "(: 'ab' (# 1 - n 'c'))");
  CHECK_PARSE_EQ("xyz?", "(: 'xy' (# 0 1 g 'z'))");
  CHECK_PARSE_EQ("xyz??", "(: 'xy' (# 0 1 n 'z'))");
  CHECK_PARSE_EQ("xyz{0,1}", "(: 'xy' (# 0 1 g 'z'))");
  CHECK_PARSE_EQ("xyz{0,1}?", "(: 'xy' (# 0 1 n 'z'))");
  CHECK_PARSE_EQ("xyz{93}", "(: 'xy' (# 93 93 g 'z'))");
  CHECK_PARSE_EQ("xyz{93}?", "(: 'xy' (# 93 93 n 'z'))");
  CHECK_PARSE_EQ("xyz{1,32}", "(: 'xy' (# 1 32 g 'z'))");
  CHECK_PARSE_EQ("xyz{1,32}?", "(: 'xy' (# 1 32 n 'z'))");
  CHECK_PARSE_EQ("xyz{1,}", "(: 'xy' (# 1 - g 'z'))");
  CHECK_PARSE_EQ("xyz{1,}?", "(: 'xy' (# 1 - n 'z'))");
  CHECK_PARSE_EQ("a\\fb\\nc\\rd\\te\\vf", "'a\\x0cb\\x0ac\\x0dd\\x09e\\x0bf'");
  CHECK_PARSE_EQ("a\\nb\\bc", "(: 'a\\x0ab' @b 'c')");
  CHECK_PARSE_EQ("(?:foo)", "'foo'");
  CHECK_PARSE_EQ("(?: foo )", "' foo '");
  CHECK_PARSE_EQ("(foo|bar|baz)", "(^ (| 'foo' 'bar' 'baz'))");
  CHECK_PARSE_EQ("foo|(bar|baz)|quux", "(| 'foo' (^ (| 'bar' 'baz')) 'quux')");
  CHECK_PARSE_EQ("foo(?=bar)baz", "(: 'foo' (-> + 'bar') 'baz')");
  CHECK_PARSE_EQ("foo(?!bar)baz", "(: 'foo' (-> - 'bar') 'baz')");
  CHECK_PARSE_EQ("()", "(^ %)");
  CHECK_PARSE_EQ("(?=)", "(-> + %)");
  CHECK_PARSE_EQ("[]", "^[\\x00-\\uffff]");   // Doesn't compile on windows
  CHECK_PARSE_EQ("[^]", "[\\x00-\\uffff]");   // \uffff isn't in codepage 1252
  CHECK_PARSE_EQ("[x]", "[x]");
  CHECK_PARSE_EQ("[xyz]", "[x y z]");
  CHECK_PARSE_EQ("[a-zA-Z0-9]", "[a-z A-Z 0-9]");
  CHECK_PARSE_EQ("[-123]", "[- 1 2 3]");
  CHECK_PARSE_EQ("[^123]", "^[1 2 3]");
  CHECK_PARSE_EQ("]", "']'");
  CHECK_PARSE_EQ("}", "'}'");
  CHECK_PARSE_EQ("[a-b-c]", "[a-b - c]");
  CHECK_PARSE_EQ("[\\d]", "[0-9]");
  CHECK_PARSE_EQ("[x\\dz]", "[x 0-9 z]");
  CHECK_PARSE_EQ("[\\d-z]", "[0-9 - z]");
  CHECK_PARSE_EQ("[\\d-\\d]", "[0-9 - 0-9]");
  CHECK_PARSE_EQ("[z-\\d]", "[z - 0-9]");
  CHECK_PARSE_EQ("\\cj\\cJ\\ci\\cI\\ck\\cK",
                 "'\\x0a\\x0a\\x09\\x09\\x0b\\x0b'");
  CHECK_PARSE_EQ("\\c!", "'c!'");
  CHECK_PARSE_EQ("\\c_", "'c_'");
  CHECK_PARSE_EQ("\\c~", "'c~'");
  CHECK_PARSE_EQ("[a\\]c]", "[a ] c]");
  CHECK_PARSE_EQ("\\[\\]\\{\\}\\(\\)\\%\\^\\#\\ ", "'[]{}()%^# '");
  CHECK_PARSE_EQ("[\\[\\]\\{\\}\\(\\)\\%\\^\\#\\ ]", "[[ ] { } ( ) % ^ #  ]");
  CHECK_PARSE_EQ("\\0", "'\\x00'");
  CHECK_PARSE_EQ("\\8", "'8'");
  CHECK_PARSE_EQ("\\9", "'9'");
  CHECK_PARSE_EQ("\\11", "'\\x09'");
  CHECK_PARSE_EQ("\\11a", "'\\x09a'");
  CHECK_PARSE_EQ("\\011", "'\\x09'");
  CHECK_PARSE_EQ("\\00011", "'\\x0011'");
  CHECK_PARSE_EQ("\\118", "'\\x098'");
  CHECK_PARSE_EQ("\\111", "'I'");
  CHECK_PARSE_EQ("\\1111", "'I1'");
  CHECK_PARSE_EQ("(x)(x)(x)\\1", "(: (^ 'x') (^ 'x') (^ 'x') (<- 1))");
  CHECK_PARSE_EQ("(x)(x)(x)\\2", "(: (^ 'x') (^ 'x') (^ 'x') (<- 2))");
  CHECK_PARSE_EQ("(x)(x)(x)\\3", "(: (^ 'x') (^ 'x') (^ 'x') (<- 3))");
  CHECK_PARSE_EQ("(x)(x)(x)\\4", "(: (^ 'x') (^ 'x') (^ 'x') '\\x04')");
  CHECK_PARSE_EQ("(x)(x)(x)\\1*", "(: (^ 'x') (^ 'x') (^ 'x')"
                               " (# 0 - g (<- 1)))");
  CHECK_PARSE_EQ("(x)(x)(x)\\2*", "(: (^ 'x') (^ 'x') (^ 'x')"
                               " (# 0 - g (<- 2)))");
  CHECK_PARSE_EQ("(x)(x)(x)\\3*", "(: (^ 'x') (^ 'x') (^ 'x')"
                               " (# 0 - g (<- 3)))");
  CHECK_PARSE_EQ("(x)(x)(x)\\4*", "(: (^ 'x') (^ 'x') (^ 'x')"
                               " (# 0 - g '\\x04'))");
  CHECK_PARSE_EQ("(x)(x)(x)(x)(x)(x)(x)(x)(x)(x)\\10",
              "(: (^ 'x') (^ 'x') (^ 'x') (^ 'x') (^ 'x') (^ 'x')"
              " (^ 'x') (^ 'x') (^ 'x') (^ 'x') (<- 10))");
  CHECK_PARSE_EQ("(x)(x)(x)(x)(x)(x)(x)(x)(x)(x)\\11",
              "(: (^ 'x') (^ 'x') (^ 'x') (^ 'x') (^ 'x') (^ 'x')"
              " (^ 'x') (^ 'x') (^ 'x') (^ 'x') '\\x09')");
  CHECK_PARSE_EQ("(a)\\1", "(: (^ 'a') (<- 1))");
  CHECK_PARSE_EQ("(a\\1)", "(^ 'a')");
  CHECK_PARSE_EQ("(\\1a)", "(^ 'a')");
  CHECK_PARSE_EQ("(?=a)?a", "'a'");
  CHECK_PARSE_EQ("(?=a){0,10}a", "'a'");
  CHECK_PARSE_EQ("(?=a){1,10}a", "(: (-> + 'a') 'a')");
  CHECK_PARSE_EQ("(?=a){9,10}a", "(: (-> + 'a') 'a')");
  CHECK_PARSE_EQ("(?!a)?a", "'a'");
  CHECK_PARSE_EQ("\\1(a)", "(^ 'a')");
  CHECK_PARSE_EQ("(?!(a))\\1", "(-> - (^ 'a'))");
  CHECK_PARSE_EQ("(?!\\1(a\\1)\\1)\\1", "(-> - (: (^ 'a') (<- 1)))");
  CHECK_PARSE_EQ("[\\0]", "[\\x00]");
  CHECK_PARSE_EQ("[\\11]", "[\\x09]");
  CHECK_PARSE_EQ("[\\11a]", "[\\x09 a]");
  CHECK_PARSE_EQ("[\\011]", "[\\x09]");
  CHECK_PARSE_EQ("[\\00011]", "[\\x00 1 1]");
  CHECK_PARSE_EQ("[\\118]", "[\\x09 8]");
  CHECK_PARSE_EQ("[\\111]", "[I]");
  CHECK_PARSE_EQ("[\\1111]", "[I 1]");
  CHECK_PARSE_EQ("\\x34", "'\x34'");
  CHECK_PARSE_EQ("\\x60", "'\x60'");
  CHECK_PARSE_EQ("\\x3z", "'x3z'");
  CHECK_PARSE_EQ("\\c", "'c'");
  CHECK_PARSE_EQ("\\u0034", "'\x34'");
  CHECK_PARSE_EQ("\\u003z", "'u003z'");
  CHECK_PARSE_EQ("foo[z]*", "(: 'foo' (# 0 - g [z]))");

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

  CHECK_PARSE_EQ("a{}", "'a{}'");
  CHECK_PARSE_EQ("a{,}", "'a{,}'");
  CHECK_PARSE_EQ("a{", "'a{'");
  CHECK_PARSE_EQ("a{z}", "'a{z}'");
  CHECK_PARSE_EQ("a{1z}", "'a{1z}'");
  CHECK_PARSE_EQ("a{12z}", "'a{12z}'");
  CHECK_PARSE_EQ("a{12,", "'a{12,'");
  CHECK_PARSE_EQ("a{12,3b", "'a{12,3b'");
  CHECK_PARSE_EQ("{}", "'{}'");
  CHECK_PARSE_EQ("{,}", "'{,}'");
  CHECK_PARSE_EQ("{", "'{'");
  CHECK_PARSE_EQ("{z}", "'{z}'");
  CHECK_PARSE_EQ("{1z}", "'{1z}'");
  CHECK_PARSE_EQ("{12z}", "'{12z}'");
  CHECK_PARSE_EQ("{12,", "'{12,'");
  CHECK_PARSE_EQ("{12,3b", "'{12,3b'");

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
  CHECK_MIN_MAX("(ab)\\1", 2, 4);
  CHECK_MIN_MAX("(ab|cde)\\1", 2, 6);
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
}

TEST(ParserRegression) {
  CHECK_PARSE_EQ("[A-Z$-][x]", "(! [A-Z $ -] [x])");
  CHECK_PARSE_EQ("a{3,4*}", "(: 'a{3,' (# 0 - g '4') '}')");
  CHECK_PARSE_EQ("{", "'{'");
  CHECK_PARSE_EQ("a|", "(| 'a' %)");
}

static void ExpectError(const char* input,
                        const char* expected) {
  V8::Initialize(NULL);
  v8::HandleScope scope;
  ZoneScope zone_scope(DELETE_ON_EXIT);
  FlatStringReader reader(CStrVector(input));
  RegExpCompileData result;
  CHECK_EQ(false, v8::internal::ParseRegExp(&reader, false, &result));
  CHECK(result.tree == NULL);
  CHECK(!result.error.is_null());
  SmartPointer<char> str = result.error->ToCString(ALLOW_NULLS);
  CHECK_EQ(expected, *str);
}


TEST(Errors) {
  V8::Initialize(NULL);
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
  HeapStringAllocator allocator;
  StringStream accumulator(&allocator);
  for (int i = 0; i <= kMaxCaptures; i++) {
    accumulator.Add("()");
  }
  SmartPointer<const char> many_captures(accumulator.ToCString());
  ExpectError(*many_captures, kTooManyCaptures);
}


static bool IsDigit(uc16 c) {
  return ('0' <= c && c <= '9');
}


static bool NotDigit(uc16 c) {
  return !IsDigit(c);
}


static bool IsWhiteSpace(uc16 c) {
  switch (c) {
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0d:
    case 0x20:
    case 0xA0:
    case 0x2028:
    case 0x2029:
      return true;
    default:
      return unibrow::Space::Is(c);
  }
}


static bool NotWhiteSpace(uc16 c) {
  return !IsWhiteSpace(c);
}


static bool NotWord(uc16 c) {
  return !IsRegExpWord(c);
}


static void TestCharacterClassEscapes(uc16 c, bool (pred)(uc16 c)) {
  ZoneScope scope(DELETE_ON_EXIT);
  ZoneList<CharacterRange>* ranges = new ZoneList<CharacterRange>(2);
  CharacterRange::AddClassEscape(c, ranges);
  for (unsigned i = 0; i < (1 << 16); i++) {
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
  TestCharacterClassEscapes('s', IsWhiteSpace);
  TestCharacterClassEscapes('S', NotWhiteSpace);
  TestCharacterClassEscapes('w', IsRegExpWord);
  TestCharacterClassEscapes('W', NotWord);
}


static RegExpNode* Compile(const char* input, bool multiline, bool is_ascii) {
  V8::Initialize(NULL);
  FlatStringReader reader(CStrVector(input));
  RegExpCompileData compile_data;
  if (!v8::internal::ParseRegExp(&reader, multiline, &compile_data))
    return NULL;
  Handle<String> pattern = Factory::NewStringFromUtf8(CStrVector(input));
  RegExpEngine::Compile(&compile_data, false, multiline, pattern, is_ascii);
  return compile_data.node;
}


static void Execute(const char* input,
                    bool multiline,
                    bool is_ascii,
                    bool dot_output = false) {
  v8::HandleScope scope;
  ZoneScope zone_scope(DELETE_ON_EXIT);
  RegExpNode* node = Compile(input, multiline, is_ascii);
  USE(node);
#ifdef DEBUG
  if (dot_output) {
    RegExpEngine::DotPrint(input, node, false);
    exit(0);
  }
#endif  // DEBUG
}


class TestConfig {
 public:
  typedef int Key;
  typedef int Value;
  static const int kNoKey;
  static const int kNoValue;
  static inline int Compare(int a, int b) {
    if (a < b)
      return -1;
    else if (a > b)
      return 1;
    else
      return 0;
  }
};


const int TestConfig::kNoKey = 0;
const int TestConfig::kNoValue = 0;


static int PseudoRandom(int i, int j) {
  return ~(~((i * 781) ^ (j * 329)));
}


TEST(SplayTreeSimple) {
  static const int kLimit = 1000;
  ZoneScope zone_scope(DELETE_ON_EXIT);
  ZoneSplayTree<TestConfig> tree;
  std::set<int> seen;
#define CHECK_MAPS_EQUAL() do {                                      \
    for (int k = 0; k < kLimit; k++)                                 \
      CHECK_EQ(seen.find(k) != seen.end(), tree.Find(k, &loc));      \
  } while (false)
  for (int i = 0; i < 50; i++) {
    for (int j = 0; j < 50; j++) {
      int next = PseudoRandom(i, j) % kLimit;
      if (seen.find(next) != seen.end()) {
        // We've already seen this one.  Check the value and remove
        // it.
        ZoneSplayTree<TestConfig>::Locator loc;
        CHECK(tree.Find(next, &loc));
        CHECK_EQ(next, loc.key());
        CHECK_EQ(3 * next, loc.value());
        tree.Remove(next);
        seen.erase(next);
        CHECK_MAPS_EQUAL();
      } else {
        // Check that it wasn't there already and then add it.
        ZoneSplayTree<TestConfig>::Locator loc;
        CHECK(!tree.Find(next, &loc));
        CHECK(tree.Insert(next, &loc));
        CHECK_EQ(next, loc.key());
        loc.set_value(3 * next);
        seen.insert(next);
        CHECK_MAPS_EQUAL();
      }
      int val = PseudoRandom(j, i) % kLimit;
      for (int k = val; k >= 0; k--) {
        if (seen.find(val) != seen.end()) {
          ZoneSplayTree<TestConfig>::Locator loc;
          CHECK(tree.FindGreatestLessThan(val, &loc));
          CHECK_EQ(loc.key(), val);
          break;
        }
      }
      val = PseudoRandom(i + j, i - j) % kLimit;
      for (int k = val; k < kLimit; k++) {
        if (seen.find(val) != seen.end()) {
          ZoneSplayTree<TestConfig>::Locator loc;
          CHECK(tree.FindLeastGreaterThan(val, &loc));
          CHECK_EQ(loc.key(), val);
          break;
        }
      }
    }
  }
}


TEST(DispatchTableConstruction) {
  // Initialize test data.
  static const int kLimit = 1000;
  static const int kRangeCount = 8;
  static const int kRangeSize = 16;
  uc16 ranges[kRangeCount][2 * kRangeSize];
  for (int i = 0; i < kRangeCount; i++) {
    Vector<uc16> range(ranges[i], 2 * kRangeSize);
    for (int j = 0; j < 2 * kRangeSize; j++) {
      range[j] = PseudoRandom(i + 25, j + 87) % kLimit;
    }
    range.Sort();
    for (int j = 1; j < 2 * kRangeSize; j++) {
      CHECK(range[j-1] <= range[j]);
    }
  }
  // Enter test data into dispatch table.
  ZoneScope zone_scope(DELETE_ON_EXIT);
  DispatchTable table;
  for (int i = 0; i < kRangeCount; i++) {
    uc16* range = ranges[i];
    for (int j = 0; j < 2 * kRangeSize; j += 2)
      table.AddRange(CharacterRange(range[j], range[j + 1]), i);
  }
  // Check that the table looks as we would expect
  for (int p = 0; p < kLimit; p++) {
    OutSet* outs = table.Get(p);
    for (int j = 0; j < kRangeCount; j++) {
      uc16* range = ranges[j];
      bool is_on = false;
      for (int k = 0; !is_on && (k < 2 * kRangeSize); k += 2)
        is_on = (range[k] <= p && p <= range[k + 1]);
      CHECK_EQ(is_on, outs->Get(j));
    }
  }
}


TEST(MacroAssembler) {
  V8::Initialize(NULL);
  byte codes[1024];
  RegExpMacroAssemblerIrregexp m(Vector<byte>(codes, 1024));
  // ^f(o)o.
  Label fail, fail2, start;
  uc16 foo_chars[3];
  foo_chars[0] = 'f';
  foo_chars[1] = 'o';
  foo_chars[2] = 'o';
  Vector<const uc16> foo(foo_chars, 3);
  m.SetRegister(4, 42);
  m.PushRegister(4, RegExpMacroAssembler::kNoStackLimitCheck);
  m.AdvanceRegister(4, 42);
  m.GoTo(&start);
  m.Fail();
  m.Bind(&start);
  m.PushBacktrack(&fail2);
  m.CheckCharacters(foo, 0, &fail, true);
  m.WriteCurrentPositionToRegister(0, 0);
  m.PushCurrentPosition();
  m.AdvanceCurrentPosition(3);
  m.WriteCurrentPositionToRegister(1, 0);
  m.PopCurrentPosition();
  m.AdvanceCurrentPosition(1);
  m.WriteCurrentPositionToRegister(2, 0);
  m.AdvanceCurrentPosition(1);
  m.WriteCurrentPositionToRegister(3, 0);
  m.Succeed();

  m.Bind(&fail);
  m.Backtrack();
  m.Succeed();

  m.Bind(&fail2);
  m.PopRegister(0);
  m.Fail();

  v8::HandleScope scope;

  Handle<String> source = Factory::NewStringFromAscii(CStrVector("^f(o)o"));
  Handle<ByteArray> array = Handle<ByteArray>::cast(m.GetCode(source));
  int captures[5];

  const uc16 str1[] = {'f', 'o', 'o', 'b', 'a', 'r'};
  Handle<String> f1_16 =
      Factory::NewStringFromTwoByte(Vector<const uc16>(str1, 6));

  CHECK(IrregexpInterpreter::Match(array, f1_16, captures, 0));
  CHECK_EQ(0, captures[0]);
  CHECK_EQ(3, captures[1]);
  CHECK_EQ(1, captures[2]);
  CHECK_EQ(2, captures[3]);
  CHECK_EQ(84, captures[4]);

  const uc16 str2[] = {'b', 'a', 'r', 'f', 'o', 'o'};
  Handle<String> f2_16 =
      Factory::NewStringFromTwoByte(Vector<const uc16>(str2, 6));

  CHECK(!IrregexpInterpreter::Match(array, f2_16, captures, 0));
  CHECK_EQ(42, captures[0]);
}


#ifndef ARM  // IA32 only tests.

class ContextInitializer {
 public:
  ContextInitializer() : env_(), scope_(), stack_guard_() {
    env_ = v8::Context::New();
    env_->Enter();
  }
  ~ContextInitializer() {
    env_->Exit();
    env_.Dispose();
  }
 private:
  v8::Persistent<v8::Context> env_;
  v8::HandleScope scope_;
  v8::internal::StackGuard stack_guard_;
};


static RegExpMacroAssemblerIA32::Result ExecuteIA32(Code* code,
                                                    String* input,
                                                    int start_offset,
                                                    const byte* input_start,
                                                    const byte* input_end,
                                                    int* captures,
                                                    bool at_start) {
  return RegExpMacroAssemblerIA32::Execute(
      code,
      input,
      start_offset,
      input_start,
      input_end,
      captures,
      at_start);
}


TEST(MacroAssemblerIA32Success) {
  v8::V8::Initialize();
  ContextInitializer initializer;

  RegExpMacroAssemblerIA32 m(RegExpMacroAssemblerIA32::ASCII, 4);

  m.Succeed();

  Handle<String> source = Factory::NewStringFromAscii(CStrVector(""));
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);

  int captures[4] = {42, 37, 87, 117};
  Handle<String> input = Factory::NewStringFromAscii(CStrVector("foofoo"));
  Handle<SeqAsciiString> seq_input = Handle<SeqAsciiString>::cast(input);
  const byte* start_adr =
      reinterpret_cast<const byte*>(seq_input->GetCharsAddress());

  RegExpMacroAssemblerIA32::Result result =
      ExecuteIA32(*code,
                  *input,
                  0,
                  start_adr,
                  start_adr + seq_input->length(),
                  captures,
                  true);

  CHECK_EQ(RegExpMacroAssemblerIA32::SUCCESS, result);
  CHECK_EQ(-1, captures[0]);
  CHECK_EQ(-1, captures[1]);
  CHECK_EQ(-1, captures[2]);
  CHECK_EQ(-1, captures[3]);
}


TEST(MacroAssemblerIA32Simple) {
  v8::V8::Initialize();
  ContextInitializer initializer;

  RegExpMacroAssemblerIA32 m(RegExpMacroAssemblerIA32::ASCII, 4);

  uc16 foo_chars[3] = {'f', 'o', 'o'};
  Vector<const uc16> foo(foo_chars, 3);

  Label fail;
  m.CheckCharacters(foo, 0, &fail, true);
  m.WriteCurrentPositionToRegister(0, 0);
  m.AdvanceCurrentPosition(3);
  m.WriteCurrentPositionToRegister(1, 0);
  m.Succeed();
  m.Bind(&fail);
  m.Fail();

  Handle<String> source = Factory::NewStringFromAscii(CStrVector("^foo"));
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);

  int captures[4] = {42, 37, 87, 117};
  Handle<String> input = Factory::NewStringFromAscii(CStrVector("foofoo"));
  Handle<SeqAsciiString> seq_input = Handle<SeqAsciiString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  RegExpMacroAssemblerIA32::Result result =
      ExecuteIA32(*code,
                  *input,
                  0,
                  start_adr,
                  start_adr + input->length(),
                  captures,
                  true);

  CHECK_EQ(RegExpMacroAssemblerIA32::SUCCESS, result);
  CHECK_EQ(0, captures[0]);
  CHECK_EQ(3, captures[1]);
  CHECK_EQ(-1, captures[2]);
  CHECK_EQ(-1, captures[3]);

  input = Factory::NewStringFromAscii(CStrVector("barbarbar"));
  seq_input = Handle<SeqAsciiString>::cast(input);
  start_adr = seq_input->GetCharsAddress();

  result = ExecuteIA32(*code,
                       *input,
                       0,
                       start_adr,
                       start_adr + input->length(),
                       captures,
                       true);

  CHECK_EQ(RegExpMacroAssemblerIA32::FAILURE, result);
}


TEST(MacroAssemblerIA32SimpleUC16) {
  v8::V8::Initialize();
  ContextInitializer initializer;

  RegExpMacroAssemblerIA32 m(RegExpMacroAssemblerIA32::UC16, 4);

  uc16 foo_chars[3] = {'f', 'o', 'o'};
  Vector<const uc16> foo(foo_chars, 3);

  Label fail;
  m.CheckCharacters(foo, 0, &fail, true);
  m.WriteCurrentPositionToRegister(0, 0);
  m.AdvanceCurrentPosition(3);
  m.WriteCurrentPositionToRegister(1, 0);
  m.Succeed();
  m.Bind(&fail);
  m.Fail();

  Handle<String> source = Factory::NewStringFromAscii(CStrVector("^foo"));
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);

  int captures[4] = {42, 37, 87, 117};
  const uc16 input_data[6] = {'f', 'o', 'o', 'f', 'o', '\xa0'};
  Handle<String> input =
      Factory::NewStringFromTwoByte(Vector<const uc16>(input_data, 6));
  Handle<SeqTwoByteString> seq_input = Handle<SeqTwoByteString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  RegExpMacroAssemblerIA32::Result result =
      ExecuteIA32(*code,
                  *input,
                  0,
                  start_adr,
                  start_adr + input->length(),
                  captures,
                  true);

  CHECK_EQ(RegExpMacroAssemblerIA32::SUCCESS, result);
  CHECK_EQ(0, captures[0]);
  CHECK_EQ(3, captures[1]);
  CHECK_EQ(-1, captures[2]);
  CHECK_EQ(-1, captures[3]);

  const uc16 input_data2[9] = {'b', 'a', 'r', 'b', 'a', 'r', 'b', 'a', '\xa0'};
  input = Factory::NewStringFromTwoByte(Vector<const uc16>(input_data2, 9));
  seq_input = Handle<SeqTwoByteString>::cast(input);
  start_adr = seq_input->GetCharsAddress();

  result = ExecuteIA32(*code,
                       *input,
                       0,
                       start_adr,
                       start_adr + input->length() * 2,
                       captures,
                       true);

  CHECK_EQ(RegExpMacroAssemblerIA32::FAILURE, result);
}


TEST(MacroAssemblerIA32Backtrack) {
  v8::V8::Initialize();
  ContextInitializer initializer;

  RegExpMacroAssemblerIA32 m(RegExpMacroAssemblerIA32::ASCII, 0);

  Label fail;
  Label backtrack;
  m.LoadCurrentCharacter(10, &fail);
  m.Succeed();
  m.Bind(&fail);
  m.PushBacktrack(&backtrack);
  m.LoadCurrentCharacter(10, NULL);
  m.Succeed();
  m.Bind(&backtrack);
  m.Fail();

  Handle<String> source = Factory::NewStringFromAscii(CStrVector(".........."));
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);

  Handle<String> input = Factory::NewStringFromAscii(CStrVector("foofoo"));
  Handle<SeqAsciiString> seq_input = Handle<SeqAsciiString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  RegExpMacroAssemblerIA32::Result result =
      ExecuteIA32(*code,
                  *input,
                  0,
                  start_adr,
                  start_adr + input->length(),
                  NULL,
                  true);

  CHECK_EQ(RegExpMacroAssemblerIA32::FAILURE, result);
}


TEST(MacroAssemblerIA32BackReferenceASCII) {
  v8::V8::Initialize();
  ContextInitializer initializer;

  RegExpMacroAssemblerIA32 m(RegExpMacroAssemblerIA32::ASCII, 3);

  m.WriteCurrentPositionToRegister(0, 0);
  m.AdvanceCurrentPosition(2);
  m.WriteCurrentPositionToRegister(1, 0);
  Label nomatch;
  m.CheckNotBackReference(0, &nomatch);
  m.Fail();
  m.Bind(&nomatch);
  m.AdvanceCurrentPosition(2);
  Label missing_match;
  m.CheckNotBackReference(0, &missing_match);
  m.WriteCurrentPositionToRegister(2, 0);
  m.Succeed();
  m.Bind(&missing_match);
  m.Fail();

  Handle<String> source = Factory::NewStringFromAscii(CStrVector("^(..)..\1"));
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);

  Handle<String> input = Factory::NewStringFromAscii(CStrVector("fooofo"));
  Handle<SeqAsciiString> seq_input = Handle<SeqAsciiString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  int output[3];
  RegExpMacroAssemblerIA32::Result result =
      ExecuteIA32(*code,
                  *input,
                  0,
                  start_adr,
                  start_adr + input->length(),
                  output,
                  true);

  CHECK_EQ(RegExpMacroAssemblerIA32::SUCCESS, result);
  CHECK_EQ(0, output[0]);
  CHECK_EQ(2, output[1]);
  CHECK_EQ(6, output[2]);
}


TEST(MacroAssemblerIA32BackReferenceUC16) {
  v8::V8::Initialize();
  ContextInitializer initializer;

  RegExpMacroAssemblerIA32 m(RegExpMacroAssemblerIA32::UC16, 3);

  m.WriteCurrentPositionToRegister(0, 0);
  m.AdvanceCurrentPosition(2);
  m.WriteCurrentPositionToRegister(1, 0);
  Label nomatch;
  m.CheckNotBackReference(0, &nomatch);
  m.Fail();
  m.Bind(&nomatch);
  m.AdvanceCurrentPosition(2);
  Label missing_match;
  m.CheckNotBackReference(0, &missing_match);
  m.WriteCurrentPositionToRegister(2, 0);
  m.Succeed();
  m.Bind(&missing_match);
  m.Fail();

  Handle<String> source = Factory::NewStringFromAscii(CStrVector("^(..)..\1"));
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);

  const uc16 input_data[6] = {'f', 0x2028, 'o', 'o', 'f', 0x2028};
  Handle<String> input =
      Factory::NewStringFromTwoByte(Vector<const uc16>(input_data, 6));
  Handle<SeqTwoByteString> seq_input = Handle<SeqTwoByteString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  int output[3];
  RegExpMacroAssemblerIA32::Result result =
      ExecuteIA32(*code,
                  *input,
                  0,
                  start_adr,
                  start_adr + input->length() * 2,
                  output,
                  true);

  CHECK_EQ(RegExpMacroAssemblerIA32::SUCCESS, result);
  CHECK_EQ(0, output[0]);
  CHECK_EQ(2, output[1]);
  CHECK_EQ(6, output[2]);
}



TEST(MacroAssemblerIA32AtStart) {
  v8::V8::Initialize();
  ContextInitializer initializer;

  RegExpMacroAssemblerIA32 m(RegExpMacroAssemblerIA32::ASCII, 0);

  Label not_at_start, newline, fail;
  m.CheckNotAtStart(&not_at_start);
  // Check that prevchar = '\n' and current = 'f'.
  m.CheckCharacter('\n', &newline);
  m.Bind(&fail);
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

  Handle<String> source = Factory::NewStringFromAscii(CStrVector("(^f|ob)"));
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);

  Handle<String> input = Factory::NewStringFromAscii(CStrVector("foobar"));
  Handle<SeqAsciiString> seq_input = Handle<SeqAsciiString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  RegExpMacroAssemblerIA32::Result result =
      ExecuteIA32(*code,
                  *input,
                  0,
                  start_adr,
                  start_adr + input->length(),
                  NULL,
                  true);

  CHECK_EQ(RegExpMacroAssemblerIA32::SUCCESS, result);

  result = ExecuteIA32(*code,
                       *input,
                       3,
                       start_adr + 3,
                       start_adr + input->length(),
                       NULL,
                       false);

  CHECK_EQ(RegExpMacroAssemblerIA32::SUCCESS, result);
}


TEST(MacroAssemblerIA32BackRefNoCase) {
  v8::V8::Initialize();
  ContextInitializer initializer;

  RegExpMacroAssemblerIA32 m(RegExpMacroAssemblerIA32::ASCII, 4);

  Label fail, succ;

  m.WriteCurrentPositionToRegister(0, 0);
  m.WriteCurrentPositionToRegister(2, 0);
  m.AdvanceCurrentPosition(3);
  m.WriteCurrentPositionToRegister(3, 0);
  m.CheckNotBackReferenceIgnoreCase(2, &fail);  // Match "AbC".
  m.CheckNotBackReferenceIgnoreCase(2, &fail);  // Match "ABC".
  Label expected_fail;
  m.CheckNotBackReferenceIgnoreCase(2, &expected_fail);
  m.Bind(&fail);
  m.Fail();

  m.Bind(&expected_fail);
  m.AdvanceCurrentPosition(3);  // Skip "xYz"
  m.CheckNotBackReferenceIgnoreCase(2, &succ);
  m.Fail();

  m.Bind(&succ);
  m.WriteCurrentPositionToRegister(1, 0);
  m.Succeed();

  Handle<String> source =
      Factory::NewStringFromAscii(CStrVector("^(abc)\1\1(?!\1)...(?!\1)"));
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);

  Handle<String> input =
      Factory::NewStringFromAscii(CStrVector("aBcAbCABCxYzab"));
  Handle<SeqAsciiString> seq_input = Handle<SeqAsciiString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  int output[4];
  RegExpMacroAssemblerIA32::Result result =
      ExecuteIA32(*code,
                  *input,
                  0,
                  start_adr,
                  start_adr + input->length(),
                  output,
                  true);

  CHECK_EQ(RegExpMacroAssemblerIA32::SUCCESS, result);
  CHECK_EQ(0, output[0]);
  CHECK_EQ(12, output[1]);
  CHECK_EQ(0, output[2]);
  CHECK_EQ(3, output[3]);
}



TEST(MacroAssemblerIA32Registers) {
  v8::V8::Initialize();
  ContextInitializer initializer;

  RegExpMacroAssemblerIA32 m(RegExpMacroAssemblerIA32::ASCII, 5);

  uc16 foo_chars[3] = {'f', 'o', 'o'};
  Vector<const uc16> foo(foo_chars, 3);

  enum registers { out1, out2, out3, out4, out5, sp, loop_cnt };
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

  m.Bind(&backtrack);
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
  m.WriteCurrentPositionToRegister(out5, 0);  // [0,3,6,9,9]

  m.Succeed();

  m.Bind(&fail);
  m.Fail();

  Handle<String> source =
      Factory::NewStringFromAscii(CStrVector("<loop test>"));
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);

  // String long enough for test (content doesn't matter).
  Handle<String> input =
      Factory::NewStringFromAscii(CStrVector("foofoofoofoofoo"));
  Handle<SeqAsciiString> seq_input = Handle<SeqAsciiString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  int output[5];
  RegExpMacroAssemblerIA32::Result result =
      ExecuteIA32(*code,
                  *input,
                  0,
                  start_adr,
                  start_adr + input->length(),
                  output,
                  true);

  CHECK_EQ(RegExpMacroAssemblerIA32::SUCCESS, result);
  CHECK_EQ(0, output[0]);
  CHECK_EQ(3, output[1]);
  CHECK_EQ(6, output[2]);
  CHECK_EQ(9, output[3]);
  CHECK_EQ(9, output[4]);
}


TEST(MacroAssemblerIA32StackOverflow) {
  v8::V8::Initialize();
  ContextInitializer initializer;

  RegExpMacroAssemblerIA32 m(RegExpMacroAssemblerIA32::ASCII, 0);

  Label loop;
  m.Bind(&loop);
  m.PushBacktrack(&loop);
  m.GoTo(&loop);

  Handle<String> source =
      Factory::NewStringFromAscii(CStrVector("<stack overflow test>"));
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);

  // String long enough for test (content doesn't matter).
  Handle<String> input =
      Factory::NewStringFromAscii(CStrVector("dummy"));
  Handle<SeqAsciiString> seq_input = Handle<SeqAsciiString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  RegExpMacroAssemblerIA32::Result result =
      ExecuteIA32(*code,
                  *input,
                  0,
                  start_adr,
                  start_adr + input->length(),
                  NULL,
                  true);

  CHECK_EQ(RegExpMacroAssemblerIA32::EXCEPTION, result);
  CHECK(Top::has_pending_exception());
  Top::clear_pending_exception();
}


TEST(MacroAssemblerIA32LotsOfRegisters) {
  v8::V8::Initialize();
  ContextInitializer initializer;

  RegExpMacroAssemblerIA32 m(RegExpMacroAssemblerIA32::ASCII, 2);

  // At least 2048, to ensure the allocated space for registers
  // span one full page.
  const int large_number = 8000;
  m.WriteCurrentPositionToRegister(large_number, 42);
  m.WriteCurrentPositionToRegister(0, 0);
  m.WriteCurrentPositionToRegister(1, 1);
  Label done;
  m.CheckNotBackReference(0, &done);  // Performs a system-stack push.
  m.Bind(&done);
  m.PushRegister(large_number, RegExpMacroAssembler::kNoStackLimitCheck);
  m.PopRegister(1);
  m.Succeed();

  Handle<String> source =
      Factory::NewStringFromAscii(CStrVector("<huge register space test>"));
  Handle<Object> code_object = m.GetCode(source);
  Handle<Code> code = Handle<Code>::cast(code_object);

  // String long enough for test (content doesn't matter).
  Handle<String> input =
      Factory::NewStringFromAscii(CStrVector("sample text"));
  Handle<SeqAsciiString> seq_input = Handle<SeqAsciiString>::cast(input);
  Address start_adr = seq_input->GetCharsAddress();

  int captures[2];
  RegExpMacroAssemblerIA32::Result result =
      ExecuteIA32(*code,
                  *input,
                  0,
                  start_adr,
                  start_adr + input->length(),
                  captures,
                  true);

  CHECK_EQ(RegExpMacroAssemblerIA32::SUCCESS, result);
  CHECK_EQ(0, captures[0]);
  CHECK_EQ(42, captures[1]);

  Top::clear_pending_exception();
}



#endif  // !defined ARM

TEST(AddInverseToTable) {
  static const int kLimit = 1000;
  static const int kRangeCount = 16;
  for (int t = 0; t < 10; t++) {
    ZoneScope zone_scope(DELETE_ON_EXIT);
    ZoneList<CharacterRange>* ranges =
        new ZoneList<CharacterRange>(kRangeCount);
    for (int i = 0; i < kRangeCount; i++) {
      int from = PseudoRandom(t + 87, i + 25) % kLimit;
      int to = from + (PseudoRandom(i + 87, t + 25) % (kLimit / 20));
      if (to > kLimit) to = kLimit;
      ranges->Add(CharacterRange(from, to));
    }
    DispatchTable table;
    DispatchTableConstructor cons(&table, false);
    cons.set_choice_index(0);
    cons.AddInverse(ranges);
    for (int i = 0; i < kLimit; i++) {
      bool is_on = false;
      for (int j = 0; !is_on && j < kRangeCount; j++)
        is_on = ranges->at(j).Contains(i);
      OutSet* set = table.Get(i);
      CHECK_EQ(is_on, set->Get(0) == false);
    }
  }
  ZoneScope zone_scope(DELETE_ON_EXIT);
  ZoneList<CharacterRange>* ranges =
          new ZoneList<CharacterRange>(1);
  ranges->Add(CharacterRange(0xFFF0, 0xFFFE));
  DispatchTable table;
  DispatchTableConstructor cons(&table, false);
  cons.set_choice_index(0);
  cons.AddInverse(ranges);
  CHECK(!table.Get(0xFFFE)->Get(0));
  CHECK(table.Get(0xFFFF)->Get(0));
}


static uc32 canonicalize(uc32 c) {
  unibrow::uchar canon[unibrow::Ecma262Canonicalize::kMaxWidth];
  int count = unibrow::Ecma262Canonicalize::Convert(c, '\0', canon, NULL);
  if (count == 0) {
    return c;
  } else {
    CHECK_EQ(1, count);
    return canon[0];
  }
}


TEST(LatinCanonicalize) {
  unibrow::Mapping<unibrow::Ecma262UnCanonicalize> un_canonicalize;
  for (char lower = 'a'; lower <= 'z'; lower++) {
    char upper = lower + ('A' - 'a');
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
  for (uc32 c = 0; c < (1 << 21); c++) {
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


static uc32 CanonRange(uc32 c) {
  unibrow::uchar canon[unibrow::CanonicalizationRange::kMaxWidth];
  int count = unibrow::CanonicalizationRange::Convert(c, '\0', canon, NULL);
  if (count == 0) {
    return c;
  } else {
    CHECK_EQ(1, count);
    return canon[0];
  }
}


TEST(RangeCanonicalization) {
  CHECK_NE(CanonRange(0) & CharacterRange::kStartMarker, 0);
  // Check that we arrive at the same result when using the basic
  // range canonicalization primitives as when using immediate
  // canonicalization.
  unibrow::Mapping<unibrow::Ecma262UnCanonicalize> un_canonicalize;
  for (int i = 0; i < CharacterRange::kRangeCanonicalizeMax; i++) {
    int range = CanonRange(i);
    int indirect_length = 0;
    unibrow::uchar indirect[unibrow::Ecma262UnCanonicalize::kMaxWidth];
    if ((range & CharacterRange::kStartMarker) == 0) {
      indirect_length = un_canonicalize.get(i - range, '\0', indirect);
      for (int i = 0; i < indirect_length; i++)
        indirect[i] += range;
    } else {
      indirect_length = un_canonicalize.get(i, '\0', indirect);
    }
    unibrow::uchar direct[unibrow::Ecma262UnCanonicalize::kMaxWidth];
    int direct_length = un_canonicalize.get(i, '\0', direct);
    CHECK_EQ(direct_length, indirect_length);
  }
  // Check that we arrive at the same results when skipping over
  // canonicalization ranges.
  int next_block = 0;
  while (next_block < CharacterRange::kRangeCanonicalizeMax) {
    uc32 start = CanonRange(next_block);
    CHECK_NE((start & CharacterRange::kStartMarker), 0);
    unsigned dist = start & CharacterRange::kPayloadMask;
    unibrow::uchar first[unibrow::Ecma262UnCanonicalize::kMaxWidth];
    int first_length = un_canonicalize.get(next_block, '\0', first);
    for (unsigned i = 1; i < dist; i++) {
      CHECK_EQ(i, CanonRange(next_block + i));
      unibrow::uchar succ[unibrow::Ecma262UnCanonicalize::kMaxWidth];
      int succ_length = un_canonicalize.get(next_block + i, '\0', succ);
      CHECK_EQ(first_length, succ_length);
      for (int j = 0; j < succ_length; j++) {
        int calc = first[j] + i;
        int found = succ[j];
        CHECK_EQ(calc, found);
      }
    }
    next_block = next_block + dist;
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


static void TestRangeCaseIndependence(CharacterRange input,
                                      Vector<CharacterRange> expected) {
  ZoneScope zone_scope(DELETE_ON_EXIT);
  int count = expected.length();
  ZoneList<CharacterRange>* list = new ZoneList<CharacterRange>(count);
  input.AddCaseEquivalents(list);
  CHECK_EQ(count, list->length());
  for (int i = 0; i < list->length(); i++) {
    CHECK_EQ(expected[i].from(), list->at(i).from());
    CHECK_EQ(expected[i].to(), list->at(i).to());
  }
}


static void TestSimpleRangeCaseIndependence(CharacterRange input,
                                            CharacterRange expected) {
  EmbeddedVector<CharacterRange, 1> vector;
  vector[0] = expected;
  TestRangeCaseIndependence(input, vector);
}


TEST(CharacterRangeCaseIndependence) {
  TestSimpleRangeCaseIndependence(CharacterRange::Singleton('a'),
                                  CharacterRange::Singleton('A'));
  TestSimpleRangeCaseIndependence(CharacterRange::Singleton('z'),
                                  CharacterRange::Singleton('Z'));
  TestSimpleRangeCaseIndependence(CharacterRange('a', 'z'),
                                  CharacterRange('A', 'Z'));
  TestSimpleRangeCaseIndependence(CharacterRange('c', 'f'),
                                  CharacterRange('C', 'F'));
  TestSimpleRangeCaseIndependence(CharacterRange('a', 'b'),
                                  CharacterRange('A', 'B'));
  TestSimpleRangeCaseIndependence(CharacterRange('y', 'z'),
                                  CharacterRange('Y', 'Z'));
  TestSimpleRangeCaseIndependence(CharacterRange('a' - 1, 'z' + 1),
                                  CharacterRange('A', 'Z'));
  TestSimpleRangeCaseIndependence(CharacterRange('A', 'Z'),
                                  CharacterRange('a', 'z'));
  TestSimpleRangeCaseIndependence(CharacterRange('C', 'F'),
                                  CharacterRange('c', 'f'));
  TestSimpleRangeCaseIndependence(CharacterRange('A' - 1, 'Z' + 1),
                                  CharacterRange('a', 'z'));
  // Here we need to add [l-z] to complete the case independence of
  // [A-Za-z] but we expect [a-z] to be added since we always add a
  // whole block at a time.
  TestSimpleRangeCaseIndependence(CharacterRange('A', 'k'),
                                  CharacterRange('a', 'z'));
}


static bool InClass(uc16 c, ZoneList<CharacterRange>* ranges) {
  if (ranges == NULL)
    return false;
  for (int i = 0; i < ranges->length(); i++) {
    CharacterRange range = ranges->at(i);
    if (range.from() <= c && c <= range.to())
      return true;
  }
  return false;
}


TEST(CharClassDifference) {
  ZoneScope zone_scope(DELETE_ON_EXIT);
  ZoneList<CharacterRange>* base = new ZoneList<CharacterRange>(1);
  base->Add(CharacterRange::Everything());
  Vector<const uc16> overlay = CharacterRange::GetWordBounds();
  ZoneList<CharacterRange>* included = NULL;
  ZoneList<CharacterRange>* excluded = NULL;
  CharacterRange::Split(base, overlay, &included, &excluded);
  for (int i = 0; i < (1 << 16); i++) {
    bool in_base = InClass(i, base);
    if (in_base) {
      bool in_overlay = false;
      for (int j = 0; !in_overlay && j < overlay.length(); j += 2) {
        if (overlay[j] <= i && i <= overlay[j+1])
          in_overlay = true;
      }
      CHECK_EQ(in_overlay, InClass(i, included));
      CHECK_EQ(!in_overlay, InClass(i, excluded));
    } else {
      CHECK(!InClass(i, included));
      CHECK(!InClass(i, excluded));
    }
  }
}


TEST(Graph) {
  V8::Initialize(NULL);
  Execute("(?:(?:x(.))?\1)+$", false, true, true);
}
