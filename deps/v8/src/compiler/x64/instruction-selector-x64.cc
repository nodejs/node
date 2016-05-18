// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "src/base/adapters.h"
#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

// Adds X64-specific methods for generating operands.
class X64OperandGenerator final : public OperandGenerator {
 public:
  explicit X64OperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  bool CanBeImmediate(Node* node) {
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
        return true;
      case IrOpcode::kInt64Constant: {
        const int64_t value = OpParameter<int64_t>(node);
        return value == static_cast<int64_t>(static_cast<int32_t>(value));
      }
      case IrOpcode::kNumberConstant: {
        const double value = OpParameter<double>(node);
        return bit_cast<int64_t>(value) == 0;
      }
      default:
        return false;
    }
  }

  AddressingMode GenerateMemoryOperandInputs(Node* index, int scale_exponent,
                                             Node* base, Node* displacement,
                                             InstructionOperand inputs[],
                                             size_t* input_count) {
    AddressingMode mode = kMode_MRI;
    if (base != nullptr) {
      inputs[(*input_count)++] = UseRegister(base);
      if (index != nullptr) {
        DCHECK(scale_exponent >= 0 && scale_exponent <= 3);
        inputs[(*input_count)++] = UseRegister(index);
        if (displacement != nullptr) {
          inputs[(*input_count)++] = UseImmediate(displacement);
          static const AddressingMode kMRnI_modes[] = {kMode_MR1I, kMode_MR2I,
                                                       kMode_MR4I, kMode_MR8I};
          mode = kMRnI_modes[scale_exponent];
        } else {
          static const AddressingMode kMRn_modes[] = {kMode_MR1, kMode_MR2,
                                                      kMode_MR4, kMode_MR8};
          mode = kMRn_modes[scale_exponent];
        }
      } else {
        if (displacement == nullptr) {
          mode = kMode_MR;
        } else {
          inputs[(*input_count)++] = UseImmediate(displacement);
          mode = kMode_MRI;
        }
      }
    } else {
      DCHECK_NOT_NULL(index);
      DCHECK(scale_exponent >= 0 && scale_exponent <= 3);
      inputs[(*input_count)++] = UseRegister(index);
      if (displacement != nullptr) {
        inputs[(*input_count)++] = UseImmediate(displacement);
        static const AddressingMode kMnI_modes[] = {kMode_MRI, kMode_M2I,
                                                    kMode_M4I, kMode_M8I};
        mode = kMnI_modes[scale_exponent];
      } else {
        static const AddressingMode kMn_modes[] = {kMode_MR, kMode_MR1,
                                                   kMode_M4, kMode_M8};
        mode = kMn_modes[scale_exponent];
        if (mode == kMode_MR1) {
          // [%r1 + %r1*1] has a smaller encoding than [%r1*2+0]
          inputs[(*input_count)++] = UseRegister(index);
        }
      }
    }
    return mode;
  }

  AddressingMode GetEffectiveAddressMemoryOperand(Node* operand,
                                                  InstructionOperand inputs[],
                                                  size_t* input_count) {
    BaseWithIndexAndDisplacement64Matcher m(operand, true);
    DCHECK(m.matches());
    if ((m.displacement() == nullptr || CanBeImmediate(m.displacement()))) {
      return GenerateMemoryOperandInputs(m.index(), m.scale(), m.base(),
                                         m.displacement(), inputs, input_count);
    } else {
      inputs[(*input_count)++] = UseRegister(operand->InputAt(0));
      inputs[(*input_count)++] = UseRegister(operand->InputAt(1));
      return kMode_MR1;
    }
  }

  bool CanBeBetterLeftOperand(Node* node) const {
    return !selector()->IsLive(node);
  }
};


void InstructionSelector::VisitLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  X64OperandGenerator g(this);

  ArchOpcode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kX64Movss;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kX64Movsd;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsSigned() ? kX64Movsxbl : kX64Movzxbl;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsSigned() ? kX64Movsxwl : kX64Movzxwl;
      break;
    case MachineRepresentation::kWord32:
      opcode = kX64Movl;
      break;
    case MachineRepresentation::kTagged:  // Fall through.
    case MachineRepresentation::kWord64:
      opcode = kX64Movq;
      break;
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }

  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(node);
  InstructionOperand inputs[3];
  size_t input_count = 0;
  AddressingMode mode =
      g.GetEffectiveAddressMemoryOperand(node, inputs, &input_count);
  InstructionCode code = opcode | AddressingModeField::encode(mode);
  Emit(code, 1, outputs, input_count, inputs);
}


void InstructionSelector::VisitStore(Node* node) {
  X64OperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());
  WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();
  MachineRepresentation rep = store_rep.representation();

  if (write_barrier_kind != kNoWriteBarrier) {
    DCHECK_EQ(MachineRepresentation::kTagged, rep);
    AddressingMode addressing_mode;
    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    if (g.CanBeImmediate(index)) {
      inputs[input_count++] = g.UseImmediate(index);
      addressing_mode = kMode_MRI;
    } else {
      inputs[input_count++] = g.UseUniqueRegister(index);
      addressing_mode = kMode_MR1;
    }
    inputs[input_count++] = (write_barrier_kind == kMapWriteBarrier)
                                ? g.UseRegister(value)
                                : g.UseUniqueRegister(value);
    RecordWriteMode record_write_mode = RecordWriteMode::kValueIsAny;
    switch (write_barrier_kind) {
      case kNoWriteBarrier:
        UNREACHABLE();
        break;
      case kMapWriteBarrier:
        record_write_mode = RecordWriteMode::kValueIsMap;
        break;
      case kPointerWriteBarrier:
        record_write_mode = RecordWriteMode::kValueIsPointer;
        break;
      case kFullWriteBarrier:
        record_write_mode = RecordWriteMode::kValueIsAny;
        break;
    }
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    size_t const temp_count = arraysize(temps);
    InstructionCode code = kArchStoreWithWriteBarrier;
    code |= AddressingModeField::encode(addressing_mode);
    code |= MiscField::encode(static_cast<int>(record_write_mode));
    Emit(code, 0, nullptr, input_count, inputs, temp_count, temps);
  } else {
    ArchOpcode opcode = kArchNop;
    switch (rep) {
      case MachineRepresentation::kFloat32:
        opcode = kX64Movss;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kX64Movsd;
        break;
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = kX64Movb;
        break;
      case MachineRepresentation::kWord16:
        opcode = kX64Movw;
        break;
      case MachineRepresentation::kWord32:
        opcode = kX64Movl;
        break;
      case MachineRepresentation::kTagged:  // Fall through.
      case MachineRepresentation::kWord64:
        opcode = kX64Movq;
        break;
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
    InstructionOperand value_operand =
        g.CanBeImmediate(value) ? g.UseImmediate(value) : g.UseRegister(value);
    inputs[input_count++] = value_operand;
    Emit(code, 0, static_cast<InstructionOperand*>(nullptr), input_count,
         inputs);
  }
}


void InstructionSelector::VisitCheckedLoad(Node* node) {
  CheckedLoadRepresentation load_rep = CheckedLoadRepresentationOf(node->op());
  X64OperandGenerator g(this);
  Node* const buffer = node->InputAt(0);
  Node* const offset = node->InputAt(1);
  Node* const length = node->InputAt(2);
  ArchOpcode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsSigned() ? kCheckedLoadInt8 : kCheckedLoadUint8;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsSigned() ? kCheckedLoadInt16 : kCheckedLoadUint16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kCheckedLoadWord32;
      break;
    case MachineRepresentation::kWord64:
      opcode = kCheckedLoadWord64;
      break;
    case MachineRepresentation::kFloat32:
      opcode = kCheckedLoadFloat32;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kCheckedLoadFloat64;
      break;
    case MachineRepresentation::kBit:      // Fall through.
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kTagged:   // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }
  if (offset->opcode() == IrOpcode::kInt32Add && CanCover(node, offset)) {
    Int32Matcher mlength(length);
    Int32BinopMatcher moffset(offset);
    if (mlength.HasValue() && moffset.right().HasValue() &&
        moffset.right().Value() >= 0 &&
        mlength.Value() >= moffset.right().Value()) {
      Emit(opcode, g.DefineAsRegister(node), g.UseRegister(buffer),
           g.UseRegister(moffset.left().node()),
           g.UseImmediate(moffset.right().node()), g.UseImmediate(length));
      return;
    }
  }
  InstructionOperand length_operand =
      g.CanBeImmediate(length) ? g.UseImmediate(length) : g.UseRegister(length);
  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(buffer),
       g.UseRegister(offset), g.TempImmediate(0), length_operand);
}


void InstructionSelector::VisitCheckedStore(Node* node) {
  MachineRepresentation rep = CheckedStoreRepresentationOf(node->op());
  X64OperandGenerator g(this);
  Node* const buffer = node->InputAt(0);
  Node* const offset = node->InputAt(1);
  Node* const length = node->InputAt(2);
  Node* const value = node->InputAt(3);
  ArchOpcode opcode = kArchNop;
  switch (rep) {
    case MachineRepresentation::kWord8:
      opcode = kCheckedStoreWord8;
      break;
    case MachineRepresentation::kWord16:
      opcode = kCheckedStoreWord16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kCheckedStoreWord32;
      break;
    case MachineRepresentation::kWord64:
      opcode = kCheckedStoreWord64;
      break;
    case MachineRepresentation::kFloat32:
      opcode = kCheckedStoreFloat32;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kCheckedStoreFloat64;
      break;
    case MachineRepresentation::kBit:      // Fall through.
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kTagged:   // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }
  InstructionOperand value_operand =
      g.CanBeImmediate(value) ? g.UseImmediate(value) : g.UseRegister(value);
  if (offset->opcode() == IrOpcode::kInt32Add && CanCover(node, offset)) {
    Int32Matcher mlength(length);
    Int32BinopMatcher moffset(offset);
    if (mlength.HasValue() && moffset.right().HasValue() &&
        moffset.right().Value() >= 0 &&
        mlength.Value() >= moffset.right().Value()) {
      Emit(opcode, g.NoOutput(), g.UseRegister(buffer),
           g.UseRegister(moffset.left().node()),
           g.UseImmediate(moffset.right().node()), g.UseImmediate(length),
           value_operand);
      return;
    }
  }
  InstructionOperand length_operand =
      g.CanBeImmediate(length) ? g.UseImmediate(length) : g.UseRegister(length);
  Emit(opcode, g.NoOutput(), g.UseRegister(buffer), g.UseRegister(offset),
       g.TempImmediate(0), length_operand, value_operand);
}


// Shared routine for multiple binary operations.
static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode, FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();
  InstructionOperand inputs[4];
  size_t input_count = 0;
  InstructionOperand outputs[2];
  size_t output_count = 0;

  // TODO(turbofan): match complex addressing modes.
  if (left == right) {
    // If both inputs refer to the same operand, enforce allocating a register
    // for both of them to ensure that we don't end up generating code like
    // this:
    //
    //   mov rax, [rbp-0x10]
    //   add rax, [rbp-0x10]
    //   jo label
    InstructionOperand const input = g.UseRegister(left);
    inputs[input_count++] = input;
    inputs[input_count++] = input;
  } else if (g.CanBeImmediate(right)) {
    inputs[input_count++] = g.UseRegister(left);
    inputs[input_count++] = g.UseImmediate(right);
  } else {
    if (node->op()->HasProperty(Operator::kCommutative) &&
        g.CanBeBetterLeftOperand(right)) {
      std::swap(left, right);
    }
    inputs[input_count++] = g.UseRegister(left);
    inputs[input_count++] = g.Use(right);
  }

  if (cont->IsBranch()) {
    inputs[input_count++] = g.Label(cont->true_block());
    inputs[input_count++] = g.Label(cont->false_block());
  }

  outputs[output_count++] = g.DefineSameAsFirst(node);
  if (cont->IsSet()) {
    outputs[output_count++] = g.DefineAsRegister(cont->result());
  }

  DCHECK_NE(0u, input_count);
  DCHECK_NE(0u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->Emit(cont->Encode(opcode), output_count, outputs, input_count,
                 inputs);
}


// Shared routine for multiple binary operations.
static void VisitBinop(InstructionSelector* selector, Node* node,
                       InstructionCode opcode) {
  FlagsContinuation cont;
  VisitBinop(selector, node, opcode, &cont);
}


void InstructionSelector::VisitWord32And(Node* node) {
  X64OperandGenerator g(this);
  Uint32BinopMatcher m(node);
  if (m.right().Is(0xff)) {
    Emit(kX64Movzxbl, g.DefineAsRegister(node), g.Use(m.left().node()));
  } else if (m.right().Is(0xffff)) {
    Emit(kX64Movzxwl, g.DefineAsRegister(node), g.Use(m.left().node()));
  } else {
    VisitBinop(this, node, kX64And32);
  }
}


void InstructionSelector::VisitWord64And(Node* node) {
  VisitBinop(this, node, kX64And);
}


void InstructionSelector::VisitWord32Or(Node* node) {
  VisitBinop(this, node, kX64Or32);
}


void InstructionSelector::VisitWord64Or(Node* node) {
  VisitBinop(this, node, kX64Or);
}


void InstructionSelector::VisitWord32Xor(Node* node) {
  X64OperandGenerator g(this);
  Uint32BinopMatcher m(node);
  if (m.right().Is(-1)) {
    Emit(kX64Not32, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()));
  } else {
    VisitBinop(this, node, kX64Xor32);
  }
}


void InstructionSelector::VisitWord64Xor(Node* node) {
  X64OperandGenerator g(this);
  Uint64BinopMatcher m(node);
  if (m.right().Is(-1)) {
    Emit(kX64Not, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()));
  } else {
    VisitBinop(this, node, kX64Xor);
  }
}


namespace {

// Shared routine for multiple 32-bit shift operations.
// TODO(bmeurer): Merge this with VisitWord64Shift using template magic?
void VisitWord32Shift(InstructionSelector* selector, Node* node,
                      ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();

  if (g.CanBeImmediate(right)) {
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.UseImmediate(right));
  } else {
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.UseFixed(right, rcx));
  }
}


// Shared routine for multiple 64-bit shift operations.
// TODO(bmeurer): Merge this with VisitWord32Shift using template magic?
void VisitWord64Shift(InstructionSelector* selector, Node* node,
                      ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Int64BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();

  if (g.CanBeImmediate(right)) {
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.UseImmediate(right));
  } else {
    if (m.right().IsWord64And()) {
      Int64BinopMatcher mright(right);
      if (mright.right().Is(0x3F)) {
        right = mright.left().node();
      }
    }
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.UseFixed(right, rcx));
  }
}


void EmitLea(InstructionSelector* selector, InstructionCode opcode,
             Node* result, Node* index, int scale, Node* base,
             Node* displacement) {
  X64OperandGenerator g(selector);

  InstructionOperand inputs[4];
  size_t input_count = 0;
  AddressingMode mode = g.GenerateMemoryOperandInputs(
      index, scale, base, displacement, inputs, &input_count);

  DCHECK_NE(0u, input_count);
  DCHECK_GE(arraysize(inputs), input_count);

  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(result);

  opcode = AddressingModeField::encode(mode) | opcode;

  selector->Emit(opcode, 1, outputs, input_count, inputs);
}

}  // namespace


void InstructionSelector::VisitWord32Shl(Node* node) {
  Int32ScaleMatcher m(node, true);
  if (m.matches()) {
    Node* index = node->InputAt(0);
    Node* base = m.power_of_two_plus_one() ? index : nullptr;
    EmitLea(this, kX64Lea32, node, index, m.scale(), base, nullptr);
    return;
  }
  VisitWord32Shift(this, node, kX64Shl32);
}


void InstructionSelector::VisitWord64Shl(Node* node) {
  X64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  if ((m.left().IsChangeInt32ToInt64() || m.left().IsChangeUint32ToUint64()) &&
      m.right().IsInRange(32, 63)) {
    // There's no need to sign/zero-extend to 64-bit if we shift out the upper
    // 32 bits anyway.
    Emit(kX64Shl, g.DefineSameAsFirst(node),
         g.UseRegister(m.left().node()->InputAt(0)),
         g.UseImmediate(m.right().node()));
    return;
  }
  VisitWord64Shift(this, node, kX64Shl);
}


void InstructionSelector::VisitWord32Shr(Node* node) {
  VisitWord32Shift(this, node, kX64Shr32);
}


void InstructionSelector::VisitWord64Shr(Node* node) {
  VisitWord64Shift(this, node, kX64Shr);
}


void InstructionSelector::VisitWord32Sar(Node* node) {
  X64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (CanCover(m.node(), m.left().node()) && m.left().IsWord32Shl()) {
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().Is(16) && m.right().Is(16)) {
      Emit(kX64Movsxwl, g.DefineAsRegister(node), g.Use(mleft.left().node()));
      return;
    } else if (mleft.right().Is(24) && m.right().Is(24)) {
      Emit(kX64Movsxbl, g.DefineAsRegister(node), g.Use(mleft.left().node()));
      return;
    }
  }
  VisitWord32Shift(this, node, kX64Sar32);
}


void InstructionSelector::VisitWord64Sar(Node* node) {
  VisitWord64Shift(this, node, kX64Sar);
}


void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitWord32Shift(this, node, kX64Ror32);
}


void InstructionSelector::VisitWord64Ror(Node* node) {
  VisitWord64Shift(this, node, kX64Ror);
}


void InstructionSelector::VisitWord64Clz(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Lzcnt, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitWord32Clz(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Lzcnt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitWord64Ctz(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Tzcnt, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitWord32Ctz(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Tzcnt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitWord32ReverseBits(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitWord64ReverseBits(Node* node) { UNREACHABLE(); }


void InstructionSelector::VisitWord32Popcnt(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Popcnt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitWord64Popcnt(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Popcnt, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitInt32Add(Node* node) {
  X64OperandGenerator g(this);

  // Try to match the Add to a leal pattern
  BaseWithIndexAndDisplacement32Matcher m(node);
  if (m.matches() &&
      (m.displacement() == nullptr || g.CanBeImmediate(m.displacement()))) {
    EmitLea(this, kX64Lea32, node, m.index(), m.scale(), m.base(),
            m.displacement());
    return;
  }

  // No leal pattern match, use addl
  VisitBinop(this, node, kX64Add32);
}


void InstructionSelector::VisitInt64Add(Node* node) {
  VisitBinop(this, node, kX64Add);
}


void InstructionSelector::VisitInt64AddWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont(kOverflow, ovf);
    VisitBinop(this, node, kX64Add, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kX64Add, &cont);
}


void InstructionSelector::VisitInt32Sub(Node* node) {
  X64OperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().Is(0)) {
    Emit(kX64Neg32, g.DefineSameAsFirst(node), g.UseRegister(m.right().node()));
  } else {
    if (m.right().HasValue() && g.CanBeImmediate(m.right().node())) {
      // Turn subtractions of constant values into immediate "leal" instructions
      // by negating the value.
      Emit(kX64Lea32 | AddressingModeField::encode(kMode_MRI),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(-m.right().Value()));
      return;
    }
    VisitBinop(this, node, kX64Sub32);
  }
}


void InstructionSelector::VisitInt64Sub(Node* node) {
  X64OperandGenerator g(this);
  Int64BinopMatcher m(node);
  if (m.left().Is(0)) {
    Emit(kX64Neg, g.DefineSameAsFirst(node), g.UseRegister(m.right().node()));
  } else {
    VisitBinop(this, node, kX64Sub);
  }
}


void InstructionSelector::VisitInt64SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitBinop(this, node, kX64Sub, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kX64Sub, &cont);
}


namespace {

void VisitMul(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Int32BinopMatcher m(node);
  Node* left = m.left().node();
  Node* right = m.right().node();
  if (g.CanBeImmediate(right)) {
    selector->Emit(opcode, g.DefineAsRegister(node), g.Use(left),
                   g.UseImmediate(right));
  } else {
    if (g.CanBeBetterLeftOperand(right)) {
      std::swap(left, right);
    }
    selector->Emit(opcode, g.DefineSameAsFirst(node), g.UseRegister(left),
                   g.Use(right));
  }
}


void VisitMulHigh(InstructionSelector* selector, Node* node,
                  ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (selector->IsLive(left) && !selector->IsLive(right)) {
    std::swap(left, right);
  }
  InstructionOperand temps[] = {g.TempRegister(rax)};
  // TODO(turbofan): We use UseUniqueRegister here to improve register
  // allocation.
  selector->Emit(opcode, g.DefineAsFixed(node, rdx), g.UseFixed(left, rax),
                 g.UseUniqueRegister(right), arraysize(temps), temps);
}


void VisitDiv(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  InstructionOperand temps[] = {g.TempRegister(rdx)};
  selector->Emit(
      opcode, g.DefineAsFixed(node, rax), g.UseFixed(node->InputAt(0), rax),
      g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps);
}


void VisitMod(InstructionSelector* selector, Node* node, ArchOpcode opcode) {
  X64OperandGenerator g(selector);
  InstructionOperand temps[] = {g.TempRegister(rax)};
  selector->Emit(
      opcode, g.DefineAsFixed(node, rdx), g.UseFixed(node->InputAt(0), rax),
      g.UseUniqueRegister(node->InputAt(1)), arraysize(temps), temps);
}

}  // namespace


void InstructionSelector::VisitInt32Mul(Node* node) {
  Int32ScaleMatcher m(node, true);
  if (m.matches()) {
    Node* index = node->InputAt(0);
    Node* base = m.power_of_two_plus_one() ? index : nullptr;
    EmitLea(this, kX64Lea32, node, index, m.scale(), base, nullptr);
    return;
  }
  VisitMul(this, node, kX64Imul32);
}


void InstructionSelector::VisitInt64Mul(Node* node) {
  VisitMul(this, node, kX64Imul);
}


void InstructionSelector::VisitInt32MulHigh(Node* node) {
  VisitMulHigh(this, node, kX64ImulHigh32);
}


void InstructionSelector::VisitInt32Div(Node* node) {
  VisitDiv(this, node, kX64Idiv32);
}


void InstructionSelector::VisitInt64Div(Node* node) {
  VisitDiv(this, node, kX64Idiv);
}


void InstructionSelector::VisitUint32Div(Node* node) {
  VisitDiv(this, node, kX64Udiv32);
}


void InstructionSelector::VisitUint64Div(Node* node) {
  VisitDiv(this, node, kX64Udiv);
}


void InstructionSelector::VisitInt32Mod(Node* node) {
  VisitMod(this, node, kX64Idiv32);
}


void InstructionSelector::VisitInt64Mod(Node* node) {
  VisitMod(this, node, kX64Idiv);
}


void InstructionSelector::VisitUint32Mod(Node* node) {
  VisitMod(this, node, kX64Udiv32);
}


void InstructionSelector::VisitUint64Mod(Node* node) {
  VisitMod(this, node, kX64Udiv);
}


void InstructionSelector::VisitUint32MulHigh(Node* node) {
  VisitMulHigh(this, node, kX64UmulHigh32);
}


void InstructionSelector::VisitChangeFloat32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat32ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeInt32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEInt32ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEUint32ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToInt32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64ToInt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeFloat64ToUint32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64ToUint32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitTruncateFloat32ToInt32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat32ToInt32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitTruncateFloat32ToUint32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat32ToUint32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitTryTruncateFloat32ToInt64(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kSSEFloat32ToInt64, output_count, outputs, 1, inputs);
}


void InstructionSelector::VisitTryTruncateFloat64ToInt64(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kSSEFloat64ToInt64, output_count, outputs, 1, inputs);
}


void InstructionSelector::VisitTryTruncateFloat32ToUint64(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kSSEFloat32ToUint64, output_count, outputs, 1, inputs);
}


void InstructionSelector::VisitTryTruncateFloat64ToUint64(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0))};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  outputs[output_count++] = g.DefineAsRegister(node);

  Node* success_output = NodeProperties::FindProjection(node, 1);
  if (success_output) {
    outputs[output_count++] = g.DefineAsRegister(success_output);
  }

  Emit(kSSEFloat64ToUint64, output_count, outputs, 1, inputs);
}


void InstructionSelector::VisitChangeInt32ToInt64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64Movsxlq, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitChangeUint32ToUint64(Node* node) {
  X64OperandGenerator g(this);
  Node* value = node->InputAt(0);
  switch (value->opcode()) {
    case IrOpcode::kWord32And:
    case IrOpcode::kWord32Or:
    case IrOpcode::kWord32Xor:
    case IrOpcode::kWord32Shl:
    case IrOpcode::kWord32Shr:
    case IrOpcode::kWord32Sar:
    case IrOpcode::kWord32Ror:
    case IrOpcode::kWord32Equal:
    case IrOpcode::kInt32Add:
    case IrOpcode::kInt32Sub:
    case IrOpcode::kInt32Mul:
    case IrOpcode::kInt32MulHigh:
    case IrOpcode::kInt32Div:
    case IrOpcode::kInt32LessThan:
    case IrOpcode::kInt32LessThanOrEqual:
    case IrOpcode::kInt32Mod:
    case IrOpcode::kUint32Div:
    case IrOpcode::kUint32LessThan:
    case IrOpcode::kUint32LessThanOrEqual:
    case IrOpcode::kUint32Mod:
    case IrOpcode::kUint32MulHigh: {
      // These 32-bit operations implicitly zero-extend to 64-bit on x64, so the
      // zero-extension is a no-op.
      Emit(kArchNop, g.DefineSameAsFirst(node), g.Use(value));
      return;
    }
    default:
      break;
  }
  Emit(kX64Movl, g.DefineAsRegister(node), g.Use(value));
}


namespace {

void VisitRO(InstructionSelector* selector, Node* node,
             InstructionCode opcode) {
  X64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void VisitRR(InstructionSelector* selector, Node* node,
             InstructionCode opcode) {
  X64OperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}


void VisitFloatBinop(InstructionSelector* selector, Node* node,
                     ArchOpcode avx_opcode, ArchOpcode sse_opcode) {
  X64OperandGenerator g(selector);
  InstructionOperand operand0 = g.UseRegister(node->InputAt(0));
  InstructionOperand operand1 = g.Use(node->InputAt(1));
  if (selector->IsSupported(AVX)) {
    selector->Emit(avx_opcode, g.DefineAsRegister(node), operand0, operand1);
  } else {
    selector->Emit(sse_opcode, g.DefineSameAsFirst(node), operand0, operand1);
  }
}


void VisitFloatUnop(InstructionSelector* selector, Node* node, Node* input,
                    ArchOpcode avx_opcode, ArchOpcode sse_opcode) {
  X64OperandGenerator g(selector);
  if (selector->IsSupported(AVX)) {
    selector->Emit(avx_opcode, g.DefineAsRegister(node), g.Use(input));
  } else {
    selector->Emit(sse_opcode, g.DefineSameAsFirst(node), g.UseRegister(input));
  }
}

}  // namespace


void InstructionSelector::VisitTruncateFloat64ToFloat32(Node* node) {
  VisitRO(this, node, kSSEFloat64ToFloat32);
}


void InstructionSelector::VisitTruncateFloat64ToInt32(Node* node) {
  switch (TruncationModeOf(node->op())) {
    case TruncationMode::kJavaScript:
      return VisitRR(this, node, kArchTruncateDoubleToI);
    case TruncationMode::kRoundToZero:
      return VisitRO(this, node, kSSEFloat64ToInt32);
  }
  UNREACHABLE();
}


void InstructionSelector::VisitTruncateInt64ToInt32(Node* node) {
  X64OperandGenerator g(this);
  Node* value = node->InputAt(0);
  if (CanCover(node, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord64Sar:
      case IrOpcode::kWord64Shr: {
        Int64BinopMatcher m(value);
        if (m.right().Is(32)) {
          Emit(kX64Shr, g.DefineSameAsFirst(node),
               g.UseRegister(m.left().node()), g.TempImmediate(32));
          return;
        }
        break;
      }
      default:
        break;
    }
  }
  Emit(kX64Movl, g.DefineAsRegister(node), g.Use(value));
}


void InstructionSelector::VisitRoundInt32ToFloat32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEInt32ToFloat32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitRoundInt64ToFloat32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEInt64ToFloat32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitRoundInt64ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEInt64ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitRoundUint32ToFloat32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEUint32ToFloat32, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitRoundUint64ToFloat32(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister()};
  Emit(kSSEUint64ToFloat32, g.DefineAsRegister(node), g.Use(node->InputAt(0)),
       arraysize(temps), temps);
}


void InstructionSelector::VisitRoundUint64ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister()};
  Emit(kSSEUint64ToFloat64, g.DefineAsRegister(node), g.Use(node->InputAt(0)),
       arraysize(temps), temps);
}


void InstructionSelector::VisitBitcastFloat32ToInt32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64BitcastFI, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitBitcastFloat64ToInt64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64BitcastDL, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitBitcastInt32ToFloat32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64BitcastIF, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitBitcastInt64ToFloat64(Node* node) {
  X64OperandGenerator g(this);
  Emit(kX64BitcastLD, g.DefineAsRegister(node), g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitFloat32Add(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat32Add, kSSEFloat32Add);
}


void InstructionSelector::VisitFloat32Sub(Node* node) {
  X64OperandGenerator g(this);
  Float32BinopMatcher m(node);
  if (m.left().IsMinusZero()) {
    VisitFloatUnop(this, node, m.right().node(), kAVXFloat32Neg,
                   kSSEFloat32Neg);
    return;
  }
  VisitFloatBinop(this, node, kAVXFloat32Sub, kSSEFloat32Sub);
}


void InstructionSelector::VisitFloat32Mul(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat32Mul, kSSEFloat32Mul);
}


void InstructionSelector::VisitFloat32Div(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat32Div, kSSEFloat32Div);
}


void InstructionSelector::VisitFloat32Max(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat32Max, kSSEFloat32Max);
}


void InstructionSelector::VisitFloat32Min(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat32Min, kSSEFloat32Min);
}


void InstructionSelector::VisitFloat32Abs(Node* node) {
  VisitFloatUnop(this, node, node->InputAt(0), kAVXFloat32Abs, kSSEFloat32Abs);
}


void InstructionSelector::VisitFloat32Sqrt(Node* node) {
  VisitRO(this, node, kSSEFloat32Sqrt);
}


void InstructionSelector::VisitFloat64Add(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat64Add, kSSEFloat64Add);
}


void InstructionSelector::VisitFloat64Sub(Node* node) {
  X64OperandGenerator g(this);
  Float64BinopMatcher m(node);
  if (m.left().IsMinusZero()) {
    if (m.right().IsFloat64RoundDown() &&
        CanCover(m.node(), m.right().node())) {
      if (m.right().InputAt(0)->opcode() == IrOpcode::kFloat64Sub &&
          CanCover(m.right().node(), m.right().InputAt(0))) {
        Float64BinopMatcher mright0(m.right().InputAt(0));
        if (mright0.left().IsMinusZero()) {
          Emit(kSSEFloat64Round | MiscField::encode(kRoundUp),
               g.DefineAsRegister(node), g.UseRegister(mright0.right().node()));
          return;
        }
      }
    }
    VisitFloatUnop(this, node, m.right().node(), kAVXFloat64Neg,
                   kSSEFloat64Neg);
    return;
  }
  VisitFloatBinop(this, node, kAVXFloat64Sub, kSSEFloat64Sub);
}


void InstructionSelector::VisitFloat64Mul(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat64Mul, kSSEFloat64Mul);
}


void InstructionSelector::VisitFloat64Div(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat64Div, kSSEFloat64Div);
}


void InstructionSelector::VisitFloat64Mod(Node* node) {
  X64OperandGenerator g(this);
  InstructionOperand temps[] = {g.TempRegister(rax)};
  Emit(kSSEFloat64Mod, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)), 1,
       temps);
}


void InstructionSelector::VisitFloat64Max(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat64Max, kSSEFloat64Max);
}


void InstructionSelector::VisitFloat64Min(Node* node) {
  VisitFloatBinop(this, node, kAVXFloat64Min, kSSEFloat64Min);
}


void InstructionSelector::VisitFloat64Abs(Node* node) {
  VisitFloatUnop(this, node, node->InputAt(0), kAVXFloat64Abs, kSSEFloat64Abs);
}


void InstructionSelector::VisitFloat64Sqrt(Node* node) {
  VisitRO(this, node, kSSEFloat64Sqrt);
}


void InstructionSelector::VisitFloat32RoundDown(Node* node) {
  VisitRR(this, node, kSSEFloat32Round | MiscField::encode(kRoundDown));
}


void InstructionSelector::VisitFloat64RoundDown(Node* node) {
  VisitRR(this, node, kSSEFloat64Round | MiscField::encode(kRoundDown));
}


void InstructionSelector::VisitFloat32RoundUp(Node* node) {
  VisitRR(this, node, kSSEFloat32Round | MiscField::encode(kRoundUp));
}


void InstructionSelector::VisitFloat64RoundUp(Node* node) {
  VisitRR(this, node, kSSEFloat64Round | MiscField::encode(kRoundUp));
}


void InstructionSelector::VisitFloat32RoundTruncate(Node* node) {
  VisitRR(this, node, kSSEFloat32Round | MiscField::encode(kRoundToZero));
}


void InstructionSelector::VisitFloat64RoundTruncate(Node* node) {
  VisitRR(this, node, kSSEFloat64Round | MiscField::encode(kRoundToZero));
}


void InstructionSelector::VisitFloat64RoundTiesAway(Node* node) {
  UNREACHABLE();
}


void InstructionSelector::VisitFloat32RoundTiesEven(Node* node) {
  VisitRR(this, node, kSSEFloat32Round | MiscField::encode(kRoundToNearest));
}


void InstructionSelector::VisitFloat64RoundTiesEven(Node* node) {
  VisitRR(this, node, kSSEFloat64Round | MiscField::encode(kRoundToNearest));
}


void InstructionSelector::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* descriptor,
    Node* node) {
  X64OperandGenerator g(this);

  // Prepare for C function call.
  if (descriptor->IsCFunctionCall()) {
    Emit(kArchPrepareCallCFunction |
             MiscField::encode(static_cast<int>(descriptor->CParameterCount())),
         0, nullptr, 0, nullptr);

    // Poke any stack arguments.
    for (size_t n = 0; n < arguments->size(); ++n) {
      PushParameter input = (*arguments)[n];
      if (input.node()) {
        int slot = static_cast<int>(n);
        InstructionOperand value = g.CanBeImmediate(input.node())
                                       ? g.UseImmediate(input.node())
                                       : g.UseRegister(input.node());
        Emit(kX64Poke | MiscField::encode(slot), g.NoOutput(), value);
      }
    }
  } else {
    // Push any stack arguments.
    for (PushParameter input : base::Reversed(*arguments)) {
      // TODO(titzer): X64Push cannot handle stack->stack double moves
      // because there is no way to encode fixed double slots.
      InstructionOperand value =
          g.CanBeImmediate(input.node())
              ? g.UseImmediate(input.node())
              : IsSupported(ATOM) ||
                        sequence()->IsFloat(GetVirtualRegister(input.node()))
                    ? g.UseRegister(input.node())
                    : g.Use(input.node());
      Emit(kX64Push, g.NoOutput(), value);
    }
  }
}


bool InstructionSelector::IsTailCallAddressImmediate() { return true; }


namespace {

void VisitCompareWithMemoryOperand(InstructionSelector* selector,
                                   InstructionCode opcode, Node* left,
                                   InstructionOperand right,
                                   FlagsContinuation* cont) {
  DCHECK(left->opcode() == IrOpcode::kLoad);
  X64OperandGenerator g(selector);
  size_t input_count = 0;
  InstructionOperand inputs[6];
  AddressingMode addressing_mode =
      g.GetEffectiveAddressMemoryOperand(left, inputs, &input_count);
  opcode |= AddressingModeField::encode(addressing_mode);
  opcode = cont->Encode(opcode);
  inputs[input_count++] = right;

  if (cont->IsBranch()) {
    inputs[input_count++] = g.Label(cont->true_block());
    inputs[input_count++] = g.Label(cont->false_block());
    selector->Emit(opcode, 0, nullptr, input_count, inputs);
  } else {
    DCHECK(cont->IsSet());
    InstructionOperand output = g.DefineAsRegister(cont->result());
    selector->Emit(opcode, 1, &output, input_count, inputs);
  }
}

// Determines if {input} of {node} can be replaced by a memory operand.
bool CanUseMemoryOperand(InstructionSelector* selector, InstructionCode opcode,
                         Node* node, Node* input) {
  if (input->opcode() != IrOpcode::kLoad || !selector->CanCover(node, input)) {
    return false;
  }
  MachineRepresentation rep =
      LoadRepresentationOf(input->op()).representation();
  if (rep == MachineRepresentation::kWord64 ||
      rep == MachineRepresentation::kTagged) {
    return opcode == kX64Cmp || opcode == kX64Test;
  } else if (rep == MachineRepresentation::kWord32) {
    return opcode == kX64Cmp32 || opcode == kX64Test32;
  }
  return false;
}

// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  InstructionOperand left, InstructionOperand right,
                  FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  opcode = cont->Encode(opcode);
  if (cont->IsBranch()) {
    selector->Emit(opcode, g.NoOutput(), left, right,
                   g.Label(cont->true_block()), g.Label(cont->false_block()));
  } else {
    DCHECK(cont->IsSet());
    selector->Emit(opcode, g.DefineAsRegister(cont->result()), left, right);
  }
}


// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  Node* left, Node* right, FlagsContinuation* cont,
                  bool commutative) {
  X64OperandGenerator g(selector);
  if (commutative && g.CanBeBetterLeftOperand(right)) {
    std::swap(left, right);
  }
  VisitCompare(selector, opcode, g.UseRegister(left), g.Use(right), cont);
}

// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);

  // If one of the two inputs is an immediate, make sure it's on the right.
  if (!g.CanBeImmediate(right) && g.CanBeImmediate(left)) {
    if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
    std::swap(left, right);
  }

  // Match immediates on right side of comparison.
  if (g.CanBeImmediate(right)) {
    if (CanUseMemoryOperand(selector, opcode, node, left)) {
      return VisitCompareWithMemoryOperand(selector, opcode, left,
                                           g.UseImmediate(right), cont);
    }
    return VisitCompare(selector, opcode, g.Use(left), g.UseImmediate(right),
                        cont);
  }

  if (g.CanBeBetterLeftOperand(right)) {
    if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
    std::swap(left, right);
  }

  if (CanUseMemoryOperand(selector, opcode, node, left)) {
    return VisitCompareWithMemoryOperand(selector, opcode, left,
                                         g.UseRegister(right), cont);
  }
  return VisitCompare(selector, opcode, left, right, cont,
                      node->op()->HasProperty(Operator::kCommutative));
}

// Shared routine for 64-bit word comparison operations.
void VisitWord64Compare(InstructionSelector* selector, Node* node,
                        FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  Int64BinopMatcher m(node);
  if (m.left().IsLoad() && m.right().IsLoadStackPointer()) {
    LoadMatcher<ExternalReferenceMatcher> mleft(m.left().node());
    ExternalReference js_stack_limit =
        ExternalReference::address_of_stack_limit(selector->isolate());
    if (mleft.object().Is(js_stack_limit) && mleft.index().Is(0)) {
      // Compare(Load(js_stack_limit), LoadStackPointer)
      if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
      InstructionCode opcode = cont->Encode(kX64StackCheck);
      if (cont->IsBranch()) {
        selector->Emit(opcode, g.NoOutput(), g.Label(cont->true_block()),
                       g.Label(cont->false_block()));
      } else {
        DCHECK(cont->IsSet());
        selector->Emit(opcode, g.DefineAsRegister(cont->result()));
      }
      return;
    }
  }
  VisitWordCompare(selector, node, kX64Cmp, cont);
}


// Shared routine for comparison with zero.
void VisitCompareZero(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont) {
  X64OperandGenerator g(selector);
  VisitCompare(selector, opcode, g.Use(node), g.TempImmediate(0), cont);
}


// Shared routine for multiple float32 compare operations (inputs commuted).
void VisitFloat32Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  Node* const left = node->InputAt(0);
  Node* const right = node->InputAt(1);
  InstructionCode const opcode =
      selector->IsSupported(AVX) ? kAVXFloat32Cmp : kSSEFloat32Cmp;
  VisitCompare(selector, opcode, right, left, cont, false);
}


// Shared routine for multiple float64 compare operations (inputs commuted).
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  Node* const left = node->InputAt(0);
  Node* const right = node->InputAt(1);
  InstructionCode const opcode =
      selector->IsSupported(AVX) ? kAVXFloat64Cmp : kSSEFloat64Cmp;
  VisitCompare(selector, opcode, right, left, cont, false);
}

}  // namespace


void InstructionSelector::VisitBranch(Node* branch, BasicBlock* tbranch,
                                      BasicBlock* fbranch) {
  X64OperandGenerator g(this);
  Node* user = branch;
  Node* value = branch->InputAt(0);

  FlagsContinuation cont(kNotEqual, tbranch, fbranch);

  // Try to combine with comparisons against 0 by simply inverting the branch.
  while (CanCover(user, value) && value->opcode() == IrOpcode::kWord32Equal) {
    Int32BinopMatcher m(value);
    if (m.right().Is(0)) {
      user = value;
      value = m.left().node();
      cont.Negate();
    } else {
      break;
    }
  }

  // Try to combine the branch with a comparison.
  if (CanCover(user, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord32Equal:
        cont.OverwriteAndNegateIfEqual(kEqual);
        return VisitWordCompare(this, value, kX64Cmp32, &cont);
      case IrOpcode::kInt32LessThan:
        cont.OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWordCompare(this, value, kX64Cmp32, &cont);
      case IrOpcode::kInt32LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWordCompare(this, value, kX64Cmp32, &cont);
      case IrOpcode::kUint32LessThan:
        cont.OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWordCompare(this, value, kX64Cmp32, &cont);
      case IrOpcode::kUint32LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWordCompare(this, value, kX64Cmp32, &cont);
      case IrOpcode::kWord64Equal: {
        cont.OverwriteAndNegateIfEqual(kEqual);
        Int64BinopMatcher m(value);
        if (m.right().Is(0)) {
          // Try to combine the branch with a comparison.
          Node* const user = m.node();
          Node* const value = m.left().node();
          if (CanCover(user, value)) {
            switch (value->opcode()) {
              case IrOpcode::kInt64Sub:
                return VisitWord64Compare(this, value, &cont);
              case IrOpcode::kWord64And:
                return VisitWordCompare(this, value, kX64Test, &cont);
              default:
                break;
            }
          }
          return VisitCompareZero(this, value, kX64Cmp, &cont);
        }
        return VisitWord64Compare(this, value, &cont);
      }
      case IrOpcode::kInt64LessThan:
        cont.OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWord64Compare(this, value, &cont);
      case IrOpcode::kInt64LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWord64Compare(this, value, &cont);
      case IrOpcode::kUint64LessThan:
        cont.OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWord64Compare(this, value, &cont);
      case IrOpcode::kUint64LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWord64Compare(this, value, &cont);
      case IrOpcode::kFloat32Equal:
        cont.OverwriteAndNegateIfEqual(kUnorderedEqual);
        return VisitFloat32Compare(this, value, &cont);
      case IrOpcode::kFloat32LessThan:
        cont.OverwriteAndNegateIfEqual(kUnsignedGreaterThan);
        return VisitFloat32Compare(this, value, &cont);
      case IrOpcode::kFloat32LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kUnsignedGreaterThanOrEqual);
        return VisitFloat32Compare(this, value, &cont);
      case IrOpcode::kFloat64Equal:
        cont.OverwriteAndNegateIfEqual(kUnorderedEqual);
        return VisitFloat64Compare(this, value, &cont);
      case IrOpcode::kFloat64LessThan:
        cont.OverwriteAndNegateIfEqual(kUnsignedGreaterThan);
        return VisitFloat64Compare(this, value, &cont);
      case IrOpcode::kFloat64LessThanOrEqual:
        cont.OverwriteAndNegateIfEqual(kUnsignedGreaterThanOrEqual);
        return VisitFloat64Compare(this, value, &cont);
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
                cont.OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kX64Add32, &cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont.OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kX64Sub32, &cont);
              case IrOpcode::kInt64AddWithOverflow:
                cont.OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kX64Add, &cont);
              case IrOpcode::kInt64SubWithOverflow:
                cont.OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kX64Sub, &cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Sub:
        return VisitWordCompare(this, value, kX64Cmp32, &cont);
      case IrOpcode::kInt64Sub:
        return VisitWord64Compare(this, value, &cont);
      case IrOpcode::kWord32And:
        return VisitWordCompare(this, value, kX64Test32, &cont);
      case IrOpcode::kWord64And:
        return VisitWordCompare(this, value, kX64Test, &cont);
      default:
        break;
    }
  }

  // Branch could not be combined with a compare, emit compare against 0.
  VisitCompareZero(this, value, kX64Cmp32, &cont);
}


void InstructionSelector::VisitSwitch(Node* node, const SwitchInfo& sw) {
  X64OperandGenerator g(this);
  InstructionOperand value_operand = g.UseRegister(node->InputAt(0));

  // Emit either ArchTableSwitch or ArchLookupSwitch.
  size_t table_space_cost = 4 + sw.value_range;
  size_t table_time_cost = 3;
  size_t lookup_space_cost = 3 + 2 * sw.case_count;
  size_t lookup_time_cost = sw.case_count;
  if (sw.case_count > 4 &&
      table_space_cost + 3 * table_time_cost <=
          lookup_space_cost + 3 * lookup_time_cost &&
      sw.min_value > std::numeric_limits<int32_t>::min()) {
    InstructionOperand index_operand = g.TempRegister();
    if (sw.min_value) {
      // The leal automatically zero extends, so result is a valid 64-bit index.
      Emit(kX64Lea32 | AddressingModeField::encode(kMode_MRI), index_operand,
           value_operand, g.TempImmediate(-sw.min_value));
    } else {
      // Zero extend, because we use it as 64-bit index into the jump table.
      Emit(kX64Movl, index_operand, value_operand);
    }
    // Generate a table lookup.
    return EmitTableSwitch(sw, index_operand);
  }

  // Generate a sequence of conditional jumps.
  return EmitLookupSwitch(sw, value_operand);
}


void InstructionSelector::VisitWord32Equal(Node* const node) {
  Node* user = node;
  FlagsContinuation cont(kEqual, node);
  Int32BinopMatcher m(user);
  if (m.right().Is(0)) {
    Node* value = m.left().node();

    // Try to combine with comparisons against 0 by simply inverting the branch.
    while (CanCover(user, value) && value->opcode() == IrOpcode::kWord32Equal) {
      Int32BinopMatcher m(value);
      if (m.right().Is(0)) {
        user = value;
        value = m.left().node();
        cont.Negate();
      } else {
        break;
      }
    }

    // Try to combine the branch with a comparison.
    if (CanCover(user, value)) {
      switch (value->opcode()) {
        case IrOpcode::kInt32Sub:
          return VisitWordCompare(this, value, kX64Cmp32, &cont);
        case IrOpcode::kWord32And:
          return VisitWordCompare(this, value, kX64Test32, &cont);
        default:
          break;
      }
    }
    return VisitCompareZero(this, value, kX64Cmp32, &cont);
  }
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}


void InstructionSelector::VisitInt32LessThan(Node* node) {
  FlagsContinuation cont(kSignedLessThan, node);
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}


void InstructionSelector::VisitInt32LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}


void InstructionSelector::VisitUint32LessThan(Node* node) {
  FlagsContinuation cont(kUnsignedLessThan, node);
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}


void InstructionSelector::VisitUint32LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kUnsignedLessThanOrEqual, node);
  VisitWordCompare(this, node, kX64Cmp32, &cont);
}


void InstructionSelector::VisitWord64Equal(Node* const node) {
  FlagsContinuation cont(kEqual, node);
  Int64BinopMatcher m(node);
  if (m.right().Is(0)) {
    // Try to combine the equality check with a comparison.
    Node* const user = m.node();
    Node* const value = m.left().node();
    if (CanCover(user, value)) {
      switch (value->opcode()) {
        case IrOpcode::kInt64Sub:
          return VisitWord64Compare(this, value, &cont);
        case IrOpcode::kWord64And:
          return VisitWordCompare(this, value, kX64Test, &cont);
        default:
          break;
      }
    }
  }
  VisitWord64Compare(this, node, &cont);
}


void InstructionSelector::VisitInt32AddWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont(kOverflow, ovf);
    VisitBinop(this, node, kX64Add32, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kX64Add32, &cont);
}


void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont(kOverflow, ovf);
    return VisitBinop(this, node, kX64Sub32, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kX64Sub32, &cont);
}


void InstructionSelector::VisitInt64LessThan(Node* node) {
  FlagsContinuation cont(kSignedLessThan, node);
  VisitWord64Compare(this, node, &cont);
}


void InstructionSelector::VisitInt64LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kSignedLessThanOrEqual, node);
  VisitWord64Compare(this, node, &cont);
}


void InstructionSelector::VisitUint64LessThan(Node* node) {
  FlagsContinuation cont(kUnsignedLessThan, node);
  VisitWord64Compare(this, node, &cont);
}


void InstructionSelector::VisitUint64LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kUnsignedLessThanOrEqual, node);
  VisitWord64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat32Equal(Node* node) {
  FlagsContinuation cont(kUnorderedEqual, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat32LessThan(Node* node) {
  FlagsContinuation cont(kUnsignedGreaterThan, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat32LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kUnsignedGreaterThanOrEqual, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64Equal(Node* node) {
  FlagsContinuation cont(kUnorderedEqual, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64LessThan(Node* node) {
  FlagsContinuation cont(kUnsignedGreaterThan, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64LessThanOrEqual(Node* node) {
  FlagsContinuation cont(kUnsignedGreaterThanOrEqual, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64ExtractLowWord32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64ExtractLowWord32, g.DefineAsRegister(node),
       g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64ExtractHighWord32(Node* node) {
  X64OperandGenerator g(this);
  Emit(kSSEFloat64ExtractHighWord32, g.DefineAsRegister(node),
       g.Use(node->InputAt(0)));
}


void InstructionSelector::VisitFloat64InsertLowWord32(Node* node) {
  X64OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Float64Matcher mleft(left);
  if (mleft.HasValue() && (bit_cast<uint64_t>(mleft.Value()) >> 32) == 0u) {
    Emit(kSSEFloat64LoadLowWord32, g.DefineAsRegister(node), g.Use(right));
    return;
  }
  Emit(kSSEFloat64InsertLowWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.Use(right));
}


void InstructionSelector::VisitFloat64InsertHighWord32(Node* node) {
  X64OperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  Emit(kSSEFloat64InsertHighWord32, g.DefineSameAsFirst(node),
       g.UseRegister(left), g.Use(right));
}


// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags =
      MachineOperatorBuilder::kFloat32Max |
      MachineOperatorBuilder::kFloat32Min |
      MachineOperatorBuilder::kFloat64Max |
      MachineOperatorBuilder::kFloat64Min |
      MachineOperatorBuilder::kWord32ShiftIsSafe |
      MachineOperatorBuilder::kWord32Ctz | MachineOperatorBuilder::kWord64Ctz;
  if (CpuFeatures::IsSupported(POPCNT)) {
    flags |= MachineOperatorBuilder::kWord32Popcnt |
             MachineOperatorBuilder::kWord64Popcnt;
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    flags |= MachineOperatorBuilder::kFloat32RoundDown |
             MachineOperatorBuilder::kFloat64RoundDown |
             MachineOperatorBuilder::kFloat32RoundUp |
             MachineOperatorBuilder::kFloat64RoundUp |
             MachineOperatorBuilder::kFloat32RoundTruncate |
             MachineOperatorBuilder::kFloat64RoundTruncate |
             MachineOperatorBuilder::kFloat32RoundTiesEven |
             MachineOperatorBuilder::kFloat64RoundTiesEven;
  }
  return flags;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
