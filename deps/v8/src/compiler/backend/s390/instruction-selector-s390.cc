// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/adapters.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/execution/frame-constants.h"

namespace v8 {
namespace internal {
namespace compiler {

enum class OperandMode : uint32_t {
  kNone = 0u,
  // Immediate mode
  kShift32Imm = 1u << 0,
  kShift64Imm = 1u << 1,
  kInt32Imm = 1u << 2,
  kInt32Imm_Negate = 1u << 3,
  kUint32Imm = 1u << 4,
  kInt20Imm = 1u << 5,
  kUint12Imm = 1u << 6,
  // Instr format
  kAllowRRR = 1u << 7,
  kAllowRM = 1u << 8,
  kAllowRI = 1u << 9,
  kAllowRRI = 1u << 10,
  kAllowRRM = 1u << 11,
  // Useful combination
  kAllowImmediate = kAllowRI | kAllowRRI,
  kAllowMemoryOperand = kAllowRM | kAllowRRM,
  kAllowDistinctOps = kAllowRRR | kAllowRRI | kAllowRRM,
  kBitWiseCommonMode = kAllowRI,
  kArithmeticCommonMode = kAllowRM | kAllowRI
};

using OperandModes = base::Flags<OperandMode, uint32_t>;
DEFINE_OPERATORS_FOR_FLAGS(OperandModes)
OperandModes immediateModeMask =
    OperandMode::kShift32Imm | OperandMode::kShift64Imm |
    OperandMode::kInt32Imm | OperandMode::kInt32Imm_Negate |
    OperandMode::kUint32Imm | OperandMode::kInt20Imm;

#define AndCommonMode                                                \
  ((OperandMode::kAllowRM |                                          \
    (CpuFeatures::IsSupported(DISTINCT_OPS) ? OperandMode::kAllowRRR \
                                            : OperandMode::kNone)))
#define And64OperandMode AndCommonMode
#define Or64OperandMode And64OperandMode
#define Xor64OperandMode And64OperandMode

#define And32OperandMode \
  (AndCommonMode | OperandMode::kAllowRI | OperandMode::kUint32Imm)
#define Or32OperandMode And32OperandMode
#define Xor32OperandMode And32OperandMode

#define Shift32OperandMode                                   \
  ((OperandMode::kAllowRI | OperandMode::kShift64Imm |       \
    (CpuFeatures::IsSupported(DISTINCT_OPS)                  \
         ? (OperandMode::kAllowRRR | OperandMode::kAllowRRI) \
         : OperandMode::kNone)))

#define Shift64OperandMode                             \
  ((OperandMode::kAllowRI | OperandMode::kShift64Imm | \
    OperandMode::kAllowRRR | OperandMode::kAllowRRI))

#define AddOperandMode                                            \
  ((OperandMode::kArithmeticCommonMode | OperandMode::kInt32Imm | \
    (CpuFeatures::IsSupported(DISTINCT_OPS)                       \
         ? (OperandMode::kAllowRRR | OperandMode::kAllowRRI)      \
         : OperandMode::kArithmeticCommonMode)))
#define SubOperandMode                                                   \
  ((OperandMode::kArithmeticCommonMode | OperandMode::kInt32Imm_Negate | \
    (CpuFeatures::IsSupported(DISTINCT_OPS)                              \
         ? (OperandMode::kAllowRRR | OperandMode::kAllowRRI)             \
         : OperandMode::kArithmeticCommonMode)))
#define MulOperandMode \
  (OperandMode::kArithmeticCommonMode | OperandMode::kInt32Imm)

// Adds S390-specific methods for generating operands.
class S390OperandGenerator final : public OperandGenerator {
 public:
  explicit S390OperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  InstructionOperand UseOperand(Node* node, OperandModes mode) {
    if (CanBeImmediate(node, mode)) {
      return UseImmediate(node);
    }
    return UseRegister(node);
  }

  InstructionOperand UseAnyExceptImmediate(Node* node) {
    if (NodeProperties::IsConstant(node))
      return UseRegister(node);
    else
      return Use(node);
  }

  int64_t GetImmediate(Node* node) {
    if (node->opcode() == IrOpcode::kInt32Constant)
      return OpParameter<int32_t>(node->op());
    else if (node->opcode() == IrOpcode::kInt64Constant)
      return OpParameter<int64_t>(node->op());
    else
      UNIMPLEMENTED();
    return 0L;
  }

  bool CanBeImmediate(Node* node, OperandModes mode) {
    int64_t value;
    if (node->opcode() == IrOpcode::kInt32Constant)
      value = OpParameter<int32_t>(node->op());
    else if (node->opcode() == IrOpcode::kInt64Constant)
      value = OpParameter<int64_t>(node->op());
    else
      return false;
    return CanBeImmediate(value, mode);
  }

  bool CanBeImmediate(int64_t value, OperandModes mode) {
    if (mode & OperandMode::kShift32Imm)
      return 0 <= value && value < 32;
    else if (mode & OperandMode::kShift64Imm)
      return 0 <= value && value < 64;
    else if (mode & OperandMode::kInt32Imm)
      return is_int32(value);
    else if (mode & OperandMode::kInt32Imm_Negate)
      return is_int32(-value);
    else if (mode & OperandMode::kUint32Imm)
      return is_uint32(value);
    else if (mode & OperandMode::kInt20Imm)
      return is_int20(value);
    else if (mode & OperandMode::kUint12Imm)
      return is_uint12(value);
    else
      return false;
  }

  bool CanBeMemoryOperand(InstructionCode opcode, Node* user, Node* input,
                          int effect_level) {
    if (input->opcode() != IrOpcode::kLoad ||
        !selector()->CanCover(user, input)) {
      return false;
    }

    if (effect_level != selector()->GetEffectLevel(input)) {
      return false;
    }

    MachineRepresentation rep =
        LoadRepresentationOf(input->op()).representation();
    switch (opcode) {
      case kS390_Cmp64:
      case kS390_LoadAndTestWord64:
        return rep == MachineRepresentation::kWord64 || IsAnyTagged(rep);
      case kS390_LoadAndTestWord32:
      case kS390_Cmp32:
        return rep == MachineRepresentation::kWord32;
      default:
        break;
    }
    return false;
  }

  AddressingMode GenerateMemoryOperandInputs(Node* index, Node* base,
                                             Node* displacement,
                                             DisplacementMode displacement_mode,
                                             InstructionOperand inputs[],
                                             size_t* input_count) {
    AddressingMode mode = kMode_MRI;
    if (base != nullptr) {
      inputs[(*input_count)++] = UseRegister(base);
      if (index != nullptr) {
        inputs[(*input_count)++] = UseRegister(index);
        if (displacement != nullptr) {
          inputs[(*input_count)++] = displacement_mode
                                         ? UseNegatedImmediate(displacement)
                                         : UseImmediate(displacement);
          mode = kMode_MRRI;
        } else {
          mode = kMode_MRR;
        }
      } else {
        if (displacement == nullptr) {
          mode = kMode_MR;
        } else {
          inputs[(*input_count)++] = displacement_mode == kNegativeDisplacement
                                         ? UseNegatedImmediate(displacement)
                                         : UseImmediate(displacement);
          mode = kMode_MRI;
        }
      }
    } else {
      DCHECK_NOT_NULL(index);
      inputs[(*input_count)++] = UseRegister(index);
      if (displacement != nullptr) {
        inputs[(*input_count)++] = displacement_mode == kNegativeDisplacement
                                       ? UseNegatedImmediate(displacement)
                                       : UseImmediate(displacement);
        mode = kMode_MRI;
      } else {
        mode = kMode_MR;
      }
    }
    return mode;
  }

  AddressingMode GetEffectiveAddressMemoryOperand(
      Node* operand, InstructionOperand inputs[], size_t* input_count,
      OperandModes immediate_mode = OperandMode::kInt20Imm) {
#if V8_TARGET_ARCH_S390X
    BaseWithIndexAndDisplacement64Matcher m(operand,
                                            AddressOption::kAllowInputSwap);
#else
    BaseWithIndexAndDisplacement32Matcher m(operand,
                                            AddressOption::kAllowInputSwap);
#endif
    DCHECK(m.matches());
    if ((m.displacement() == nullptr ||
         CanBeImmediate(m.displacement(), immediate_mode))) {
      DCHECK_EQ(0, m.scale());
      return GenerateMemoryOperandInputs(m.index(), m.base(), m.displacement(),
                                         m.displacement_mode(), inputs,
                                         input_count);
    } else {
      inputs[(*input_count)++] = UseRegister(operand->InputAt(0));
      inputs[(*input_count)++] = UseRegister(operand->InputAt(1));
      return kMode_MRR;
    }
  }

  bool CanBeBetterLeftOperand(Node* node) const {
    return !selector()->IsLive(node);
  }

  MachineRepresentation GetRepresentation(Node* node) {
    return sequence()->GetRepresentation(selector()->GetVirtualRegister(node));
  }

  bool Is64BitOperand(Node* node) {
    return MachineRepresentation::kWord64 == GetRepresentation(node);
  }

  // Use the stack pointer if the node is LoadStackPointer, otherwise assign a
  // register.
  InstructionOperand UseRegisterOrStackPointer(Node* node) {
    if (node->opcode() == IrOpcode::kLoadStackPointer) {
      return LocationOperand(LocationOperand::EXPLICIT,
                             LocationOperand::REGISTER,
                             MachineRepresentation::kWord32, sp.code());
    }
    return UseRegister(node);
  }
};

namespace {

bool S390OpcodeOnlySupport12BitDisp(ArchOpcode opcode) {
  switch (opcode) {
    case kS390_AddFloat:
    case kS390_AddDouble:
    case kS390_CmpFloat:
    case kS390_CmpDouble:
    case kS390_Float32ToDouble:
      return true;
    default:
      return false;
  }
}

bool S390OpcodeOnlySupport12BitDisp(InstructionCode op) {
  ArchOpcode opcode = ArchOpcodeField::decode(op);
  return S390OpcodeOnlySupport12BitDisp(opcode);
}

#define OpcodeImmMode(op)                                       \
  (S390OpcodeOnlySupport12BitDisp(op) ? OperandMode::kUint12Imm \
                                      : OperandMode::kInt20Imm)

ArchOpcode SelectLoadOpcode(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  ArchOpcode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kS390_LoadFloat32;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kS390_LoadDouble;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsSigned() ? kS390_LoadWordS8 : kS390_LoadWordU8;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsSigned() ? kS390_LoadWordS16 : kS390_LoadWordU16;
      break;
#if !V8_TARGET_ARCH_S390X
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
#endif
    case MachineRepresentation::kWord32:
      opcode = kS390_LoadWordU32;
      break;
#if V8_TARGET_ARCH_S390X
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord64:
      opcode = kS390_LoadWord64;
      break;
#else
    case MachineRepresentation::kWord64:  // Fall through.
#endif
    case MachineRepresentation::kCompressedSigned:   // Fall through.
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:         // Fall through.
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
    default:
      UNREACHABLE();
  }
  return opcode;
}

#define RESULT_IS_WORD32_LIST(V)   \
  /* Float unary op*/              \
  V(BitcastFloat32ToInt32)         \
  /* V(TruncateFloat64ToWord32) */ \
  V(RoundFloat64ToInt32)           \
  V(TruncateFloat32ToInt32)        \
  V(TruncateFloat32ToUint32)       \
  V(TruncateFloat64ToUint32)       \
  V(ChangeFloat64ToInt32)          \
  V(ChangeFloat64ToUint32)         \
  /* Word32 unary op */            \
  V(Word32Clz)                     \
  V(Word32Popcnt)                  \
  V(Float64ExtractLowWord32)       \
  V(Float64ExtractHighWord32)      \
  V(SignExtendWord8ToInt32)        \
  V(SignExtendWord16ToInt32)       \
  /* Word32 bin op */              \
  V(Int32Add)                      \
  V(Int32Sub)                      \
  V(Int32Mul)                      \
  V(Int32AddWithOverflow)          \
  V(Int32SubWithOverflow)          \
  V(Int32MulWithOverflow)          \
  V(Int32MulHigh)                  \
  V(Uint32MulHigh)                 \
  V(Int32Div)                      \
  V(Uint32Div)                     \
  V(Int32Mod)                      \
  V(Uint32Mod)                     \
  V(Word32Ror)                     \
  V(Word32And)                     \
  V(Word32Or)                      \
  V(Word32Xor)                     \
  V(Word32Shl)                     \
  V(Word32Shr)                     \
  V(Word32Sar)

bool ProduceWord32Result(Node* node) {
#if !V8_TARGET_ARCH_S390X
  return true;
#else
  switch (node->opcode()) {
#define VISITOR(name) case IrOpcode::k##name:
    RESULT_IS_WORD32_LIST(VISITOR)
#undef VISITOR
    return true;
    // TODO(john.yan): consider the following case to be valid
    // case IrOpcode::kWord32Equal:
    // case IrOpcode::kInt32LessThan:
    // case IrOpcode::kInt32LessThanOrEqual:
    // case IrOpcode::kUint32LessThan:
    // case IrOpcode::kUint32LessThanOrEqual:
    // case IrOpcode::kUint32MulHigh:
    //   // These 32-bit operations implicitly zero-extend to 64-bit on x64, so
    //   the
    //   // zero-extension is a no-op.
    //   return true;
    // case IrOpcode::kProjection: {
    //   Node* const value = node->InputAt(0);
    //   switch (value->opcode()) {
    //     case IrOpcode::kInt32AddWithOverflow:
    //     case IrOpcode::kInt32SubWithOverflow:
    //     case IrOpcode::kInt32MulWithOverflow:
    //       return true;
    //     default:
    //       return false;
    //   }
    // }
    case IrOpcode::kLoad: {
      LoadRepresentation load_rep = LoadRepresentationOf(node->op());
      switch (load_rep.representation()) {
        case MachineRepresentation::kWord32:
          return true;
        case MachineRepresentation::kWord8:
          if (load_rep.IsSigned())
            return false;
          else
            return true;
        default:
          return false;
      }
    }
    default:
      return false;
  }
#endif
}

static inline bool DoZeroExtForResult(Node* node) {
#if V8_TARGET_ARCH_S390X
  return ProduceWord32Result(node);
#else
  return false;
#endif
}

// TODO(john.yan): Create VisiteShift to match dst = src shift (R+I)
#if 0
void VisitShift() { }
#endif

#if V8_TARGET_ARCH_S390X
void VisitTryTruncateDouble(InstructionSelector* selector, ArchOpcode opcode,
                            Node* node) {
  S390OperandGenerator g(selector);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  selector->Emit(opcode, output_count, outputs, 1, inputs);
}
#endif

template <class CanCombineWithLoad>
void GenerateRightOperands(InstructionSelector* selector, Node* node,
                           Node* right, InstructionCode& opcode,
                           OperandModes& operand_mode,
                           InstructionOperand* inputs, size_t& input_count,
                           CanCombineWithLoad canCombineWithLoad) {
  S390OperandGenerator g(selector);

  if ((operand_mode & OperandMode::kAllowImmediate) &&
      g.CanBeImmediate(right, operand_mode)) {
    inputs[input_count++] = g.UseImmediate(right);
    // Can only be RI or RRI
    operand_mode &= OperandMode::kAllowImmediate;
  } else if (operand_mode & OperandMode::kAllowMemoryOperand) {
    NodeMatcher mright(right);
    if (mright.IsLoad() && selector->CanCover(node, right) &&
        canCombineWithLoad(SelectLoadOpcode(right))) {
      AddressingMode mode = g.GetEffectiveAddressMemoryOperand(
          right, inputs, &input_count, OpcodeImmMode(opcode));
      opcode |= AddressingModeField::encode(mode);
      operand_mode &= ~OperandMode::kAllowImmediate;
      if (operand_mode & OperandMode::kAllowRM)
        operand_mode &= ~OperandMode::kAllowDistinctOps;
    } else if (operand_mode & OperandMode::kAllowRM) {
      DCHECK(!(operand_mode & OperandMode::kAllowRRM));
      inputs[input_count++] = g.UseAnyExceptImmediate(right);
      // Can not be Immediate
      operand_mode &=
          ~OperandMode::kAllowImmediate & ~OperandMode::kAllowDistinctOps;
    } else if (operand_mode & OperandMode::kAllowRRM) {
      DCHECK(!(operand_mode & OperandMode::kAllowRM));
      inputs[input_count++] = g.UseAnyExceptImmediate(right);
      // Can not be Immediate
      operand_mode &= ~OperandMode::kAllowImmediate;
    } else {
      UNREACHABLE();
    }
  } else {
    inputs[input_count++] = g.UseRegister(right);
    // Can only be RR or RRR
    operand_mode &= OperandMode::kAllowRRR;
  }
}

template <class CanCombineWithLoad>
void GenerateBinOpOperands(InstructionSelector* selector, Node* node,
                           Node* left, Node* right, InstructionCode& opcode,
                           OperandModes& operand_mode,
                           InstructionOperand* inputs, size_t& input_count,
                           CanCombineWithLoad canCombineWithLoad) {
  S390OperandGenerator g(selector);
  // left is always register
  InstructionOperand const left_input = g.UseRegister(left);
  inputs[input_count++] = left_input;

  if (left == right) {
    inputs[input_count++] = left_input;
    // Can only be RR or RRR
    operand_mode &= OperandMode::kAllowRRR;
  } else {
    GenerateRightOperands(selector, node, right, opcode, operand_mode, inputs,
                          input_count, canCombineWithLoad);
  }
}

template <class CanCombineWithLoad>
void VisitUnaryOp(InstructionSelector* selector, Node* node,
                  InstructionCode opcode, OperandModes operand_mode,
                  FlagsContinuation* cont,
                  CanCombineWithLoad canCombineWithLoad);

template <class CanCombineWithLoad>
void VisitBinOp(InstructionSelector* selector, Node* node,
                InstructionCode opcode, OperandModes operand_mode,
                FlagsContinuation* cont, CanCombineWithLoad canCombineWithLoad);

// Generate The following variations:
//   VisitWord32UnaryOp, VisitWord32BinOp,
//   VisitWord64UnaryOp, VisitWord64BinOp,
//   VisitFloat32UnaryOp, VisitFloat32BinOp,
//   VisitFloat64UnaryOp, VisitFloat64BinOp
#define VISIT_OP_LIST_32(V)                                            \
  V(Word32, Unary, [](ArchOpcode opcode) {                             \
    return opcode == kS390_LoadWordS32 || opcode == kS390_LoadWordU32; \
  })                                                                   \
  V(Word64, Unary,                                                     \
    [](ArchOpcode opcode) { return opcode == kS390_LoadWord64; })      \
  V(Float32, Unary,                                                    \
    [](ArchOpcode opcode) { return opcode == kS390_LoadFloat32; })     \
  V(Float64, Unary,                                                    \
    [](ArchOpcode opcode) { return opcode == kS390_LoadDouble; })      \
  V(Word32, Bin, [](ArchOpcode opcode) {                               \
    return opcode == kS390_LoadWordS32 || opcode == kS390_LoadWordU32; \
  })                                                                   \
  V(Float32, Bin,                                                      \
    [](ArchOpcode opcode) { return opcode == kS390_LoadFloat32; })     \
  V(Float64, Bin, [](ArchOpcode opcode) { return opcode == kS390_LoadDouble; })

#if V8_TARGET_ARCH_S390X
#define VISIT_OP_LIST(V) \
  VISIT_OP_LIST_32(V)    \
  V(Word64, Bin, [](ArchOpcode opcode) { return opcode == kS390_LoadWord64; })
#else
#define VISIT_OP_LIST VISIT_OP_LIST_32
#endif

#define DECLARE_VISIT_HELPER_FUNCTIONS(type1, type2, canCombineWithLoad)  \
  static inline void Visit##type1##type2##Op(                             \
      InstructionSelector* selector, Node* node, InstructionCode opcode,  \
      OperandModes operand_mode, FlagsContinuation* cont) {               \
    Visit##type2##Op(selector, node, opcode, operand_mode, cont,          \
                     canCombineWithLoad);                                 \
  }                                                                       \
  static inline void Visit##type1##type2##Op(                             \
      InstructionSelector* selector, Node* node, InstructionCode opcode,  \
      OperandModes operand_mode) {                                        \
    FlagsContinuation cont;                                               \
    Visit##type1##type2##Op(selector, node, opcode, operand_mode, &cont); \
  }
VISIT_OP_LIST(DECLARE_VISIT_HELPER_FUNCTIONS)
#undef DECLARE_VISIT_HELPER_FUNCTIONS
#undef VISIT_OP_LIST_32
#undef VISIT_OP_LIST

template <class CanCombineWithLoad>
void VisitUnaryOp(InstructionSelector* selector, Node* node,
                  InstructionCode opcode, OperandModes operand_mode,
                  FlagsContinuation* cont,
                  CanCombineWithLoad canCombineWithLoad) {
  S390OperandGenerator g(selector);
  InstructionOperand inputs[8];
  size_t input_count = 0;
  InstructionOperand outputs[2];
  size_t output_count = 0;
  Node* input = node->InputAt(0);

  GenerateRightOperands(selector, node, input, opcode, operand_mode, inputs,
                        input_count, canCombineWithLoad);

  bool input_is_word32 = ProduceWord32Result(input);

  bool doZeroExt = DoZeroExtForResult(node);
  bool canEliminateZeroExt = input_is_word32;

  if (doZeroExt) {
    // Add zero-ext indication
    inputs[input_count++] = g.TempImmediate(!canEliminateZeroExt);
  }

  if (!cont->IsDeoptimize()) {
    // If we can deoptimize as a result of the binop, we need to make sure
    // that the deopt inputs are not overwritten by the binop result. One way
    // to achieve that is to declare the output register as same-as-first.
    if (doZeroExt && canEliminateZeroExt) {
      // we have to make sure result and left use the same register
      outputs[output_count++] = g.DefineSameAsFirst(node);
    } else {
      outputs[output_count++] = g.DefineAsRegister(node);
    }
  } else {
    outputs[output_count++] = g.DefineSameAsFirst(node);
  }

  DCHECK_NE(0u, input_count);
  DCHECK_NE(0u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}

template <class CanCombineWithLoad>
void VisitBinOp(InstructionSelector* selector, Node* node,
                InstructionCode opcode, OperandModes operand_mode,
                FlagsContinuation* cont,
                CanCombineWithLoad canCombineWithLoad) {
  S390OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();
  InstructionOperand inputs[8];
  size_t input_count = 0;
  InstructionOperand outputs[2];
  size_t output_count = 0;

  if (node->op()->HasProperty(Operator::kCommutative) &&
      !g.CanBeImmediate(right, operand_mode) &&
      (g.CanBeBetterLeftOperand(right))) {
    std::swap(left, right);
  }

  GenerateBinOpOperands(selector, node, left, right, opcode, operand_mode,
                        inputs, input_count, canCombineWithLoad);

  bool left_is_word32 = ProduceWord32Result(left);

  bool doZeroExt = DoZeroExtForResult(node);
  bool canEliminateZeroExt = left_is_word32;

  if (doZeroExt) {
    // Add zero-ext indication
    inputs[input_count++] = g.TempImmediate(!canEliminateZeroExt);
  }

  if ((operand_mode & OperandMode::kAllowDistinctOps) &&
      // If we can deoptimize as a result of the binop, we need to make sure
      // that the deopt inputs are not overwritten by the binop result. One way
      // to achieve that is to declare the output register as same-as-first.
      !cont->IsDeoptimize()) {
    if (doZeroExt && canEliminateZeroExt) {
      // we have to make sure result and left use the same register
      outputs[output_count++] = g.DefineSameAsFirst(node);
    } else {
      outputs[output_count++] = g.DefineAsRegister(node);
    }
  } else {
    outputs[output_count++] = g.DefineSameAsFirst(node);
  }

  DCHECK_NE(0u, input_count);
  DCHECK_NE(0u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}

}  // namespace

void InstructionSelector::VisitStackSlot(Node* node) {
  StackSlotRepresentation rep = StackSlotRepresentationOf(node->op());
  int slot = frame_->AllocateSpillSlot(rep.size());
  OperandGenerator g(this);

  Emit(kArchStackSlot, g.DefineAsRegister(node),
       sequence()->AddImmediate(Constant(slot)), 0, nullptr);
}

void InstructionSelector::VisitDebugAbort(Node* node) {
  S390OperandGenerator g(this);
  Emit(kArchDebugAbort, g.NoOutput(), g.UseFixed(node->InputAt(0), r3));
}

void InstructionSelector::VisitLoad(Node* node) {
  S390OperandGenerator g(this);
  InstructionCode opcode = SelectLoadOpcode(node);
  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(node);
  InstructionOperand inputs[3];
  size_t input_count = 0;
  AddressingMode mode =
      g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
  opcode |= AddressingModeField::encode(mode);
  if (node->opcode() == IrOpcode::kPoisonedLoad) {
    CHECK_NE(poisoning_level_, PoisoningMitigationLevel::kDontPoison);
    opcode |= MiscField::encode(kMemoryAccessPoisoned);
  }

  Emit(opcode, 1, outputs, input_count, inputs);
}

void InstructionSelector::VisitPoisonedLoad(Node* node) { VisitLoad(node); }

void InstructionSelector::VisitProtectedLoad(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

static void VisitGeneralStore(
    InstructionSelector* selector, Node* node, MachineRepresentation rep,
    WriteBarrierKind write_barrier_kind = kNoWriteBarrier) {
  S390OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* offset = node->InputAt(1);
  Node* value = node->InputAt(2);
  if (write_barrier_kind != kNoWriteBarrier) {
    DCHECK(CanBeTaggedPointer(rep));
    AddressingMode addressing_mode;
    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    // OutOfLineRecordWrite uses the offset in an 'AddP' instruction as well as
    // for the store itself, so we must check compatibility with both.
    if (g.CanBeImmediate(offset, OperandMode::kInt20Imm)) {
      inputs[input_count++] = g.UseImmediate(offset);
      addressing_mode = kMode_MRI;
    } else {
      inputs[input_count++] = g.UseUniqueRegister(offset);
      addressing_mode = kMode_MRR;
    }
    inputs[input_count++] = g.UseUniqueRegister(value);
    RecordWriteMode record_write_mode =
        WriteBarrierKindToRecordWriteMode(write_barrier_kind);
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    size_t const temp_count = arraysize(temps);
    InstructionCode code = kArchStoreWithWriteBarrier;
    code |= AddressingModeField::encode(addressing_mode);
    code |= MiscField::encode(static_cast<int>(record_write_mode));
    selector->Emit(code, 0, nullptr, input_count, inputs, temp_count, temps);
  } else {
    ArchOpcode opcode = kArchNop;
    NodeMatcher m(value);
    switch (rep) {
      case MachineRepresentation::kFloat32:
        opcode = kS390_StoreFloat32;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kS390_StoreDouble;
        break;
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = kS390_StoreWord8;
        break;
      case MachineRepresentation::kWord16:
        opcode = kS390_StoreWord16;
        break;
#if !V8_TARGET_ARCH_S390X
      case MachineRepresentation::kTaggedSigned:       // Fall through.
      case MachineRepresentation::kTaggedPointer:      // Fall through.
      case MachineRepresentation::kTagged:             // Fall through.
      case MachineRepresentation::kCompressedSigned:   // Fall through.
      case MachineRepresentation::kCompressedPointer:  // Fall through.
      case MachineRepresentation::kCompressed:         // Fall through.
#endif
      case MachineRepresentation::kWord32:
        opcode = kS390_StoreWord32;
        if (m.IsWord32ReverseBytes()) {
          opcode = kS390_StoreReverse32;
          value = value->InputAt(0);
        }
        break;
#if V8_TARGET_ARCH_S390X
      case MachineRepresentation::kTaggedSigned:       // Fall through.
      case MachineRepresentation::kTaggedPointer:      // Fall through.
      case MachineRepresentation::kTagged:             // Fall through.
      case MachineRepresentation::kCompressedSigned:   // Fall through.
      case MachineRepresentation::kCompressedPointer:  // Fall through.
      case MachineRepresentation::kCompressed:         // Fall through.
      case MachineRepresentation::kWord64:
        opcode = kS390_StoreWord64;
        if (m.IsWord64ReverseBytes()) {
          opcode = kS390_StoreReverse64;
          value = value->InputAt(0);
        }
        break;
#else
      case MachineRepresentation::kWord64:  // Fall through.
#endif
      case MachineRepresentation::kSimd128:  // Fall through.
      case MachineRepresentation::kNone:
        UNREACHABLE();
        return;
    }
    InstructionOperand inputs[4];
    size_t input_count = 0;
    AddressingMode addressing_mode =
        g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
    InstructionCode code =
        opcode | AddressingModeField::encode(addressing_mode);
    InstructionOperand value_operand = g.UseRegister(value);
    inputs[input_count++] = value_operand;
    selector->Emit(code, 0, static_cast<InstructionOperand*>(nullptr),
                   input_count, inputs);
  }
}

void InstructionSelector::VisitStore(Node* node) {
  StoreRepresentation store_rep = StoreRepresentationOf(node->op());
  WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();
  MachineRepresentation rep = store_rep.representation();

  VisitGeneralStore(this, node, rep, write_barrier_kind);
}

void InstructionSelector::VisitProtectedStore(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

// Architecture supports unaligned access, therefore VisitLoad is used instead
void InstructionSelector::VisitUnalignedLoad(Node* node) { UNREACHABLE(); }

// Architecture supports unaligned access, therefore VisitStore is used instead
void InstructionSelector::VisitUnalignedStore(Node* node) { UNREACHABLE(); }

#if 0
static inline bool IsContiguousMask32(uint32_t value, int* mb, int* me) {
  int mask_width = base::bits::CountPopulation(value);
  int mask_msb = base::bits::CountLeadingZeros32(value);
  int mask_lsb = base::bits::CountTrailingZeros32(value);
  if ((mask_width == 0) || (mask_msb + mask_width + mask_lsb != 32))
    return false;
  *mb = mask_lsb + mask_width - 1;
  *me = mask_lsb;
  return true;
}
#endif

#if V8_TARGET_ARCH_S390X
static inline bool IsContiguousMask64(uint64_t value, int* mb, int* me) {
  int mask_width = base::bits::CountPopulation(value);
  int mask_msb = base::bits::CountLeadingZeros64(value);
  int mask_lsb = base::bits::CountTrailingZeros64(value);
  if ((mask_width == 0) || (mask_msb + mask_width + mask_lsb != 64))
    return false;
  *mb = mask_lsb + mask_width - 1;
  *me = mask_lsb;
  return true;
}
#endif

#if V8_TARGET_ARCH_S390X
void InstructionSelector::VisitWord64And(Node* node) {
  S390OperandGenerator g(this);
  Int64BinopMatcher m(node);
  int mb = 0;
  int me = 0;
  if (m.right().HasValue() && IsContiguousMask64(m.right().Value(), &mb, &me)) {
    int sh = 0;
    Node* left = m.left().node();
    if ((m.left().IsWord64Shr() || m.left().IsWord64Shl()) &&
        CanCover(node, left)) {
      Int64BinopMatcher mleft(m.left().node());
      if (mleft.right().IsInRange(0, 63)) {
        left = mleft.left().node();
        sh = mleft.right().Value();
        if (m.left().IsWord64Shr()) {
          // Adjust the mask such that it doesn't include any rotated bits.
          if (mb > 63 - sh) mb = 63 - sh;
          sh = (64 - sh) & 0x3F;
        } else {
          // Adjust the mask such that it doesn't include any rotated bits.
          if (me < sh) me = sh;
        }
      }
    }
    if (mb >= me) {
      bool match = false;
      ArchOpcode opcode;
      int mask;
      if (me == 0) {
        match = true;
        opcode = kS390_RotLeftAndClearLeft64;
        mask = mb;
      } else if (mb == 63) {
        match = true;
        opcode = kS390_RotLeftAndClearRight64;
        mask = me;
      } else if (sh && me <= sh && m.left().IsWord64Shl()) {
        match = true;
        opcode = kS390_RotLeftAndClear64;
        mask = mb;
      }
      if (match && CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
        Emit(opcode, g.DefineAsRegister(node), g.UseRegister(left),
             g.TempImmediate(sh), g.TempImmediate(mask));
        return;
      }
    }
  }
  VisitWord64BinOp(this, node, kS390_And64, And64OperandMode);
}

void InstructionSelector::VisitWord64Shl(Node* node) {
  S390OperandGenerator g(this);
  Int64BinopMatcher m(node);
  // TODO(mbrandy): eliminate left sign extension if right >= 32
  if (m.left().IsWord64And() && m.right().IsInRange(0, 63)) {
    Int64BinopMatcher mleft(m.left().node());
    int sh = m.right().Value();
    int mb;
    int me;
    if (mleft.right().HasValue() &&
        IsContiguousMask64(mleft.right().Value() << sh, &mb, &me)) {
      // Adjust the mask such that it doesn't include any rotated bits.
      if (me < sh) me = sh;
      if (mb >= me) {
        bool match = false;
        ArchOpcode opcode;
        int mask;
        if (me == 0) {
          match = true;
          opcode = kS390_RotLeftAndClearLeft64;
          mask = mb;
        } else if (mb == 63) {
          match = true;
          opcode = kS390_RotLeftAndClearRight64;
          mask = me;
        } else if (sh && me <= sh) {
          match = true;
          opcode = kS390_RotLeftAndClear64;
          mask = mb;
        }
        if (match && CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
          Emit(opcode, g.DefineAsRegister(node),
               g.UseRegister(mleft.left().node()), g.TempImmediate(sh),
               g.TempImmediate(mask));
          return;
        }
      }
    }
  }
  VisitWord64BinOp(this, node, kS390_ShiftLeft64, Shift64OperandMode);
}

void InstructionSelector::VisitWord64Shr(Node* node) {
  S390OperandGenerator g(this);
  Int64BinopMatcher m(node);
  if (m.left().IsWord64And() && m.right().IsInRange(0, 63)) {
    Int64BinopMatcher mleft(m.left().node());
    int sh = m.right().Value();
    int mb;
    int me;
    if (mleft.right().HasValue() &&
        IsContiguousMask64((uint64_t)(mleft.right().Value()) >> sh, &mb, &me)) {
      // Adjust the mask such that it doesn't include any rotated bits.
      if (mb > 63 - sh) mb = 63 - sh;
      sh = (64 - sh) & 0x3F;
      if (mb >= me) {
        bool match = false;
        ArchOpcode opcode;
        int mask;
        if (me == 0) {
          match = true;
          opcode = kS390_RotLeftAndClearLeft64;
          mask = mb;
        } else if (mb == 63) {
          match = true;
          opcode = kS390_RotLeftAndClearRight64;
          mask = me;
        }
        if (match) {
          Emit(opcode, g.DefineAsRegister(node),
               g.UseRegister(mleft.left().node()), g.TempImmediate(sh),
               g.TempImmediate(mask));
          return;
        }
      }
    }
  }
  VisitWord64BinOp(this, node, kS390_ShiftRight64, Shift64OperandMode);
}
#endif

static inline bool TryMatchSignExtInt16OrInt8FromWord32Sar(
    InstructionSelector* selector, Node* node) {
  S390OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  if (selector->CanCover(node, m.left().node()) && m.left().IsWord32Shl()) {
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().Is(16) && m.right().Is(16)) {
      bool canEliminateZeroExt = ProduceWord32Result(mleft.left().node());
      selector->Emit(kS390_SignExtendWord16ToInt32,
                     canEliminateZeroExt ? g.DefineSameAsFirst(node)
                                         : g.DefineAsRegister(node),
                     g.UseRegister(mleft.left().node()),
                     g.TempImmediate(!canEliminateZeroExt));
      return true;
    } else if (mleft.right().Is(24) && m.right().Is(24)) {
      bool canEliminateZeroExt = ProduceWord32Result(mleft.left().node());
      selector->Emit(kS390_SignExtendWord8ToInt32,
                     canEliminateZeroExt ? g.DefineSameAsFirst(node)
                                         : g.DefineAsRegister(node),
                     g.UseRegister(mleft.left().node()),
                     g.TempImmediate(!canEliminateZeroExt));
      return true;
    }
  }
  return false;
}

#if !V8_TARGET_ARCH_S390X
void VisitPairBinop(InstructionSelector* selector, InstructionCode opcode,
                    InstructionCode opcode2, Node* node) {
  S390OperandGenerator g(selector);

  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    // We use UseUniqueRegister here to avoid register sharing with the output
    // registers.
    InstructionOperand inputs[] = {
        g.UseRegister(node->InputAt(0)), g.UseUniqueRegister(node->InputAt(1)),
        g.UseRegister(node->InputAt(2)), g.UseUniqueRegister(node->InputAt(3))};

    InstructionOperand outputs[] = {
        g.DefineAsRegister(node),
        g.DefineAsRegister(NodeProperties::FindProjection(node, 1))};

    selector->Emit(opcode, 2, outputs, 4, inputs);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    selector->Emit(opcode2, g.DefineSameAsFirst(node),
                   g.UseRegister(node->InputAt(0)),
                   g.UseRegister(node->InputAt(2)), g.TempImmediate(0));
  }
}

void InstructionSelector::VisitInt32PairAdd(Node* node) {
  VisitPairBinop(this, kS390_AddPair, kS390_Add32, node);
}

void InstructionSelector::VisitInt32PairSub(Node* node) {
  VisitPairBinop(this, kS390_SubPair, kS390_Sub32, node);
}

void InstructionSelector::VisitInt32PairMul(Node* node) {
  S390OperandGenerator g(this);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    InstructionOperand inputs[] = {g.UseUniqueRegister(node->InputAt(0)),
                                   g.UseUniqueRegister(node->InputAt(1)),
                                   g.UseUniqueRegister(node->InputAt(2)),
                                   g.UseUniqueRegister(node->InputAt(3))};

    InstructionOperand outputs[] = {
        g.DefineAsRegister(node),
        g.DefineAsRegister(NodeProperties::FindProjection(node, 1))};

    Emit(kS390_MulPair, 2, outputs, 4, inputs);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    Emit(kS390_Mul32, g.DefineSameAsFirst(node),
         g.UseRegister(node->InputAt(0)), g.Use(node->InputAt(2)),
         g.TempImmediate(0));
  }
}

namespace {
// Shared routine for multiple shift operations.
void VisitPairShift(InstructionSelector* selector, InstructionCode opcode,
                    Node* node) {
  S390OperandGenerator g(selector);
  // We use g.UseUniqueRegister here to guarantee that there is
  // no register aliasing of input registers with output registers.
  Int32Matcher m(node->InputAt(2));
  InstructionOperand shift_operand;
  if (m.HasValue()) {
    shift_operand = g.UseImmediate(m.node());
  } else {
    shift_operand = g.UseUniqueRegister(m.node());
  }

  InstructionOperand inputs[] = {g.UseUniqueRegister(node->InputAt(0)),
                                 g.UseUniqueRegister(node->InputAt(1)),
                                 shift_operand};

  Node* projection1 = NodeProperties::FindProjection(node, 1);

  InstructionOperand outputs[2];
  InstructionOperand temps[1];
  int32_t output_count = 0;
  int32_t temp_count = 0;

  outputs[output_count++] = g.DefineAsRegister(node);
  if (projection1) {
    outputs[output_count++] = g.DefineAsRegister(projection1);
  } else {
    temps[temp_count++] = g.TempRegister();
  }

  selector->Emit(opcode, output_count, outputs, 3, inputs, temp_count, temps);
}
}  // namespace

void InstructionSelector::VisitWord32PairShl(Node* node) {
  VisitPairShift(this, kS390_ShiftLeftPair, node);
}

void InstructionSelector::VisitWord32PairShr(Node* node) {
  VisitPairShift(this, kS390_ShiftRightPair, node);
}

void InstructionSelector::VisitWord32PairSar(Node* node) {
  VisitPairShift(this, kS390_ShiftRightArithPair, node);
}
#endif

void InstructionSelector::VisitWord32Ctz(Node* node) { UNREACHABLE(); }

#if V8_TARGET_ARCH_S390X
void InstructionSelector::VisitWord64Ctz(Node* node) { UNREACHABLE(); }
#endif

void InstructionSelector::VisitWord32ReverseBits(Node* node) { UNREACHABLE(); }

#if V8_TARGET_ARCH_S390X
void InstructionSelector::VisitWord64ReverseBits(Node* node) { UNREACHABLE(); }
#endif

void InstructionSelector::VisitInt32AbsWithOverflow(Node* node) {
  VisitWord32UnaryOp(this, node, kS390_Abs32, OperandMode::kNone);
}

void InstructionSelector::VisitInt64AbsWithOverflow(Node* node) {
  VisitWord64UnaryOp(this, node, kS390_Abs64, OperandMode::kNone);
}

void InstructionSelector::VisitWord64ReverseBytes(Node* node) {
  S390OperandGenerator g(this);
  Emit(kS390_LoadReverse64RR, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

void InstructionSelector::VisitWord32ReverseBytes(Node* node) {
  S390OperandGenerator g(this);
  NodeMatcher input(node->InputAt(0));
  if (CanCover(node, input.node()) && input.IsLoad()) {
    LoadRepresentation load_rep = LoadRepresentationOf(input.node()->op());
    if (load_rep.representation() == MachineRepresentation::kWord32) {
      Node* base = input.node()->InputAt(0);
      Node* offset = input.node()->InputAt(1);
      Emit(kS390_LoadReverse32 | AddressingModeField::encode(kMode_MRR),
           // TODO(john.yan): one of the base and offset can be imm.
           g.DefineAsRegister(node), g.UseRegister(base),
           g.UseRegister(offset));
      return;
    }
  }
  Emit(kS390_LoadReverse32RR, g.DefineAsRegister(node),
       g.UseRegister(node->InputAt(0)));
}

template <class Matcher, ArchOpcode neg_opcode>
static inline bool TryMatchNegFromSub(InstructionSelector* selector,
                                      Node* node) {
  S390OperandGenerator g(selector);
  Matcher m(node);
  static_assert(neg_opcode == kS390_Neg32 || neg_opcode == kS390_Neg64,
                "Provided opcode is not a Neg opcode.");
  if (m.left().Is(0)) {
    Node* value = m.right().node();
    bool doZeroExt = DoZeroExtForResult(node);
    bool canEliminateZeroExt = ProduceWord32Result(value);
    if (doZeroExt) {
      selector->Emit(neg_opcode,
                     canEliminateZeroExt ? g.DefineSameAsFirst(node)
                                         : g.DefineAsRegister(node),
                     g.UseRegister(value),
                     g.TempImmediate(!canEliminateZeroExt));
    } else {
      selector->Emit(neg_opcode, g.DefineAsRegister(node),
                     g.UseRegister(value));
    }
    return true;
  }
  return false;
}

template <class Matcher, ArchOpcode shift_op>
bool TryMatchShiftFromMul(InstructionSelector* selector, Node* node) {
  S390OperandGenerator g(selector);
  Matcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();
  if (g.CanBeImmediate(right, OperandMode::kInt32Imm) &&
      base::bits::IsPowerOfTwo(g.GetImmediate(right))) {
    int power = 63 - base::bits::CountLeadingZeros64(g.GetImmediate(right));
    bool doZeroExt = DoZeroExtForResult(node);
    bool canEliminateZeroExt = ProduceWord32Result(left);
    InstructionOperand dst = (doZeroExt && !canEliminateZeroExt &&
                              CpuFeatures::IsSupported(DISTINCT_OPS))
                                 ? g.DefineAsRegister(node)
                                 : g.DefineSameAsFirst(node);

    if (doZeroExt) {
      selector->Emit(shift_op, dst, g.UseRegister(left), g.UseImmediate(power),
                     g.TempImmediate(!canEliminateZeroExt));
    } else {
      selector->Emit(shift_op, dst, g.UseRegister(left), g.UseImmediate(power));
    }
    return true;
  }
  return false;
}

template <ArchOpcode opcode>
static inline bool TryMatchInt32OpWithOverflow(InstructionSelector* selector,
                                               Node* node, OperandModes mode) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    VisitWord32BinOp(selector, node, opcode, mode, &cont);
    return true;
  }
  return false;
}

static inline bool TryMatchInt32AddWithOverflow(InstructionSelector* selector,
                                                Node* node) {
  return TryMatchInt32OpWithOverflow<kS390_Add32>(selector, node,
                                                  AddOperandMode);
}

static inline bool TryMatchInt32SubWithOverflow(InstructionSelector* selector,
                                                Node* node) {
  return TryMatchInt32OpWithOverflow<kS390_Sub32>(selector, node,
                                                  SubOperandMode);
}

static inline bool TryMatchInt32MulWithOverflow(InstructionSelector* selector,
                                                Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    if (CpuFeatures::IsSupported(MISC_INSTR_EXT2)) {
      TryMatchInt32OpWithOverflow<kS390_Mul32>(
          selector, node, OperandMode::kAllowRRR | OperandMode::kAllowRM);
    } else {
      FlagsContinuation cont = FlagsContinuation::ForSet(kNotEqual, ovf);
      VisitWord32BinOp(selector, node, kS390_Mul32WithOverflow,
                       OperandMode::kInt32Imm | OperandMode::kAllowDistinctOps,
                       &cont);
    }
    return true;
  }
  return TryMatchShiftFromMul<Int32BinopMatcher, kS390_ShiftLeft32>(selector,
                                                                    node);
}

#if V8_TARGET_ARCH_S390X
template <ArchOpcode opcode>
static inline bool TryMatchInt64OpWithOverflow(InstructionSelector* selector,
                                               Node* node, OperandModes mode) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    VisitWord64BinOp(selector, node, opcode, mode, &cont);
    return true;
  }
  return false;
}

static inline bool TryMatchInt64AddWithOverflow(InstructionSelector* selector,
                                                Node* node) {
  return TryMatchInt64OpWithOverflow<kS390_Add64>(selector, node,
                                                  AddOperandMode);
}

static inline bool TryMatchInt64SubWithOverflow(InstructionSelector* selector,
                                                Node* node) {
  return TryMatchInt64OpWithOverflow<kS390_Sub64>(selector, node,
                                                  SubOperandMode);
}
#endif

static inline bool TryMatchDoubleConstructFromInsert(
    InstructionSelector* selector, Node* node) {
  S390OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Node* lo32 = nullptr;
  Node* hi32 = nullptr;

  if (node->opcode() == IrOpcode::kFloat64InsertLowWord32) {
    lo32 = right;
  } else if (node->opcode() == IrOpcode::kFloat64InsertHighWord32) {
    hi32 = right;
  } else {
    return false;  // doesn't match
  }

  if (left->opcode() == IrOpcode::kFloat64InsertLowWord32) {
    lo32 = left->InputAt(1);
  } else if (left->opcode() == IrOpcode::kFloat64InsertHighWord32) {
    hi32 = left->InputAt(1);
  } else {
    return false;  // doesn't match
  }

  if (!lo32 || !hi32) return false;  // doesn't match

  selector->Emit(kS390_DoubleConstruct, g.DefineAsRegister(node),
                 g.UseRegister(hi32), g.UseRegister(lo32));
  return true;
}

#define null ([]() { return false; })
// TODO(john.yan): place kAllowRM where available
#define FLOAT_UNARY_OP_LIST_32(V)                                              \
  V(Float32, ChangeFloat32ToFloat64, kS390_Float32ToDouble,                    \
    OperandMode::kAllowRM, null)                                               \
  V(Float32, BitcastFloat32ToInt32, kS390_BitcastFloat32ToInt32,               \
    OperandMode::kAllowRM, null)                                               \
  V(Float64, TruncateFloat64ToFloat32, kS390_DoubleToFloat32,                  \
    OperandMode::kNone, null)                                                  \
  V(Float64, TruncateFloat64ToWord32, kArchTruncateDoubleToI,                  \
    OperandMode::kNone, null)                                                  \
  V(Float64, RoundFloat64ToInt32, kS390_DoubleToInt32, OperandMode::kNone,     \
    null)                                                                      \
  V(Float32, TruncateFloat32ToInt32, kS390_Float32ToInt32, OperandMode::kNone, \
    null)                                                                      \
  V(Float32, TruncateFloat32ToUint32, kS390_Float32ToUint32,                   \
    OperandMode::kNone, null)                                                  \
  V(Float64, TruncateFloat64ToUint32, kS390_DoubleToUint32,                    \
    OperandMode::kNone, null)                                                  \
  V(Float64, ChangeFloat64ToInt32, kS390_DoubleToInt32, OperandMode::kNone,    \
    null)                                                                      \
  V(Float64, ChangeFloat64ToUint32, kS390_DoubleToUint32, OperandMode::kNone,  \
    null)                                                                      \
  V(Float64, Float64SilenceNaN, kS390_Float64SilenceNaN, OperandMode::kNone,   \
    null)                                                                      \
  V(Float32, Float32Abs, kS390_AbsFloat, OperandMode::kNone, null)             \
  V(Float64, Float64Abs, kS390_AbsDouble, OperandMode::kNone, null)            \
  V(Float32, Float32Sqrt, kS390_SqrtFloat, OperandMode::kNone, null)           \
  V(Float64, Float64Sqrt, kS390_SqrtDouble, OperandMode::kNone, null)          \
  V(Float32, Float32RoundDown, kS390_FloorFloat, OperandMode::kNone, null)     \
  V(Float64, Float64RoundDown, kS390_FloorDouble, OperandMode::kNone, null)    \
  V(Float32, Float32RoundUp, kS390_CeilFloat, OperandMode::kNone, null)        \
  V(Float64, Float64RoundUp, kS390_CeilDouble, OperandMode::kNone, null)       \
  V(Float32, Float32RoundTruncate, kS390_TruncateFloat, OperandMode::kNone,    \
    null)                                                                      \
  V(Float64, Float64RoundTruncate, kS390_TruncateDouble, OperandMode::kNone,   \
    null)                                                                      \
  V(Float64, Float64RoundTiesAway, kS390_RoundDouble, OperandMode::kNone,      \
    null)                                                                      \
  V(Float32, Float32Neg, kS390_NegFloat, OperandMode::kNone, null)             \
  V(Float64, Float64Neg, kS390_NegDouble, OperandMode::kNone, null)            \
  /* TODO(john.yan): can use kAllowRM */                                       \
  V(Word32, Float64ExtractLowWord32, kS390_DoubleExtractLowWord32,             \
    OperandMode::kNone, null)                                                  \
  V(Word32, Float64ExtractHighWord32, kS390_DoubleExtractHighWord32,           \
    OperandMode::kNone, null)

#define FLOAT_BIN_OP_LIST(V)                                           \
  V(Float32, Float32Add, kS390_AddFloat, OperandMode::kAllowRM, null)  \
  V(Float64, Float64Add, kS390_AddDouble, OperandMode::kAllowRM, null) \
  V(Float32, Float32Sub, kS390_SubFloat, OperandMode::kAllowRM, null)  \
  V(Float64, Float64Sub, kS390_SubDouble, OperandMode::kAllowRM, null) \
  V(Float32, Float32Mul, kS390_MulFloat, OperandMode::kAllowRM, null)  \
  V(Float64, Float64Mul, kS390_MulDouble, OperandMode::kAllowRM, null) \
  V(Float32, Float32Div, kS390_DivFloat, OperandMode::kAllowRM, null)  \
  V(Float64, Float64Div, kS390_DivDouble, OperandMode::kAllowRM, null) \
  V(Float32, Float32Max, kS390_MaxFloat, OperandMode::kNone, null)     \
  V(Float64, Float64Max, kS390_MaxDouble, OperandMode::kNone, null)    \
  V(Float32, Float32Min, kS390_MinFloat, OperandMode::kNone, null)     \
  V(Float64, Float64Min, kS390_MinDouble, OperandMode::kNone, null)

#define WORD32_UNARY_OP_LIST_32(V)                                           \
  V(Word32, Word32Clz, kS390_Cntlz32, OperandMode::kNone, null)              \
  V(Word32, Word32Popcnt, kS390_Popcnt32, OperandMode::kNone, null)          \
  V(Word32, RoundInt32ToFloat32, kS390_Int32ToFloat32, OperandMode::kNone,   \
    null)                                                                    \
  V(Word32, RoundUint32ToFloat32, kS390_Uint32ToFloat32, OperandMode::kNone, \
    null)                                                                    \
  V(Word32, ChangeInt32ToFloat64, kS390_Int32ToDouble, OperandMode::kNone,   \
    null)                                                                    \
  V(Word32, ChangeUint32ToFloat64, kS390_Uint32ToDouble, OperandMode::kNone, \
    null)                                                                    \
  V(Word32, SignExtendWord8ToInt32, kS390_SignExtendWord8ToInt32,            \
    OperandMode::kNone, null)                                                \
  V(Word32, SignExtendWord16ToInt32, kS390_SignExtendWord16ToInt32,          \
    OperandMode::kNone, null)                                                \
  V(Word32, BitcastInt32ToFloat32, kS390_BitcastInt32ToFloat32,              \
    OperandMode::kNone, null)

#ifdef V8_TARGET_ARCH_S390X
#define FLOAT_UNARY_OP_LIST(V)                                                \
  FLOAT_UNARY_OP_LIST_32(V)                                                   \
  V(Float64, ChangeFloat64ToUint64, kS390_DoubleToUint64, OperandMode::kNone, \
    null)                                                                     \
  V(Float64, ChangeFloat64ToInt64, kS390_DoubleToInt64, OperandMode::kNone,   \
    null)                                                                     \
  V(Float64, TruncateFloat64ToInt64, kS390_DoubleToInt64, OperandMode::kNone, \
    null)                                                                     \
  V(Float64, BitcastFloat64ToInt64, kS390_BitcastDoubleToInt64,               \
    OperandMode::kNone, null)

#define WORD32_UNARY_OP_LIST(V)                                             \
  WORD32_UNARY_OP_LIST_32(V)                                                \
  V(Word32, ChangeInt32ToInt64, kS390_SignExtendWord32ToInt64,              \
    OperandMode::kNone, null)                                               \
  V(Word32, SignExtendWord8ToInt64, kS390_SignExtendWord8ToInt64,           \
    OperandMode::kNone, null)                                               \
  V(Word32, SignExtendWord16ToInt64, kS390_SignExtendWord16ToInt64,         \
    OperandMode::kNone, null)                                               \
  V(Word32, SignExtendWord32ToInt64, kS390_SignExtendWord32ToInt64,         \
    OperandMode::kNone, null)                                               \
  V(Word32, ChangeUint32ToUint64, kS390_Uint32ToUint64, OperandMode::kNone, \
    [&]() -> bool {                                                         \
      if (ProduceWord32Result(node->InputAt(0))) {                          \
        EmitIdentity(node);                                                 \
        return true;                                                        \
      }                                                                     \
      return false;                                                         \
    })

#else
#define FLOAT_UNARY_OP_LIST(V) FLOAT_UNARY_OP_LIST_32(V)
#define WORD32_UNARY_OP_LIST(V) WORD32_UNARY_OP_LIST_32(V)
#endif

#define WORD32_BIN_OP_LIST(V)                                                  \
  V(Word32, Int32Add, kS390_Add32, AddOperandMode, null)                       \
  V(Word32, Int32Sub, kS390_Sub32, SubOperandMode, ([&]() {                    \
      return TryMatchNegFromSub<Int32BinopMatcher, kS390_Neg32>(this, node);   \
    }))                                                                        \
  V(Word32, Int32Mul, kS390_Mul32, MulOperandMode, ([&]() {                    \
      return TryMatchShiftFromMul<Int32BinopMatcher, kS390_ShiftLeft32>(this,  \
                                                                        node); \
    }))                                                                        \
  V(Word32, Int32AddWithOverflow, kS390_Add32, AddOperandMode,                 \
    ([&]() { return TryMatchInt32AddWithOverflow(this, node); }))              \
  V(Word32, Int32SubWithOverflow, kS390_Sub32, SubOperandMode,                 \
    ([&]() { return TryMatchInt32SubWithOverflow(this, node); }))              \
  V(Word32, Int32MulWithOverflow, kS390_Mul32, MulOperandMode,                 \
    ([&]() { return TryMatchInt32MulWithOverflow(this, node); }))              \
  V(Word32, Int32MulHigh, kS390_MulHigh32,                                     \
    OperandMode::kInt32Imm | OperandMode::kAllowDistinctOps, null)             \
  V(Word32, Uint32MulHigh, kS390_MulHighU32,                                   \
    OperandMode::kAllowRRM | OperandMode::kAllowRRR, null)                     \
  V(Word32, Int32Div, kS390_Div32,                                             \
    OperandMode::kAllowRRM | OperandMode::kAllowRRR, null)                     \
  V(Word32, Uint32Div, kS390_DivU32,                                           \
    OperandMode::kAllowRRM | OperandMode::kAllowRRR, null)                     \
  V(Word32, Int32Mod, kS390_Mod32,                                             \
    OperandMode::kAllowRRM | OperandMode::kAllowRRR, null)                     \
  V(Word32, Uint32Mod, kS390_ModU32,                                           \
    OperandMode::kAllowRRM | OperandMode::kAllowRRR, null)                     \
  V(Word32, Word32Ror, kS390_RotRight32,                                       \
    OperandMode::kAllowRI | OperandMode::kAllowRRR | OperandMode::kAllowRRI |  \
        OperandMode::kShift32Imm,                                              \
    null)                                                                      \
  V(Word32, Word32And, kS390_And32, And32OperandMode, null)                    \
  V(Word32, Word32Or, kS390_Or32, Or32OperandMode, null)                       \
  V(Word32, Word32Xor, kS390_Xor32, Xor32OperandMode, null)                    \
  V(Word32, Word32Shl, kS390_ShiftLeft32, Shift32OperandMode, null)            \
  V(Word32, Word32Shr, kS390_ShiftRight32, Shift32OperandMode, null)           \
  V(Word32, Word32Sar, kS390_ShiftRightArith32, Shift32OperandMode,            \
    [&]() { return TryMatchSignExtInt16OrInt8FromWord32Sar(this, node); })     \
  V(Word32, Float64InsertLowWord32, kS390_DoubleInsertLowWord32,               \
    OperandMode::kAllowRRR,                                                    \
    [&]() -> bool { return TryMatchDoubleConstructFromInsert(this, node); })   \
  V(Word32, Float64InsertHighWord32, kS390_DoubleInsertHighWord32,             \
    OperandMode::kAllowRRR,                                                    \
    [&]() -> bool { return TryMatchDoubleConstructFromInsert(this, node); })

#define WORD64_UNARY_OP_LIST(V)                                              \
  V(Word64, Word64Popcnt, kS390_Popcnt64, OperandMode::kNone, null)          \
  V(Word64, Word64Clz, kS390_Cntlz64, OperandMode::kNone, null)              \
  V(Word64, TruncateInt64ToInt32, kS390_Int64ToInt32, OperandMode::kNone,    \
    null)                                                                    \
  V(Word64, RoundInt64ToFloat32, kS390_Int64ToFloat32, OperandMode::kNone,   \
    null)                                                                    \
  V(Word64, RoundInt64ToFloat64, kS390_Int64ToDouble, OperandMode::kNone,    \
    null)                                                                    \
  V(Word64, ChangeInt64ToFloat64, kS390_Int64ToDouble, OperandMode::kNone,   \
    null)                                                                    \
  V(Word64, RoundUint64ToFloat32, kS390_Uint64ToFloat32, OperandMode::kNone, \
    null)                                                                    \
  V(Word64, RoundUint64ToFloat64, kS390_Uint64ToDouble, OperandMode::kNone,  \
    null)                                                                    \
  V(Word64, BitcastInt64ToFloat64, kS390_BitcastInt64ToDouble,               \
    OperandMode::kNone, null)

#define WORD64_BIN_OP_LIST(V)                                                  \
  V(Word64, Int64Add, kS390_Add64, AddOperandMode, null)                       \
  V(Word64, Int64Sub, kS390_Sub64, SubOperandMode, ([&]() {                    \
      return TryMatchNegFromSub<Int64BinopMatcher, kS390_Neg64>(this, node);   \
    }))                                                                        \
  V(Word64, Int64AddWithOverflow, kS390_Add64, AddOperandMode,                 \
    ([&]() { return TryMatchInt64AddWithOverflow(this, node); }))              \
  V(Word64, Int64SubWithOverflow, kS390_Sub64, SubOperandMode,                 \
    ([&]() { return TryMatchInt64SubWithOverflow(this, node); }))              \
  V(Word64, Int64Mul, kS390_Mul64, MulOperandMode, ([&]() {                    \
      return TryMatchShiftFromMul<Int64BinopMatcher, kS390_ShiftLeft64>(this,  \
                                                                        node); \
    }))                                                                        \
  V(Word64, Int64Div, kS390_Div64,                                             \
    OperandMode::kAllowRRM | OperandMode::kAllowRRR, null)                     \
  V(Word64, Uint64Div, kS390_DivU64,                                           \
    OperandMode::kAllowRRM | OperandMode::kAllowRRR, null)                     \
  V(Word64, Int64Mod, kS390_Mod64,                                             \
    OperandMode::kAllowRRM | OperandMode::kAllowRRR, null)                     \
  V(Word64, Uint64Mod, kS390_ModU64,                                           \
    OperandMode::kAllowRRM | OperandMode::kAllowRRR, null)                     \
  V(Word64, Word64Sar, kS390_ShiftRightArith64, Shift64OperandMode, null)      \
  V(Word64, Word64Ror, kS390_RotRight64, Shift64OperandMode, null)             \
  V(Word64, Word64Or, kS390_Or64, Or64OperandMode, null)                       \
  V(Word64, Word64Xor, kS390_Xor64, Xor64OperandMode, null)

#define DECLARE_UNARY_OP(type, name, op, mode, try_extra) \
  void InstructionSelector::Visit##name(Node* node) {     \
    if (std::function<bool()>(try_extra)()) return;       \
    Visit##type##UnaryOp(this, node, op, mode);           \
  }

#define DECLARE_BIN_OP(type, name, op, mode, try_extra) \
  void InstructionSelector::Visit##name(Node* node) {   \
    if (std::function<bool()>(try_extra)()) return;     \
    Visit##type##BinOp(this, node, op, mode);           \
  }

WORD32_BIN_OP_LIST(DECLARE_BIN_OP)
WORD32_UNARY_OP_LIST(DECLARE_UNARY_OP)
FLOAT_UNARY_OP_LIST(DECLARE_UNARY_OP)
FLOAT_BIN_OP_LIST(DECLARE_BIN_OP)

#if V8_TARGET_ARCH_S390X
WORD64_UNARY_OP_LIST(DECLARE_UNARY_OP)
WORD64_BIN_OP_LIST(DECLARE_BIN_OP)
#endif

#undef DECLARE_BIN_OP
#undef DECLARE_UNARY_OP
#undef WORD64_BIN_OP_LIST
#undef WORD64_UNARY_OP_LIST
#undef WORD32_BIN_OP_LIST
#undef WORD32_UNARY_OP_LIST
#undef FLOAT_UNARY_OP_LIST
#undef WORD32_UNARY_OP_LIST_32
#undef FLOAT_BIN_OP_LIST
#undef FLOAT_BIN_OP_LIST_32
#undef null

#if V8_TARGET_ARCH_S390X
void InstructionSelector::VisitTryTruncateFloat32ToInt64(Node* node) {
  VisitTryTruncateDouble(this, kS390_Float32ToInt64, node);
}

void InstructionSelector::VisitTryTruncateFloat64ToInt64(Node* node) {
  VisitTryTruncateDouble(this, kS390_DoubleToInt64, node);
}

void InstructionSelector::VisitTryTruncateFloat32ToUint64(Node* node) {
  VisitTryTruncateDouble(this, kS390_Float32ToUint64, node);
}

void InstructionSelector::VisitTryTruncateFloat64ToUint64(Node* node) {
  VisitTryTruncateDouble(this, kS390_DoubleToUint64, node);
}

#endif

void InstructionSelector::VisitFloat64Mod(Node* node) {
  S390OperandGenerator g(this);
  Emit(kS390_ModDouble, g.DefineAsFixed(node, d1),
       g.UseFixed(node->InputAt(0), d1), g.UseFixed(node->InputAt(1), d2))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat64Ieee754Unop(Node* node,
                                                  InstructionCode opcode) {
  S390OperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, d1), g.UseFixed(node->InputAt(0), d1))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat64Ieee754Binop(Node* node,
                                                   InstructionCode opcode) {
  S390OperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, d1), g.UseFixed(node->InputAt(0), d1),
       g.UseFixed(node->InputAt(1), d2))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat32RoundTiesEven(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitFloat64RoundTiesEven(Node* node) {
  UNREACHABLE();
}

static bool CompareLogical(FlagsContinuation* cont) {
  switch (cont->condition()) {
    case kUnsignedLessThan:
    case kUnsignedGreaterThanOrEqual:
    case kUnsignedLessThanOrEqual:
    case kUnsignedGreaterThan:
      return true;
    default:
      return false;
  }
  UNREACHABLE();
}

namespace {

// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  InstructionOperand left, InstructionOperand right,
                  FlagsContinuation* cont) {
  selector->EmitWithContinuation(opcode, left, right, cont);
}

void VisitLoadAndTest(InstructionSelector* selector, InstructionCode opcode,
                      Node* node, Node* value, FlagsContinuation* cont,
                      bool discard_output = false);

// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont,
                      OperandModes immediate_mode) {
  S390OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  DCHECK(IrOpcode::IsComparisonOpcode(node->opcode()) ||
         node->opcode() == IrOpcode::kInt32Sub ||
         node->opcode() == IrOpcode::kInt64Sub);

  InstructionOperand inputs[8];
  InstructionOperand outputs[1];
  size_t input_count = 0;
  size_t output_count = 0;

  // If one of the two inputs is an immediate, make sure it's on the right, or
  // if one of the two inputs is a memory operand, make sure it's on the left.
  int effect_level = selector->GetEffectLevel(node);
  if (cont->IsBranch()) {
    effect_level = selector->GetEffectLevel(
        cont->true_block()->PredecessorAt(0)->control_input());
  }

  if ((!g.CanBeImmediate(right, immediate_mode) &&
       g.CanBeImmediate(left, immediate_mode)) ||
      (!g.CanBeMemoryOperand(opcode, node, right, effect_level) &&
       g.CanBeMemoryOperand(opcode, node, left, effect_level))) {
    if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
    std::swap(left, right);
  }

  // check if compare with 0
  if (g.CanBeImmediate(right, immediate_mode) && g.GetImmediate(right) == 0) {
    DCHECK(opcode == kS390_Cmp32 || opcode == kS390_Cmp64);
    ArchOpcode load_and_test = (opcode == kS390_Cmp32)
                                   ? kS390_LoadAndTestWord32
                                   : kS390_LoadAndTestWord64;
    return VisitLoadAndTest(selector, load_and_test, node, left, cont, true);
  }

  inputs[input_count++] = g.UseRegisterOrStackPointer(left);
  if (g.CanBeMemoryOperand(opcode, node, right, effect_level)) {
    // generate memory operand
    AddressingMode addressing_mode = g.GetEffectiveAddressMemoryOperand(
        right, inputs, &input_count, OpcodeImmMode(opcode));
    opcode |= AddressingModeField::encode(addressing_mode);
  } else if (g.CanBeImmediate(right, immediate_mode)) {
    inputs[input_count++] = g.UseImmediate(right);
  } else {
    inputs[input_count++] = g.UseAnyExceptImmediate(right);
  }

  DCHECK(input_count <= 8 && output_count <= 1);
  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}

void VisitWord32Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  OperandModes mode =
      (CompareLogical(cont) ? OperandMode::kUint32Imm : OperandMode::kInt32Imm);
  VisitWordCompare(selector, node, kS390_Cmp32, cont, mode);
}

#if V8_TARGET_ARCH_S390X
void VisitWord64Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  OperandModes mode =
      (CompareLogical(cont) ? OperandMode::kUint32Imm : OperandMode::kInt32Imm);
  VisitWordCompare(selector, node, kS390_Cmp64, cont, mode);
}
#endif

// Shared routine for multiple float32 compare operations.
void VisitFloat32Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  VisitWordCompare(selector, node, kS390_CmpFloat, cont, OperandMode::kNone);
}

// Shared routine for multiple float64 compare operations.
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  VisitWordCompare(selector, node, kS390_CmpDouble, cont, OperandMode::kNone);
}

void VisitTestUnderMask(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  DCHECK(node->opcode() == IrOpcode::kWord32And ||
         node->opcode() == IrOpcode::kWord64And);
  ArchOpcode opcode =
      (node->opcode() == IrOpcode::kWord32And) ? kS390_Tst32 : kS390_Tst64;
  S390OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (!g.CanBeImmediate(right, OperandMode::kUint32Imm) &&
      g.CanBeImmediate(left, OperandMode::kUint32Imm)) {
    std::swap(left, right);
  }
  VisitCompare(selector, opcode, g.UseRegister(left),
               g.UseOperand(right, OperandMode::kUint32Imm), cont);
}

void VisitLoadAndTest(InstructionSelector* selector, InstructionCode opcode,
                      Node* node, Node* value, FlagsContinuation* cont,
                      bool discard_output) {
  static_assert(kS390_LoadAndTestFloat64 - kS390_LoadAndTestWord32 == 3,
                "LoadAndTest Opcode shouldn't contain other opcodes.");

  // TODO(john.yan): Add support for Float32/Float64.
  DCHECK(opcode >= kS390_LoadAndTestWord32 ||
         opcode <= kS390_LoadAndTestWord64);

  S390OperandGenerator g(selector);
  InstructionOperand inputs[8];
  InstructionOperand outputs[2];
  size_t input_count = 0;
  size_t output_count = 0;
  bool use_value = false;

  int effect_level = selector->GetEffectLevel(node);
  if (cont->IsBranch()) {
    effect_level = selector->GetEffectLevel(
        cont->true_block()->PredecessorAt(0)->control_input());
  }

  if (g.CanBeMemoryOperand(opcode, node, value, effect_level)) {
    // generate memory operand
    AddressingMode addressing_mode =
        g.GetEffectiveAddressMemoryOperand(value, inputs, &input_count);
    opcode |= AddressingModeField::encode(addressing_mode);
  } else {
    inputs[input_count++] = g.UseAnyExceptImmediate(value);
    use_value = true;
  }

  if (!discard_output && !use_value) {
    outputs[output_count++] = g.DefineAsRegister(value);
  }

  DCHECK(input_count <= 8 && output_count <= 2);
  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}

}  // namespace

// Shared routine for word comparisons against zero.
void InstructionSelector::VisitWordCompareZero(Node* user, Node* value,
                                               FlagsContinuation* cont) {
  // Try to combine with comparisons against 0 by simply inverting the branch.
  while (value->opcode() == IrOpcode::kWord32Equal && CanCover(user, value)) {
    Int32BinopMatcher m(value);
    if (!m.right().Is(0)) break;

    user = value;
    value = m.left().node();
    cont->Negate();
  }

  FlagsCondition fc = cont->condition();
  if (CanCover(user, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord32Equal: {
        cont->OverwriteAndNegateIfEqual(kEqual);
        Int32BinopMatcher m(value);
        if (m.right().Is(0)) {
          // Try to combine the branch with a comparison.
          Node* const user = m.node();
          Node* const value = m.left().node();
          if (CanCover(user, value)) {
            switch (value->opcode()) {
              case IrOpcode::kInt32Sub:
                return VisitWord32Compare(this, value, cont);
              case IrOpcode::kWord32And:
                return VisitTestUnderMask(this, value, cont);
              default:
                break;
            }
          }
        }
        return VisitWord32Compare(this, value, cont);
      }
      case IrOpcode::kInt32LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWord32Compare(this, value, cont);
      case IrOpcode::kInt32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWord32Compare(this, value, cont);
      case IrOpcode::kUint32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWord32Compare(this, value, cont);
      case IrOpcode::kUint32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWord32Compare(this, value, cont);
#if V8_TARGET_ARCH_S390X
      case IrOpcode::kWord64Equal: {
        cont->OverwriteAndNegateIfEqual(kEqual);
        Int64BinopMatcher m(value);
        if (m.right().Is(0)) {
          // Try to combine the branch with a comparison.
          Node* const user = m.node();
          Node* const value = m.left().node();
          if (CanCover(user, value)) {
            switch (value->opcode()) {
              case IrOpcode::kInt64Sub:
                return VisitWord64Compare(this, value, cont);
              case IrOpcode::kWord64And:
                return VisitTestUnderMask(this, value, cont);
              default:
                break;
            }
          }
        }
        return VisitWord64Compare(this, value, cont);
      }
      case IrOpcode::kInt64LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWord64Compare(this, value, cont);
      case IrOpcode::kInt64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWord64Compare(this, value, cont);
      case IrOpcode::kUint64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWord64Compare(this, value, cont);
      case IrOpcode::kUint64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWord64Compare(this, value, cont);
#endif
      case IrOpcode::kFloat32Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat64Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitFloat64Compare(this, value, cont);
      case IrOpcode::kFloat64LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitFloat64Compare(this, value, cont);
      case IrOpcode::kFloat64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitFloat64Compare(this, value, cont);
      case IrOpcode::kProjection:
        // Check if this is the overflow output projection of an
        // <Operation>WithOverflow node.
        if (ProjectionIndexOf(value->op()) == 1u) {
          // We cannot combine the <Operation>WithOverflow with this branch
          // unless the 0th projection (the use of the actual value of the
          // <Operation> is either nullptr, which means there's no use of the
          // actual value, or was already defined, which means it is scheduled
          // *AFTER* this branch).
          Node* const node = value->InputAt(0);
          Node* const result = NodeProperties::FindProjection(node, 0);
          if (result == nullptr || IsDefined(result)) {
            switch (node->opcode()) {
              case IrOpcode::kInt32AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitWord32BinOp(this, node, kS390_Add32, AddOperandMode,
                                        cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitWord32BinOp(this, node, kS390_Sub32, SubOperandMode,
                                        cont);
              case IrOpcode::kInt32MulWithOverflow:
                if (CpuFeatures::IsSupported(MISC_INSTR_EXT2)) {
                  cont->OverwriteAndNegateIfEqual(kOverflow);
                  return VisitWord32BinOp(
                      this, node, kS390_Mul32,
                      OperandMode::kAllowRRR | OperandMode::kAllowRM, cont);
                } else {
                  cont->OverwriteAndNegateIfEqual(kNotEqual);
                  return VisitWord32BinOp(
                      this, node, kS390_Mul32WithOverflow,
                      OperandMode::kInt32Imm | OperandMode::kAllowDistinctOps,
                      cont);
                }
              case IrOpcode::kInt32AbsWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitWord32UnaryOp(this, node, kS390_Abs32,
                                          OperandMode::kNone, cont);
#if V8_TARGET_ARCH_S390X
              case IrOpcode::kInt64AbsWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitWord64UnaryOp(this, node, kS390_Abs64,
                                          OperandMode::kNone, cont);
              case IrOpcode::kInt64AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitWord64BinOp(this, node, kS390_Add64, AddOperandMode,
                                        cont);
              case IrOpcode::kInt64SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitWord64BinOp(this, node, kS390_Sub64, SubOperandMode,
                                        cont);
#endif
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Sub:
        if (fc == kNotEqual || fc == kEqual)
          return VisitWord32Compare(this, value, cont);
        break;
      case IrOpcode::kWord32And:
        return VisitTestUnderMask(this, value, cont);
      case IrOpcode::kLoad: {
        LoadRepresentation load_rep = LoadRepresentationOf(value->op());
        switch (load_rep.representation()) {
          case MachineRepresentation::kWord32:
            return VisitLoadAndTest(this, kS390_LoadAndTestWord32, user, value,
                                    cont);
          default:
            break;
        }
        break;
      }
      case IrOpcode::kInt32Add:
        // can't handle overflow case.
        break;
      case IrOpcode::kWord32Or:
        if (fc == kNotEqual || fc == kEqual)
          return VisitWord32BinOp(this, value, kS390_Or32, Or32OperandMode,
                                  cont);
        break;
      case IrOpcode::kWord32Xor:
        if (fc == kNotEqual || fc == kEqual)
          return VisitWord32BinOp(this, value, kS390_Xor32, Xor32OperandMode,
                                  cont);
        break;
      case IrOpcode::kWord32Sar:
      case IrOpcode::kWord32Shl:
      case IrOpcode::kWord32Shr:
      case IrOpcode::kWord32Ror:
        // doesn't generate cc, so ignore.
        break;
#if V8_TARGET_ARCH_S390X
      case IrOpcode::kInt64Sub:
        if (fc == kNotEqual || fc == kEqual)
          return VisitWord64Compare(this, value, cont);
        break;
      case IrOpcode::kWord64And:
        return VisitTestUnderMask(this, value, cont);
      case IrOpcode::kInt64Add:
        // can't handle overflow case.
        break;
      case IrOpcode::kWord64Or:
        if (fc == kNotEqual || fc == kEqual)
          return VisitWord64BinOp(this, value, kS390_Or64, Or64OperandMode,
                                  cont);
        break;
      case IrOpcode::kWord64Xor:
        if (fc == kNotEqual || fc == kEqual)
          return VisitWord64BinOp(this, value, kS390_Xor64, Xor64OperandMode,
                                  cont);
        break;
      case IrOpcode::kWord64Sar:
      case IrOpcode::kWord64Shl:
      case IrOpcode::kWord64Shr:
      case IrOpcode::kWord64Ror:
        // doesn't generate cc, so ignore
        break;
#endif
      default:
        break;
    }
  }

  // Branch could not be combined with a compare, emit LoadAndTest
  VisitLoadAndTest(this, kS390_LoadAndTestWord32, user, value, cont, true);
}

void InstructionSelector::VisitSwitch(Node* node, const SwitchInfo& sw) {
  S390OperandGenerator g(this);
  InstructionOperand value_operand = g.UseRegister(node->InputAt(0));

  // Emit either ArchTableSwitch or ArchLookupSwitch.
  if (enable_switch_jump_table_ == kEnableSwitchJumpTable) {
    static const size_t kMaxTableSwitchValueRange = 2 << 16;
    size_t table_space_cost = 4 + sw.value_range();
    size_t table_time_cost = 3;
    size_t lookup_space_cost = 3 + 2 * sw.case_count();
    size_t lookup_time_cost = sw.case_count();
    if (sw.case_count() > 0 &&
        table_space_cost + 3 * table_time_cost <=
            lookup_space_cost + 3 * lookup_time_cost &&
        sw.min_value() > std::numeric_limits<int32_t>::min() &&
        sw.value_range() <= kMaxTableSwitchValueRange) {
      InstructionOperand index_operand = value_operand;
      if (sw.min_value()) {
        index_operand = g.TempRegister();
        Emit(kS390_Lay | AddressingModeField::encode(kMode_MRI), index_operand,
             value_operand, g.TempImmediate(-sw.min_value()));
      }
#if V8_TARGET_ARCH_S390X
      InstructionOperand index_operand_zero_ext = g.TempRegister();
      Emit(kS390_Uint32ToUint64, index_operand_zero_ext, index_operand);
      index_operand = index_operand_zero_ext;
#endif
      // Generate a table lookup.
      return EmitTableSwitch(sw, index_operand);
    }
  }

  // Generate a tree of conditional jumps.
  return EmitBinarySearchSwitch(sw, value_operand);
}

void InstructionSelector::VisitWord32Equal(Node* const node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) {
    return VisitLoadAndTest(this, kS390_LoadAndTestWord32, m.node(),
                            m.left().node(), &cont, true);
  }
  VisitWord32Compare(this, node, &cont);
}

void InstructionSelector::VisitInt32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWord32Compare(this, node, &cont);
}

void InstructionSelector::VisitInt32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWord32Compare(this, node, &cont);
}

void InstructionSelector::VisitUint32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWord32Compare(this, node, &cont);
}

void InstructionSelector::VisitUint32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWord32Compare(this, node, &cont);
}

#if V8_TARGET_ARCH_S390X
void InstructionSelector::VisitWord64Equal(Node* const node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int64BinopMatcher m(node);
  if (m.right().Is(0)) {
    return VisitLoadAndTest(this, kS390_LoadAndTestWord64, m.node(),
                            m.left().node(), &cont, true);
  }
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelector::VisitInt64LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelector::VisitInt64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelector::VisitUint64LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWord64Compare(this, node, &cont);
}

void InstructionSelector::VisitUint64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWord64Compare(this, node, &cont);
}
#endif

void InstructionSelector::VisitFloat32Equal(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitFloat32Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64Equal(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* call_descriptor,
    Node* node) {
  S390OperandGenerator g(this);

  // Prepare for C function call.
  if (call_descriptor->IsCFunctionCall()) {
    Emit(kArchPrepareCallCFunction | MiscField::encode(static_cast<int>(
                                         call_descriptor->ParameterCount())),
         0, nullptr, 0, nullptr);

    // Poke any stack arguments.
    int slot = kStackFrameExtraParamSlot;
    for (PushParameter input : (*arguments)) {
      if (input.node == nullptr) continue;
      Emit(kS390_StoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
           g.TempImmediate(slot));
      ++slot;
    }
  } else {
    // Push any stack arguments.
    int num_slots = 0;
    int slot = 0;

    for (PushParameter input : *arguments) {
      if (input.node == nullptr) continue;
      num_slots += input.location.GetType().representation() ==
                           MachineRepresentation::kFloat64
                       ? kDoubleSize / kSystemPointerSize
                       : 1;
    }
    Emit(kS390_StackClaim, g.NoOutput(), g.TempImmediate(num_slots));
    for (PushParameter input : *arguments) {
      // Skip any alignment holes in pushed nodes.
      if (input.node) {
        Emit(kS390_StoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
             g.TempImmediate(slot));
        slot += input.location.GetType().representation() ==
                        MachineRepresentation::kFloat64
                    ? (kDoubleSize / kSystemPointerSize)
                    : 1;
      }
    }
    DCHECK(num_slots == slot);
  }
}

bool InstructionSelector::IsTailCallAddressImmediate() { return false; }

int InstructionSelector::GetTempsCountForTailCallFromJSFunction() { return 3; }

void InstructionSelector::VisitChangeTaggedToCompressed(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitChangeTaggedPointerToCompressedPointer(
    Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitChangeTaggedSignedToCompressedSigned(
    Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitChangeCompressedToTagged(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitChangeCompressedPointerToTaggedPointer(
    Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitChangeCompressedSignedToTaggedSigned(
    Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitWord32AtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  DCHECK(load_rep.representation() == MachineRepresentation::kWord8 ||
         load_rep.representation() == MachineRepresentation::kWord16 ||
         load_rep.representation() == MachineRepresentation::kWord32);
  USE(load_rep);
  VisitLoad(node);
}

void InstructionSelector::VisitWord32AtomicStore(Node* node) {
  MachineRepresentation rep = AtomicStoreRepresentationOf(node->op());
  VisitGeneralStore(this, node, rep);
}

void VisitAtomicExchange(InstructionSelector* selector, Node* node,
                         ArchOpcode opcode) {
  S390OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  AddressingMode addressing_mode = kMode_MRR;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);
  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(node);
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  selector->Emit(code, 1, outputs, input_count, inputs);
}

void InstructionSelector::VisitWord32AtomicExchange(Node* node) {
  ArchOpcode opcode = kArchNop;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Int8()) {
    opcode = kWord32AtomicExchangeInt8;
  } else if (type == MachineType::Uint8()) {
    opcode = kWord32AtomicExchangeUint8;
  } else if (type == MachineType::Int16()) {
    opcode = kWord32AtomicExchangeInt16;
  } else if (type == MachineType::Uint16()) {
    opcode = kWord32AtomicExchangeUint16;
  } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
    opcode = kWord32AtomicExchangeWord32;
  } else {
    UNREACHABLE();
    return;
  }
  VisitAtomicExchange(this, node, opcode);
}

void InstructionSelector::VisitWord64AtomicExchange(Node* node) {
  ArchOpcode opcode = kArchNop;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Uint8()) {
    opcode = kS390_Word64AtomicExchangeUint8;
  } else if (type == MachineType::Uint16()) {
    opcode = kS390_Word64AtomicExchangeUint16;
  } else if (type == MachineType::Uint32()) {
    opcode = kS390_Word64AtomicExchangeUint32;
  } else if (type == MachineType::Uint64()) {
    opcode = kS390_Word64AtomicExchangeUint64;
  } else {
    UNREACHABLE();
    return;
  }
  VisitAtomicExchange(this, node, opcode);
}

void VisitAtomicCompareExchange(InstructionSelector* selector, Node* node,
                                ArchOpcode opcode) {
  S390OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* old_value = node->InputAt(2);
  Node* new_value = node->InputAt(3);

  InstructionOperand inputs[4];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(old_value);
  inputs[input_count++] = g.UseUniqueRegister(new_value);
  inputs[input_count++] = g.UseUniqueRegister(base);

  AddressingMode addressing_mode;
  if (g.CanBeImmediate(index, OperandMode::kInt20Imm)) {
    inputs[input_count++] = g.UseImmediate(index);
    addressing_mode = kMode_MRI;
  } else {
    inputs[input_count++] = g.UseUniqueRegister(index);
    addressing_mode = kMode_MRR;
  }

  InstructionOperand outputs[1];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineSameAsFirst(node);

  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  selector->Emit(code, output_count, outputs, input_count, inputs);
}

void InstructionSelector::VisitWord32AtomicCompareExchange(Node* node) {
  MachineType type = AtomicOpType(node->op());
  ArchOpcode opcode = kArchNop;
  if (type == MachineType::Int8()) {
    opcode = kWord32AtomicCompareExchangeInt8;
  } else if (type == MachineType::Uint8()) {
    opcode = kWord32AtomicCompareExchangeUint8;
  } else if (type == MachineType::Int16()) {
    opcode = kWord32AtomicCompareExchangeInt16;
  } else if (type == MachineType::Uint16()) {
    opcode = kWord32AtomicCompareExchangeUint16;
  } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
    opcode = kWord32AtomicCompareExchangeWord32;
  } else {
    UNREACHABLE();
    return;
  }
  VisitAtomicCompareExchange(this, node, opcode);
}

void InstructionSelector::VisitWord64AtomicCompareExchange(Node* node) {
  MachineType type = AtomicOpType(node->op());
  ArchOpcode opcode = kArchNop;
  if (type == MachineType::Uint8()) {
    opcode = kS390_Word64AtomicCompareExchangeUint8;
  } else if (type == MachineType::Uint16()) {
    opcode = kS390_Word64AtomicCompareExchangeUint16;
  } else if (type == MachineType::Uint32()) {
    opcode = kS390_Word64AtomicCompareExchangeUint32;
  } else if (type == MachineType::Uint64()) {
    opcode = kS390_Word64AtomicCompareExchangeUint64;
  } else {
    UNREACHABLE();
    return;
  }
  VisitAtomicCompareExchange(this, node, opcode);
}

void VisitAtomicBinop(InstructionSelector* selector, Node* node,
                      ArchOpcode opcode) {
  S390OperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);

  AddressingMode addressing_mode;
  if (g.CanBeImmediate(index, OperandMode::kInt20Imm)) {
    inputs[input_count++] = g.UseImmediate(index);
    addressing_mode = kMode_MRI;
  } else {
    inputs[input_count++] = g.UseUniqueRegister(index);
    addressing_mode = kMode_MRR;
  }

  inputs[input_count++] = g.UseUniqueRegister(value);

  InstructionOperand outputs[1];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  InstructionOperand temps[1];
  size_t temp_count = 0;
  temps[temp_count++] = g.TempRegister();

  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  selector->Emit(code, output_count, outputs, input_count, inputs, temp_count,
                 temps);
}

void InstructionSelector::VisitWord32AtomicBinaryOperation(
    Node* node, ArchOpcode int8_op, ArchOpcode uint8_op, ArchOpcode int16_op,
    ArchOpcode uint16_op, ArchOpcode word32_op) {
  MachineType type = AtomicOpType(node->op());
  ArchOpcode opcode = kArchNop;

  if (type == MachineType::Int8()) {
    opcode = int8_op;
  } else if (type == MachineType::Uint8()) {
    opcode = uint8_op;
  } else if (type == MachineType::Int16()) {
    opcode = int16_op;
  } else if (type == MachineType::Uint16()) {
    opcode = uint16_op;
  } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
    opcode = word32_op;
  } else {
    UNREACHABLE();
    return;
  }
  VisitAtomicBinop(this, node, opcode);
}

#define VISIT_ATOMIC_BINOP(op)                                   \
  void InstructionSelector::VisitWord32Atomic##op(Node* node) {  \
    VisitWord32AtomicBinaryOperation(                            \
        node, kWord32Atomic##op##Int8, kWord32Atomic##op##Uint8, \
        kWord32Atomic##op##Int16, kWord32Atomic##op##Uint16,     \
        kWord32Atomic##op##Word32);                              \
  }
VISIT_ATOMIC_BINOP(Add)
VISIT_ATOMIC_BINOP(Sub)
VISIT_ATOMIC_BINOP(And)
VISIT_ATOMIC_BINOP(Or)
VISIT_ATOMIC_BINOP(Xor)
#undef VISIT_ATOMIC_BINOP

void InstructionSelector::VisitWord64AtomicBinaryOperation(
    Node* node, ArchOpcode uint8_op, ArchOpcode uint16_op, ArchOpcode word32_op,
    ArchOpcode word64_op) {
  MachineType type = AtomicOpType(node->op());
  ArchOpcode opcode = kArchNop;

  if (type == MachineType::Uint8()) {
    opcode = uint8_op;
  } else if (type == MachineType::Uint16()) {
    opcode = uint16_op;
  } else if (type == MachineType::Uint32()) {
    opcode = word32_op;
  } else if (type == MachineType::Uint64()) {
    opcode = word64_op;
  } else {
    UNREACHABLE();
    return;
  }
  VisitAtomicBinop(this, node, opcode);
}

#define VISIT_ATOMIC64_BINOP(op)                                             \
  void InstructionSelector::VisitWord64Atomic##op(Node* node) {              \
    VisitWord64AtomicBinaryOperation(                                        \
        node, kS390_Word64Atomic##op##Uint8, kS390_Word64Atomic##op##Uint16, \
        kS390_Word64Atomic##op##Uint32, kS390_Word64Atomic##op##Uint64);     \
  }
VISIT_ATOMIC64_BINOP(Add)
VISIT_ATOMIC64_BINOP(Sub)
VISIT_ATOMIC64_BINOP(And)
VISIT_ATOMIC64_BINOP(Or)
VISIT_ATOMIC64_BINOP(Xor)
#undef VISIT_ATOMIC64_BINOP

void InstructionSelector::VisitWord64AtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  USE(load_rep);
  VisitLoad(node);
}

void InstructionSelector::VisitWord64AtomicStore(Node* node) {
  MachineRepresentation rep = AtomicStoreRepresentationOf(node->op());
  VisitGeneralStore(this, node, rep);
}

void InstructionSelector::VisitI32x4Splat(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4ExtractLane(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4ReplaceLane(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Add(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Sub(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Shl(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4ShrS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Mul(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4MaxS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4MinS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Eq(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Ne(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4MinU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4MaxU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4ShrU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4Neg(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4GtS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4GeS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4GtU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI32x4GeU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8Splat(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8ExtractLane(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8ReplaceLane(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8Shl(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8ShrS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8ShrU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8Add(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8AddSaturateS(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8Sub(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8SubSaturateS(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8Mul(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8MinS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8MaxS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8Eq(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8Ne(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8AddSaturateU(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8SubSaturateU(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8MinU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8MaxU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8Neg(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8GtS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8GeS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8GtU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI16x8GeU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16Neg(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16Splat(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16ExtractLane(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16ReplaceLane(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16Add(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16AddSaturateS(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16Sub(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16SubSaturateS(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16MinS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16MaxS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16Eq(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16Ne(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16GtS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16GeS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16AddSaturateU(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16SubSaturateU(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16MinU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16MaxU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16GtU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16GeU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS128And(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS128Or(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS128Xor(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS128Not(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS128Zero(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Eq(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Ne(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Lt(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Le(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Splat(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4ExtractLane(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4ReplaceLane(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::EmitPrepareResults(
    ZoneVector<PushParameter>* results, const CallDescriptor* call_descriptor,
    Node* node) {
  S390OperandGenerator g(this);

  int reverse_slot = 0;
  for (PushParameter output : *results) {
    if (!output.location.IsCallerFrameSlot()) continue;
    // Skip any alignment holes in nodes.
    if (output.node != nullptr) {
      DCHECK(!call_descriptor->IsCFunctionCall());
      if (output.location.GetType() == MachineType::Float32()) {
        MarkAsFloat32(output.node);
      } else if (output.location.GetType() == MachineType::Float64()) {
        MarkAsFloat64(output.node);
      }
      Emit(kS390_Peek, g.DefineAsRegister(output.node),
           g.UseImmediate(reverse_slot));
    }
    reverse_slot += output.location.GetSizeInPointers();
  }
}

void InstructionSelector::VisitF32x4Add(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Sub(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Mul(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Min(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Max(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS128Select(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Neg(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4Abs(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4RecipSqrtApprox(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitF32x4RecipApprox(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4AddHoriz(Node* node) { UNIMPLEMENTED(); }
void InstructionSelector::VisitI32x4AddHoriz(Node* node) { UNIMPLEMENTED(); }
void InstructionSelector::VisitI16x8AddHoriz(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitF32x4SConvertI32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitF32x4UConvertI32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI32x4SConvertF32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI32x4UConvertF32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI32x4SConvertI16x8Low(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI32x4SConvertI16x8High(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI32x4UConvertI16x8Low(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI32x4UConvertI16x8High(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8SConvertI8x16Low(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8SConvertI8x16High(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8UConvertI8x16Low(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8UConvertI8x16High(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI16x8SConvertI32x4(Node* node) {
  UNIMPLEMENTED();
}
void InstructionSelector::VisitI16x8UConvertI32x4(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16SConvertI16x8(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitI8x16UConvertI16x8(Node* node) {
  UNIMPLEMENTED();
}

void InstructionSelector::VisitS1x4AnyTrue(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS1x4AllTrue(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS1x8AnyTrue(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS1x8AllTrue(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS1x16AnyTrue(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS1x16AllTrue(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16Shl(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16ShrS(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16ShrU(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitI8x16Mul(Node* node) { UNIMPLEMENTED(); }

void InstructionSelector::VisitS8x16Shuffle(Node* node) { UNIMPLEMENTED(); }

// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  return MachineOperatorBuilder::kFloat32RoundDown |
         MachineOperatorBuilder::kFloat64RoundDown |
         MachineOperatorBuilder::kFloat32RoundUp |
         MachineOperatorBuilder::kFloat64RoundUp |
         MachineOperatorBuilder::kFloat32RoundTruncate |
         MachineOperatorBuilder::kFloat64RoundTruncate |
         MachineOperatorBuilder::kFloat64RoundTiesAway |
         MachineOperatorBuilder::kWord32Popcnt |
         MachineOperatorBuilder::kInt32AbsWithOverflow |
         MachineOperatorBuilder::kInt64AbsWithOverflow |
         MachineOperatorBuilder::kWord64Popcnt;
}

// static
MachineOperatorBuilder::AlignmentRequirements
InstructionSelector::AlignmentRequirements() {
  return MachineOperatorBuilder::AlignmentRequirements::
      FullUnalignedAccessSupport();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
