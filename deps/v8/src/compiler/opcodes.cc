// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/opcodes.h"

#include <algorithm>
#include <ostream>

#include "src/base/macros.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

char const* const kMnemonics[] = {
#define DECLARE_MNEMONIC(x) #x,
    ALL_OP_LIST(DECLARE_MNEMONIC)
#undef DECLARE_MNEMONIC
        "UnknownOpcode"};

}  // namespace


// static
char const* IrOpcode::Mnemonic(Value value) {
  size_t const n = std::min<size_t>(value, arraysize(kMnemonics) - 1);
  return kMnemonics[n];
}


std::ostream& operator<<(std::ostream& os, IrOpcode::Value opcode) {
  return os << IrOpcode::Mnemonic(opcode);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
