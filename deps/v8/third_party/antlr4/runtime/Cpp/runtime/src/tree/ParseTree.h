/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "support/Any.h"

namespace antlr4 {
namespace tree {

/// An interface to access the tree of <seealso cref="RuleContext"/> objects
/// created during a parse that makes the data structure look like a simple
/// parse tree. This node represents both internal nodes, rule invocations, and
/// leaf nodes, token matches.
///
/// The payload is either a <seealso cref="Token"/> or a <seealso
/// cref="RuleContext"/> object.
// ml: This class unites 4 Java classes: RuleNode, ParseTree, SyntaxTree and
// Tree.
class ANTLR4CPP_PUBLIC ParseTree {
 public:
  ParseTree();
  ParseTree(ParseTree const&) = delete;
  virtual ~ParseTree() {}

  ParseTree& operator=(ParseTree const&) = delete;

  /// The parent of this node. If the return value is null, then this
  /// node is the root of the tree.
  ParseTree* parent;

  /// If we are debugging or building a parse tree for a visitor,
  /// we need to track all of the tokens and rule invocations associated
  /// with this rule's context. This is empty for parsing w/o tree constr.
  /// operation because we don't the need to track the details about
  /// how we parse this rule.
  // ml: memory is not managed here, but by the owning class. This is just for
  // the structure.
  std::vector<ParseTree*> children;

  /// Print out a whole tree, not just a node, in LISP format
  /// {@code (root child1 .. childN)}. Print just a node if this is a leaf.
  virtual std::string toStringTree() = 0;
  virtual std::string toString() = 0;

  /// Specialize toStringTree so that it can print out more information
  /// based upon the parser.
  virtual std::string toStringTree(Parser* parser) = 0;

  virtual bool operator==(const ParseTree& other) const;

  /// The <seealso cref="ParseTreeVisitor"/> needs a double dispatch method.
  // ml: This has been changed to use Any instead of a template parameter, to
  // avoid the need of a virtual template function.
  virtual antlrcpp::Any accept(ParseTreeVisitor* visitor) = 0;

  /// Return the combined text of all leaf nodes. Does not get any
  /// off-channel tokens (if any) so won't return whitespace and
  /// comments if they are sent to parser on hidden channel.
  virtual std::string getText() = 0;

  /**
   * Return an {@link Interval} indicating the index in the
   * {@link TokenStream} of the first and last token associated with this
   * subtree. If this node is a leaf, then the interval represents a single
   * token and has interval i..i for token index i.
   *
   * <p>An interval of i..i-1 indicates an empty interval at position
   * i in the input stream, where 0 &lt;= i &lt;= the size of the input
   * token stream.  Currently, the code base can only have i=0..n-1 but
   * in concept one could have an empty interval after EOF. </p>
   *
   * <p>If source interval is unknown, this returns {@link
   * Interval#INVALID}.</p>
   *
   * <p>As a weird special case, the source interval for rules matched after
   * EOF is unspecified.</p>
   */
  virtual misc::Interval getSourceInterval() = 0;
};

// A class to help managing ParseTree instances without the need of a
// shared_ptr.
class ANTLR4CPP_PUBLIC ParseTreeTracker {
 public:
  template <typename T, typename... Args>
  T* createInstance(Args&&... args) {
    static_assert(std::is_base_of<ParseTree, T>::value,
                  "Argument must be a parse tree type");
    T* result = new T(args...);
    _allocated.push_back(result);
    return result;
  }

  void reset() {
    for (auto entry : _allocated) delete entry;
    _allocated.clear();
  }

 private:
  std::vector<ParseTree*> _allocated;
};

}  // namespace tree
}  // namespace antlr4
