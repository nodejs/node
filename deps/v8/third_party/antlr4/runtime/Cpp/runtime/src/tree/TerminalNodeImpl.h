/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "tree/TerminalNode.h"

namespace antlr4 {
namespace tree {

class ANTLR4CPP_PUBLIC TerminalNodeImpl : public virtual TerminalNode {
 public:
  Token* symbol;

  TerminalNodeImpl(Token* symbol);

  virtual Token* getSymbol() override;
  virtual void setParent(RuleContext* parent) override;
  virtual misc::Interval getSourceInterval() override;

  virtual antlrcpp::Any accept(ParseTreeVisitor* visitor) override;

  virtual std::string getText() override;
  virtual std::string toStringTree(Parser* parser) override;
  virtual std::string toString() override;
  virtual std::string toStringTree() override;
};

}  // namespace tree
}  // namespace antlr4
