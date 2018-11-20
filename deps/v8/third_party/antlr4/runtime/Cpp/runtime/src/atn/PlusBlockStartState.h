/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/BlockStartState.h"

namespace antlr4 {
namespace atn {

/// Start of {@code (A|B|...)+} loop. Technically a decision state, but
/// we don't use for code generation; somebody might need it, so I'm defining
/// it for completeness. In reality, the <seealso cref="PlusLoopbackState"/>
/// node is the real decision-making note for {@code A+}.
class ANTLR4CPP_PUBLIC PlusBlockStartState final : public BlockStartState {
 public:
  PlusLoopbackState* loopBackState = nullptr;

  virtual size_t getStateType() override;
};

}  // namespace atn
}  // namespace antlr4
