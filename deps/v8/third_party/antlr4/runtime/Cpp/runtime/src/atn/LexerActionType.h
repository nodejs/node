/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {
namespace atn {

/// <summary>
/// Represents the serialization type of a <seealso cref="LexerAction"/>.
///
/// @author Sam Harwell
/// @since 4.2
/// </summary>
enum class LexerActionType : size_t {
  /// <summary>
  /// The type of a <seealso cref="LexerChannelAction"/> action.
  /// </summary>
  CHANNEL,
  /// <summary>
  /// The type of a <seealso cref="LexerCustomAction"/> action.
  /// </summary>
  CUSTOM,
  /// <summary>
  /// The type of a <seealso cref="LexerModeAction"/> action.
  /// </summary>
  MODE,
  /// <summary>
  /// The type of a <seealso cref="LexerMoreAction"/> action.
  /// </summary>
  MORE,
  /// <summary>
  /// The type of a <seealso cref="LexerPopModeAction"/> action.
  /// </summary>
  POP_MODE,
  /// <summary>
  /// The type of a <seealso cref="LexerPushModeAction"/> action.
  /// </summary>
  PUSH_MODE,
  /// <summary>
  /// The type of a <seealso cref="LexerSkipAction"/> action.
  /// </summary>
  SKIP,
  /// <summary>
  /// The type of a <seealso cref="LexerTypeAction"/> action.
  /// </summary>
  TYPE,
};

}  // namespace atn
}  // namespace antlr4
