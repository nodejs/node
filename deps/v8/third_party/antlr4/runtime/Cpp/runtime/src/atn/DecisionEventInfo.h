/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {
namespace atn {

/// <summary>
/// This is the base class for gathering detailed information about prediction
/// events which occur during parsing.
///
/// Note that we could record the parser call stack at the time this event
/// occurred but in the presence of left recursive rules, the stack is kind of
/// meaningless. It's better to look at the individual configurations for their
/// individual stacks. Of course that is a <seealso cref="PredictionContext"/>
/// object not a parse tree node and so it does not have information about the
/// extent (start...stop) of the various subtrees. Examining the stack tops of
/// all configurations provide the return states for the rule invocations. From
/// there you can get the enclosing rule.
///
/// @since 4.3
/// </summary>
class ANTLR4CPP_PUBLIC DecisionEventInfo {
 public:
  /// <summary>
  /// The invoked decision number which this event is related to.
  /// </summary>
  /// <seealso cref= ATN#decisionToState </seealso>
  const size_t decision;

  /// <summary>
  /// The configuration set containing additional information relevant to the
  /// prediction state when the current event occurred, or {@code null} if no
  /// additional information is relevant or available.
  /// </summary>
  const ATNConfigSet* configs;

  /// <summary>
  /// The input token stream which is being parsed.
  /// </summary>
  const TokenStream* input;

  /// <summary>
  /// The token index in the input stream at which the current prediction was
  /// originally invoked.
  /// </summary>
  const size_t startIndex;

  /// <summary>
  /// The token index in the input stream at which the current event occurred.
  /// </summary>
  const size_t stopIndex;

  /// <summary>
  /// {@code true} if the current event occurred during LL prediction;
  /// otherwise, {@code false} if the input occurred during SLL prediction.
  /// </summary>
  const bool fullCtx;

  DecisionEventInfo(size_t decision, ATNConfigSet* configs, TokenStream* input,
                    size_t startIndex, size_t stopIndex, bool fullCtx);
};

}  // namespace atn
}  // namespace antlr4
