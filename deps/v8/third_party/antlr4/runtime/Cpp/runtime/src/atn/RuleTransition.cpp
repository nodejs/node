/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/RuleTransition.h"
#include "atn/RuleStartState.h"

using namespace antlr4::atn;

RuleTransition::RuleTransition(RuleStartState* ruleStart, size_t ruleIndex,
                               ATNState* followState)
    : RuleTransition(ruleStart, ruleIndex, 0, followState) {}

RuleTransition::RuleTransition(RuleStartState* ruleStart, size_t ruleIndex,
                               int precedence, ATNState* followState)
    : Transition(ruleStart), ruleIndex(ruleIndex), precedence(precedence) {
  this->followState = followState;
}

Transition::SerializationType RuleTransition::getSerializationType() const {
  return RULE;
}

bool RuleTransition::isEpsilon() const { return true; }

bool RuleTransition::matches(size_t /*symbol*/, size_t /*minVocabSymbol*/,
                             size_t /*maxVocabSymbol*/) const {
  return false;
}

std::string RuleTransition::toString() const {
  std::stringstream ss;
  ss << "RULE " << Transition::toString() << " { ruleIndex: " << ruleIndex
     << ", precedence: " << precedence << ", followState: " << std::hex
     << followState << " }";
  return ss.str();
}
