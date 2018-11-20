/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/ErrorInfo.h"
#include "atn/LookaheadEventInfo.h"

#include "atn/DecisionInfo.h"

using namespace antlr4::atn;

DecisionInfo::DecisionInfo(size_t decision) : decision(decision) {}

std::string DecisionInfo::toString() const {
  std::stringstream ss;

  ss << "{decision=" << decision
     << ", contextSensitivities=" << contextSensitivities.size() << ", errors=";
  ss << errors.size() << ", ambiguities=" << ambiguities.size()
     << ", SLL_lookahead=" << SLL_TotalLook;
  ss << ", SLL_ATNTransitions=" << SLL_ATNTransitions
     << ", SLL_DFATransitions=" << SLL_DFATransitions;
  ss << ", LL_Fallback=" << LL_Fallback << ", LL_lookahead=" << LL_TotalLook
     << ", LL_ATNTransitions=" << LL_ATNTransitions << '}';

  return ss.str();
}
