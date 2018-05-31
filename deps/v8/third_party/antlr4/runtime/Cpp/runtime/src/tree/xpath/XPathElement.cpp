/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "support/CPPUtils.h"

#include "XPathElement.h"

using namespace antlr4::tree;
using namespace antlr4::tree::xpath;

XPathElement::XPathElement(const std::string& nodeName) {
  _nodeName = nodeName;
}

XPathElement::~XPathElement() {}

std::vector<ParseTree*> XPathElement::evaluate(ParseTree* /*t*/) { return {}; }

std::string XPathElement::toString() const {
  std::string inv = _invert ? "!" : "";
  return antlrcpp::toString(*this) + "[" + inv + _nodeName + "]";
}

void XPathElement::setInvert(bool value) { _invert = value; }
