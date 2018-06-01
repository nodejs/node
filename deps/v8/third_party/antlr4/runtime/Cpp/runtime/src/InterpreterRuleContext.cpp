/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "InterpreterRuleContext.h"

using namespace antlr4;

InterpreterRuleContext::InterpreterRuleContext() : ParserRuleContext() {}

InterpreterRuleContext::InterpreterRuleContext(ParserRuleContext* parent,
                                               size_t invokingStateNumber,
                                               size_t ruleIndex)
    : ParserRuleContext(parent, invokingStateNumber), _ruleIndex(ruleIndex) {}

size_t InterpreterRuleContext::getRuleIndex() const { return _ruleIndex; }
