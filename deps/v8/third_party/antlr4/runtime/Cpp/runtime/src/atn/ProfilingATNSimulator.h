/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/DecisionInfo.h"
#include "atn/ParserATNSimulator.h"

namespace antlr4 {
namespace atn {

class ANTLR4CPP_PUBLIC ProfilingATNSimulator : public ParserATNSimulator {
 public:
  ProfilingATNSimulator(Parser* parser);

  virtual size_t adaptivePredict(TokenStream* input, size_t decision,
                                 ParserRuleContext* outerContext) override;

  virtual std::vector<DecisionInfo> getDecisionInfo() const;
  virtual dfa::DFAState* getCurrentState() const;

 protected:
  std::vector<DecisionInfo> _decisions;

  int _sllStopIndex = 0;
  int _llStopIndex = 0;

  size_t _currentDecision = 0;
  dfa::DFAState* _currentState;

  /// <summary>
  /// At the point of LL failover, we record how SLL would resolve the conflict
  /// so that
  ///  we can determine whether or not a decision / input pair is
  ///  context-sensitive. If LL gives a different result than SLL's predicted
  ///  alternative, we have a context sensitivity for sure. The converse is not
  ///  necessarily true, however. It's possible that after conflict resolution
  ///  chooses minimum alternatives, SLL could get the same answer as LL.
  ///  Regardless of whether or not the result indicates an ambiguity, it is not
  ///  treated as a context sensitivity because LL prediction was not required
  ///  in order to produce a correct prediction for this decision and input
  ///  sequence. It may in fact still be a context sensitivity but we don't know
  ///  by looking at the minimum alternatives for the current input.
  /// </summary>
  size_t conflictingAltResolvedBySLL = 0;

  virtual dfa::DFAState* getExistingTargetState(dfa::DFAState* previousD,
                                                size_t t) override;
  virtual dfa::DFAState* computeTargetState(dfa::DFA& dfa,
                                            dfa::DFAState* previousD,
                                            size_t t) override;
  virtual std::unique_ptr<ATNConfigSet> computeReachSet(ATNConfigSet* closure,
                                                        size_t t,
                                                        bool fullCtx) override;
  virtual bool evalSemanticContext(Ref<SemanticContext> const& pred,
                                   ParserRuleContext* parserCallStack,
                                   size_t alt, bool fullCtx) override;
  virtual void reportAttemptingFullContext(
      dfa::DFA& dfa, const antlrcpp::BitSet& conflictingAlts,
      ATNConfigSet* configs, size_t startIndex, size_t stopIndex) override;
  virtual void reportContextSensitivity(dfa::DFA& dfa, size_t prediction,
                                        ATNConfigSet* configs,
                                        size_t startIndex,
                                        size_t stopIndex) override;
  virtual void reportAmbiguity(dfa::DFA& dfa, dfa::DFAState* D,
                               size_t startIndex, size_t stopIndex, bool exact,
                               const antlrcpp::BitSet& ambigAlts,
                               ATNConfigSet* configs) override;
};

}  // namespace atn
}  // namespace antlr4
