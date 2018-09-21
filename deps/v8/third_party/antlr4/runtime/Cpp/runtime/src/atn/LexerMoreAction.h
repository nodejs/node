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
/// Implements the {@code more} lexer action by calling <seealso
/// cref="Lexer#more"/>.
///
/// <para>The {@code more} command does not have any parameters, so this action
/// is implemented as a singleton instance exposed by <seealso
/// cref="#INSTANCE"/>.</para>
///
/// @author Sam Harwell
/// @since 4.2
/// </summary>
class ANTLR4CPP_PUBLIC LexerMoreAction final : public LexerAction {
 public:
  /// <summary>
  /// Provides a singleton instance of this parameterless lexer action.
  /// </summary>
  static const Ref<LexerMoreAction> getInstance();

  /// <summary>
  /// {@inheritDoc} </summary>
  /// <returns> This method returns <seealso cref="LexerActionType#MORE"/>.
  /// </returns>
  virtual LexerActionType getActionType() const override;

  /// <summary>
  /// {@inheritDoc} </summary>
  /// <returns> This method returns {@code false}. </returns>
  virtual bool isPositionDependent() const override;

  /// <summary>
  /// {@inheritDoc}
  ///
  /// <para>This action is implemented by calling <seealso
  /// cref="Lexer#more"/>.</para>
  /// </summary>
  virtual void execute(Lexer* lexer) override;

  virtual size_t hashCode() const override;
  virtual bool operator==(const LexerAction& obj) const override;
  virtual std::string toString() const override;

 private:
  /// Constructs the singleton instance of the lexer {@code more} command.
  LexerMoreAction();
};

}  // namespace atn
}  // namespace antlr4
