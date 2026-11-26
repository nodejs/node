// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests v8::internal::Scanner. Note that presently most unit tests for the
// Scanner are in parsing-unittest.cc, rather than here.

#include "src/parsing/scanner.h"

#include "src/handles/handles-inl.h"
#include "src/objects/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/scanner-character-streams.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class ScannerTest : public TestWithIsolate {
 public:
  struct ScannerTestHelper {
    ScannerTestHelper() = default;
    ScannerTestHelper(ScannerTestHelper&& other) V8_NOEXCEPT
        : stream(std::move(other.stream)),
          scanner(std::move(other.scanner)) {}

    std::unique_ptr<Utf16CharacterStream> stream;
    std::unique_ptr<Scanner> scanner;

    Scanner* operator->() const { return scanner.get(); }
    Scanner* get() const { return scanner.get(); }
  };
  ScannerTestHelper make_scanner(const char* src) {
    ScannerTestHelper helper;
    helper.stream = ScannerStream::ForTesting(src);
    helper.scanner = std::unique_ptr<Scanner>(new Scanner(
        helper.stream.get(),
        UnoptimizedCompileFlags::ForTest(
            reinterpret_cast<i::Isolate*>(v8::Isolate::GetCurrent()))));

    helper.scanner->Initialize();
    return helper;
  }
};
namespace {

const char src_simple[] = "function foo() { var x = 2 * a() + b; }";

}  // anonymous namespace

// CHECK_TOK checks token equality, but by checking for equality of the token
// names. That should have the same result, but has much nicer error messaages.
#define CHECK_TOK(a, b) CHECK_EQ(Token::Name(a), Token::Name(b))

TEST_F(ScannerTest, HasImmediateCommaOrRightBracket) {
  const struct {
    const char* source;
    Token::Value literal;
    Token::Value following;
    bool is_immediate_delimiter;
  } cases[] = {
      {"1,2", Token::kSmi, Token::kComma, true},
      {"1]", Token::kSmi, Token::kRightBracket, true},
      {"1 ,2", Token::kSmi, Token::kComma, false},
      {"1\n]", Token::kSmi, Token::kRightBracket, false},
      {"1/*comment*/,2", Token::kSmi, Token::kComma, false},
      {"1//comment\n]", Token::kSmi, Token::kRightBracket, false},
      {"1n]", Token::kBigInt, Token::kRightBracket, true},
      {"'s',2", Token::kString, Token::kComma, true},
      {"null]", Token::kNullLiteral, Token::kRightBracket, true},
      {"1", Token::kSmi, Token::kEos, false},
      {"1/*", Token::kSmi, Token::kIllegal, false},
      {"1+2]", Token::kSmi, Token::kAdd, false},
      {"1/*]*/+2", Token::kSmi, Token::kAdd, false},
      {"'s'.length]", Token::kString, Token::kPeriod, false},
  };
  for (const auto& test_case : cases) {
    for (int lookahead = 0; lookahead < 3; ++lookahead) {
      auto scanner = make_scanner(test_case.source);
      if (lookahead > 0) scanner->PeekAhead();
      if (lookahead > 1) scanner->PeekAheadAhead();
      CHECK_EQ(test_case.is_immediate_delimiter && lookahead == 0,
               scanner->HasImmediateCommaOrRightBracket());
      CHECK_TOK(test_case.literal, scanner->Next());
      CHECK_TOK(test_case.following, scanner->Next());

      auto error_scanner = make_scanner(test_case.source);
      if (lookahead > 0) error_scanner->PeekAhead();
      if (lookahead > 1) error_scanner->PeekAheadAhead();
      error_scanner->set_parser_error();
      CHECK(!error_scanner->HasImmediateCommaOrRightBracket());
      CHECK_TOK(Token::kIllegal, error_scanner->Next());
    }
  }
}

namespace {

// Scans {scanner} to the end, consuming array literal element runs wherever
// peek() is a comma, and checks that it agrees with {reference}, which scans
// the same source token by token. Returns the number of run elements.
int ExpectRunsMatchReference(Scanner* scanner, Scanner* reference) {
  int elements = 0;
  while (true) {
    while (scanner->peek() == Token::kComma) {
      base::Vector<const Scanner::ArrayLiteralElement> run =
          scanner->ScanArrayLiteralElements();
      if (run.empty()) break;
      for (const Scanner::ArrayLiteralElement& element : run) {
        CHECK_TOK(Token::kComma, reference->Next());
        Token::Value token = reference->Next();
        if (element.is_null) {
          CHECK_TOK(Token::kNullLiteral, token);
        } else if (token == Token::kSmi) {
          EXPECT_EQ(reference->smi_value(), element.number);
        } else {
          CHECK_TOK(Token::kNumber, token);
          EXPECT_EQ(reference->DoubleValue(), element.number);
        }
        EXPECT_EQ(reference->location().beg_pos, element.location.beg_pos);
        EXPECT_EQ(reference->location().end_pos, element.location.end_pos);
        ++elements;
      }
      EXPECT_EQ(reference->peek_location().beg_pos,
                scanner->peek_location().beg_pos);
      EXPECT_EQ(reference->peek_location().end_pos,
                scanner->peek_location().end_pos);
    }
    CHECK_TOK(reference->Next(), scanner->Next());
    EXPECT_EQ(reference->location().beg_pos, scanner->location().beg_pos);
    EXPECT_EQ(reference->location().end_pos, scanner->location().end_pos);
    EXPECT_EQ(reference->HasLineTerminatorBeforeNext(),
              scanner->HasLineTerminatorBeforeNext());
    if (scanner->current_token() == Token::kSmi) {
      EXPECT_EQ(reference->smi_value(), scanner->smi_value());
    }
    if (scanner->current_token() == Token::kEos ||
        scanner->current_token() == Token::kIllegal) {
      break;
    }
  }
  EXPECT_EQ(reference->error(), scanner->error());
  return elements;
}

}  // namespace

TEST_F(ScannerTest, ArrayLiteralElements) {
  const char* source = ",0,1,2147483648,999999999999999,null,7]";
  for (bool two_byte : {false, true}) {
    std::vector<uint16_t> utf16(source, source + strlen(source));
    auto stream = two_byte
                      ? ScannerStream::ForTesting(utf16.data(), utf16.size())
                      : ScannerStream::ForTesting(source);
    Scanner scanner(stream.get(),
                    UnoptimizedCompileFlags::ForTest(i_isolate()));
    scanner.Initialize();
    auto reference = make_scanner(source);
    EXPECT_EQ(5, ExpectRunsMatchReference(&scanner, reference.get()));
  }
}

TEST_F(ScannerTest, ArrayLiteralElementFallback) {
  const char* sources[] = {
      ",01,]",     ",08,]",     ",0x10,]", ",0b10,]",
      ",0o10,]",   ",1n,]",     ",1.5,]",  ",1e2,]",
      ",1_000,]",  ",-1,]",     ",+1,]",   ",1 ,]",
      ",1/*x*/,]", ",1//x\n,]", ", 1,]",   ",\nnull,]",
      ",nullx,]",  ",nu,]",     ",nul,]",  ",null?.x,]",
      ",,]",       ",1]",       ",null]",  ",1",
      ",null",     ",",         ",n",      ",nul",
      ",1e+",      ",1,",       ",null,",  ",1000000000000000,]"};
  for (const char* source : sources) {
    SCOPED_TRACE(source);
    auto scanner = make_scanner(source);
    auto reference = make_scanner(source);
    EXPECT_EQ(0, ExpectRunsMatchReference(scanner.get(), reference.get()));
  }
}

TEST_F(ScannerTest, ArrayLiteralElementLookahead) {
  const char* source = ",0,1,]";
  for (int lookahead = 0; lookahead < 3; ++lookahead) {
    SCOPED_TRACE(lookahead);
    auto scanner = make_scanner(source);
    if (lookahead > 0) scanner->PeekAhead();
    if (lookahead > 1) scanner->PeekAheadAhead();
    size_t stream_pos = scanner.stream->pos();
    if (lookahead > 0) {
      EXPECT_TRUE(scanner->ScanArrayLiteralElements().empty());
      EXPECT_EQ(stream_pos, scanner.stream->pos());
    }
    auto reference = make_scanner(source);
    EXPECT_EQ(lookahead > 0 ? 1 : 2,
              ExpectRunsMatchReference(scanner.get(), reference.get()));
  }
}

TEST_F(ScannerTest, ArrayLiteralElementLineTerminator) {
  auto scanner = make_scanner("x\n,0,\n1]");
  CHECK_TOK(Token::kIdentifier, scanner->Next());
  ASSERT_TRUE(scanner->HasLineTerminatorBeforeNext());
  ASSERT_EQ(1u, scanner->ScanArrayLiteralElements().size());
  EXPECT_FALSE(scanner->HasLineTerminatorBeforeNext());
  CHECK_TOK(Token::kComma, scanner->Next());
  EXPECT_TRUE(scanner->HasLineTerminatorBeforeNext());
  CHECK_TOK(Token::kSmi, scanner->Next());
  EXPECT_EQ(1u, scanner->smi_value());
}

TEST_F(ScannerTest, ArrayLiteralElementParserError) {
  auto scanner = make_scanner(",0,null,1]");
  ASSERT_EQ(2u, scanner->ScanArrayLiteralElements().size());
  scanner->set_parser_error();
  EXPECT_TRUE(scanner->ScanArrayLiteralElements().empty());
  CHECK_TOK(Token::kIllegal, scanner->Next());
}

TEST_F(ScannerTest, ArrayLiteralElementBatches) {
  // Longer than one batch and than one 512 code unit stream buffer.
  std::string source = ",";
  for (int i = 0; i < 1000; ++i) source += (i % 3 == 0) ? "null," : "42,";
  source += "]";
  auto scanner = make_scanner(source.c_str());
  auto reference = make_scanner(source.c_str());
  // Elements straddling a buffer boundary are scanned as tokens instead.
  EXPECT_LT(990, ExpectRunsMatchReference(scanner.get(), reference.get()));
}

TEST_F(ScannerTest, ArrayLiteralElementStreamBoundary) {
  // The one-byte testing stream refills its UTF-16 buffer every 512 code units.
  for (int padding = 490; padding < 520; ++padding) {
    for (const char* suffix :
         {"123456789012345,]", "null,]", "1234567890123456,]", "nullx,]",
          "123456789012345", "nul", "1,2,3,null,4,]"}) {
      SCOPED_TRACE(padding);
      SCOPED_TRACE(suffix);
      std::string source(padding, ' ');
      source += ',';
      source += suffix;
      auto scanner = make_scanner(source.c_str());
      auto reference = make_scanner(source.c_str());
      ExpectRunsMatchReference(scanner.get(), reference.get());
    }
  }
}

TEST_F(ScannerTest, Bookmarks) {
  // Scan through the given source and record the tokens for use as reference
  // below.
  std::vector<Token::Value> tokens;
  {
    auto scanner = make_scanner(src_simple);
    do {
      tokens.push_back(scanner->Next());
    } while (scanner->current_token() != Token::kEos);
  }

  // For each position:
  // - Scan through file,
  // - set a bookmark once the position is reached,
  // - scan a bit more,
  // - reset to the bookmark, and
  // - scan until the end.
  // At each step, compare to the reference token sequence generated above.
  for (size_t bookmark_pos = 0; bookmark_pos < tokens.size(); bookmark_pos++) {
    auto scanner = make_scanner(src_simple);
    Scanner::BookmarkScope bookmark(scanner.get());

    for (size_t i = 0; i < std::min(bookmark_pos + 10, tokens.size()); i++) {
      if (i == bookmark_pos) {
        bookmark.Set(scanner->peek_location().beg_pos);
      }
      CHECK_TOK(tokens[i], scanner->Next());
    }

    bookmark.Apply();
    for (size_t i = bookmark_pos; i < tokens.size(); i++) {
      CHECK_TOK(tokens[i], scanner->Next());
    }
  }
}

TEST_F(ScannerTest, AllThePushbacks) {
  const struct {
    const char* src;
    const Token::Value tokens[5];  // Large enough for any of the test cases.
  } test_cases[] = {
      {"<-x", {Token::kLessThan, Token::kSub, Token::kIdentifier, Token::kEos}},
      {"<!x", {Token::kLessThan, Token::kNot, Token::kIdentifier, Token::kEos}},
      {"<!-x",
       {Token::kLessThan, Token::kNot, Token::kSub, Token::kIdentifier,
        Token::kEos}},
      {"<!-- xx -->\nx", {Token::kIdentifier, Token::kEos}},
  };

  for (const auto& test_case : test_cases) {
    auto scanner = make_scanner(test_case.src);
    for (size_t i = 0; test_case.tokens[i] != Token::kEos; i++) {
      CHECK_TOK(test_case.tokens[i], scanner->Next());
    }
    CHECK_TOK(Token::kEos, scanner->Next());
  }
}

TEST_F(ScannerTest, PeekAheadAheadAwaitUsingDeclaration) {
  const char src[] = "await using a = 2;";

  std::vector<Token::Value> tokens;
  {
    auto scanner = make_scanner(src);
    do {
      tokens.push_back(scanner->Next());
    } while (scanner->current_token() != Token::kEos);
  }

  auto scanner = make_scanner(src);
  Scanner::BookmarkScope bookmark(scanner.get());
  bookmark.Set(scanner->peek_location().beg_pos);
  bookmark.Apply();

  CHECK_TOK(tokens[0], scanner->Next());
  CHECK_TOK(tokens[1], scanner->peek());
  CHECK_TOK(tokens[2], scanner->PeekAhead());
  CHECK_TOK(tokens[3], scanner->PeekAheadAhead());
}

TEST_F(ScannerTest, PeekAheadAheadAwaitExpression) {
  const char src[] = "await using + 5;";

  std::vector<Token::Value> tokens;
  {
    auto scanner = make_scanner(src);
    do {
      tokens.push_back(scanner->Next());
    } while (scanner->current_token() != Token::kEos);
  }

  auto scanner = make_scanner(src);
  Scanner::BookmarkScope bookmark(scanner.get());
  bookmark.Set(scanner->peek_location().beg_pos);
  bookmark.Apply();

  CHECK_TOK(tokens[0], scanner->Next());
  CHECK_TOK(tokens[1], scanner->peek());
  CHECK_TOK(tokens[2], scanner->PeekAhead());
  CHECK_TOK(tokens[3], scanner->PeekAheadAhead());
}

}  // namespace internal
}  // namespace v8
