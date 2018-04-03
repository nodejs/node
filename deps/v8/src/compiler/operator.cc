// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/operator.h"

#include <limits>

namespace v8 {
namespace internal {
namespace compiler {

namespace {

template <typename N>
V8_INLINE N CheckRange(size_t val) {
  // The getters on Operator for input and output counts currently return int.
  // Thus check that the given value fits in the integer range.
  // TODO(titzer): Remove this check once the getters return size_t.
  CHECK_LE(val, std::min(static_cast<size_t>(std::numeric_limits<N>::max()),
                         static_cast<size_t>(kMaxInt)));
  return static_cast<N>(val);
}

}  // namespace

Operator::Operator(Opcode opcode, Properties properties, const char* mnemonic,
                   size_t value_in, size_t effect_in, size_t control_in,
                   size_t value_out, size_t effect_out, size_t control_out)
    : mnemonic_(mnemonic),
      opcode_(opcode),
      properties_(properties),
      value_in_(CheckRange<uint32_t>(value_in)),
      effect_in_(CheckRange<uint32_t>(effect_in)),
      control_in_(CheckRange<uint32_t>(control_in)),
      value_out_(CheckRange<uint32_t>(value_out)),
      effect_out_(CheckRange<uint8_t>(effect_out)),
      control_out_(CheckRange<uint32_t>(control_out)) {}

std::ostream& operator<<(std::ostream& os, const Operator& op) {
  op.PrintTo(os);
  return os;
}

void Operator::PrintToImpl(std::ostream& os, PrintVerbosity verbose) const {
  os << mnemonic();
}

void Operator::PrintPropsTo(std::ostream& os) const {
  std::string separator = "";

#define PRINT_PROP_IF_SET(name)         \
  if (HasProperty(Operator::k##name)) { \
    os << separator;                    \
    os << #name;                        \
    separator = ", ";                   \
  }
  OPERATOR_PROPERTY_LIST(PRINT_PROP_IF_SET)
#undef PRINT_PROP_IF_SET
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
