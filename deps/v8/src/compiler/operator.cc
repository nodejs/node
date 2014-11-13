// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/operator.h"

#include <limits>

namespace v8 {
namespace internal {
namespace compiler {


template <typename N>
static inline N CheckRange(size_t val) {
  CHECK(val <= std::numeric_limits<N>::max());
  return static_cast<N>(val);
}


Operator::Operator(Opcode opcode, Properties properties, const char* mnemonic,
                   size_t value_in, size_t effect_in, size_t control_in,
                   size_t value_out, size_t effect_out, size_t control_out)
    : opcode_(opcode),
      properties_(properties),
      mnemonic_(mnemonic),
      value_in_(CheckRange<uint32_t>(value_in)),
      effect_in_(CheckRange<uint16_t>(effect_in)),
      control_in_(CheckRange<uint16_t>(control_in)),
      value_out_(CheckRange<uint16_t>(value_out)),
      effect_out_(CheckRange<uint8_t>(effect_out)),
      control_out_(CheckRange<uint8_t>(control_out)) {}


std::ostream& operator<<(std::ostream& os, const Operator& op) {
  op.PrintTo(os);
  return os;
}


void Operator::PrintTo(std::ostream& os) const { os << mnemonic(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
