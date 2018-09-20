/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/ATNConfigSet.h"
#include "atn/StarLoopEntryState.h"
#include "dfa/DFASerializer.h"
#include "dfa/LexerDFASerializer.h"
#include "support/CPPUtils.h"

#include "dfa/DFA.h"

using namespace antlr4;
using namespace antlr4::dfa;
using namespace antlrcpp;

DFA::DFA(atn::DecisionState* atnStartState) : DFA(atnStartState, 0) {}

DFA::DFA(atn::DecisionState* atnStartState, size_t decision)
    : atnStartState(atnStartState), s0(nullptr), decision(decision) {
  _precedenceDfa = false;
  if (is<atn::StarLoopEntryState*>(atnStartState)) {
    if (static_cast<atn::StarLoopEntryState*>(atnStartState)
            ->isPrecedenceDecision) {
      _precedenceDfa = true;
      s0 = new DFAState(
          std::unique_ptr<atn::ATNConfigSet>(new atn::ATNConfigSet()));
      s0->isAcceptState = false;
      s0->requiresFullContext = false;
    }
  }
}

DFA::DFA(DFA&& other)
    : atnStartState(other.atnStartState), decision(other.decision) {
  // Source states are implicitly cleared by the move.
  states = std::move(other.states);

  other.atnStartState = nullptr;
  other.decision = 0;
  s0 = other.s0;
  other.s0 = nullptr;
  _precedenceDfa = other._precedenceDfa;
  other._precedenceDfa = false;
}

DFA::~DFA() {
  bool s0InList = (s0 == nullptr);
  for (auto state : states) {
    if (state == s0) s0InList = true;
    delete state;
  }

  if (!s0InList) delete s0;
}

bool DFA::isPrecedenceDfa() const { return _precedenceDfa; }

DFAState* DFA::getPrecedenceStartState(int precedence) const {
  assert(_precedenceDfa);  // Only precedence DFAs may contain a precedence
                           // start state.

  auto iterator = s0->edges.find(precedence);
  if (iterator == s0->edges.end()) return nullptr;

  return iterator->second;
}

void DFA::setPrecedenceStartState(int precedence, DFAState* startState,
                                  SingleWriteMultipleReadLock& lock) {
  if (!isPrecedenceDfa()) {
    throw IllegalStateException(
        "Only precedence DFAs may contain a precedence start state.");
  }

  if (precedence < 0) {
    return;
  }

  {
    lock.writeLock();
    s0->edges[precedence] = startState;
    lock.writeUnlock();
  }
}

std::vector<DFAState*> DFA::getStates() const {
  std::vector<DFAState*> result;
  for (auto state : states) result.push_back(state);

  std::sort(result.begin(), result.end(),
            [](DFAState* o1, DFAState* o2) -> bool {
              return o1->stateNumber < o2->stateNumber;
            });

  return result;
}

std::string DFA::toString(const std::vector<std::string>& tokenNames) {
  if (s0 == nullptr) {
    return "";
  }
  DFASerializer serializer(this, tokenNames);

  return serializer.toString();
}

std::string DFA::toString(const Vocabulary& vocabulary) const {
  if (s0 == nullptr) {
    return "";
  }

  DFASerializer serializer(this, vocabulary);
  return serializer.toString();
}

std::string DFA::toLexerString() {
  if (s0 == nullptr) {
    return "";
  }
  LexerDFASerializer serializer(this);

  return serializer.toString();
}
