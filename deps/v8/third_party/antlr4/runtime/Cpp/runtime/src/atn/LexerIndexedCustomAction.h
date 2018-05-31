/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "RuleContext.h"
#include "atn/LexerAction.h"

namespace antlr4 {
namespace atn {

/// <summary>
/// This implementation of <seealso cref="LexerAction"/> is used for tracking
/// input offsets for position-dependent actions within a <seealso
/// cref="LexerActionExecutor"/>.
///
/// <para>This action is not serialized as part of the ATN, and is only required
/// for position-dependent lexer actions which appear at a location other than
/// the end of a rule. For more information about DFA optimizations employed for
/// lexer actions, see <seealso cref="LexerActionExecutor#append"/> and
/// <seealso cref="LexerActionExecutor#fixOffsetBeforeMatch"/>.</para>
///
/// @author Sam Harwell
/// @since 4.2
/// </summary>
class ANTLR4CPP_PUBLIC LexerIndexedCustomAction final : public LexerAction {
 public:
  /// <summary>
  /// Constructs a new indexed custom action by associating a character offset
  /// with a <seealso cref="LexerAction"/>.
  ///
  /// <para>Note: This class is only required for lexer actions for which
  /// <seealso cref="LexerAction#isPositionDependent"/> returns {@code
  /// true}.</para>
  /// </summary>
  /// <param name="offset"> The offset into the input <seealso
  /// cref="CharStream"/>, relative to the token start index, at which the
  /// specified lexer action should be executed. </param> <param name="action">
  /// The lexer action to execute at a particular offset in the input <seealso
  /// cref="CharStream"/>. </param>
  LexerIndexedCustomAction(int offset, Ref<LexerAction> const& action);

  /// <summary>
  /// Gets the location in the input <seealso cref="CharStream"/> at which the
  /// lexer action should be executed. The value is interpreted as an offset
  /// relative to the token start index.
  /// </summary>
  /// <returns> The location in the input <seealso cref="CharStream"/> at which
  /// the lexer action should be executed. </returns>
  int getOffset() const;

  /// <summary>
  /// Gets the lexer action to execute.
  /// </summary>
  /// <returns> A <seealso cref="LexerAction"/> object which executes the lexer
  /// action. </returns>
  Ref<LexerAction> getAction() const;

  /// <summary>
  /// {@inheritDoc}
  /// </summary>
  /// <returns> This method returns the result of calling <seealso
  /// cref="#getActionType"/> on the <seealso cref="LexerAction"/> returned by
  /// <seealso cref="#getAction"/>. </returns>
  virtual LexerActionType getActionType() const override;

  /// <summary>
  /// {@inheritDoc} </summary>
  /// <returns> This method returns {@code true}. </returns>
  virtual bool isPositionDependent() const override;

  virtual void execute(Lexer* lexer) override;
  virtual size_t hashCode() const override;
  virtual bool operator==(const LexerAction& obj) const override;
  virtual std::string toString() const override;

 private:
  const int _offset;
  const Ref<LexerAction> _action;
};

}  // namespace atn
}  // namespace antlr4
