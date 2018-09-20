/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/DecisionState.h"

namespace antlr4 {
namespace atn {

/// Decision state for {@code A+} and {@code (A|B)+}. It has two transitions:
/// one to the loop back to start of the block and one to exit.
class ANTLR4CPP_PUBLIC PlusLoopbackState final : public DecisionState {
 public:
  virtual size_t getStateType() override;
};

}  // namespace atn
}  // namespace antlr4
