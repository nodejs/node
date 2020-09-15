// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/experimental/experimental-bytecode.h"

#include <iomanip>

namespace v8 {
namespace internal {

namespace {

std::ostream& PrintAsciiOrHex(std::ostream& os, uc16 c) {
  if (c < 128 && std::isprint(c)) {
    os << static_cast<char>(c);
  } else {
    os << "0x" << std::hex << static_cast<int>(c);
  }
  return os;
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const RegExpInstruction& inst) {
  switch (inst.opcode) {
    case RegExpInstruction::CONSUME_RANGE: {
      os << "CONSUME_RANGE [";
      PrintAsciiOrHex(os, inst.payload.consume_range.min);
      os << ", ";
      PrintAsciiOrHex(os, inst.payload.consume_range.max);
      os << "]";
      break;
    }
    case RegExpInstruction::FORK:
      os << "FORK " << inst.payload.pc;
      break;
    case RegExpInstruction::JMP:
      os << "JMP " << inst.payload.pc;
      break;
    case RegExpInstruction::ACCEPT:
      os << "ACCEPT";
      break;
  }
  return os;
}

namespace {

// The maximum number of digits required to display a non-negative number < n
// in base 10.
int DigitsRequiredBelow(int n) {
  DCHECK_GE(n, 0);

  int result = 1;
  for (int i = 10; i < n; i *= 10) {
    result += 1;
  }
  return result;
}

}  // namespace

std::ostream& operator<<(std::ostream& os,
                         Vector<const RegExpInstruction> insts) {
  int inst_num = insts.length();
  int line_digit_num = DigitsRequiredBelow(inst_num);

  for (int i = 0; i != inst_num; ++i) {
    const RegExpInstruction& inst = insts[i];
    os << std::setfill('0') << std::setw(line_digit_num) << i << ": " << inst
       << std::endl;
  }
  return os;
}

}  // namespace internal
}  // namespace v8
