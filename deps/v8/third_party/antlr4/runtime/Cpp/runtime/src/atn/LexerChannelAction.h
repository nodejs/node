/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/LexerAction.h"
#include "atn/LexerActionType.h"

namespace antlr4 {
namespace atn {

using antlr4::Lexer;

/// <summary>
/// Implements the {@code channel} lexer action by calling
/// <seealso cref="Lexer#setChannel"/> with the assigned channel.
///
/// @author Sam Harwell
/// @since 4.2
/// </summary>
class ANTLR4CPP_PUBLIC LexerChannelAction final : public LexerAction {
 public:
  /// <summary>
  /// Constructs a new {@code channel} action with the specified channel value.
  /// </summary> <param name="channel"> The channel value to pass to <seealso
  /// cref="Lexer#setChannel"/>. </param>
  LexerChannelAction(int channel);

  /// <summary>
  /// Gets the channel to use for the <seealso cref="Token"/> created by the
  /// lexer.
  /// </summary>
  /// <returns> The channel to use for the <seealso cref="Token"/> created by
  /// the lexer. </returns>
  int getChannel() const;

  /// <summary>
  /// {@inheritDoc} </summary>
  /// <returns> This method returns <seealso cref="LexerActionType#CHANNEL"/>.
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
  /// cref="Lexer#setChannel"/> with the value provided by <seealso
  /// cref="#getChannel"/>.</para>
  /// </summary>
  virtual void execute(Lexer* lexer) override;

  virtual size_t hashCode() const override;
  virtual bool operator==(const LexerAction& obj) const override;
  virtual std::string toString() const override;

 private:
  const int _channel;
};

}  // namespace atn
}  // namespace antlr4
