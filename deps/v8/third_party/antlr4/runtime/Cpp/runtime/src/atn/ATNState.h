/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "misc/IntervalSet.h"

namespace antlr4 {
namespace atn {

/// <summary>
/// The following images show the relation of states and
/// <seealso cref="ATNState#transitions"/> for various grammar constructs.
///
/// <ul>
///
/// <li>Solid edges marked with an &#0949; indicate a required
/// <seealso cref="EpsilonTransition"/>.</li>
///
/// <li>Dashed edges indicate locations where any transition derived from
/// <seealso cref="Transition"/> might appear.</li>
///
/// <li>Dashed nodes are place holders for either a sequence of linked
/// <seealso cref="BasicState"/> states or the inclusion of a block representing
/// a nested construct in one of the forms below.</li>
///
/// <li>Nodes showing multiple outgoing alternatives with a {@code ...} support
/// any number of alternatives (one or more). Nodes without the {@code ...} only
/// support the exact number of alternatives shown in the diagram.</li>
///
/// </ul>
///
/// <h2>Basic Blocks</h2>
///
/// <h3>Rule</h3>
///
/// <embed src="images/Rule.svg" type="image/svg+xml"/>
///
/// <h3>Block of 1 or more alternatives</h3>
///
/// <embed src="images/Block.svg" type="image/svg+xml"/>
///
/// <h2>Greedy Loops</h2>
///
/// <h3>Greedy Closure: {@code (...)*}</h3>
///
/// <embed src="images/ClosureGreedy.svg" type="image/svg+xml"/>
///
/// <h3>Greedy Positive Closure: {@code (...)+}</h3>
///
/// <embed src="images/PositiveClosureGreedy.svg" type="image/svg+xml"/>
///
/// <h3>Greedy Optional: {@code (...)?}</h3>
///
/// <embed src="images/OptionalGreedy.svg" type="image/svg+xml"/>
///
/// <h2>Non-Greedy Loops</h2>
///
/// <h3>Non-Greedy Closure: {@code (...)*?}</h3>
///
/// <embed src="images/ClosureNonGreedy.svg" type="image/svg+xml"/>
///
/// <h3>Non-Greedy Positive Closure: {@code (...)+?}</h3>
///
/// <embed src="images/PositiveClosureNonGreedy.svg" type="image/svg+xml"/>
///
/// <h3>Non-Greedy Optional: {@code (...)??}</h3>
///
/// <embed src="images/OptionalNonGreedy.svg" type="image/svg+xml"/>
/// </summary>
class ATN;

class ANTLR4CPP_PUBLIC ATNState {
 public:
  ATNState();
  ATNState(ATNState const&) = delete;

  virtual ~ATNState();

  ATNState& operator=(ATNState const&) = delete;

  static const size_t INITIAL_NUM_TRANSITIONS = 4;
  static const size_t INVALID_STATE_NUMBER =
      static_cast<size_t>(-1);  // std::numeric_limits<size_t>::max();

  enum {
    ATN_INVALID_TYPE = 0,
    BASIC = 1,
    RULE_START = 2,
    BLOCK_START = 3,
    PLUS_BLOCK_START = 4,
    STAR_BLOCK_START = 5,
    TOKEN_START = 6,
    RULE_STOP = 7,
    BLOCK_END = 8,
    STAR_LOOP_BACK = 9,
    STAR_LOOP_ENTRY = 10,
    PLUS_LOOP_BACK = 11,
    LOOP_END = 12
  };

  static const std::vector<std::string> serializationNames;

  size_t stateNumber = INVALID_STATE_NUMBER;
  size_t ruleIndex = 0;  // at runtime, we don't have Rule objects
  bool epsilonOnlyTransitions = false;

 public:
  virtual size_t hashCode();
  bool operator==(const ATNState& other);

  /// Track the transitions emanating from this ATN state.
  std::vector<Transition*> transitions;

  virtual bool isNonGreedyExitState();
  virtual std::string toString() const;
  virtual void addTransition(Transition* e);
  virtual void addTransition(size_t index, Transition* e);
  virtual Transition* removeTransition(size_t index);
  virtual size_t getStateType() = 0;

 private:
  /// Used to cache lookahead during parsing, not used during construction.

  misc::IntervalSet _nextTokenWithinRule;
  std::atomic<bool> _nextTokenUpdated{false};

  friend class ATN;
};

}  // namespace atn
}  // namespace antlr4
