/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "tree/pattern/TokenTagToken.h"

using namespace antlr4::tree::pattern;

TokenTagToken::TokenTagToken(const std::string& /*tokenName*/, int type)
    : CommonToken(type), tokenName(""), label("") {}

TokenTagToken::TokenTagToken(const std::string& tokenName, int type,
                             const std::string& label)
    : CommonToken(type), tokenName(tokenName), label(label) {}

std::string TokenTagToken::getTokenName() const { return tokenName; }

std::string TokenTagToken::getLabel() const { return label; }

std::string TokenTagToken::getText() const {
  if (!label.empty()) {
    return "<" + label + ":" + tokenName + ">";
  }

  return "<" + tokenName + ">";
}

std::string TokenTagToken::toString() const {
  return tokenName + ":" + std::to_string(_type);
}
