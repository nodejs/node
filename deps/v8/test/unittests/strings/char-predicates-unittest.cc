// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/strings/char-predicates.h"
#include "src/strings/char-predicates-inl.h"
#include "src/strings/unicode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

TEST(CharPredicatesTest, WhiteSpace) {
  EXPECT_TRUE(IsWhiteSpace(0x0009));
  EXPECT_TRUE(IsWhiteSpace(0x000B));
  EXPECT_TRUE(IsWhiteSpace(0x000C));
  EXPECT_TRUE(IsWhiteSpace(' '));
  EXPECT_TRUE(IsWhiteSpace(0x00A0));
  EXPECT_TRUE(IsWhiteSpace(0x1680));
  EXPECT_TRUE(IsWhiteSpace(0x2000));
  EXPECT_TRUE(IsWhiteSpace(0x2007));
  EXPECT_TRUE(IsWhiteSpace(0x202F));
  EXPECT_TRUE(IsWhiteSpace(0x205F));
  EXPECT_TRUE(IsWhiteSpace(0x3000));
  EXPECT_TRUE(IsWhiteSpace(0xFEFF));
  EXPECT_FALSE(IsWhiteSpace(0x180E));
}

TEST(CharPredicatesTest, WhiteSpaceOrLineTerminator) {
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0x0009));
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0x000B));
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0x000C));
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(' '));
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0x00A0));
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0x1680));
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0x2000));
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0x2007));
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0x202F));
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0x205F));
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0xFEFF));
  // Line terminators
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0x000A));
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0x000D));
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0x2028));
  EXPECT_TRUE(IsWhiteSpaceOrLineTerminator(0x2029));
  EXPECT_FALSE(IsWhiteSpaceOrLineTerminator(0x180E));
}

TEST(CharPredicatesTest, IdentifierStart) {
  EXPECT_TRUE(IsIdentifierStart('$'));
  EXPECT_TRUE(IsIdentifierStart('_'));
  EXPECT_TRUE(IsIdentifierStart('\\'));

  // http://www.unicode.org/reports/tr31/
  // curl http://www.unicode.org/Public/UCD/latest/ucd/PropList.txt |
  // grep 'Other_ID_Start'
  // Other_ID_Start
  EXPECT_TRUE(IsIdentifierStart(0x1885));
  EXPECT_TRUE(IsIdentifierStart(0x1886));
  EXPECT_TRUE(IsIdentifierStart(0x2118));
  EXPECT_TRUE(IsIdentifierStart(0x212E));
  EXPECT_TRUE(IsIdentifierStart(0x309B));
  EXPECT_TRUE(IsIdentifierStart(0x309C));

  // Issue 2892:
  // \u2E2F has the Pattern_Syntax property, excluding it from ID_Start.
  EXPECT_FALSE(IsIdentifierStart(0x2E2F));

#ifdef V8_INTL_SUPPORT
  // New in Unicode 8.0 (6,847 code points)
  // [:ID_Start:] & [[:Age=8.0:] - [:Age=7.0:]]
  EXPECT_TRUE(IsIdentifierStart(0x08B3));
  EXPECT_TRUE(IsIdentifierStart(0x0AF9));
  EXPECT_TRUE(IsIdentifierStart(0x13F8));
  EXPECT_TRUE(IsIdentifierStart(0x9FCD));
  EXPECT_TRUE(IsIdentifierStart(0xAB60));
  EXPECT_TRUE(IsIdentifierStart(0x10CC0));
  EXPECT_TRUE(IsIdentifierStart(0x108E0));
  EXPECT_TRUE(IsIdentifierStart(0x2B820));

  // New in Unicode 9.0 (7,177 code points)
  // [:ID_Start:] & [[:Age=9.0:] - [:Age=8.0:]]

  EXPECT_TRUE(IsIdentifierStart(0x1C80));
  EXPECT_TRUE(IsIdentifierStart(0x104DB));
  EXPECT_TRUE(IsIdentifierStart(0x1E922));
#endif
}

TEST(CharPredicatesTest, IdentifierPart) {
  EXPECT_TRUE(IsIdentifierPart('$'));
  EXPECT_TRUE(IsIdentifierPart('_'));
  EXPECT_TRUE(IsIdentifierPart('\\'));
  EXPECT_TRUE(IsIdentifierPart(0x200C));
  EXPECT_TRUE(IsIdentifierPart(0x200D));

#ifdef V8_INTL_SUPPORT
  // New in Unicode 8.0 (6,847 code points)
  // [:ID_Start:] & [[:Age=8.0:] - [:Age=7.0:]]
  EXPECT_TRUE(IsIdentifierPart(0x08B3));
  EXPECT_TRUE(IsIdentifierPart(0x0AF9));
  EXPECT_TRUE(IsIdentifierPart(0x13F8));
  EXPECT_TRUE(IsIdentifierPart(0x9FCD));
  EXPECT_TRUE(IsIdentifierPart(0xAB60));
  EXPECT_TRUE(IsIdentifierPart(0x10CC0));
  EXPECT_TRUE(IsIdentifierPart(0x108E0));
  EXPECT_TRUE(IsIdentifierPart(0x2B820));

  // [[:ID_Continue:]-[:ID_Start:]] &  [[:Age=8.0:]-[:Age=7.0:]]
  // 162 code points
  EXPECT_TRUE(IsIdentifierPart(0x08E3));
  EXPECT_TRUE(IsIdentifierPart(0xA69E));
  EXPECT_TRUE(IsIdentifierPart(0x11730));

  // New in Unicode 9.0 (7,177 code points)
  // [:ID_Start:] & [[:Age=9.0:] - [:Age=8.0:]]
  EXPECT_TRUE(IsIdentifierPart(0x1C80));
  EXPECT_TRUE(IsIdentifierPart(0x104DB));
  EXPECT_TRUE(IsIdentifierPart(0x1E922));

  // [[:ID_Continue:]-[:ID_Start:]] &  [[:Age=9.0:]-[:Age=8.0:]]
  // 162 code points
  EXPECT_TRUE(IsIdentifierPart(0x08D4));
  EXPECT_TRUE(IsIdentifierPart(0x1DFB));
  EXPECT_TRUE(IsIdentifierPart(0xA8C5));
  EXPECT_TRUE(IsIdentifierPart(0x11450));
#endif

  // http://www.unicode.org/reports/tr31/
  // curl http://www.unicode.org/Public/UCD/latest/ucd/PropList.txt |
  // grep 'Other_ID_(Continue|Start)'

  // Other_ID_Start
  EXPECT_TRUE(IsIdentifierPart(0x1885));
  EXPECT_TRUE(IsIdentifierPart(0x1886));
  EXPECT_TRUE(IsIdentifierPart(0x2118));
  EXPECT_TRUE(IsIdentifierPart(0x212E));
  EXPECT_TRUE(IsIdentifierPart(0x309B));
  EXPECT_TRUE(IsIdentifierPart(0x309C));

  // Other_ID_Continue
  EXPECT_TRUE(IsIdentifierPart(0x00B7));
  EXPECT_TRUE(IsIdentifierPart(0x0387));
  EXPECT_TRUE(IsIdentifierPart(0x1369));
  EXPECT_TRUE(IsIdentifierPart(0x1370));
  EXPECT_TRUE(IsIdentifierPart(0x1371));
  EXPECT_TRUE(IsIdentifierPart(0x19DA));

  // Issue 2892:
  // \u2E2F has the Pattern_Syntax property, excluding it from ID_Start.
  EXPECT_FALSE(IsIdentifierPart(0x2E2F));
}

#ifdef V8_INTL_SUPPORT
TEST(CharPredicatesTest, SupplementaryPlaneIdentifiers) {
  // Both ID_Start and ID_Continue.
  EXPECT_TRUE(IsIdentifierStart(0x10403));  // Category Lu
  EXPECT_TRUE(IsIdentifierPart(0x10403));
  EXPECT_TRUE(IsIdentifierStart(0x1043C));  // Category Ll
  EXPECT_TRUE(IsIdentifierPart(0x1043C));
  EXPECT_TRUE(IsIdentifierStart(0x16F9C));  // Category Lm
  EXPECT_TRUE(IsIdentifierPart(0x16F9C));
  EXPECT_TRUE(IsIdentifierStart(0x10048));  // Category Lo
  EXPECT_TRUE(IsIdentifierPart(0x10048));
  EXPECT_TRUE(IsIdentifierStart(0x1014D));  // Category Nl
  EXPECT_TRUE(IsIdentifierPart(0x1014D));

  // New in Unicode 8.0
  // [ [:ID_Start=Yes:] & [:Age=8.0:]] - [:Age=7.0:]
  EXPECT_TRUE(IsIdentifierStart(0x108E0));
  EXPECT_TRUE(IsIdentifierStart(0x10C80));

  // Only ID_Continue.
  EXPECT_FALSE(IsIdentifierStart(0x101FD));  // Category Mn
  EXPECT_TRUE(IsIdentifierPart(0x101FD));
  EXPECT_FALSE(IsIdentifierStart(0x11002));  // Category Mc
  EXPECT_TRUE(IsIdentifierPart(0x11002));
  EXPECT_FALSE(IsIdentifierStart(0x104A9));  // Category Nd
  EXPECT_TRUE(IsIdentifierPart(0x104A9));

  // Neither.
  EXPECT_FALSE(IsIdentifierStart(0x10111));  // Category No
  EXPECT_FALSE(IsIdentifierPart(0x10111));
  EXPECT_FALSE(IsIdentifierStart(0x1F4A9));  // Category So
  EXPECT_FALSE(IsIdentifierPart(0x1F4A9));
}
#endif  // V8_INTL_SUPPORT

}  // namespace internal
}  // namespace v8
