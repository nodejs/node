/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/Transition.h"

namespace antlr4 {
namespace atn {

/// TO_DO: make all transitions sets? no, should remove set edges.
class ANTLR4CPP_PUBLIC AtomTransition final : public Transition {
 public:
  /// The token type or character value; or, signifies special label.
  const size_t _label;

  AtomTransition(ATNState* target, size_t label);

  virtual SerializationType getSerializationType() const override;

  virtual misc::IntervalSet label() const override;
  virtual bool matches(size_t symbol, size_t minVocabSymbol,
                       size_t maxVocabSymbol) const override;

  virtual std::string toString() const override;
};

}  // namespace atn
}  // namespace antlr4
