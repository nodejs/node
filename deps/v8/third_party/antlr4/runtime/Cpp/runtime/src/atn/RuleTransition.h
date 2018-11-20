/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/Transition.h"

namespace antlr4 {
namespace atn {

class ANTLR4CPP_PUBLIC RuleTransition : public Transition {
 public:
  /// Ptr to the rule definition object for this rule ref.
  const size_t ruleIndex;  // no Rule object at runtime

  const int precedence;

  /// What node to begin computations following ref to rule.
  ATNState* followState;

  /// @deprecated Use
  /// <seealso cref="#RuleTransition(RuleStartState, size_t, int, ATNState)"/>
  /// instead.
  RuleTransition(RuleStartState* ruleStart, size_t ruleIndex,
                 ATNState* followState);

  RuleTransition(RuleStartState* ruleStart, size_t ruleIndex, int precedence,
                 ATNState* followState);
  RuleTransition(RuleTransition const&) = delete;
  RuleTransition& operator=(RuleTransition const&) = delete;

  virtual SerializationType getSerializationType() const override;

  virtual bool isEpsilon() const override;
  virtual bool matches(size_t symbol, size_t minVocabSymbol,
                       size_t maxVocabSymbol) const override;

  virtual std::string toString() const override;
};

}  // namespace atn
}  // namespace antlr4
