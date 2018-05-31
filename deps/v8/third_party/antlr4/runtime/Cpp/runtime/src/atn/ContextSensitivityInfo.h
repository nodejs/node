/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/DecisionEventInfo.h"

namespace antlr4 {
namespace atn {

/// <summary>
/// This class represents profiling event information for a context sensitivity.
/// Context sensitivities are decisions where a particular input resulted in an
/// SLL conflict, but LL prediction produced a single unique alternative.
///
/// <para>
/// In some cases, the unique alternative identified by LL prediction is not
/// equal to the minimum represented alternative in the conflicting SLL
/// configuration set. Grammars and inputs which result in this scenario are
/// unable to use <seealso cref="PredictionMode#SLL"/>, which in turn means they
/// cannot use the two-stage parsing strategy to improve parsing performance for
/// that input.</para>
/// </summary>
/// <seealso cref= ParserATNSimulator#reportContextSensitivity </seealso>
/// <seealso cref= ANTLRErrorListener#reportContextSensitivity
///
/// @since 4.3 </seealso>
class ANTLR4CPP_PUBLIC ContextSensitivityInfo : public DecisionEventInfo {
 public:
  /// <summary>
  /// Constructs a new instance of the <seealso cref="ContextSensitivityInfo"/>
  /// class with the specified detailed context sensitivity information.
  /// </summary>
  /// <param name="decision"> The decision number </param>
  /// <param name="configs"> The final configuration set containing the unique
  /// alternative identified by full-context prediction </param>
  /// <param name="input"> The input token stream </param>
  /// <param name="startIndex"> The start index for the current prediction
  /// </param> <param name="stopIndex"> The index at which the context
  /// sensitivity was identified during full-context prediction </param>
  ContextSensitivityInfo(size_t decision, ATNConfigSet* configs,
                         TokenStream* input, size_t startIndex,
                         size_t stopIndex);
};

}  // namespace atn
}  // namespace antlr4
