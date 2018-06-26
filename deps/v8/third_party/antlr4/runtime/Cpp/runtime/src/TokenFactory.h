/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {

/// The default mechanism for creating tokens. It's used by default in Lexer and
///  the error handling strategy (to create missing tokens).  Notifying the
///  parser of a new factory means that it notifies it's token source and error
///  strategy.
template <typename Symbol>
class ANTLR4CPP_PUBLIC TokenFactory {
 public:
  virtual ~TokenFactory() {}

  /// This is the method used to create tokens in the lexer and in the
  /// error handling strategy. If text!=null, than the start and stop positions
  /// are wiped to -1 in the text override is set in the CommonToken.
  virtual std::unique_ptr<Symbol> create(
      std::pair<TokenSource*, CharStream*> source, size_t type,
      const std::string& text, size_t channel, size_t start, size_t stop,
      size_t line, size_t charPositionInLine) = 0;

  /// Generically useful
  virtual std::unique_ptr<Symbol> create(size_t type,
                                         const std::string& text) = 0;
};

}  // namespace antlr4
