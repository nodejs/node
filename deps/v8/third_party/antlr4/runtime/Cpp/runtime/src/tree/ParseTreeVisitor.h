/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "support/Any.h"

namespace antlr4 {
namespace tree {

/// <summary>
/// This interface defines the basic notion of a parse tree visitor. Generated
/// visitors implement this interface and the {@code XVisitor} interface for
/// grammar {@code X}.
/// </summary>
/// @param <T> The return type of the visit operation. Use <seealso
/// cref="Void"/> for operations with no return type. </param>
// ml: no template parameter here, to avoid the need for virtual template
// functions. Instead we have our Any class.
class ANTLR4CPP_PUBLIC ParseTreeVisitor {
 public:
  virtual ~ParseTreeVisitor();

  /// <summary>
  /// Visit a parse tree, and return a user-defined result of the operation.
  /// </summary>
  /// <param name="tree"> The <seealso cref="ParseTree"/> to visit. </param>
  /// <returns> The result of visiting the parse tree. </returns>
  virtual antlrcpp::Any visit(ParseTree* tree) = 0;

  /// <summary>
  /// Visit the children of a node, and return a user-defined result of the
  /// operation.
  /// </summary>
  /// <param name="node"> The <seealso cref="ParseTree"/> whose children should
  /// be visited. </param> <returns> The result of visiting the children of the
  /// node. </returns>
  virtual antlrcpp::Any visitChildren(ParseTree* node) = 0;

  /// <summary>
  /// Visit a terminal node, and return a user-defined result of the operation.
  /// </summary>
  /// <param name="node"> The <seealso cref="TerminalNode"/> to visit. </param>
  /// <returns> The result of visiting the node. </returns>
  virtual antlrcpp::Any visitTerminal(TerminalNode* node) = 0;

  /// <summary>
  /// Visit an error node, and return a user-defined result of the operation.
  /// </summary>
  /// <param name="node"> The <seealso cref="ErrorNode"/> to visit. </param>
  /// <returns> The result of visiting the node. </returns>
  virtual antlrcpp::Any visitErrorNode(ErrorNode* node) = 0;
};

}  // namespace tree
}  // namespace antlr4
