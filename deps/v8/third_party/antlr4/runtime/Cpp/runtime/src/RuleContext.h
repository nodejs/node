/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "tree/ParseTree.h"

namespace antlr4 {

/** A rule context is a record of a single rule invocation.
 *
 *  We form a stack of these context objects using the parent
 *  pointer. A parent pointer of null indicates that the current
 *  context is the bottom of the stack. The ParserRuleContext subclass
 *  as a children list so that we can turn this data structure into a
 *  tree.
 *
 *  The root node always has a null pointer and invokingState of -1.
 *
 *  Upon entry to parsing, the first invoked rule function creates a
 *  context object (asubclass specialized for that rule such as
 *  SContext) and makes it the root of a parse tree, recorded by field
 *  Parser._ctx.
 *
 *  public final SContext s() throws RecognitionException {
 *      SContext _localctx = new SContext(_ctx, getState()); <-- create new node
 *      enterRule(_localctx, 0, RULE_s);                     <-- push it
 *      ...
 *      exitRule();                                          <-- pop back to
 * _localctx return _localctx;
 *  }
 *
 *  A subsequent rule invocation of r from the start rule s pushes a
 *  new context object for r whose parent points at s and use invoking
 *  state is the state with r emanating as edge label.
 *
 *  The invokingState fields from a context object to the root
 *  together form a stack of rule indication states where the root
 *  (bottom of the stack) has a -1 sentinel value. If we invoke start
 *  symbol s then call r1, which calls r2, the  would look like
 *  this:
 *
 *     SContext[-1]   <- root node (bottom of the stack)
 *     R1Context[p]   <- p in rule s called r1
 *     R2Context[q]   <- q in rule r1 called r2
 *
 *  So the top of the stack, _ctx, represents a call to the current
 *  rule and it holds the return address from another rule that invoke
 *  to this rule. To invoke a rule, we must always have a current context.
 *
 *  The parent contexts are useful for computing lookahead sets and
 *  getting error information.
 *
 *  These objects are used during parsing and prediction.
 *  For the special case of parsers, we use the subclass
 *  ParserRuleContext.
 *
 *  @see ParserRuleContext
 */
class ANTLR4CPP_PUBLIC RuleContext : public tree::ParseTree {
 public:
  /// What state invoked the rule associated with this context?
  /// The "return address" is the followState of invokingState
  /// If parent is null, this should be -1 and this context object represents
  /// the start rule.
  size_t invokingState;

  RuleContext();
  RuleContext(RuleContext* parent, size_t invokingState);

  virtual int depth();

  /// A context is empty if there is no invoking state; meaning nobody called
  /// current context.
  virtual bool isEmpty();

  // satisfy the ParseTree / SyntaxTree interface

  virtual misc::Interval getSourceInterval() override;

  virtual std::string getText() override;

  virtual size_t getRuleIndex() const;

  /** For rule associated with this parse tree internal node, return
   *  the outer alternative number used to match the input. Default
   *  implementation does not compute nor store this alt num. Create
   *  a subclass of ParserRuleContext with backing field and set
   *  option contextSuperClass.
   *  to set it.
   *
   *  @since 4.5.3
   */
  virtual size_t getAltNumber() const;

  /** Set the outer alternative number for this context node. Default
   *  implementation does nothing to avoid backing field overhead for
   *  trees that don't need it.  Create
   *  a subclass of ParserRuleContext with backing field and set
   *  option contextSuperClass.
   *
   *  @since 4.5.3
   */
  virtual void setAltNumber(size_t altNumber);

  virtual antlrcpp::Any accept(tree::ParseTreeVisitor* visitor) override;

  /// <summary>
  /// Print out a whole tree, not just a node, in LISP format
  ///  (root child1 .. childN). Print just a node if this is a leaf.
  ///  We have to know the recognizer so we can get rule names.
  /// </summary>
  virtual std::string toStringTree(Parser* recog) override;

  /// <summary>
  /// Print out a whole tree, not just a node, in LISP format
  ///  (root child1 .. childN). Print just a node if this is a leaf.
  /// </summary>
  virtual std::string toStringTree(std::vector<std::string>& ruleNames);

  virtual std::string toStringTree() override;
  virtual std::string toString() override;
  std::string toString(Recognizer* recog);
  std::string toString(const std::vector<std::string>& ruleNames);

  // recog null unless ParserRuleContext, in which case we use subclass
  // toString(...)
  std::string toString(Recognizer* recog, RuleContext* stop);

  virtual std::string toString(const std::vector<std::string>& ruleNames,
                               RuleContext* stop);

  bool operator==(const RuleContext& other) {
    return this == &other;
  }  // Simple address comparison.

 private:
  void InitializeInstanceFields();
};

}  // namespace antlr4
