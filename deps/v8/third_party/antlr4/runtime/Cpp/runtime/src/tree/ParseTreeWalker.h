/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {
namespace tree {

class ANTLR4CPP_PUBLIC ParseTreeWalker {
 public:
  static ParseTreeWalker& DEFAULT;

  virtual ~ParseTreeWalker();

  virtual void walk(ParseTreeListener* listener, ParseTree* t) const;

 protected:
  /// The discovery of a rule node, involves sending two events: the generic
  /// <seealso cref="ParseTreeListener#enterEveryRule"/> and a
  /// <seealso cref="RuleContext"/>-specific event. First we trigger the generic
  /// and then the rule specific. We do them in reverse order upon finishing the
  /// node.
  virtual void enterRule(ParseTreeListener* listener, ParseTree* r) const;
  virtual void exitRule(ParseTreeListener* listener, ParseTree* r) const;
};

}  // namespace tree
}  // namespace antlr4
