/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "tree/ParseTree.h"

namespace antlr4 {
namespace tree {

class ANTLR4CPP_PUBLIC TerminalNode : public ParseTree {
 public:
  ~TerminalNode() override;

  virtual Token* getSymbol() = 0;

  /** Set the parent for this leaf node.
   *
   *  Technically, this is not backward compatible as it changes
   *  the interface but no one was able to create custom
   *  TerminalNodes anyway so I'm adding as it improves internal
   *  code quality.
   *
   *  @since 4.7
   */
  virtual void setParent(RuleContext* parent) = 0;
};

}  // namespace tree
}  // namespace antlr4
