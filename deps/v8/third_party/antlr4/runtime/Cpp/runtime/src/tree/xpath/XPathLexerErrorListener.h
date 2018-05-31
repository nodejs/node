/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "BaseErrorListener.h"

namespace antlr4 {
namespace tree {
namespace xpath {

class ANTLR4CPP_PUBLIC XPathLexerErrorListener : public BaseErrorListener {
 public:
  virtual void syntaxError(Recognizer* recognizer, Token* offendingSymbol,
                           size_t line, size_t charPositionInLine,
                           const std::string& msg,
                           std::exception_ptr e) override;
};

}  // namespace xpath
}  // namespace tree
}  // namespace antlr4
