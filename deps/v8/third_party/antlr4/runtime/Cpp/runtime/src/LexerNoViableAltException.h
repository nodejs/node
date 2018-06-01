/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "RecognitionException.h"
#include "atn/ATNConfigSet.h"

namespace antlr4 {

class ANTLR4CPP_PUBLIC LexerNoViableAltException : public RecognitionException {
 public:
  LexerNoViableAltException(Lexer* lexer, CharStream* input, size_t startIndex,
                            atn::ATNConfigSet* deadEndConfigs);

  virtual size_t getStartIndex();
  virtual atn::ATNConfigSet* getDeadEndConfigs();
  virtual std::string toString();

 private:
  /// Matching attempted at what input index?
  const size_t _startIndex;

  /// Which configurations did we try at input.index() that couldn't match
  /// input.LA(1)?
  atn::ATNConfigSet* _deadEndConfigs;
};

}  // namespace antlr4
