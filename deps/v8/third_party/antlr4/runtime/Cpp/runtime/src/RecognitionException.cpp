/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "ParserRuleContext.h"
#include "Recognizer.h"
#include "atn/ATN.h"
#include "misc/IntervalSet.h"
#include "support/StringUtils.h"

#include "RecognitionException.h"

using namespace antlr4;

RecognitionException::RecognitionException(Recognizer* recognizer,
                                           IntStream* input,
                                           ParserRuleContext* ctx,
                                           Token* offendingToken)
    : RecognitionException("", recognizer, input, ctx, offendingToken) {}

RecognitionException::RecognitionException(const std::string& message,
                                           Recognizer* recognizer,
                                           IntStream* input,
                                           ParserRuleContext* ctx,
                                           Token* offendingToken)
    : RuntimeException(message),
      _recognizer(recognizer),
      _input(input),
      _ctx(ctx),
      _offendingToken(offendingToken) {
  InitializeInstanceFields();
  if (recognizer != nullptr) {
    _offendingState = recognizer->getState();
  }
}

RecognitionException::~RecognitionException() {}

size_t RecognitionException::getOffendingState() const {
  return _offendingState;
}

void RecognitionException::setOffendingState(size_t offendingState) {
  _offendingState = offendingState;
}

misc::IntervalSet RecognitionException::getExpectedTokens() const {
  if (_recognizer) {
    return _recognizer->getATN().getExpectedTokens(_offendingState, _ctx);
  }
  return misc::IntervalSet::EMPTY_SET;
}

RuleContext* RecognitionException::getCtx() const { return _ctx; }

IntStream* RecognitionException::getInputStream() const { return _input; }

Token* RecognitionException::getOffendingToken() const {
  return _offendingToken;
}

Recognizer* RecognitionException::getRecognizer() const { return _recognizer; }

void RecognitionException::InitializeInstanceFields() {
  _offendingState = INVALID_INDEX;
}
