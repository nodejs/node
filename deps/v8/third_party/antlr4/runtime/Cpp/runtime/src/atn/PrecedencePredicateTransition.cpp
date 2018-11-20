/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/PrecedencePredicateTransition.h"

using namespace antlr4::atn;

PrecedencePredicateTransition::PrecedencePredicateTransition(ATNState* target,
                                                             int precedence)
    : AbstractPredicateTransition(target), precedence(precedence) {}

Transition::SerializationType
PrecedencePredicateTransition::getSerializationType() const {
  return PRECEDENCE;
}

bool PrecedencePredicateTransition::isEpsilon() const { return true; }

bool PrecedencePredicateTransition::matches(size_t /*symbol*/,
                                            size_t /*minVocabSymbol*/,
                                            size_t /*maxVocabSymbol*/) const {
  return false;
}

Ref<SemanticContext::PrecedencePredicate>
PrecedencePredicateTransition::getPredicate() const {
  return std::make_shared<SemanticContext::PrecedencePredicate>(precedence);
}

std::string PrecedencePredicateTransition::toString() const {
  return "PRECEDENCE " + Transition::toString() +
         " { precedence: " + std::to_string(precedence) + " }";
}
