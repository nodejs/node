/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/ATNState.h"

namespace antlr4 {
namespace atn {

/// The last node in the ATN for a rule, unless that rule is the start symbol.
/// In that case, there is one transition to EOF. Later, we might encode
/// references to all calls to this rule to compute FOLLOW sets for
/// error handling.
class ANTLR4CPP_PUBLIC RuleStopState final : public ATNState {
 public:
  virtual size_t getStateType() override;
};

}  // namespace atn
}  // namespace antlr4
