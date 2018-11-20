/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Token.h"
#include "support/CPPUtils.h"
#include "tree/ParseTree.h"
#include "tree/Trees.h"

#include "XPathTokenElement.h"

using namespace antlr4;
using namespace antlr4::tree;
using namespace antlr4::tree::xpath;

XPathTokenElement::XPathTokenElement(const std::string& tokenName,
                                     size_t tokenType)
    : XPathElement(tokenName) {
  _tokenType = tokenType;
}

std::vector<ParseTree*> XPathTokenElement::evaluate(ParseTree* t) {
  // return all children of t that match nodeName
  std::vector<ParseTree*> nodes;
  for (auto c : t->children) {
    if (antlrcpp::is<TerminalNode*>(c)) {
      TerminalNode* tnode = dynamic_cast<TerminalNode*>(c);
      if ((tnode->getSymbol()->getType() == _tokenType && !_invert) ||
          (tnode->getSymbol()->getType() != _tokenType && _invert)) {
        nodes.push_back(tnode);
      }
    }
  }
  return nodes;
}
