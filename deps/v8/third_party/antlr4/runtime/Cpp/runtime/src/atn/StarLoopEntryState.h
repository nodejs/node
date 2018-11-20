/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/DecisionState.h"

namespace antlr4 {
namespace atn {

class ANTLR4CPP_PUBLIC StarLoopEntryState final : public DecisionState {
 public:
  StarLoopEntryState();

  /**
   * Indicates whether this state can benefit from a precedence DFA during SLL
   * decision making.
   *
   * <p>This is a computed property that is calculated during ATN
   * deserialization and stored for use in {@link ParserATNSimulator} and
   * {@link ParserInterpreter}.</p>
   *
   * @see DFA#isPrecedenceDfa()
   */
  bool isPrecedenceDecision = false;

  StarLoopbackState* loopBackState = nullptr;

  virtual size_t getStateType() override;
};

}  // namespace atn
}  // namespace antlr4
