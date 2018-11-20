/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/LexerAction.h"
#include "atn/LexerActionType.h"

namespace antlr4 {
namespace atn {

/// Implements the {@code type} lexer action by calling <seealso
/// cref="Lexer#setType"/> with the assigned type.
class ANTLR4CPP_PUBLIC LexerTypeAction : public LexerAction {
 public:
  /// <summary>
  /// Constructs a new {@code type} action with the specified token type value.
  /// </summary> <param name="type"> The type to assign to the token using
  /// <seealso cref="Lexer#setType"/>. </param>
  LexerTypeAction(int type);

  /// <summary>
  /// Gets the type to assign to a token created by the lexer. </summary>
  /// <returns> The type to assign to a token created by the lexer. </returns>
  virtual int getType() const;

  /// <summary>
  /// {@inheritDoc} </summary>
  /// <returns> This method returns <seealso cref="LexerActionType#TYPE"/>.
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
  /// cref="Lexer#setType"/> with the value provided by <seealso
  /// cref="#getType"/>.</para>
  /// </summary>
  virtual void execute(Lexer* lexer) override;

  virtual size_t hashCode() const override;
  virtual bool operator==(const LexerAction& obj) const override;
  virtual std::string toString() const override;

 private:
  const int _type;
};

}  // namespace atn
}  // namespace antlr4
