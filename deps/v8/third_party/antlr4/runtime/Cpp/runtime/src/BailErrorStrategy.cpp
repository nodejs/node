/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Exceptions.h"
#include "InputMismatchException.h"
#include "Parser.h"
#include "ParserRuleContext.h"

#include "BailErrorStrategy.h"

using namespace antlr4;

void BailErrorStrategy::recover(Parser* recognizer, std::exception_ptr e) {
  ParserRuleContext* context = recognizer->getContext();
  do {
    context->exception = e;
    if (context->parent == nullptr) break;
    context = static_cast<ParserRuleContext*>(context->parent);
  } while (true);

  try {
    std::rethrow_exception(
        e);  // Throw the exception to be able to catch and rethrow nested.
#if defined(_MSC_FULL_VER) && _MSC_FULL_VER < 190023026
  } catch (RecognitionException& inner) {
    throw ParseCancellationException(inner.what());
#else
  } catch (RecognitionException& /*inner*/) {
    std::throw_with_nested(ParseCancellationException());
#endif
  }
}

Token* BailErrorStrategy::recoverInline(Parser* recognizer) {
  InputMismatchException e(recognizer);
  std::exception_ptr exception = std::make_exception_ptr(e);

  ParserRuleContext* context = recognizer->getContext();
  do {
    context->exception = exception;
    if (context->parent == nullptr) break;
    context = static_cast<ParserRuleContext*>(context->parent);
  } while (true);

  try {
    throw e;
#if defined(_MSC_FULL_VER) && _MSC_FULL_VER < 190023026
  } catch (InputMismatchException& inner) {
    throw ParseCancellationException(inner.what());
#else
  } catch (InputMismatchException& /*inner*/) {
    std::throw_with_nested(ParseCancellationException());
#endif
  }
}

void BailErrorStrategy::sync(Parser* /*recognizer*/) {}
