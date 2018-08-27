/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {
namespace tree {
namespace xpath {

/// Represent a subset of XPath XML path syntax for use in identifying nodes in
/// parse trees.
///
/// <para>
/// Split path into words and separators {@code /} and {@code //} via ANTLR
/// itself then walk path elements from left to right. At each separator-word
/// pair, find set of nodes. Next stage uses those as work list.</para>
///
/// <para>
/// The basic interface is
/// <seealso cref="XPath#findAll ParseTree.findAll"/>{@code (tree, pathString,
/// parser)}. But that is just shorthand for:</para>
///
/// <pre>
/// <seealso cref="XPath"/> p = new <seealso cref="XPath#XPath XPath"/>(parser,
/// pathString); return p.<seealso cref="#evaluate evaluate"/>(tree);
/// </pre>
///
/// <para>
/// See {@code org.antlr.v4.test.TestXPath} for descriptions. In short, this
/// allows operators:</para>
///
/// <dl>
/// <dt>/</dt> <dd>root</dd>
/// <dt>//</dt> <dd>anywhere</dd>
/// <dt>!</dt> <dd>invert; this must appear directly after root or anywhere
/// operator</dd>
/// </dl>
///
/// <para>
/// and path elements:</para>
///
/// <dl>
/// <dt>ID</dt> <dd>token name</dd>
/// <dt>'string'</dt> <dd>any string literal token from the grammar</dd>
/// <dt>expr</dt> <dd>rule name</dd>
/// <dt>*</dt> <dd>wildcard matching any node</dd>
/// </dl>
///
/// <para>
/// Whitespace is not allowed.</para>

class ANTLR4CPP_PUBLIC XPath {
 public:
  static const std::string WILDCARD;  // word not operator/separator
  static const std::string NOT;       // word for invert operator

  XPath(Parser* parser, const std::string& path);
  virtual ~XPath() {}

  // TO_DO: check for invalid token/rule names, bad syntax
  virtual std::vector<XPathElement> split(const std::string& path);

  /// Return a list of all nodes starting at {@code t} as root that satisfy the
  /// path. The root {@code /} is relative to the node passed to
  /// <seealso cref="#evaluate"/>.
  virtual std::vector<ParseTree*> evaluate(ParseTree* t);

 protected:
  std::string _path;
  std::vector<XPathElement> _elements;
  Parser* _parser;

  /// Convert word like {@code *} or {@code ID} or {@code expr} to a path
  /// element. {@code anywhere} is {@code true} if {@code //} precedes the
  /// word.
  virtual XPathElement getXPathElement(Token* wordToken, bool anywhere);
};

}  // namespace xpath
}  // namespace tree
}  // namespace antlr4
