/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "XPathLexerErrorListener.h"

using namespace antlr4;
using namespace antlr4::tree::xpath;

void XPathLexerErrorListener::syntaxError(Recognizer* /*recognizer*/,
                                          Token* /*offendingSymbol*/,
                                          size_t /*line*/,
                                          size_t /*charPositionInLine*/,
                                          const std::string& /*msg*/,
                                          std::exception_ptr /*e*/) {}
