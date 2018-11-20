/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/DecisionEventInfo.h"

namespace antlr4 {
namespace atn {

/// <summary>
/// This class represents profiling event information for semantic predicate
/// evaluations which occur during prediction.
/// </summary>
/// <seealso cref= ParserATNSimulator#evalSemanticContext
///
/// @since 4.3 </seealso>
class ANTLR4CPP_PUBLIC PredicateEvalInfo : public DecisionEventInfo {
 public:
  /// The semantic context which was evaluated.
  const Ref<SemanticContext> semctx;

  /// <summary>
  /// The alternative number for the decision which is guarded by the semantic
  /// context <seealso cref="#semctx"/>. Note that other ATN
  /// configurations may predict the same alternative which are guarded by
  /// other semantic contexts and/or <seealso cref="SemanticContext#NONE"/>.
  /// </summary>
  const size_t predictedAlt;

  /// The result of evaluating the semantic context <seealso cref="#semctx"/>.
  const bool evalResult;

  /// <summary>
  /// Constructs a new instance of the <seealso cref="PredicateEvalInfo"/> class
  /// with the specified detailed predicate evaluation information.
  /// </summary>
  /// <param name="decision"> The decision number </param>
  /// <param name="input"> The input token stream </param>
  /// <param name="startIndex"> The start index for the current prediction
  /// </param> <param name="stopIndex"> The index at which the predicate
  /// evaluation was triggered. Note that the input stream may be reset to other
  /// positions for the actual evaluation of individual predicates. </param>
  /// <param name="semctx"> The semantic context which was evaluated </param>
  /// <param name="evalResult"> The results of evaluating the semantic context
  /// </param> <param name="predictedAlt"> The alternative number for the
  /// decision which is guarded by the semantic context {@code semctx}. See
  /// <seealso cref="#predictedAlt"/> for more information. </param> <param
  /// name="fullCtx"> {@code true} if the semantic context was evaluated during
  /// LL prediction; otherwise, {@code false} if the semantic context was
  /// evaluated during SLL prediction
  /// </param>
  /// <seealso cref= ParserATNSimulator#evalSemanticContext(SemanticContext,
  /// ParserRuleContext, int, boolean) </seealso> <seealso cref=
  /// SemanticContext#eval(Recognizer, RuleContext) </seealso>
  PredicateEvalInfo(size_t decision, TokenStream* input, size_t startIndex,
                    size_t stopIndex, Ref<SemanticContext> const& semctx,
                    bool evalResult, size_t predictedAlt, bool fullCtx);
};

}  // namespace atn
}  // namespace antlr4
