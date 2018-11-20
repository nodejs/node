/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Vocabulary.h"
#include "dfa/DFA.h"

#include "dfa/DFASerializer.h"

using namespace antlr4::dfa;

DFASerializer::DFASerializer(const DFA* dfa,
                             const std::vector<std::string>& tokenNames)
    : DFASerializer(dfa, Vocabulary::fromTokenNames(tokenNames)) {}

DFASerializer::DFASerializer(const DFA* dfa, const Vocabulary& vocabulary)
    : _dfa(dfa), _vocabulary(vocabulary) {}

DFASerializer::~DFASerializer() {}

std::string DFASerializer::toString() const {
  if (_dfa->s0 == nullptr) {
    return "";
  }

  std::stringstream ss;
  std::vector<DFAState*> states = _dfa->getStates();
  for (auto s : states) {
    for (size_t i = 0; i < s->edges.size(); i++) {
      DFAState* t = s->edges[i];
      if (t != nullptr && t->stateNumber != INT32_MAX) {
        ss << getStateString(s);
        std::string label = getEdgeLabel(i);
        ss << "-" << label << "->" << getStateString(t) << "\n";
      }
    }
  }

  return ss.str();
}

std::string DFASerializer::getEdgeLabel(size_t i) const {
  return _vocabulary.getDisplayName(
      i);  // ml: no longer needed -1 as we use a map for edges, without offset.
}

std::string DFASerializer::getStateString(DFAState* s) const {
  size_t n = s->stateNumber;

  const std::string baseStateStr = std::string(s->isAcceptState ? ":" : "") +
                                   "s" + std::to_string(n) +
                                   (s->requiresFullContext ? "^" : "");

  if (s->isAcceptState) {
    if (!s->predicates.empty()) {
      std::string buf;
      for (size_t i = 0; i < s->predicates.size(); i++) {
        buf.append(s->predicates[i]->toString());
      }
      return baseStateStr + "=>" + buf;
    } else {
      return baseStateStr + "=>" + std::to_string(s->prediction);
    }
  } else {
    return baseStateStr;
  }
}
