/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "XPath.h"
#include "tree/ParseTree.h"
#include "tree/Trees.h"

#include "XPathWildcardElement.h"

using namespace antlr4::tree;
using namespace antlr4::tree::xpath;

XPathWildcardElement::XPathWildcardElement() : XPathElement(XPath::WILDCARD) {}

std::vector<ParseTree*> XPathWildcardElement::evaluate(ParseTree* t) {
  if (_invert) {
    return {};  // !* is weird but valid (empty)
  }

  return t->children;
}
