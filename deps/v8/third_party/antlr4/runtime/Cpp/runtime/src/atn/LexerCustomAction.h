/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/LexerAction.h"
#include "atn/LexerActionType.h"

namespace antlr4 {
namespace atn {

/// <summary>
/// Executes a custom lexer action by calling <seealso
/// cref="Recognizer#action"/> with the rule and action indexes assigned to the
/// custom action. The implementation of a custom action is added to the
/// generated code for the lexer in an override of <seealso
/// cref="Recognizer#action"/> when the grammar is compiled.
///
/// <para>This class may represent embedded actions created with the
/// <code>{...}</code> syntax in ANTLR 4, as well as actions created for lexer
/// commands where the command argument could not be evaluated when the grammar
/// was compiled.</para>
///
/// @author Sam Harwell
/// @since 4.2
/// </summary>
class ANTLR4CPP_PUBLIC LexerCustomAction final : public LexerAction {
 public:
  /// <summary>
  /// Constructs a custom lexer action with the specified rule and action
  /// indexes.
  /// </summary>
  /// <param name="ruleIndex"> The rule index to use for calls to
  /// <seealso cref="Recognizer#action"/>. </param>
  /// <param name="actionIndex"> The action index to use for calls to
  /// <seealso cref="Recognizer#action"/>. </param>
  LexerCustomAction(size_t ruleIndex, size_t actionIndex);

  /// <summary>
  /// Gets the rule index to use for calls to <seealso
  /// cref="Recognizer#action"/>.
  /// </summary>
  /// <returns> The rule index for the custom action. </returns>
  size_t getRuleIndex() const;

  /// <summary>
  /// Gets the action index to use for calls to <seealso
  /// cref="Recognizer#action"/>.
  /// </summary>
  /// <returns> The action index for the custom action. </returns>
  size_t getActionIndex() const;

  /// <summary>
  /// {@inheritDoc}
  /// </summary>
  /// <returns> This method returns <seealso cref="LexerActionType#CUSTOM"/>.
  /// </returns>
  virtual LexerActionType getActionType() const override;

  /// <summary>
  /// Gets whether the lexer action is position-dependent. Position-dependent
  /// actions may have different semantics depending on the <seealso
  /// cref="CharStream"/> index at the time the action is executed.
  ///
  /// <para>Custom actions are position-dependent since they may represent a
  /// user-defined embedded action which makes calls to methods like
  /// <seealso cref="Lexer#getText"/>.</para>
  /// </summary>
  /// <returns> This method returns {@code true}. </returns>
  virtual bool isPositionDependent() const override;

  /// <summary>
  /// {@inheritDoc}
  ///
  /// <para>Custom actions are implemented by calling <seealso
  /// cref="Lexer#action"/> with the appropriate rule and action indexes.</para>
  /// </summary>
  virtual void execute(Lexer* lexer) override;

  virtual size_t hashCode() const override;
  virtual bool operator==(const LexerAction& obj) const override;
  virtual std::string toString() const override;

 private:
  const size_t _ruleIndex;
  const size_t _actionIndex;
};

}  // namespace atn
}  // namespace antlr4
