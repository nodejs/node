/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "Token.h"

namespace antlr4 {

class ANTLR4CPP_PUBLIC WritableToken : public Token {
 public:
  virtual ~WritableToken();
  virtual void setText(const std::string& text) = 0;
  virtual void setType(size_t ttype) = 0;
  virtual void setLine(size_t line) = 0;
  virtual void setCharPositionInLine(size_t pos) = 0;
  virtual void setChannel(size_t channel) = 0;
  virtual void setTokenIndex(size_t index) = 0;
};

}  // namespace antlr4
