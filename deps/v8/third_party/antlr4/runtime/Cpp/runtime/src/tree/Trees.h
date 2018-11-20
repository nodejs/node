/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "ParserRuleContext.h"
#include "Recognizer.h"
#include "tree/TerminalNode.h"

namespace antlr4 {
namespace tree {

/// A set of utility routines useful for all kinds of ANTLR trees.
class ANTLR4CPP_PUBLIC Trees {
 public:
  /// Print out a whole tree in LISP form. getNodeText is used on the
  /// node payloads to get the text for the nodes.  Detect
  /// parse trees and extract data appropriately.
  static std::string toStringTree(ParseTree* t);

  /// Print out a whole tree in LISP form. getNodeText is used on the
  ///  node payloads to get the text for the nodes.  Detect
  ///  parse trees and extract data appropriately.
  static std::string toStringTree(ParseTree* t, Parser* recog);

  /// Print out a whole tree in LISP form. getNodeText is used on the
  /// node payloads to get the text for the nodes.  Detect
  /// parse trees and extract data appropriately.
  static std::string toStringTree(ParseTree* t,
                                  const std::vector<std::string>& ruleNames);
  static std::string getNodeText(ParseTree* t, Parser* recog);
  static std::string getNodeText(ParseTree* t,
                                 const std::vector<std::string>& ruleNames);

  /// Return a list of all ancestors of this node.  The first node of
  ///  list is the root and the last is the parent of this node.
  static std::vector<ParseTree*> getAncestors(ParseTree* t);

  /** Return true if t is u's parent or a node on path to root from u.
   *  Use == not equals().
   *
   *  @since 4.5.1
   */
  static bool isAncestorOf(ParseTree* t, ParseTree* u);
  static std::vector<ParseTree*> findAllTokenNodes(ParseTree* t, size_t ttype);
  static std::vector<ParseTree*> findAllRuleNodes(ParseTree* t,
                                                  size_t ruleIndex);
  static std::vector<ParseTree*> findAllNodes(ParseTree* t, size_t index,
                                              bool findTokens);

  /** Get all descendents; includes t itself.
   *
   * @since 4.5.1
   */
  static std::vector<ParseTree*> getDescendants(ParseTree* t);

  /** @deprecated */
  static std::vector<ParseTree*> descendants(ParseTree* t);

  /** Find smallest subtree of t enclosing range startTokenIndex..stopTokenIndex
   *  inclusively using postorder traversal.  Recursive depth-first-search.
   *
   *  @since 4.5.1
   */
  static ParserRuleContext* getRootOfSubtreeEnclosingRegion(
      ParseTree* t,
      size_t startTokenIndex,  // inclusive
      size_t stopTokenIndex);  // inclusive

  /** Return first node satisfying the pred
   *
   *  @since 4.5.1
   */
  static ParseTree* findNodeSuchThat(ParseTree* t,
                                     Ref<misc::Predicate> const& pred);

 private:
  Trees();
};

}  // namespace tree
}  // namespace antlr4
