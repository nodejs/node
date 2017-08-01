// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/char-predicates.h"
#include "src/unicode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

TEST(CharPredicatesTest, WhiteSpace) {
  // As of Unicode 6.3.0, \u180E is no longer a white space. We still consider
  // it to be one though, since JS recognizes all white spaces in Unicode 5.1.
  EXPECT_TRUE(WhiteSpace::Is(0x0009));
  EXPECT_TRUE(WhiteSpace::Is(0x000B));
  EXPECT_TRUE(WhiteSpace::Is(0x000C));
  EXPECT_TRUE(WhiteSpace::Is(' '));
  EXPECT_TRUE(WhiteSpace::Is(0x00A0));
  EXPECT_TRUE(WhiteSpace::Is(0xFEFF));
}


TEST(CharPredicatesTest, WhiteSpaceOrLineTerminator) {
  // As of Unicode 6.3.0, \u180E is no longer a white space. We still consider
  // it to be one though, since JS recognizes all white spaces in Unicode 5.1.
  // White spaces
  EXPECT_TRUE(WhiteSpaceOrLineTerminator::Is(0x0009));
  EXPECT_TRUE(WhiteSpaceOrLineTerminator::Is(0x000B));
  EXPECT_TRUE(WhiteSpaceOrLineTerminator::Is(0x000C));
  EXPECT_TRUE(WhiteSpaceOrLineTerminator::Is(' '));
  EXPECT_TRUE(WhiteSpaceOrLineTerminator::Is(0x00A0));
  EXPECT_TRUE(WhiteSpaceOrLineTerminator::Is(0xFEFF));
  // Line terminators
  EXPECT_TRUE(WhiteSpaceOrLineTerminator::Is(0x000A));
  EXPECT_TRUE(WhiteSpaceOrLineTerminator::Is(0x000D));
  EXPECT_TRUE(WhiteSpaceOrLineTerminator::Is(0x2028));
  EXPECT_TRUE(WhiteSpaceOrLineTerminator::Is(0x2029));
}


TEST(CharPredicatesTest, IdentifierStart) {
  EXPECT_TRUE(IdentifierStart::Is('$'));
  EXPECT_TRUE(IdentifierStart::Is('_'));
  EXPECT_TRUE(IdentifierStart::Is('\\'));

  // http://www.unicode.org/reports/tr31/
  // Other_ID_Start
  EXPECT_TRUE(IdentifierStart::Is(0x2118));
  EXPECT_TRUE(IdentifierStart::Is(0x212E));
  EXPECT_TRUE(IdentifierStart::Is(0x309B));
  EXPECT_TRUE(IdentifierStart::Is(0x309C));

  // Issue 2892:
  // \u2E2F has the Pattern_Syntax property, excluding it from ID_Start.
  EXPECT_FALSE(unibrow::ID_Start::Is(0x2E2F));
}


TEST(CharPredicatesTest, IdentifierPart) {
  EXPECT_TRUE(IdentifierPart::Is('$'));
  EXPECT_TRUE(IdentifierPart::Is('_'));
  EXPECT_TRUE(IdentifierPart::Is('\\'));
  EXPECT_TRUE(IdentifierPart::Is(0x200C));
  EXPECT_TRUE(IdentifierPart::Is(0x200D));

  // http://www.unicode.org/reports/tr31/
  // Other_ID_Start
  EXPECT_TRUE(IdentifierPart::Is(0x2118));
  EXPECT_TRUE(IdentifierPart::Is(0x212E));
  EXPECT_TRUE(IdentifierPart::Is(0x309B));
  EXPECT_TRUE(IdentifierPart::Is(0x309C));

  // Other_ID_Continue
  EXPECT_TRUE(IdentifierPart::Is(0x00B7));
  EXPECT_TRUE(IdentifierPart::Is(0x0387));
  EXPECT_TRUE(IdentifierPart::Is(0x1369));
  EXPECT_TRUE(IdentifierPart::Is(0x1370));
  EXPECT_TRUE(IdentifierPart::Is(0x1371));
  EXPECT_TRUE(IdentifierPart::Is(0x19DA));

  // Issue 2892:
  // \u2E2F has the Pattern_Syntax property, excluding it from ID_Start.
  EXPECT_FALSE(IdentifierPart::Is(0x2E2F));
}

#ifdef V8_INTL_SUPPORT
TEST(CharPredicatesTest, SupplementaryPlaneIdentifiers) {
  // Both ID_Start and ID_Continue.
  EXPECT_TRUE(IdentifierStart::Is(0x10403));  // Category Lu
  EXPECT_TRUE(IdentifierPart::Is(0x10403));
  EXPECT_TRUE(IdentifierStart::Is(0x1043C));  // Category Ll
  EXPECT_TRUE(IdentifierPart::Is(0x1043C));
  EXPECT_TRUE(IdentifierStart::Is(0x16F9C));  // Category Lm
  EXPECT_TRUE(IdentifierPart::Is(0x16F9C));
  EXPECT_TRUE(IdentifierStart::Is(0x10048));  // Category Lo
  EXPECT_TRUE(IdentifierPart::Is(0x10048));
  EXPECT_TRUE(IdentifierStart::Is(0x1014D));  // Category Nl
  EXPECT_TRUE(IdentifierPart::Is(0x1014D));

  // Only ID_Continue.
  EXPECT_FALSE(IdentifierStart::Is(0x101FD));  // Category Mn
  EXPECT_TRUE(IdentifierPart::Is(0x101FD));
  EXPECT_FALSE(IdentifierStart::Is(0x11002));  // Category Mc
  EXPECT_TRUE(IdentifierPart::Is(0x11002));
  EXPECT_FALSE(IdentifierStart::Is(0x104A9));  // Category Nd
  EXPECT_TRUE(IdentifierPart::Is(0x104A9));

  // Neither.
  EXPECT_FALSE(IdentifierStart::Is(0x10111));  // Category No
  EXPECT_FALSE(IdentifierPart::Is(0x10111));
  EXPECT_FALSE(IdentifierStart::Is(0x1F4A9));  // Category So
  EXPECT_FALSE(IdentifierPart::Is(0x1F4A9));
}
#endif  // V8_INTL_SUPPORT

}  // namespace internal
}  // namespace v8
