/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Parser.h"
#include "atn/ATNConfig.h"
#include "atn/ATNConfigSet.h"
#include "atn/PredictionContext.h"
#include "dfa/DFA.h"
#include "misc/Interval.h"

#include "DiagnosticErrorListener.h"

using namespace antlr4;

DiagnosticErrorListener::DiagnosticErrorListener()
    : DiagnosticErrorListener(true) {}

DiagnosticErrorListener::DiagnosticErrorListener(bool exactOnly_)
    : exactOnly(exactOnly_) {}

void DiagnosticErrorListener::reportAmbiguity(Parser* recognizer,
                                              const dfa::DFA& dfa,
                                              size_t startIndex,
                                              size_t stopIndex, bool exact,
                                              const antlrcpp::BitSet& ambigAlts,
                                              atn::ATNConfigSet* configs) {
  if (exactOnly && !exact) {
    return;
  }

  std::string decision = getDecisionDescription(recognizer, dfa);
  antlrcpp::BitSet conflictingAlts = getConflictingAlts(ambigAlts, configs);
  std::string text = recognizer->getTokenStream()->getText(
      misc::Interval(startIndex, stopIndex));
  std::string message = "reportAmbiguity d=" + decision +
                        ": ambigAlts=" + conflictingAlts.toString() +
                        ", input='" + text + "'";

  recognizer->notifyErrorListeners(message);
}

void DiagnosticErrorListener::reportAttemptingFullContext(
    Parser* recognizer, const dfa::DFA& dfa, size_t startIndex,
    size_t stopIndex, const antlrcpp::BitSet& /*conflictingAlts*/,
    atn::ATNConfigSet* /*configs*/) {
  std::string decision = getDecisionDescription(recognizer, dfa);
  std::string text = recognizer->getTokenStream()->getText(
      misc::Interval(startIndex, stopIndex));
  std::string message =
      "reportAttemptingFullContext d=" + decision + ", input='" + text + "'";
  recognizer->notifyErrorListeners(message);
}

void DiagnosticErrorListener::reportContextSensitivity(
    Parser* recognizer, const dfa::DFA& dfa, size_t startIndex,
    size_t stopIndex, size_t /*prediction*/, atn::ATNConfigSet* /*configs*/) {
  std::string decision = getDecisionDescription(recognizer, dfa);
  std::string text = recognizer->getTokenStream()->getText(
      misc::Interval(startIndex, stopIndex));
  std::string message =
      "reportContextSensitivity d=" + decision + ", input='" + text + "'";
  recognizer->notifyErrorListeners(message);
}

std::string DiagnosticErrorListener::getDecisionDescription(
    Parser* recognizer, const dfa::DFA& dfa) {
  size_t decision = dfa.decision;
  size_t ruleIndex =
      (reinterpret_cast<atn::ATNState*>(dfa.atnStartState))->ruleIndex;

  const std::vector<std::string>& ruleNames = recognizer->getRuleNames();
  if (ruleIndex == INVALID_INDEX || ruleIndex >= ruleNames.size()) {
    return std::to_string(decision);
  }

  std::string ruleName = ruleNames[ruleIndex];
  if (ruleName == "" || ruleName.empty()) {
    return std::to_string(decision);
  }

  return std::to_string(decision) + " (" + ruleName + ")";
}

antlrcpp::BitSet DiagnosticErrorListener::getConflictingAlts(
    const antlrcpp::BitSet& reportedAlts, atn::ATNConfigSet* configs) {
  if (reportedAlts.count() > 0) {  // Not exactly like the original Java code,
                                   // but this listener is only used in the
                                   // TestRig (where it never provides a good
                                   // alt set), so it's probably ok so.
    return reportedAlts;
  }

  antlrcpp::BitSet result;
  for (auto& config : configs->configs) {
    result.set(config->alt);
  }

  return result;
}
