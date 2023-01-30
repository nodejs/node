// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/operations.h"

#include <atomic>
#include <sstream>

#include "src/base/logging.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/codegen/machine-type.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/frame-states.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/turboshaft/deopt-data.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/handles/handles-inl.h"

namespace v8::internal::compiler::turboshaft {

const char* OpcodeName(Opcode opcode) {
#define OPCODE_NAME(Name) #Name,
  const char* table[kNumberOfOpcodes] = {
      TURBOSHAFT_OPERATION_LIST(OPCODE_NAME)};
#undef OPCODE_NAME
  return table[OpcodeIndex(opcode)];
}

std::ostream& operator<<(std::ostream& os, OperationPrintStyle styled_op) {
  const Operation& op = styled_op.op;
  os << OpcodeName(op.opcode);
  op.PrintInputs(os, styled_op.op_index_prefix);
  op.PrintOptions(os);
  return os;
}

std::ostream& operator<<(std::ostream& os, WordUnaryOp::Kind kind) {
  switch (kind) {
    case WordUnaryOp::Kind::kReverseBytes:
      return os << "ReverseBytes";
    case WordUnaryOp::Kind::kCountLeadingZeros:
      return os << "CountLeadingZeros";
    case WordUnaryOp::Kind::kCountTrailingZeros:
      return os << "CountTrailingZeros";
    case WordUnaryOp::Kind::kPopCount:
      return os << "PopCount";
    case WordUnaryOp::Kind::kSignExtend8:
      return os << "SignExtend8";
    case WordUnaryOp::Kind::kSignExtend16:
      return os << "SignExtend16";
  }
}

std::ostream& operator<<(std::ostream& os, FloatUnaryOp::Kind kind) {
  switch (kind) {
    case FloatUnaryOp::Kind::kAbs:
      return os << "Abs";
    case FloatUnaryOp::Kind::kNegate:
      return os << "Negate";
    case FloatUnaryOp::Kind::kSilenceNaN:
      return os << "SilenceNaN";
    case FloatUnaryOp::Kind::kRoundUp:
      return os << "RoundUp";
    case FloatUnaryOp::Kind::kRoundDown:
      return os << "RoundDown";
    case FloatUnaryOp::Kind::kRoundToZero:
      return os << "RoundToZero";
    case FloatUnaryOp::Kind::kRoundTiesEven:
      return os << "RoundTiesEven";
    case FloatUnaryOp::Kind::kLog:
      return os << "Log";
    case FloatUnaryOp::Kind::kLog2:
      return os << "Log2";
    case FloatUnaryOp::Kind::kLog10:
      return os << "Log10";
    case FloatUnaryOp::Kind::kLog1p:
      return os << "Log1p";
    case FloatUnaryOp::Kind::kSqrt:
      return os << "Sqrt";
    case FloatUnaryOp::Kind::kCbrt:
      return os << "Cbrt";
    case FloatUnaryOp::Kind::kExp:
      return os << "Exp";
    case FloatUnaryOp::Kind::kExpm1:
      return os << "Expm1";
    case FloatUnaryOp::Kind::kSin:
      return os << "Sin";
    case FloatUnaryOp::Kind::kCos:
      return os << "Cos";
    case FloatUnaryOp::Kind::kAsin:
      return os << "Asin";
    case FloatUnaryOp::Kind::kAcos:
      return os << "Acos";
    case FloatUnaryOp::Kind::kSinh:
      return os << "Sinh";
    case FloatUnaryOp::Kind::kCosh:
      return os << "Cosh";
    case FloatUnaryOp::Kind::kAsinh:
      return os << "Asinh";
    case FloatUnaryOp::Kind::kAcosh:
      return os << "Acosh";
    case FloatUnaryOp::Kind::kTan:
      return os << "Tan";
    case FloatUnaryOp::Kind::kTanh:
      return os << "Tanh";
    case FloatUnaryOp::Kind::kAtan:
      return os << "Atan";
    case FloatUnaryOp::Kind::kAtanh:
      return os << "Atanh";
  }
}

// static
bool FloatUnaryOp::IsSupported(Kind kind, FloatRepresentation rep) {
  switch (rep.value()) {
    case FloatRepresentation::Float32():
      switch (kind) {
        case Kind::kRoundDown:
          return SupportedOperations::float32_round_down();
        case Kind::kRoundUp:
          return SupportedOperations::float32_round_up();
        case Kind::kRoundToZero:
          return SupportedOperations::float32_round_to_zero();
        case Kind::kRoundTiesEven:
          return SupportedOperations::float32_round_ties_even();
        default:
          return true;
      }
    case FloatRepresentation::Float64():
      switch (kind) {
        case Kind::kRoundDown:
          return SupportedOperations::float64_round_down();
        case Kind::kRoundUp:
          return SupportedOperations::float64_round_up();
        case Kind::kRoundToZero:
          return SupportedOperations::float64_round_to_zero();
        case Kind::kRoundTiesEven:
          return SupportedOperations::float64_round_ties_even();
        default:
          return true;
      }
  }
}

// static
bool WordUnaryOp::IsSupported(Kind kind, WordRepresentation rep) {
  switch (kind) {
    case Kind::kCountLeadingZeros:
    case Kind::kReverseBytes:
    case Kind::kSignExtend8:
    case Kind::kSignExtend16:
      return true;
    case Kind::kCountTrailingZeros:
      return rep == WordRepresentation::Word32()
                 ? SupportedOperations::word32_ctz()
                 : SupportedOperations::word64_ctz();
    case Kind::kPopCount:
      return rep == WordRepresentation::Word32()
                 ? SupportedOperations::word32_popcnt()
                 : SupportedOperations::word64_popcnt();
  }
}

std::ostream& operator<<(std::ostream& os, ShiftOp::Kind kind) {
  switch (kind) {
    case ShiftOp::Kind::kShiftRightArithmeticShiftOutZeros:
      return os << "ShiftRightArithmeticShiftOutZeros";
    case ShiftOp::Kind::kShiftRightArithmetic:
      return os << "ShiftRightArithmetic";
    case ShiftOp::Kind::kShiftRightLogical:
      return os << "ShiftRightLogical";
    case ShiftOp::Kind::kShiftLeft:
      return os << "ShiftLeft";
    case ShiftOp::Kind::kRotateRight:
      return os << "RotateRight";
    case ShiftOp::Kind::kRotateLeft:
      return os << "RotateLeft";
  }
}

std::ostream& operator<<(std::ostream& os, ComparisonOp::Kind kind) {
  switch (kind) {
    case ComparisonOp::Kind::kSignedLessThan:
      return os << "SignedLessThan";
    case ComparisonOp::Kind::kSignedLessThanOrEqual:
      return os << "SignedLessThanOrEqual";
    case ComparisonOp::Kind::kUnsignedLessThan:
      return os << "UnsignedLessThan";
    case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
      return os << "UnsignedLessThanOrEqual";
  }
}

std::ostream& operator<<(std::ostream& os, ChangeOp::Kind kind) {
  switch (kind) {
    case ChangeOp::Kind::kFloatConversion:
      return os << "FloatConversion";
    case ChangeOp::Kind::kJSFloatTruncate:
      return os << "JSFloatTruncate";
    case ChangeOp::Kind::kSignedFloatTruncateOverflowToMin:
      return os << "SignedFloatTruncateOverflowToMin";
    case ChangeOp::Kind::kUnsignedFloatTruncateOverflowToMin:
      return os << "UnsignedFloatTruncateOverflowToMin";
    case ChangeOp::Kind::kSignedToFloat:
      return os << "SignedToFloat";
    case ChangeOp::Kind::kUnsignedToFloat:
      return os << "UnsignedToFloat";
    case ChangeOp::Kind::kExtractHighHalf:
      return os << "ExtractHighHalf";
    case ChangeOp::Kind::kExtractLowHalf:
      return os << "ExtractLowHalf";
    case ChangeOp::Kind::kZeroExtend:
      return os << "ZeroExtend";
    case ChangeOp::Kind::kSignExtend:
      return os << "SignExtend";
    case ChangeOp::Kind::kBitcast:
      return os << "Bitcast";
  }
}

std::ostream& operator<<(std::ostream& os, TryChangeOp::Kind kind) {
  switch (kind) {
    case TryChangeOp::Kind::kSignedFloatTruncateOverflowUndefined:
      return os << "SignedFloatTruncateOverflowUndefined";
    case TryChangeOp::Kind::kUnsignedFloatTruncateOverflowUndefined:
      return os << "UnsignedFloatTruncateOverflowUndefined";
  }
}

std::ostream& operator<<(std::ostream& os, ChangeOp::Assumption assumption) {
  switch (assumption) {
    case ChangeOp::Assumption::kNoAssumption:
      return os << "NoAssumption";
    case ChangeOp::Assumption::kNoOverflow:
      return os << "NoOverflow";
    case ChangeOp::Assumption::kReversible:
      return os << "Reversible";
  }
}

std::ostream& operator<<(std::ostream& os, Float64InsertWord32Op::Kind kind) {
  switch (kind) {
    case Float64InsertWord32Op::Kind::kLowHalf:
      return os << "LowHalf";
    case Float64InsertWord32Op::Kind::kHighHalf:
      return os << "HighHalf";
  }
}

std::ostream& operator<<(std::ostream& os, SelectOp::Implementation kind) {
  switch (kind) {
    case SelectOp::Implementation::kBranch:
      return os << "Branch";
    case SelectOp::Implementation::kCMove:
      return os << "CMove";
  }
}

std::ostream& operator<<(std::ostream& os, FrameConstantOp::Kind kind) {
  switch (kind) {
    case FrameConstantOp::Kind::kStackCheckOffset:
      return os << "stack check offset";
    case FrameConstantOp::Kind::kFramePointer:
      return os << "frame pointer";
    case FrameConstantOp::Kind::kParentFramePointer:
      return os << "parent frame pointer";
  }
}

void Operation::PrintInputs(std::ostream& os,
                            const std::string& op_index_prefix) const {
  switch (opcode) {
#define SWITCH_CASE(Name)                              \
  case Opcode::k##Name:                                \
    Cast<Name##Op>().PrintInputs(os, op_index_prefix); \
    break;
    TURBOSHAFT_OPERATION_LIST(SWITCH_CASE)
#undef SWITCH_CASE
  }
}

void Operation::PrintOptions(std::ostream& os) const {
  switch (opcode) {
#define SWITCH_CASE(Name)              \
  case Opcode::k##Name:                \
    Cast<Name##Op>().PrintOptions(os); \
    break;
    TURBOSHAFT_OPERATION_LIST(SWITCH_CASE)
#undef SWITCH_CASE
  }
}

void PendingLoopPhiOp::PrintOptions(std::ostream& os) const {
  os << "[" << rep << ", #o" << old_backedge_index.id() << "]";
}

void ConstantOp::PrintOptions(std::ostream& os) const {
  os << "[";
  switch (kind) {
    case Kind::kWord32:
      os << "word32: " << static_cast<int32_t>(storage.integral);
      break;
    case Kind::kWord64:
      os << "word64: " << static_cast<int64_t>(storage.integral);
      break;
    case Kind::kNumber:
      os << "number: " << number();
      break;
    case Kind::kTaggedIndex:
      os << "tagged index: " << tagged_index();
      break;
    case Kind::kFloat64:
      os << "float64: " << float64();
      break;
    case Kind::kFloat32:
      os << "float32: " << float32();
      break;
    case Kind::kExternal:
      os << "external: " << external_reference();
      break;
    case Kind::kHeapObject:
      os << "heap object: " << handle();
      break;
    case Kind::kCompressedHeapObject:
      os << "compressed heap object: " << handle();
      break;
    case Kind::kRelocatableWasmCall:
      os << "relocatable wasm call: 0x"
         << reinterpret_cast<void*>(storage.integral);
      break;
    case Kind::kRelocatableWasmStubCall:
      os << "relocatable wasm stub call: 0x"
         << reinterpret_cast<void*>(storage.integral);
      break;
  }
  os << "]";
}

void ParameterOp::PrintOptions(std::ostream& os) const {
  os << "[" << parameter_index;
  if (debug_name) os << ", " << debug_name;
  os << "]";
}

void LoadOp::PrintInputs(std::ostream& os,
                         const std::string& op_index_prefix) const {
  os << " *(" << op_index_prefix << base().id();
  if (offset < 0) {
    os << " - " << -offset;
  } else if (offset > 0) {
    os << " + " << offset;
  }
  if (index().valid()) {
    os << " + " << op_index_prefix << index().id();
    if (element_size_log2 > 0) os << "*" << (1 << element_size_log2);
  }
  os << ") ";
}
void LoadOp::PrintOptions(std::ostream& os) const {
  os << "[";
  os << (kind.tagged_base ? "tagged base" : "raw");
  if (kind.maybe_unaligned) os << ", unaligned";
  os << ", " << loaded_rep;
  if (element_size_log2 != 0)
    os << ", element size: 2^" << int{element_size_log2};
  if (offset != 0) os << ", offset: " << offset;
  os << "]";
}

void StoreOp::PrintInputs(std::ostream& os,
                          const std::string& op_index_prefix) const {
  os << " *(" << op_index_prefix << base().id();
  if (offset < 0) {
    os << " - " << -offset;
  } else if (offset > 0) {
    os << " + " << offset;
  }
  if (index().valid()) {
    os << " + " << op_index_prefix << index().id();
    if (element_size_log2 > 0) os << "*" << (1 << element_size_log2);
  }
  os << ") = " << op_index_prefix << value().id() << " ";
}
void StoreOp::PrintOptions(std::ostream& os) const {
  os << "[";
  os << (kind.tagged_base ? "tagged base" : "raw");
  if (kind.maybe_unaligned) os << ", unaligned";
  os << ", " << stored_rep;
  os << ", " << write_barrier;
  if (element_size_log2 != 0)
    os << ", element size: 2^" << int{element_size_log2};
  if (offset != 0) os << ", offset: " << offset;
  os << "]";
}

void AllocateOp::PrintOptions(std::ostream& os) const {
  os << "[";
  os << type << ", ";
  os << (allow_large_objects == AllowLargeObjects::kTrue ? "allow large objects"
                                                         : "no large objects");
  os << "]";
}

void DecodeExternalPointerOp::PrintOptions(std::ostream& os) const {
  os << "[";
  os << "tag: " << std::hex << tag << std::dec;
  os << "]";
}

void FrameStateOp::PrintOptions(std::ostream& os) const {
  os << "[";
  os << (inlined ? "inlined" : "not inlined");
  os << ", ";
  os << data->frame_state_info;
  os << ", state values:";
  FrameStateData::Iterator it = data->iterator(state_values());
  while (it.has_more()) {
    os << " ";
    switch (it.current_instr()) {
      case FrameStateData::Instr::kInput: {
        MachineType type;
        OpIndex input;
        it.ConsumeInput(&type, &input);
        os << "#" << input.id() << "(" << type << ")";
        break;
      }
      case FrameStateData::Instr::kUnusedRegister:
        it.ConsumeUnusedRegister();
        os << ".";
        break;
      case FrameStateData::Instr::kDematerializedObject: {
        uint32_t id;
        uint32_t field_count;
        it.ConsumeDematerializedObject(&id, &field_count);
        os << "$" << id << "(field count: " << field_count << ")";
        break;
      }
      case FrameStateData::Instr::kDematerializedObjectReference: {
        uint32_t id;
        it.ConsumeDematerializedObjectReference(&id);
        os << "$" << id;
        break;
      }
      case FrameStateData::Instr::kArgumentsElements: {
        CreateArgumentsType type;
        it.ConsumeArgumentsElements(&type);
        os << "ArgumentsElements(" << type << ")";
        break;
      }
      case FrameStateData::Instr::kArgumentsLength: {
        it.ConsumeArgumentsLength();
        os << "ArgumentsLength";
        break;
      }
    }
  }
  os << "]";
}

void WordBinopOp::PrintOptions(std::ostream& os) const {
  os << "[";
  switch (kind) {
    case Kind::kAdd:
      os << "Add, ";
      break;
    case Kind::kSub:
      os << "Sub, ";
      break;
    case Kind::kMul:
      os << "Mul, ";
      break;
    case Kind::kSignedMulOverflownBits:
      os << "SignedMulOverflownBits, ";
      break;
    case Kind::kUnsignedMulOverflownBits:
      os << "UnsignedMulOverflownBits, ";
      break;
    case Kind::kSignedDiv:
      os << "SignedDiv, ";
      break;
    case Kind::kUnsignedDiv:
      os << "UnsignedDiv, ";
      break;
    case Kind::kSignedMod:
      os << "SignedMod, ";
      break;
    case Kind::kUnsignedMod:
      os << "UnsignedMod, ";
      break;
    case Kind::kBitwiseAnd:
      os << "BitwiseAnd, ";
      break;
    case Kind::kBitwiseOr:
      os << "BitwiseOr, ";
      break;
    case Kind::kBitwiseXor:
      os << "BitwiseXor, ";
      break;
  }
  os << rep;
  os << "]";
}

void FloatBinopOp::PrintOptions(std::ostream& os) const {
  os << "[";
  switch (kind) {
    case Kind::kAdd:
      os << "Add, ";
      break;
    case Kind::kSub:
      os << "Sub, ";
      break;
    case Kind::kMul:
      os << "Mul, ";
      break;
    case Kind::kDiv:
      os << "Div, ";
      break;
    case Kind::kMod:
      os << "Mod, ";
      break;
    case Kind::kMin:
      os << "Min, ";
      break;
    case Kind::kMax:
      os << "Max, ";
      break;
    case Kind::kPower:
      os << "Power, ";
      break;
    case Kind::kAtan2:
      os << "Atan2, ";
      break;
  }
  os << rep;
  os << "]";
}

void OverflowCheckedBinopOp::PrintOptions(std::ostream& os) const {
  os << "[";
  switch (kind) {
    case Kind::kSignedAdd:
      os << "signed add, ";
      break;
    case Kind::kSignedSub:
      os << "signed sub, ";
      break;
    case Kind::kSignedMul:
      os << "signed mul, ";
      break;
  }
  os << rep;
  os << "]";
}

std::ostream& operator<<(std::ostream& os, OpIndex idx) {
  if (!idx.valid()) {
    return os << "<invalid OpIndex>";
  }
  return os << idx.id();
}

std::ostream& operator<<(std::ostream& os, BlockIndex b) {
  if (!b.valid()) {
    return os << "<invalid block>";
  }
  return os << 'B' << b.id();
}

std::ostream& operator<<(std::ostream& os, const Block* b) {
  return os << b->index();
}

std::ostream& operator<<(std::ostream& os, OpProperties opProperties) {
#define PRINT_PROPERTY(Name, ...)             \
  if (opProperties == OpProperties::Name()) { \
    return os << #Name;                       \
  }

  ALL_OP_PROPERTIES(PRINT_PROPERTY)

#undef PRINT_PROPERTY

  UNREACHABLE();
}

void SwitchOp::PrintOptions(std::ostream& os) const {
  os << "[";
  for (const Case& c : cases) {
    os << "case " << c.value << ": " << c.destination << ", ";
  }
  os << " default: " << default_case << "]";
}

std::string Operation::ToString() const {
  std::stringstream ss;
  ss << *this;
  return ss.str();
}

base::LazyMutex SupportedOperations::mutex_;
SupportedOperations SupportedOperations::instance_;
bool SupportedOperations::initialized_;

void SupportedOperations::Initialize() {
  base::MutexGuard lock(mutex_.Pointer());
  if (initialized_) return;
  initialized_ = true;

  MachineOperatorBuilder::Flags supported =
      InstructionSelector::SupportedMachineOperatorFlags();
#define SET_SUPPORTED(name, machine_name) \
  instance_.name##_ = supported & MachineOperatorBuilder::Flag::k##machine_name;

  SUPPORTED_OPERATIONS_LIST(SET_SUPPORTED)
#undef SET_SUPPORTED
}

}  // namespace v8::internal::compiler::turboshaft
