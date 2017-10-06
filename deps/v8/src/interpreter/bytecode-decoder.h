// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_DECODER_H_
#define V8_INTERPRETER_BYTECODE_DECODER_H_

#include <iosfwd>

#include "src/globals.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {
namespace interpreter {

class V8_EXPORT_PRIVATE BytecodeDecoder final {
 public:
  // Decodes a register operand in a byte array.
  static Register DecodeRegisterOperand(const uint8_t* operand_start,
                                        OperandType operand_type,
                                        OperandScale operand_scale);

  // Decodes a register list operand in a byte array.
  static RegisterList DecodeRegisterListOperand(const uint8_t* operand_start,
                                                uint32_t count,
                                                OperandType operand_type,
                                                OperandScale operand_scale);

  // Decodes a signed operand in a byte array.
  static int32_t DecodeSignedOperand(const uint8_t* operand_start,
                                     OperandType operand_type,
                                     OperandScale operand_scale);

  // Decodes an unsigned operand in a byte array.
  static uint32_t DecodeUnsignedOperand(const uint8_t* operand_start,
                                        OperandType operand_type,
                                        OperandScale operand_scale);

  // Decode a single bytecode and operands to |os|.
  static std::ostream& Decode(std::ostream& os, const uint8_t* bytecode_start,
                              int number_of_parameters);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_DECODER_H_
