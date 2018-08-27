/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/AmbiguityInfo.h"
#include "atn/ContextSensitivityInfo.h"
#include "atn/ErrorInfo.h"
#include "atn/PredicateEvalInfo.h"

namespace antlr4 {
namespace atn {

class LookaheadEventInfo;

/// <summary>
/// This class contains profiling gathered for a particular decision.
///
/// <para>
/// Parsing performance in ANTLR 4 is heavily influenced by both static factors
/// (e.g. the form of the rules in the grammar) and dynamic factors (e.g. the
/// choice of input and the state of the DFA cache at the time profiling
/// operations are started). For best results, gather and use aggregate
/// statistics from a large sample of inputs representing the inputs expected in
/// production before using the results to make changes in the grammar.</para>
///
/// @since 4.3
/// </summary>
class ANTLR4CPP_PUBLIC DecisionInfo {
 public:
  /// <summary>
  /// The decision number, which is an index into <seealso
  /// cref="ATN#decisionToState"/>.
  /// </summary>
  const size_t decision;

  /// <summary>
  /// The total number of times <seealso
  /// cref="ParserATNSimulator#adaptivePredict"/> was invoked for this decision.
  /// </summary>
  long long invocations = 0;

  /// <summary>
  /// The total time spent in <seealso
  /// cref="ParserATNSimulator#adaptivePredict"/> for this decision, in
  /// nanoseconds.
  ///
  /// <para>
  /// The value of this field contains the sum of differential results obtained
  /// by <seealso cref="System#nanoTime()"/>, and is not adjusted to compensate
  /// for JIT and/or garbage collection overhead. For best accuracy, use a
  /// modern JVM implementation that provides precise results from <seealso
  /// cref="System#nanoTime()"/>, and perform profiling in a separate process
  /// which is warmed up by parsing the input prior to profiling. If desired,
  /// call <seealso cref="ATNSimulator#clearDFA"/> to reset the DFA cache to its
  /// initial state before starting the profiling measurement pass.</para>
  /// </summary>
  long long timeInPrediction = 0;

  /// <summary>
  /// The sum of the lookahead required for SLL prediction for this decision.
  /// Note that SLL prediction is used before LL prediction for performance
  /// reasons even when <seealso cref="PredictionMode#LL"/> or
  /// <seealso cref="PredictionMode#LL_EXACT_AMBIG_DETECTION"/> is used.
  /// </summary>
  long long SLL_TotalLook = 0;

  /// <summary>
  /// Gets the minimum lookahead required for any single SLL prediction to
  /// complete for this decision, by reaching a unique prediction, reaching an
  /// SLL conflict state, or encountering a syntax error.
  /// </summary>
  long long SLL_MinLook = 0;

  /// <summary>
  /// Gets the maximum lookahead required for any single SLL prediction to
  /// complete for this decision, by reaching a unique prediction, reaching an
  /// SLL conflict state, or encountering a syntax error.
  /// </summary>
  long long SLL_MaxLook = 0;

  /// Gets the <seealso cref="LookaheadEventInfo"/> associated with the event
  /// where the <seealso cref="#SLL_MaxLook"/> value was set.
  Ref<LookaheadEventInfo> SLL_MaxLookEvent;

  /// <summary>
  /// The sum of the lookahead required for LL prediction for this decision.
  /// Note that LL prediction is only used when SLL prediction reaches a
  /// conflict state.
  /// </summary>
  long long LL_TotalLook = 0;

  /// <summary>
  /// Gets the minimum lookahead required for any single LL prediction to
  /// complete for this decision. An LL prediction completes when the algorithm
  /// reaches a unique prediction, a conflict state (for
  /// <seealso cref="PredictionMode#LL"/>, an ambiguity state (for
  /// <seealso cref="PredictionMode#LL_EXACT_AMBIG_DETECTION"/>, or a syntax
  /// error.
  /// </summary>
  long long LL_MinLook = 0;

  /// <summary>
  /// Gets the maximum lookahead required for any single LL prediction to
  /// complete for this decision. An LL prediction completes when the algorithm
  /// reaches a unique prediction, a conflict state (for
  /// <seealso cref="PredictionMode#LL"/>, an ambiguity state (for
  /// <seealso cref="PredictionMode#LL_EXACT_AMBIG_DETECTION"/>, or a syntax
  /// error.
  /// </summary>
  long long LL_MaxLook = 0;

  /// <summary>
  /// Gets the <seealso cref="LookaheadEventInfo"/> associated with the event
  /// where the <seealso cref="#LL_MaxLook"/> value was set.
  /// </summary>
  Ref<LookaheadEventInfo> LL_MaxLookEvent;

  /// <summary>
  /// A collection of <seealso cref="ContextSensitivityInfo"/> instances
  /// describing the context sensitivities encountered during LL prediction for
  /// this decision.
  /// </summary>
  /// <seealso cref= ContextSensitivityInfo </seealso>
  std::vector<ContextSensitivityInfo> contextSensitivities;

  /// <summary>
  /// A collection of <seealso cref="ErrorInfo"/> instances describing the parse
  /// errors identified during calls to <seealso
  /// cref="ParserATNSimulator#adaptivePredict"/> for this decision.
  /// </summary>
  /// <seealso cref= ErrorInfo </seealso>
  std::vector<ErrorInfo> errors;

  /// <summary>
  /// A collection of <seealso cref="AmbiguityInfo"/> instances describing the
  /// ambiguities encountered during LL prediction for this decision.
  /// </summary>
  /// <seealso cref= AmbiguityInfo </seealso>
  std::vector<AmbiguityInfo> ambiguities;

  /// <summary>
  /// A collection of <seealso cref="PredicateEvalInfo"/> instances describing
  /// the results of evaluating individual predicates during prediction for this
  /// decision.
  /// </summary>
  /// <seealso cref= PredicateEvalInfo </seealso>
  std::vector<PredicateEvalInfo> predicateEvals;

  /// <summary>
  /// The total number of ATN transitions required during SLL prediction for
  /// this decision. An ATN transition is determined by the number of times the
  /// DFA does not contain an edge that is required for prediction, resulting
  /// in on-the-fly computation of that edge.
  ///
  /// <para>
  /// If DFA caching of SLL transitions is employed by the implementation, ATN
  /// computation may cache the computed edge for efficient lookup during
  /// future parsing of this decision. Otherwise, the SLL parsing algorithm
  /// will use ATN transitions exclusively.</para>
  /// </summary>
  /// <seealso cref= #SLL_ATNTransitions </seealso>
  /// <seealso cref= ParserATNSimulator#computeTargetState </seealso>
  /// <seealso cref= LexerATNSimulator#computeTargetState </seealso>
  long long SLL_ATNTransitions = 0;

  /// <summary>
  /// The total number of DFA transitions required during SLL prediction for
  /// this decision.
  ///
  /// <para>If the ATN simulator implementation does not use DFA caching for SLL
  /// transitions, this value will be 0.</para>
  /// </summary>
  /// <seealso cref= ParserATNSimulator#getExistingTargetState </seealso>
  /// <seealso cref= LexerATNSimulator#getExistingTargetState </seealso>
  long long SLL_DFATransitions = 0;

  /// <summary>
  /// Gets the total number of times SLL prediction completed in a conflict
  /// state, resulting in fallback to LL prediction.
  ///
  /// <para>Note that this value is not related to whether or not
  /// <seealso cref="PredictionMode#SLL"/> may be used successfully with a
  /// particular grammar. If the ambiguity resolution algorithm applied to the
  /// SLL conflicts for this decision produce the same result as LL prediction
  /// for this decision, <seealso cref="PredictionMode#SLL"/> would produce the
  /// same overall parsing result as <seealso cref="PredictionMode#LL"/>.</para>
  /// </summary>
  long long LL_Fallback = 0;

  /// <summary>
  /// The total number of ATN transitions required during LL prediction for
  /// this decision. An ATN transition is determined by the number of times the
  /// DFA does not contain an edge that is required for prediction, resulting
  /// in on-the-fly computation of that edge.
  ///
  /// <para>
  /// If DFA caching of LL transitions is employed by the implementation, ATN
  /// computation may cache the computed edge for efficient lookup during
  /// future parsing of this decision. Otherwise, the LL parsing algorithm will
  /// use ATN transitions exclusively.</para>
  /// </summary>
  /// <seealso cref= #LL_DFATransitions </seealso>
  /// <seealso cref= ParserATNSimulator#computeTargetState </seealso>
  /// <seealso cref= LexerATNSimulator#computeTargetState </seealso>
  long long LL_ATNTransitions = 0;

  /// <summary>
  /// The total number of DFA transitions required during LL prediction for
  /// this decision.
  ///
  /// <para>If the ATN simulator implementation does not use DFA caching for LL
  /// transitions, this value will be 0.</para>
  /// </summary>
  /// <seealso cref= ParserATNSimulator#getExistingTargetState </seealso>
  /// <seealso cref= LexerATNSimulator#getExistingTargetState </seealso>
  long long LL_DFATransitions = 0;

  /// <summary>
  /// Constructs a new instance of the <seealso cref="DecisionInfo"/> class to
  /// contain statistics for a particular decision.
  /// </summary>
  /// <param name="decision"> The decision number </param>
  DecisionInfo(size_t decision);

  std::string toString() const;
};

}  // namespace atn
}  // namespace antlr4
