/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/SetTransition.h"

namespace antlr4 {
namespace atn {

class ANTLR4CPP_PUBLIC NotSetTransition final : public SetTransition {
 public:
  NotSetTransition(ATNState* target, const misc::IntervalSet& set);

  virtual SerializationType getSerializationType() const override;

  virtual bool matches(size_t symbol, size_t minVocabSymbol,
                       size_t maxVocabSymbol) const override;

  virtual std::string toString() const override;
};

}  // namespace atn
}  // namespace antlr4
