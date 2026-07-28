// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-decoder.h"

#include <iomanip>

#include "src/base/sanitizer/tsan.h"
#include "src/codegen/bailout-reason.h"
#include "src/interpreter/interpreter-intrinsics.h"
#include "src/objects/contexts.h"
#include "src/objects/fixed-array.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

// static
Register BytecodeDecoder::DecodeRegisterOperand(Address operand_start,
                                                OperandType operand_type,
                                                OperandScale operand_scale) {
  DCHECK(Bytecodes::IsRegisterOperandType(operand_type));
  int32_t operand =
      DecodeSignedOperand(operand_start, operand_type, operand_scale);
  return Register::FromOperand(operand);
}

// static
RegisterList BytecodeDecoder::DecodeRegisterListOperand(
    Address operand_start, uint32_t count, OperandType operand_type,
    OperandScale operand_scale) {
  Register first_reg =
      DecodeRegisterOperand(operand_start, operand_type, operand_scale);
  return RegisterList(first_reg.index(), static_cast<int>(count));
}

// static
int32_t BytecodeDecoder::DecodeSignedOperand(Address operand_start,
                                             OperandType operand_type,
                                             OperandScale operand_scale) {
  DCHECK(!Bytecodes::IsUnsignedOperandType(operand_type));
  switch (Bytecodes::SizeOfOperand(operand_type, operand_scale)) {
    case OperandSize::kByte:
      return *reinterpret_cast<const int8_t*>(operand_start);
    case OperandSize::kShort:
      return static_cast<int16_t>(
          base::ReadUnalignedValue<uint16_t>(operand_start));
    case OperandSize::kQuad:
      return static_cast<int32_t>(
          base::ReadUnalignedValue<uint32_t>(operand_start));
    case OperandSize::kNone:
      UNREACHABLE();
  }
  return 0;
}

// static
uint32_t BytecodeDecoder::DecodeUnsignedOperand(Address operand_start,
                                                OperandType operand_type,
                                                OperandScale operand_scale) {
  DCHECK(Bytecodes::IsUnsignedOperandType(operand_type));
  switch (Bytecodes::SizeOfOperand(operand_type, operand_scale)) {
    case OperandSize::kByte:
      return *reinterpret_cast<const uint8_t*>(operand_start);
    case OperandSize::kShort:
      return base::ReadUnalignedValue<uint16_t>(operand_start);
    case OperandSize::kQuad:
      return base::ReadUnalignedValue<uint32_t>(operand_start);
    case OperandSize::kNone:
      UNREACHABLE();
  }
  return 0;
}

// static
#ifdef V8_IS_TSAN
V8_NOINLINE

DISABLE_TSAN
#endif
uint32_t BytecodeDecoder::RacyDecodeEmbeddedFeedback(Address operand_start) {
  uint16_t value;
  memcpy(&value, reinterpret_cast<const void*>(operand_start), sizeof(value));
  return value;
}

namespace {

const char* NameForRuntimeId(Runtime::FunctionId idx) {
  return Runtime::FunctionForId(idx)->name;
}

const char* NameForNativeContextIndex(uint32_t idx) {
  switch (idx) {
#define CASE(index_name, type, name) \
  case Context::index_name:          \
    return #name;
    NATIVE_CONTEXT_FIELDS(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}

}  // anonymous namespace

// static
std::ostream& BytecodeDecoder::Decode(std::ostream& os,
                                      const uint8_t* bytecode_start,
                                      Tagged<TrustedFixedArray> constant_pool,
                                      bool with_hex) {
  Bytecode bytecode = Bytecodes::FromByte(bytecode_start[0]);
  int prefix_offset = 0;
  OperandScale operand_scale = OperandScale::kSingle;
  if (Bytecodes::IsPrefixScalingBytecode(bytecode)) {
    prefix_offset = 1;
    operand_scale = Bytecodes::PrefixBytecodeToOperandScale(bytecode);
    bytecode = Bytecodes::FromByte(bytecode_start[1]);
  }

  // Prepare to print bytecode and operands as hex digits.
  if (with_hex) {
    std::ios saved_format(nullptr);
    saved_format.copyfmt(saved_format);
    os.fill('0');
    os.flags(std::ios::hex);

    int bytecode_size = Bytecodes::Size(bytecode, operand_scale);
    for (int i = 0; i < prefix_offset + bytecode_size; i++) {
      os << std::setw(2) << static_cast<uint32_t>(bytecode_start[i]) << ' ';
    }
    os.copyfmt(saved_format);

    const int kBytecodeColumnSize = Bytecodes::kMaxOperands + 1;
    for (int i = prefix_offset + bytecode_size; i < kBytecodeColumnSize; i++) {
      os << "   ";
    }
  }

  os << Bytecodes::ToString(bytecode, operand_scale);

  // Operands for the debug break are from the original instruction.
  if (Bytecodes::IsDebugBreak(bytecode)) return os;

  int number_of_operands = Bytecodes::NumberOfOperands(bytecode);
  if (number_of_operands > 0) os << " ";
  for (int i = 0; i < number_of_operands; i++) {
    OperandType op_type = Bytecodes::GetOperandType(bytecode, i);
    int operand_offset =
        Bytecodes::GetOperandOffset(bytecode, i, operand_scale);
    Address operand_start = reinterpret_cast<Address>(
        &bytecode_start[prefix_offset + operand_offset]);
    switch (op_type) {
      case interpreter::OperandType::kConstantPoolIndex: {
        uint32_t idx =
            DecodeUnsignedOperand(operand_start, op_type, operand_scale);
        Tagged<Object> obj = constant_pool->get(idx);

        os << "[" << idx << ":";
        if (Tagged<String> string; TryCast(obj, &string)) {
          os << '"';
          if (string->length() > String::kMaxShortPrintLength) {
            string->PrintUC16(os, 0, String::kMaxShortPrintLength - 3);
            os << "...";
          } else {
            string->PrintUC16(os);
          }
          os << '"';
        } else {
          os << Brief(obj);
        }
        os << "]";
        break;
      }
      case interpreter::OperandType::kFeedbackSlot:
        // TODO(leszeks): If we had the feedback metadata here, we could print
        // the feedback slot type -- or with the feedback vector we could even
        // print the feedback itself inline.
        os << "FBV["
           << DecodeUnsignedOperand(operand_start, op_type, operand_scale)
           << "]";
        break;
      case interpreter::OperandType::kEmbeddedFeedback:
        os << "EmbeddedFeedback[0x" << std::hex
           << RacyDecodeEmbeddedFeedback(operand_start) << std::dec << "]";
        break;
      case interpreter::OperandType::kContextSlot:
        // TODO(leszeks): If we had the Context here we could print the context
        // contents.
        // TODO(leszeks): We should add a ContextDepth type and print that
        // combined with the context slot similar to RegList.
      case interpreter::OperandType::kCoverageSlot:
      case interpreter::OperandType::kUImm:
        os << "["
           << DecodeUnsignedOperand(operand_start, op_type, operand_scale)
           << "]";
        break;
      case interpreter::OperandType::kIntrinsicId: {
        auto id = static_cast<IntrinsicsHelper::IntrinsicId>(
            DecodeUnsignedOperand(operand_start, op_type, operand_scale));
        os << "[" << NameForRuntimeId(IntrinsicsHelper::ToRuntimeId(id)) << "]";
        break;
      }
      case interpreter::OperandType::kNativeContextIndex: {
        auto id = DecodeUnsignedOperand(operand_start, op_type, operand_scale);
        os << "[" << NameForNativeContextIndex(id) << "]";
        break;
      }
      case interpreter::OperandType::kRuntimeId:
        os << "["
           << NameForRuntimeId(static_cast<Runtime::FunctionId>(
                  DecodeUnsignedOperand(operand_start, op_type, operand_scale)))
           << "]";
        break;
      case interpreter::OperandType::kAbortReason:
        os << "["
           << GetAbortReason(static_cast<AbortReason>(
                  DecodeUnsignedOperand(operand_start, op_type, operand_scale)))
           << "]";
        break;
      case interpreter::OperandType::kImm:
        os << "[" << DecodeSignedOperand(operand_start, op_type, operand_scale)
           << "]";
        break;
      case interpreter::OperandType::kFlag8:
      case interpreter::OperandType::kFlag16:
        os << "#" << std::hex
           << DecodeUnsignedOperand(operand_start, op_type, operand_scale)
           << std::dec;
        break;
      case interpreter::OperandType::kReg:
      case interpreter::OperandType::kRegOut:
      case interpreter::OperandType::kRegInOut: {
        Register reg =
            DecodeRegisterOperand(operand_start, op_type, operand_scale);
        os << reg.ToString();
        break;
      }
      case interpreter::OperandType::kRegOutTriple: {
        RegisterList reg_list =
            DecodeRegisterListOperand(operand_start, 3, op_type, operand_scale);
        os << reg_list.first_register().ToString() << "-"
           << reg_list.last_register().ToString();
        break;
      }
      case interpreter::OperandType::kRegOutPair:
      case interpreter::OperandType::kRegPair: {
        RegisterList reg_list =
            DecodeRegisterListOperand(operand_start, 2, op_type, operand_scale);
        os << reg_list.first_register().ToString() << "-"
           << reg_list.last_register().ToString();
        break;
      }
      case interpreter::OperandType::kRegOutList:
      case interpreter::OperandType::kRegList: {
        DCHECK_LT(i, number_of_operands - 1);
        DCHECK_EQ(Bytecodes::GetOperandType(bytecode, i + 1),
                  OperandType::kRegCount);
        int reg_count_offset =
            Bytecodes::GetOperandOffset(bytecode, i + 1, operand_scale);
        Address reg_count_operand = reinterpret_cast<Address>(
            &bytecode_start[prefix_offset + reg_count_offset]);
        uint32_t count = DecodeUnsignedOperand(
            reg_count_operand, OperandType::kRegCount, operand_scale);
        RegisterList reg_list = DecodeRegisterListOperand(
            operand_start, count, op_type, operand_scale);
        os << reg_list.first_register().ToString() << "-"
           << reg_list.last_register().ToString();
        i++;  // Skip kRegCount.
        break;
      }
      case interpreter::OperandType::kNone:
      case interpreter::OperandType::kRegCount:  // Dealt with in kRegList.
        UNREACHABLE();
    }
    if (i != number_of_operands - 1) {
      os << ", ";
    }
  }
  return os;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
