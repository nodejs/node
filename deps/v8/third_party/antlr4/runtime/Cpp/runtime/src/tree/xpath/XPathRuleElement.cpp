/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "tree/ParseTree.h"
#include "tree/Trees.h"

#include "XPathRuleElement.h"

using namespace antlr4::tree;
using namespace antlr4::tree::xpath;

XPathRuleElement::XPathRuleElement(const std::string& ruleName,
                                   size_t ruleIndex)
    : XPathElement(ruleName) {
  _ruleIndex = ruleIndex;
}

std::vector<ParseTree*> XPathRuleElement::evaluate(ParseTree* t) {
  // return all children of t that match nodeName
  std::vector<ParseTree*> nodes;
  for (auto c : t->children) {
    if (antlrcpp::is<ParserRuleContext*>(c)) {
      ParserRuleContext* ctx = dynamic_cast<ParserRuleContext*>(c);
      if ((ctx->getRuleIndex() == _ruleIndex && !_invert) ||
          (ctx->getRuleIndex() != _ruleIndex && _invert)) {
        nodes.push_back(ctx);
      }
    }
  }
  return nodes;
}
