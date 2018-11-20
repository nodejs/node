/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Parser.h"
#include "atn/ATN.h"
#include "atn/ATNState.h"
#include "atn/ParserATNSimulator.h"
#include "atn/PredicateTransition.h"
#include "support/CPPUtils.h"

#include "FailedPredicateException.h"

using namespace antlr4;
using namespace antlrcpp;

FailedPredicateException::FailedPredicateException(Parser* recognizer)
    : FailedPredicateException(recognizer, "", "") {}

FailedPredicateException::FailedPredicateException(Parser* recognizer,
                                                   const std::string& predicate)
    : FailedPredicateException(recognizer, predicate, "") {}

FailedPredicateException::FailedPredicateException(Parser* recognizer,
                                                   const std::string& predicate,
                                                   const std::string& message)
    : RecognitionException(
          !message.empty() ? message : "failed predicate: " + predicate + "?",
          recognizer, recognizer->getInputStream(), recognizer->getContext(),
          recognizer->getCurrentToken()) {
  atn::ATNState* s = recognizer->getInterpreter<atn::ATNSimulator>()
                         ->atn.states[recognizer->getState()];
  atn::Transition* transition = s->transitions[0];
  if (is<atn::PredicateTransition*>(transition)) {
    _ruleIndex = static_cast<atn::PredicateTransition*>(transition)->ruleIndex;
    _predicateIndex =
        static_cast<atn::PredicateTransition*>(transition)->predIndex;
  } else {
    _ruleIndex = 0;
    _predicateIndex = 0;
  }

  _predicate = predicate;
}

size_t FailedPredicateException::getRuleIndex() { return _ruleIndex; }

size_t FailedPredicateException::getPredIndex() { return _predicateIndex; }

std::string FailedPredicateException::getPredicate() { return _predicate; }
