/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "CharStream.h"
#include "Lexer.h"
#include "misc/Interval.h"
#include "support/CPPUtils.h"

#include "LexerNoViableAltException.h"

using namespace antlr4;

LexerNoViableAltException::LexerNoViableAltException(
    Lexer* lexer, CharStream* input, size_t startIndex,
    atn::ATNConfigSet* deadEndConfigs)
    : RecognitionException(lexer, input, nullptr, nullptr),
      _startIndex(startIndex),
      _deadEndConfigs(deadEndConfigs) {}

size_t LexerNoViableAltException::getStartIndex() { return _startIndex; }

atn::ATNConfigSet* LexerNoViableAltException::getDeadEndConfigs() {
  return _deadEndConfigs;
}

std::string LexerNoViableAltException::toString() {
  std::string symbol;
  if (_startIndex < getInputStream()->size()) {
    symbol = static_cast<CharStream*>(getInputStream())
                 ->getText(misc::Interval(_startIndex, _startIndex));
    symbol = antlrcpp::escapeWhitespace(symbol, false);
  }
  std::string format = "LexerNoViableAltException('" + symbol + "')";
  return format;
}
