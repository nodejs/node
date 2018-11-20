/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "RecognitionException.h"
#include "Token.h"
#include "atn/ATNConfigSet.h"

namespace antlr4 {

/// Indicates that the parser could not decide which of two or more paths
/// to take based upon the remaining input. It tracks the starting token
/// of the offending input and also knows where the parser was
/// in the various paths when the error. Reported by reportNoViableAlternative()
class ANTLR4CPP_PUBLIC NoViableAltException : public RecognitionException {
 public:
  NoViableAltException(Parser* recognizer);  // LL(1) error
  NoViableAltException(Parser* recognizer, TokenStream* input,
                       Token* startToken, Token* offendingToken,
                       atn::ATNConfigSet* deadEndConfigs,
                       ParserRuleContext* ctx);

  virtual Token* getStartToken() const;
  virtual atn::ATNConfigSet* getDeadEndConfigs() const;

 private:
  /// Which configurations did we try at input.index() that couldn't match
  /// input.LT(1)?
  atn::ATNConfigSet* _deadEndConfigs;

  /// The token object at the start index; the input stream might
  /// not be buffering tokens so get a reference to it. (At the
  /// time the error occurred, of course the stream needs to keep a
  /// buffer all of the tokens but later we might not have access to those.)
  Token* _startToken;
};

}  // namespace antlr4
