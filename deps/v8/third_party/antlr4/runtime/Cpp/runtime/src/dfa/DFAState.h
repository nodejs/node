/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {
namespace dfa {

/// <summary>
/// A DFA state represents a set of possible ATN configurations.
///  As Aho, Sethi, Ullman p. 117 says "The DFA uses its state
///  to keep track of all possible states the ATN can be in after
///  reading each input symbol.  That is to say, after reading
///  input a1a2..an, the DFA is in a state that represents the
///  subset T of the states of the ATN that are reachable from the
///  ATN's start state along some path labeled a1a2..an."
///  In conventional NFA->DFA conversion, therefore, the subset T
///  would be a bitset representing the set of states the
///  ATN could be in.  We need to track the alt predicted by each
///  state as well, however.  More importantly, we need to maintain
///  a stack of states, tracking the closure operations as they
///  jump from rule to rule, emulating rule invocations (method calls).
///  I have to add a stack to simulate the proper lookahead sequences for
///  the underlying LL grammar from which the ATN was derived.
/// <p/>
///  I use a set of ATNConfig objects not simple states.  An ATNConfig
///  is both a state (ala normal conversion) and a RuleContext describing
///  the chain of rules (if any) followed to arrive at that state.
/// <p/>
///  A DFA state may have multiple references to a particular state,
///  but with different ATN contexts (with same or different alts)
///  meaning that state was reached via a different set of rule invocations.
/// </summary>
class ANTLR4CPP_PUBLIC DFAState {
 public:
  class PredPrediction {
   public:
    Ref<atn::SemanticContext>
        pred;  // never null; at least SemanticContext.NONE
    int alt;

    PredPrediction(const Ref<atn::SemanticContext>& pred, int alt);
    virtual ~PredPrediction();

    virtual std::string toString();

   private:
    void InitializeInstanceFields();
  };

  int stateNumber;

  std::unique_ptr<atn::ATNConfigSet> configs;

  /// {@code edges[symbol]} points to target of symbol. Shift up by 1 so (-1)
  ///  <seealso cref="Token#EOF"/> maps to {@code edges[0]}.
  // ml: this is a sparse list, so we use a map instead of a vector.
  //     Watch out: we no longer have the -1 offset, as it isn't needed anymore.
  std::unordered_map<size_t, DFAState*> edges;

  bool isAcceptState;

  /// if accept state, what ttype do we match or alt do we predict?
  /// This is set to <seealso cref="ATN#INVALID_ALT_NUMBER"/> when <seealso
  /// cref="#predicates"/>{@code !=null} or <seealso
  /// cref="#requiresFullContext"/>.
  size_t prediction;

  Ref<atn::LexerActionExecutor> lexerActionExecutor;

  /// <summary>
  /// Indicates that this state was created during SLL prediction that
  /// discovered a conflict between the configurations in the state. Future
  /// <seealso cref="ParserATNSimulator#execATN"/> invocations immediately
  /// jumped doing full context prediction if this field is true.
  /// </summary>
  bool requiresFullContext;

  /// <summary>
  /// During SLL parsing, this is a list of predicates associated with the
  ///  ATN configurations of the DFA state. When we have predicates,
  ///  <seealso cref="#requiresFullContext"/> is {@code false} since full
  ///  context prediction evaluates predicates on-the-fly. If this is not null,
  ///  then <seealso cref="#prediction"/> is <seealso
  ///  cref="ATN#INVALID_ALT_NUMBER"/>.
  /// <p/>
  ///  We only use these for non-<seealso cref="#requiresFullContext"/> but
  ///  conflicting states. That means we know from the context (it's $ or we
  ///  don't dip into outer context) that it's an ambiguity not a conflict.
  /// <p/>
  ///  This list is computed by <seealso
  ///  cref="ParserATNSimulator#predicateDFAState"/>.
  /// </summary>
  std::vector<PredPrediction*> predicates;

  /// Map a predicate to a predicted alternative.
  DFAState();
  DFAState(int state);
  DFAState(std::unique_ptr<atn::ATNConfigSet> configs);
  virtual ~DFAState();

  /// <summary>
  /// Get the set of all alts mentioned by all ATN configurations in this
  ///  DFA state.
  /// </summary>
  virtual std::set<size_t> getAltSet();

  virtual size_t hashCode() const;

  /// Two DFAState instances are equal if their ATN configuration sets
  /// are the same. This method is used to see if a state already exists.
  ///
  /// Because the number of alternatives and number of ATN configurations are
  /// finite, there is a finite number of DFA states that can be processed.
  /// This is necessary to show that the algorithm terminates.
  ///
  /// Cannot test the DFA state numbers here because in
  /// ParserATNSimulator#addDFAState we need to know if any other state
  /// exists that has this exact set of ATN configurations. The
  /// stateNumber is irrelevant.
  bool operator==(const DFAState& o) const;

  virtual std::string toString();

  struct Hasher {
    size_t operator()(DFAState* k) const { return k->hashCode(); }
  };

  struct Comparer {
    bool operator()(DFAState* lhs, DFAState* rhs) const { return *lhs == *rhs; }
  };

 private:
  void InitializeInstanceFields();
};

}  // namespace dfa
}  // namespace antlr4
