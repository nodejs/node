/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Exceptions.h"
#include "tree/ParseTreeVisitor.h"

#include "tree/ErrorNodeImpl.h"

using namespace antlr4;
using namespace antlr4::misc;
using namespace antlr4::tree;

ErrorNodeImpl::ErrorNodeImpl(Token* token) : TerminalNodeImpl(token) {}

ErrorNodeImpl::~ErrorNodeImpl() {}

antlrcpp::Any ErrorNodeImpl::accept(ParseTreeVisitor* visitor) {
  return visitor->visitErrorNode(this);
}
