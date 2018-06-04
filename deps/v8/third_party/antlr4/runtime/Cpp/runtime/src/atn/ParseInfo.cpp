/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/ProfilingATNSimulator.h"
#include "dfa/DFA.h"

#include "atn/ParseInfo.h"

using namespace antlr4::atn;

ParseInfo::ParseInfo(ProfilingATNSimulator* atnSimulator)
    : _atnSimulator(atnSimulator) {}

ParseInfo::~ParseInfo() {}

std::vector<DecisionInfo> ParseInfo::getDecisionInfo() {
  return _atnSimulator->getDecisionInfo();
}

std::vector<size_t> ParseInfo::getLLDecisions() {
  std::vector<DecisionInfo> decisions = _atnSimulator->getDecisionInfo();
  std::vector<size_t> LL;
  for (size_t i = 0; i < decisions.size(); ++i) {
    long long fallBack = decisions[i].LL_Fallback;
    if (fallBack > 0) {
      LL.push_back(i);
    }
  }
  return LL;
}

long long ParseInfo::getTotalTimeInPrediction() {
  std::vector<DecisionInfo> decisions = _atnSimulator->getDecisionInfo();
  long long t = 0;
  for (size_t i = 0; i < decisions.size(); ++i) {
    t += decisions[i].timeInPrediction;
  }
  return t;
}

long long ParseInfo::getTotalSLLLookaheadOps() {
  std::vector<DecisionInfo> decisions = _atnSimulator->getDecisionInfo();
  long long k = 0;
  for (size_t i = 0; i < decisions.size(); ++i) {
    k += decisions[i].SLL_TotalLook;
  }
  return k;
}

long long ParseInfo::getTotalLLLookaheadOps() {
  std::vector<DecisionInfo> decisions = _atnSimulator->getDecisionInfo();
  long long k = 0;
  for (size_t i = 0; i < decisions.size(); i++) {
    k += decisions[i].LL_TotalLook;
  }
  return k;
}

long long ParseInfo::getTotalSLLATNLookaheadOps() {
  std::vector<DecisionInfo> decisions = _atnSimulator->getDecisionInfo();
  long long k = 0;
  for (size_t i = 0; i < decisions.size(); ++i) {
    k += decisions[i].SLL_ATNTransitions;
  }
  return k;
}

long long ParseInfo::getTotalLLATNLookaheadOps() {
  std::vector<DecisionInfo> decisions = _atnSimulator->getDecisionInfo();
  long long k = 0;
  for (size_t i = 0; i < decisions.size(); ++i) {
    k += decisions[i].LL_ATNTransitions;
  }
  return k;
}

long long ParseInfo::getTotalATNLookaheadOps() {
  std::vector<DecisionInfo> decisions = _atnSimulator->getDecisionInfo();
  long long k = 0;
  for (size_t i = 0; i < decisions.size(); ++i) {
    k += decisions[i].SLL_ATNTransitions;
    k += decisions[i].LL_ATNTransitions;
  }
  return k;
}

size_t ParseInfo::getDFASize() {
  size_t n = 0;
  std::vector<dfa::DFA>& decisionToDFA = _atnSimulator->decisionToDFA;
  for (size_t i = 0; i < decisionToDFA.size(); ++i) {
    n += getDFASize(i);
  }
  return n;
}

size_t ParseInfo::getDFASize(size_t decision) {
  dfa::DFA& decisionToDFA = _atnSimulator->decisionToDFA[decision];
  return decisionToDFA.states.size();
}
