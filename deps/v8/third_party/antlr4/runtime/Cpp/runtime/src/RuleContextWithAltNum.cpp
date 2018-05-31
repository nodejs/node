/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/ATN.h"

#include "RuleContextWithAltNum.h"

using namespace antlr4;
using namespace antlr4::atn;

RuleContextWithAltNum::RuleContextWithAltNum() : ParserRuleContext() {
  altNum = ATN::INVALID_ALT_NUMBER;
}

RuleContextWithAltNum::RuleContextWithAltNum(ParserRuleContext* parent,
                                             int invokingStateNumber)
    : ParserRuleContext(parent, invokingStateNumber) {}

size_t RuleContextWithAltNum::getAltNumber() const { return altNum; }

void RuleContextWithAltNum::setAltNumber(size_t number) { altNum = number; }
