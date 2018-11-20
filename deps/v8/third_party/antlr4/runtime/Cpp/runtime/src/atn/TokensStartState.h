/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/DecisionState.h"

namespace antlr4 {
namespace atn {

/// The Tokens rule start state linking to each lexer rule start state.
class ANTLR4CPP_PUBLIC TokensStartState final : public DecisionState {
 public:
  virtual size_t getStateType();
};

}  // namespace atn
}  // namespace antlr4
