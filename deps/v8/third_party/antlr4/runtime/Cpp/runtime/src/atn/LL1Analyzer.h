/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "Token.h"
#include "atn/ATNConfig.h"
#include "atn/PredictionContext.h"
#include "support/BitSet.h"

namespace antlr4 {
namespace atn {

class ANTLR4CPP_PUBLIC LL1Analyzer {
 public:
  /// Special value added to the lookahead sets to indicate that we hit
  ///  a predicate during analysis if {@code seeThruPreds==false}.
  static const size_t HIT_PRED = Token::INVALID_TYPE;

  const atn::ATN& _atn;

  LL1Analyzer(const atn::ATN& atn);
  virtual ~LL1Analyzer();

  /// <summary>
  /// Calculates the SLL(1) expected lookahead set for each outgoing transition
  /// of an <seealso cref="ATNState"/>. The returned array has one element for
  /// each outgoing transition in {@code s}. If the closure from transition
  /// <em>i</em> leads to a semantic predicate before matching a symbol, the
  /// element at index <em>i</em> of the result will be {@code null}.
  /// </summary>
  /// <param name="s"> the ATN state </param>
  /// <returns> the expected symbols for each outgoing transition of {@code s}.
  /// </returns>
  virtual std::vector<misc::IntervalSet> getDecisionLookahead(
      ATNState* s) const;

  /// <summary>
  /// Compute set of tokens that can follow {@code s} in the ATN in the
  /// specified {@code ctx}.
  /// <p/>
  /// If {@code ctx} is {@code null} and the end of the rule containing
  /// {@code s} is reached, <seealso cref="Token#EPSILON"/> is added to the
  /// result set. If {@code ctx} is not {@code null} and the end of the
  /// outermost rule is reached, <seealso cref="Token#EOF"/> is added to the
  /// result set.
  /// </summary>
  /// <param name="s"> the ATN state </param>
  /// <param name="ctx"> the complete parser context, or {@code null} if the
  /// context should be ignored
  /// </param>
  /// <returns> The set of tokens that can follow {@code s} in the ATN in the
  /// specified {@code ctx}. </returns>
  virtual misc::IntervalSet LOOK(ATNState* s, RuleContext* ctx) const;

  /// <summary>
  /// Compute set of tokens that can follow {@code s} in the ATN in the
  /// specified {@code ctx}.
  /// <p/>
  /// If {@code ctx} is {@code null} and the end of the rule containing
  /// {@code s} is reached, <seealso cref="Token#EPSILON"/> is added to the
  /// result set. If {@code ctx} is not {@code null} and the end of the
  /// outermost rule is reached, <seealso cref="Token#EOF"/> is added to the
  /// result set.
  /// </summary>
  /// <param name="s"> the ATN state </param>
  /// <param name="stopState"> the ATN state to stop at. This can be a
  /// <seealso cref="BlockEndState"/> to detect epsilon paths through a closure.
  /// </param> <param name="ctx"> the complete parser context, or {@code null}
  /// if the context should be ignored
  /// </param>
  /// <returns> The set of tokens that can follow {@code s} in the ATN in the
  /// specified {@code ctx}. </returns>
  virtual misc::IntervalSet LOOK(ATNState* s, ATNState* stopState,
                                 RuleContext* ctx) const;

  /// <summary>
  /// Compute set of tokens that can follow {@code s} in the ATN in the
  /// specified {@code ctx}.
  /// <p/>
  /// If {@code ctx} is {@code null} and {@code stopState} or the end of the
  /// rule containing {@code s} is reached, <seealso cref="Token#EPSILON"/> is
  /// added to the result set. If {@code ctx} is not {@code null} and {@code
  /// addEOF} is
  /// {@code true} and {@code stopState} or the end of the outermost rule is
  /// reached, <seealso cref="Token#EOF"/> is added to the result set.
  /// </summary>
  /// <param name="s"> the ATN state. </param>
  /// <param name="stopState"> the ATN state to stop at. This can be a
  /// <seealso cref="BlockEndState"/> to detect epsilon paths through a closure.
  /// </param> <param name="ctx"> The outer context, or {@code null} if the
  /// outer context should not be used. </param> <param name="look"> The result
  /// lookahead set. </param> <param name="lookBusy"> A set used for preventing
  /// epsilon closures in the ATN from causing a stack overflow. Outside code
  /// should pass
  /// {@code new HashSet<ATNConfig>} for this argument. </param>
  /// <param name="calledRuleStack"> A set used for preventing left recursion in
  /// the ATN from causing a stack overflow. Outside code should pass
  /// {@code new BitSet()} for this argument. </param>
  /// <param name="seeThruPreds"> {@code true} to true semantic predicates as
  /// implicitly {@code true} and "see through them", otherwise {@code false}
  /// to treat semantic predicates as opaque and add <seealso cref="#HIT_PRED"/>
  /// to the result if one is encountered. </param> <param name="addEOF"> Add
  /// <seealso cref="Token#EOF"/> to the result if the end of the outermost
  /// context is reached. This parameter has no effect if {@code ctx} is {@code
  /// null}. </param>
 protected:
  virtual void _LOOK(ATNState* s, ATNState* stopState,
                     Ref<PredictionContext> const& ctx, misc::IntervalSet& look,
                     ATNConfig::Set& lookBusy,
                     antlrcpp::BitSet& calledRuleStack, bool seeThruPreds,
                     bool addEOF) const;
};

}  // namespace atn
}  // namespace antlr4
