/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"
#include "atn/LexerActionType.h"

namespace antlr4 {
namespace atn {

/// <summary>
/// Represents a single action which can be executed following the successful
/// match of a lexer rule. Lexer actions are used for both embedded action
/// syntax and ANTLR 4's new lexer command syntax.
///
/// @author Sam Harwell
/// @since 4.2
/// </summary>
class ANTLR4CPP_PUBLIC LexerAction {
 public:
  virtual ~LexerAction();

  /// <summary>
  /// Gets the serialization type of the lexer action.
  /// </summary>
  /// <returns> The serialization type of the lexer action. </returns>
  virtual LexerActionType getActionType() const = 0;

  /// <summary>
  /// Gets whether the lexer action is position-dependent. Position-dependent
  /// actions may have different semantics depending on the <seealso
  /// cref="CharStream"/> index at the time the action is executed.
  ///
  /// <para>Many lexer commands, including {@code type}, {@code skip}, and
  /// {@code more}, do not check the input index during their execution.
  /// Actions like this are position-independent, and may be stored more
  /// efficiently as part of the <seealso
  /// cref="LexerATNConfig#lexerActionExecutor"/>.</para>
  /// </summary>
  /// <returns> {@code true} if the lexer action semantics can be affected by
  /// the position of the input <seealso cref="CharStream"/> at the time it is
  /// executed; otherwise, {@code false}. </returns>
  virtual bool isPositionDependent() const = 0;

  /// <summary>
  /// Execute the lexer action in the context of the specified <seealso
  /// cref="Lexer"/>.
  ///
  /// <para>For position-dependent actions, the input stream must already be
  /// positioned correctly prior to calling this method.</para>
  /// </summary>
  /// <param name="lexer"> The lexer instance. </param>
  virtual void execute(Lexer* lexer) = 0;

  virtual size_t hashCode() const = 0;
  virtual bool operator==(const LexerAction& obj) const = 0;
  virtual bool operator!=(const LexerAction& obj) const {
    return !(*this == obj);
  }

  virtual std::string toString() const = 0;
};

}  // namespace atn
}  // namespace antlr4
