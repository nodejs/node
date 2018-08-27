/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {
namespace tree {
class ParseTree;

namespace xpath {

class ANTLR4CPP_PUBLIC XPathElement {
 public:
  /// Construct element like {@code /ID} or {@code ID} or {@code /*} etc...
  ///  op is null if just node
  XPathElement(const std::string& nodeName);
  XPathElement(XPathElement const&) = default;
  virtual ~XPathElement();

  XPathElement& operator=(XPathElement const&) = default;

  /// Given tree rooted at {@code t} return all nodes matched by this path
  /// element.
  virtual std::vector<ParseTree*> evaluate(ParseTree* t);
  virtual std::string toString() const;

  void setInvert(bool value);

 protected:
  std::string _nodeName;
  bool _invert = false;
};

}  // namespace xpath
}  // namespace tree
}  // namespace antlr4
