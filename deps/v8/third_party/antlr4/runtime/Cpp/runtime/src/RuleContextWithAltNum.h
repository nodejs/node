/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "ParserRuleContext.h"

namespace antlr4 {

/// A handy class for use with
///
///  options {contextSuperClass=org.antlr.v4.runtime.RuleContextWithAltNum;}
///
///  that provides a backing field / impl for the outer alternative number
///  matched for an internal parse tree node.
///
///  I'm only putting into Java runtime as I'm certain I'm the only one that
///  will really every use this.
class ANTLR4CPP_PUBLIC RuleContextWithAltNum : public ParserRuleContext {
 public:
  size_t altNum = 0;

  RuleContextWithAltNum();
  RuleContextWithAltNum(ParserRuleContext* parent, int invokingStateNumber);

  virtual size_t getAltNumber() const override;
  virtual void setAltNumber(size_t altNum) override;
};

}  // namespace antlr4
