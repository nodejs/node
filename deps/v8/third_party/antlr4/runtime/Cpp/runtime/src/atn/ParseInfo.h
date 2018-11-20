/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/DecisionInfo.h"

namespace antlr4 {
namespace atn {

class ProfilingATNSimulator;

/// This class provides access to specific and aggregate statistics gathered
/// during profiling of a parser.
class ANTLR4CPP_PUBLIC ParseInfo {
 public:
  ParseInfo(ProfilingATNSimulator* atnSimulator);
  ParseInfo(ParseInfo const&) = default;
  virtual ~ParseInfo();

  ParseInfo& operator=(ParseInfo const&) = default;

  /// <summary>
  /// Gets an array of <seealso cref="DecisionInfo"/> instances containing the
  /// profiling information gathered for each decision in the ATN.
  /// </summary>
  /// <returns> An array of <seealso cref="DecisionInfo"/> instances, indexed by
  /// decision number. </returns>
  virtual std::vector<DecisionInfo> getDecisionInfo();

  /// <summary>
  /// Gets the decision numbers for decisions that required one or more
  /// full-context predictions during parsing. These are decisions for which
  /// <seealso cref="DecisionInfo#LL_Fallback"/> is non-zero.
  /// </summary>
  /// <returns> A list of decision numbers which required one or more
  /// full-context predictions during parsing. </returns>
  virtual std::vector<size_t> getLLDecisions();

  /// <summary>
  /// Gets the total time spent during prediction across all decisions made
  /// during parsing. This value is the sum of
  /// <seealso cref="DecisionInfo#timeInPrediction"/> for all decisions.
  /// </summary>
  virtual long long getTotalTimeInPrediction();

  /// <summary>
  /// Gets the total number of SLL lookahead operations across all decisions
  /// made during parsing. This value is the sum of
  /// <seealso cref="DecisionInfo#SLL_TotalLook"/> for all decisions.
  /// </summary>
  virtual long long getTotalSLLLookaheadOps();

  /// <summary>
  /// Gets the total number of LL lookahead operations across all decisions
  /// made during parsing. This value is the sum of
  /// <seealso cref="DecisionInfo#LL_TotalLook"/> for all decisions.
  /// </summary>
  virtual long long getTotalLLLookaheadOps();

  /// <summary>
  /// Gets the total number of ATN lookahead operations for SLL prediction
  /// across all decisions made during parsing.
  /// </summary>
  virtual long long getTotalSLLATNLookaheadOps();

  /// <summary>
  /// Gets the total number of ATN lookahead operations for LL prediction
  /// across all decisions made during parsing.
  /// </summary>
  virtual long long getTotalLLATNLookaheadOps();

  /// <summary>
  /// Gets the total number of ATN lookahead operations for SLL and LL
  /// prediction across all decisions made during parsing.
  ///
  /// <para>
  /// This value is the sum of <seealso cref="#getTotalSLLATNLookaheadOps"/> and
  /// <seealso cref="#getTotalLLATNLookaheadOps"/>.</para>
  /// </summary>
  virtual long long getTotalATNLookaheadOps();

  /// <summary>
  /// Gets the total number of DFA states stored in the DFA cache for all
  /// decisions in the ATN.
  /// </summary>
  virtual size_t getDFASize();

  /// <summary>
  /// Gets the total number of DFA states stored in the DFA cache for a
  /// particular decision.
  /// </summary>
  virtual size_t getDFASize(size_t decision);

 protected:
  const ProfilingATNSimulator*
      _atnSimulator;  // non-owning, we are created by this simulator.
};

}  // namespace atn
}  // namespace antlr4
