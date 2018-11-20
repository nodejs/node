/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "dfa/DFAState.h"

namespace antlrcpp {
class SingleWriteMultipleReadLock;
}

namespace antlr4 {
namespace dfa {

class ANTLR4CPP_PUBLIC DFA {
 public:
  /// A set of all DFA states. Use a map so we can get old state back.
  /// Set only allows you to see if it's there.

  /// From which ATN state did we create this DFA?
  atn::DecisionState* atnStartState;
  std::unordered_set<DFAState*, DFAState::Hasher, DFAState::Comparer>
      states;  // States are owned by this class.
  DFAState* s0;
  size_t decision;

  DFA(atn::DecisionState* atnStartState);
  DFA(atn::DecisionState* atnStartState, size_t decision);
  DFA(const DFA& other) = delete;
  DFA(DFA&& other);
  virtual ~DFA();

  /**
   * Gets whether this DFA is a precedence DFA. Precedence DFAs use a special
   * start state {@link #s0} which is not stored in {@link #states}. The
   * {@link DFAState#edges} array for this start state contains outgoing edges
   * supplying individual start states corresponding to specific precedence
   * values.
   *
   * @return {@code true} if this is a precedence DFA; otherwise,
   * {@code false}.
   * @see Parser#getPrecedence()
   */
  bool isPrecedenceDfa() const;

  /**
   * Get the start state for a specific precedence value.
   *
   * @param precedence The current precedence.
   * @return The start state corresponding to the specified precedence, or
   * {@code null} if no start state exists for the specified precedence.
   *
   * @throws IllegalStateException if this is not a precedence DFA.
   * @see #isPrecedenceDfa()
   */
  DFAState* getPrecedenceStartState(int precedence) const;

  /**
   * Set the start state for a specific precedence value.
   *
   * @param precedence The current precedence.
   * @param startState The start state corresponding to the specified
   * precedence.
   *
   * @throws IllegalStateException if this is not a precedence DFA.
   * @see #isPrecedenceDfa()
   */
  void setPrecedenceStartState(int precedence, DFAState* startState,
                               antlrcpp::SingleWriteMultipleReadLock& lock);

  /// Return a list of all states in this DFA, ordered by state number.
  virtual std::vector<DFAState*> getStates() const;

  /**
   * @deprecated Use {@link #toString(Vocabulary)} instead.
   */
  virtual std::string toString(const std::vector<std::string>& tokenNames);
  std::string toString(const Vocabulary& vocabulary) const;

  virtual std::string toLexerString();

 private:
  /**
   * {@code true} if this DFA is for a precedence decision; otherwise,
   * {@code false}. This is the backing field for {@link #isPrecedenceDfa}.
   */
  bool _precedenceDfa;
};

}  // namespace dfa
}  // namespace antlr4
