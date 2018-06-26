/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/DecisionEventInfo.h"

namespace antlr4 {
namespace atn {

/// This class represents profiling event information for tracking the lookahead
/// depth required in order to make a prediction.
class ANTLR4CPP_PUBLIC LookaheadEventInfo : public DecisionEventInfo {
 public:
  /// The alternative chosen by adaptivePredict(), not necessarily
  ///  the outermost alt shown for a rule; left-recursive rules have
  ///  user-level alts that differ from the rewritten rule with a (...) block
  ///  and a (..)* loop.
  size_t predictedAlt = 0;

  /// <summary>
  /// Constructs a new instance of the <seealso cref="LookaheadEventInfo"/>
  /// class with the specified detailed lookahead information.
  /// </summary>
  /// <param name="decision"> The decision number </param>
  /// <param name="configs"> The final configuration set containing the
  /// necessary information to determine the result of a prediction, or {@code
  /// null} if the final configuration set is not available </param> <param
  /// name="input"> The input token stream </param> <param name="startIndex">
  /// The start index for the current prediction </param> <param
  /// name="stopIndex"> The index at which the prediction was finally made
  /// </param> <param name="fullCtx"> {@code true} if the current lookahead is
  /// part of an LL prediction; otherwise, {@code false} if the current
  /// lookahead is part of an SLL prediction </param>
  LookaheadEventInfo(size_t decision, ATNConfigSet* configs,
                     size_t predictedAlt, TokenStream* input, size_t startIndex,
                     size_t stopIndex, bool fullCtx);
};

}  // namespace atn
}  // namespace antlr4
