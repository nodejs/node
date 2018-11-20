/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "tree/ParseTreeVisitor.h"

namespace antlr4 {
namespace tree {

class ANTLR4CPP_PUBLIC AbstractParseTreeVisitor : public ParseTreeVisitor {
 public:
  /// The default implementation calls <seealso cref="ParseTree#accept"/> on the
  /// specified tree.
  virtual antlrcpp::Any visit(ParseTree* tree) override {
    return tree->accept(this);
  }

  /**
   * <p>The default implementation initializes the aggregate result to
   * {@link #defaultResult defaultResult()}. Before visiting each child, it
   * calls {@link #shouldVisitNextChild shouldVisitNextChild}; if the result
   * is {@code false} no more children are visited and the current aggregate
   * result is returned. After visiting a child, the aggregate result is
   * updated by calling {@link #aggregateResult aggregateResult} with the
   * previous aggregate result and the result of visiting the child.</p>
   *
   * <p>The default implementation is not safe for use in visitors that modify
   * the tree structure. Visitors that modify the tree should override this
   * method to behave properly in respect to the specific algorithm in use.</p>
   */
  virtual antlrcpp::Any visitChildren(ParseTree* node) override {
    antlrcpp::Any result = defaultResult();
    size_t n = node->children.size();
    for (size_t i = 0; i < n; i++) {
      if (!shouldVisitNextChild(node, result)) {
        break;
      }

      antlrcpp::Any childResult = node->children[i]->accept(this);
      result = aggregateResult(result, childResult);
    }

    return result;
  }

  /// The default implementation returns the result of
  /// <seealso cref="#defaultResult defaultResult"/>.
  virtual antlrcpp::Any visitTerminal(TerminalNode* /*node*/) override {
    return defaultResult();
  }

  /// The default implementation returns the result of
  /// <seealso cref="#defaultResult defaultResult"/>.
  virtual antlrcpp::Any visitErrorNode(ErrorNode* /*node*/) override {
    return defaultResult();
  }

 protected:
  /// <summary>
  /// Gets the default value returned by visitor methods. This value is
  /// returned by the default implementations of
  /// <seealso cref="#visitTerminal visitTerminal"/>, <seealso
  /// cref="#visitErrorNode visitErrorNode"/>. The default implementation of
  /// <seealso cref="#visitChildren visitChildren"/> initializes its aggregate
  /// result to this value. <p/> The base implementation returns {@code null}.
  /// </summary>
  /// <returns> The default value returned by visitor methods. </returns>
  virtual antlrcpp::Any defaultResult() {
    return nullptr;  // support isNotNull
  }

  /// <summary>
  /// Aggregates the results of visiting multiple children of a node. After
  /// either all children are visited or <seealso cref="#shouldVisitNextChild"/>
  /// returns
  /// {@code false}, the aggregate value is returned as the result of
  /// <seealso cref="#visitChildren"/>.
  /// <p/>
  /// The default implementation returns {@code nextResult}, meaning
  /// <seealso cref="#visitChildren"/> will return the result of the last child
  /// visited (or return the initial value if the node has no children).
  /// </summary>
  /// <param name="aggregate"> The previous aggregate value. In the default
  /// implementation, the aggregate value is initialized to
  /// <seealso cref="#defaultResult"/>, which is passed as the {@code aggregate}
  /// argument to this method after the first child node is visited. </param>
  /// <param name="nextResult"> The result of the immediately preceeding call to
  /// visit a child node.
  /// </param>
  /// <returns> The updated aggregate result. </returns>
  virtual antlrcpp::Any aggregateResult(antlrcpp::Any /*aggregate*/,
                                        const antlrcpp::Any& nextResult) {
    return nextResult;
  }

  /// <summary>
  /// This method is called after visiting each child in
  /// <seealso cref="#visitChildren"/>. This method is first called before the
  /// first child is visited; at that point {@code currentResult} will be the
  /// initial value (in the default implementation, the initial value is
  /// returned by a call to <seealso cref="#defaultResult"/>. This method is not
  /// called after the last child is visited. <p/> The default implementation
  /// always returns {@code true}, indicating that
  /// {@code visitChildren} should only return after all children are visited.
  /// One reason to override this method is to provide a "short circuit"
  /// evaluation option for situations where the result of visiting a single
  /// child has the potential to determine the result of the visit operation as
  /// a whole.
  /// </summary>
  /// <param name="node"> The <seealso cref="ParseTree"/> whose children are
  /// currently being visited. </param> <param name="currentResult"> The current
  /// aggregate result of the children visited to the current point.
  /// </param>
  /// <returns> {@code true} to continue visiting children. Otherwise return
  /// {@code false} to stop visiting children and immediately return the
  /// current aggregate result from <seealso cref="#visitChildren"/>. </returns>
  virtual bool shouldVisitNextChild(ParseTree* /*node*/,
                                    const antlrcpp::Any& /*currentResult*/) {
    return true;
  }
};

}  // namespace tree
}  // namespace antlr4
