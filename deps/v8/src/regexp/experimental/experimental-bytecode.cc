// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/experimental/experimental-bytecode.h"

#include <cctype>
#include <iomanip>

namespace v8 {
namespace internal {
namespace regexp {

namespace {

std::ostream& PrintAsciiOrHex(std::ostream& os, base::uc16 c) {
  if (c < 128 && std::isprint(c)) {
    os << static_cast<char>(c);
  } else {
    os << "0x" << std::hex << static_cast<int>(c);
  }
  return os;
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const Instruction& inst) {
  switch (inst.opcode) {
    case Instruction::CONSUME_RANGE: {
      os << "CONSUME_RANGE [";
      PrintAsciiOrHex(os, inst.payload.consume_range.min);
      os << ", ";
      PrintAsciiOrHex(os, inst.payload.consume_range.max);
      os << "]";
      break;
    }
    case Instruction::RANGE_COUNT: {
      os << "RANGE_COUNT " << inst.payload.num_ranges;
      break;
    }
    case Instruction::ASSERTION:
      os << "ASSERTION ";
      switch (inst.payload.assertion_type) {
        case Assertion::Type::START_OF_INPUT:
          os << "START_OF_INPUT";
          break;
        case Assertion::Type::END_OF_INPUT:
          os << "END_OF_INPUT";
          break;
        case Assertion::Type::START_OF_LINE:
          os << "START_OF_LINE";
          break;
        case Assertion::Type::END_OF_LINE:
          os << "END_OF_LINE";
          break;
        case Assertion::Type::BOUNDARY:
          os << "BOUNDARY";
          break;
        case Assertion::Type::NON_BOUNDARY:
          os << "NON_BOUNDARY";
          break;
      }
      break;
    case Instruction::FORK:
      os << "FORK " << inst.payload.pc;
      break;
    case Instruction::JMP:
      os << "JMP " << inst.payload.pc;
      break;
    case Instruction::ACCEPT:
      os << "ACCEPT";
      break;
    case Instruction::SET_REGISTER_TO_CP:
      os << "SET_REGISTER_TO_CP " << inst.payload.register_index;
      break;
    case Instruction::CLEAR_REGISTER:
      os << "CLEAR_REGISTER " << inst.payload.register_index;
      break;
    case Instruction::SET_QUANTIFIER_TO_CLOCK:
      os << "SET_QUANTIFIER_TO_CLOCK " << inst.payload.quantifier_id;
      break;
    case Instruction::FILTER_QUANTIFIER:
      os << "FILTER_QUANTIFIER " << inst.payload.quantifier_id;
      break;
    case Instruction::FILTER_GROUP:
      os << "FILTER_GROUP " << inst.payload.group_id;
      break;
    case Instruction::FILTER_LOOKAROUND:
      os << "FILTER_LOOKAROUND " << inst.payload.lookaround_id;
      break;
    case Instruction::FILTER_CHILD:
      os << "FILTER_CHILD " << inst.payload.pc;
      break;
    case Instruction::BEGIN_LOOP:
      os << "BEGIN_LOOP";
      break;
    case Instruction::END_LOOP:
      os << "END_LOOP";
      break;
    case Instruction::START_LOOKAROUND:
      os << "START_LOOKAROUND " << inst.payload.lookaround;
      break;
    case Instruction::END_LOOKAROUND:
      os << "END_LOOKAROUND";
      break;
    case Instruction::WRITE_LOOKAROUND_TABLE:
      os << "WRITE_LOOKAROUND_TABLE " << inst.payload.lookaround_id;
      break;
    case Instruction::READ_LOOKAROUND_TABLE:
      os << "READ_LOOKAROUND_TABLE " << inst.payload.lookaround;
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
                         base::Vector<const Instruction> insts) {
  int inst_num = insts.length();
  int line_digit_num = DigitsRequiredBelow(inst_num);

  for (int i = 0; i != inst_num; ++i) {
    const Instruction& inst = insts[i];
    os << std::setfill('0') << std::setw(line_digit_num) << i << ": " << inst
       << std::endl;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const Instruction::LookaroundPayload& payload) {
  return os << payload.index() << " ("
            << (payload.type() == Lookaround::Type::LOOKAHEAD ? "ahead"
                                                              : "behind")
            << ", " << (payload.is_positive() ? "positive" : "negative") << ")";
}

}  // namespace regexp
}  // namespace internal
}  // namespace v8
