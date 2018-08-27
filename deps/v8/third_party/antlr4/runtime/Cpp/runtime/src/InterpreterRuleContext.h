/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "ParserRuleContext.h"

namespace antlr4 {

/**
 * This class extends {@link ParserRuleContext} by allowing the value of
 * {@link #getRuleIndex} to be explicitly set for the context.
 *
 * <p>
 * {@link ParserRuleContext} does not include field storage for the rule index
 * since the context classes created by the code generator override the
 * {@link #getRuleIndex} method to return the correct value for that context.
 * Since the parser interpreter does not use the context classes generated for a
 * parser, this class (with slightly more memory overhead per node) is used to
 * provide equivalent functionality.</p>
 */
class ANTLR4CPP_PUBLIC InterpreterRuleContext : public ParserRuleContext {
 public:
  InterpreterRuleContext();

  /**
   * Constructs a new {@link InterpreterRuleContext} with the specified
   * parent, invoking state, and rule index.
   *
   * @param parent The parent context.
   * @param invokingStateNumber The invoking state number.
   * @param ruleIndex The rule index for the current context.
   */
  InterpreterRuleContext(ParserRuleContext* parent, size_t invokingStateNumber,
                         size_t ruleIndex);

  virtual size_t getRuleIndex() const override;

 protected:
  /** This is the backing field for {@link #getRuleIndex}. */
  const size_t _ruleIndex = INVALID_INDEX;
};

}  // namespace antlr4
