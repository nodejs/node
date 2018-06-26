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
/// Implements the {@code pushMode} lexer action by calling
/// <seealso cref="Lexer#pushMode"/> with the assigned mode.
///
/// @author Sam Harwell
/// @since 4.2
/// </summary>
class ANTLR4CPP_PUBLIC LexerPushModeAction final : public LexerAction {
 public:
  /// <summary>
  /// Constructs a new {@code pushMode} action with the specified mode value.
  /// </summary> <param name="mode"> The mode value to pass to <seealso
  /// cref="Lexer#pushMode"/>. </param>
  LexerPushModeAction(int mode);

  /// <summary>
  /// Get the lexer mode this action should transition the lexer to.
  /// </summary>
  /// <returns> The lexer mode for this {@code pushMode} command. </returns>
  int getMode() const;

  /// <summary>
  /// {@inheritDoc} </summary>
  /// <returns> This method returns <seealso cref="LexerActionType#PUSH_MODE"/>.
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
  /// cref="Lexer#pushMode"/> with the value provided by <seealso
  /// cref="#getMode"/>.</para>
  /// </summary>
  virtual void execute(Lexer* lexer) override;

  virtual size_t hashCode() const override;
  virtual bool operator==(const LexerAction& obj) const override;
  virtual std::string toString() const override;

 private:
  const int _mode;
};

}  // namespace atn
}  // namespace antlr4
