/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/PredicateTransition.h"

using namespace antlr4::atn;

PredicateTransition::PredicateTransition(ATNState* target, size_t ruleIndex,
                                         size_t predIndex, bool isCtxDependent)
    : AbstractPredicateTransition(target),
      ruleIndex(ruleIndex),
      predIndex(predIndex),
      isCtxDependent(isCtxDependent) {}

Transition::SerializationType PredicateTransition::getSerializationType()
    const {
  return PREDICATE;
}

bool PredicateTransition::isEpsilon() const { return true; }

bool PredicateTransition::matches(size_t /*symbol*/, size_t /*minVocabSymbol*/,
                                  size_t /*maxVocabSymbol*/) const {
  return false;
}

Ref<SemanticContext::Predicate> PredicateTransition::getPredicate() const {
  return std::make_shared<SemanticContext::Predicate>(ruleIndex, predIndex,
                                                      isCtxDependent);
}

std::string PredicateTransition::toString() const {
  return "PREDICATE " + Transition::toString() +
         " { ruleIndex: " + std::to_string(ruleIndex) +
         ", predIndex: " + std::to_string(predIndex) +
         ", isCtxDependent: " + std::to_string(isCtxDependent) + " }";

  // Generate and add a predicate context here?
}
