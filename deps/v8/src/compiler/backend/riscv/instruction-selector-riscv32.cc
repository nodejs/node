// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/machine-type.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/backend/riscv/instruction-selector-riscv.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...) PrintF(__VA_ARGS__)

bool RiscvOperandGeneratorT::CanBeImmediate(int64_t value,
                                            InstructionCode opcode) {
  switch (ArchOpcodeField::decode(opcode)) {
    case kRiscvShl32:
    case kRiscvSar32:
    case kRiscvShr32:
      return is_uint5(value);
    case kRiscvAdd32:
    case kRiscvAnd32:
    case kRiscvAnd:
    case kRiscvOr32:
    case kRiscvOr:
    case kRiscvTst32:
    case kRiscvXor:
      return is_int12(value);
    case kRiscvLb:
    case kRiscvLbu:
    case kRiscvSb:
    case kRiscvLh:
    case kRiscvLhu:
    case kRiscvSh:
    case kRiscvLw:
    case kRiscvSw:
    case kRiscvLoadFloat:
    case kRiscvStoreFloat:
    case kRiscvLoadDouble:
    case kRiscvStoreDouble:
      return is_int32(value);
    default:
      return is_int12(value);
  }
}

void EmitLoad(InstructionSelectorT* selector, OpIndex node,
              InstructionCode opcode, OpIndex output = OpIndex()) {
  RiscvOperandGeneratorT g(selector);
  const Operation& op = selector->Get(node);
  const LoadOp& load = op.Cast<LoadOp>();
  // The LoadStoreSimplificationReducer transforms all loads into
  // *(base + index).
  OpIndex base = load.base();
  OptionalOpIndex index = load.index();
  DCHECK_EQ(load.offset, 0);
  DCHECK_EQ(load.element_size_log2, 0);

  InstructionOperand inputs[3];
  size_t input_count = 0;
  InstructionOperand output_op;

  // If output is valid, use that as the output register. This is used when we
  // merge a conversion into the load.
  output_op = g.DefineAsRegister(output.valid() ? output : node);

  const Operation& base_op = selector->Get(base);
  if (base_op.Is<Opmask::kExternalConstant>() && g.IsIntegerConstant(index)) {
    const ConstantOp& constant_base = base_op.Cast<ConstantOp>();
    if (selector->CanAddressRelativeToRootsRegister(
            constant_base.external_reference())) {
      ptrdiff_t const delta =
          *g.GetOptionalIntegerConstant(index.value()) +
          MacroAssemblerBase::RootRegisterOffsetForExternalReference(
              selector->isolate(), constant_base.external_reference());
      input_count = 1;
      // Check that the delta is a 32-bit integer due to the limitations of
      // immediate operands.
      if (is_int32(delta)) {
        inputs[0] = g.UseImmediate(static_cast<int32_t>(delta));
        opcode |= AddressingModeField::encode(kMode_Root);
        selector->Emit(opcode, 1, &output_op, input_count, inputs);
        return;
      }
    }
  }

  if (base_op.Is<LoadRootRegisterOp>()) {
    DCHECK(g.IsIntegerConstant(index));
    input_count = 1;
    inputs[0] = g.UseImmediate64(*g.GetOptionalIntegerConstant(index.value()));
    opcode |= AddressingModeField::encode(kMode_Root);
    selector->Emit(opcode, 1, &output_op, input_count, inputs);
    return;
  }

  if (load.index().has_value() && g.CanBeImmediate(index.value(), opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(output.valid() ? output : node),
                   g.UseRegister(base), g.UseImmediate(index.value()));
  } else {
    if (index.has_value()) {
      InstructionOperand addr_reg = g.TempRegister();
      selector->Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None),
                     addr_reg, g.UseRegister(index.value()),
                     g.UseRegister(base));
      // Emit desired load opcode, using temp addr_reg.
      selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                     g.DefineAsRegister(output.valid() ? output : node),
                     addr_reg, g.TempImmediate(0));
    } else {
      selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                     g.DefineAsRegister(output.valid() ? output : node),
                     g.UseRegister(base), g.TempImmediate(0));
    }
  }
}

void EmitS128Load(InstructionSelectorT* selector, OpIndex node,
                  InstructionCode opcode, VSew sew, Vlmul lmul) {
  RiscvOperandGeneratorT g(selector);
  OpIndex base = selector->input_at(node, 0);
  OpIndex index = selector->input_at(node, 1);

  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(node), g.UseRegister(base),
                   g.UseImmediate(index), g.UseImmediate(sew),
                   g.UseImmediate(lmul));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI),
                   g.DefineAsRegister(node), addr_reg, g.TempImmediate(0),
                   g.UseImmediate(sew), g.UseImmediate(lmul));
  }
}

void InstructionSelectorT::VisitStoreLane(OpIndex node) {
  const Simd128LaneMemoryOp& store = Get(node).Cast<Simd128LaneMemoryOp>();
  InstructionCode opcode = kRiscvS128StoreLane;
  opcode |= LaneSizeField::encode(store.lane_size() * kBitsPerByte);
  if (store.kind.with_trap_handler) {
    opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
  }

  RiscvOperandGeneratorT g(this);
  OpIndex base = this->input_at(node, 0);
  OpIndex index = this->input_at(node, 1);
  InstructionOperand addr_reg = g.TempRegister();
  Emit(kRiscvAdd32, addr_reg, g.UseRegister(base), g.UseRegister(index));
  InstructionOperand inputs[4] = {
      g.UseRegister(input_at(node, 2)),
      g.UseImmediate(store.lane),
      addr_reg,
      g.TempImmediate(0),
  };
  opcode |= AddressingModeField::encode(kMode_MRI);
  Emit(opcode, 0, nullptr, 4, inputs);
}

void InstructionSelectorT::VisitLoadLane(OpIndex node) {
  const Simd128LaneMemoryOp& load = this->Get(node).Cast<Simd128LaneMemoryOp>();
  InstructionCode opcode = kRiscvS128LoadLane;
  opcode |= LaneSizeField::encode(load.lane_size() * kBitsPerByte);
  if (load.kind.with_trap_handler) {
    opcode |= AccessModeField::encode(kMemoryAccessProtectedMemOutOfBounds);
  }

  RiscvOperandGeneratorT g(this);
  OpIndex base = this->input_at(node, 0);
  OpIndex index = this->input_at(node, 1);
  InstructionOperand addr_reg = g.TempRegister();
  Emit(kRiscvAdd32, addr_reg, g.UseRegister(base), g.UseRegister(index));
  opcode |= AddressingModeField::encode(kMode_MRI);
  Emit(opcode, g.DefineSameAsFirst(node),
       g.UseRegister(this->input_at(node, 2)), g.UseImmediate(load.lane),
       addr_reg, g.TempImmediate(0));
}

void InstructionSelectorT::VisitLoad(OpIndex node) {
  auto load = this->load_view(node);
  LoadRepresentation load_rep = load.loaded_rep();
  InstructionCode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kRiscvLoadFloat;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kRiscvLoadDouble;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsUnsigned() ? kRiscvLbu : kRiscvLb;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsUnsigned() ? kRiscvLhu : kRiscvLh;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord32:
      opcode = kRiscvLw;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kRiscvRvvLd;
      break;
    case MachineRepresentation::kCompressedPointer:
    case MachineRepresentation::kCompressed:
    case MachineRepresentation::kSandboxedPointer:
    case MachineRepresentation::kMapWord:  // Fall through.
    case MachineRepresentation::kWord64:
    case MachineRepresentation::kNone:
    case MachineRepresentation::kSimd256:  // Fall through.
    case MachineRepresentation::kProtectedPointer:  // Fall through.
    case MachineRepresentation::kIndirectPointer:
    case MachineRepresentation::kFloat16:
    case MachineRepresentation::kFloat16RawBits:
      UNREACHABLE();
    }

    EmitLoad(this, node, opcode);
}

void InstructionSelectorT::VisitStorePair(OpIndex node) { UNREACHABLE(); }

void InstructionSelectorT::VisitStore(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  StoreView store_view = this->store_view(node);
  OpIndex base = store_view.base();
  OptionalOpIndex index = store_view.index();
  OpIndex value = store_view.value();

  WriteBarrierKind write_barrier_kind =
      store_view.stored_rep().write_barrier_kind();
  MachineRepresentation rep = store_view.stored_rep().representation();

  // TODO(riscv): I guess this could be done in a better way.
  if (write_barrier_kind != kNoWriteBarrier && index.has_value() &&
      V8_LIKELY(!v8_flags.disable_write_barriers)) {
    DCHECK(CanBeTaggedPointer(rep));
    InstructionOperand inputs[4];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    inputs[input_count++] = g.UseUniqueRegister(this->value(index));
    inputs[input_count++] = g.UseUniqueRegister(value);
    RecordWriteMode record_write_mode =
        WriteBarrierKindToRecordWriteMode(write_barrier_kind);
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    size_t const temp_count = arraysize(temps);
    InstructionCode code;
    if (rep == MachineRepresentation::kIndirectPointer) {
      DCHECK_EQ(write_barrier_kind, kIndirectPointerWriteBarrier);
      // In this case we need to add the IndirectPointerTag as additional input.
      code = kArchStoreIndirectWithWriteBarrier;
      IndirectPointerTag tag = store_view.indirect_pointer_tag();
      inputs[input_count++] = g.UseImmediate64(static_cast<int64_t>(tag));
    } else {
      code = kArchStoreWithWriteBarrier;
    }
    code |= RecordWriteModeField::encode(record_write_mode);
    if (store_view.is_store_trap_on_null()) {
      code |= AccessModeField::encode(kMemoryAccessProtectedNullDereference);
    }
    Emit(code, 0, nullptr, input_count, inputs, temp_count, temps);
  } else {
    InstructionCode code;
    switch (rep) {
      case MachineRepresentation::kFloat32:
        code = kRiscvStoreFloat;
        break;
      case MachineRepresentation::kFloat64:
        code = kRiscvStoreDouble;
        break;
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        code = kRiscvSb;
        break;
      case MachineRepresentation::kWord16:
        code = kRiscvSh;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:
      case MachineRepresentation::kWord32:
        code = kRiscvSw;
        break;
      case MachineRepresentation::kSimd128:
        code = kRiscvRvvSt;
        break;
      case MachineRepresentation::kCompressedPointer:  // Fall through.
      case MachineRepresentation::kCompressed:
      case MachineRepresentation::kSandboxedPointer:
      case MachineRepresentation::kMapWord:  // Fall through.
      case MachineRepresentation::kNone:
      case MachineRepresentation::kWord64:
      case MachineRepresentation::kSimd256:  // Fall through.
      case MachineRepresentation::kProtectedPointer:  // Fall through.
      case MachineRepresentation::kIndirectPointer:
      case MachineRepresentation::kFloat16:
      case MachineRepresentation::kFloat16RawBits:
        UNREACHABLE();
    }

    if (this->is_load_root_register(base)) {
      Emit(code | AddressingModeField::encode(kMode_Root), g.NoOutput(),
           g.UseRegisterOrImmediateZero(value),
           index.has_value() ? g.UseImmediate(this->value(index))
                             : g.UseImmediate(0));
      return;
    }

    if (index.has_value() && g.CanBeImmediate(this->value(index), code)) {
      Emit(code | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
           g.UseRegisterOrImmediateZero(value), g.UseRegister(base),
           index.has_value() ? g.UseImmediate(this->value(index))
                             : g.UseImmediate(0));
    } else {
      if (index.has_value()) {
        InstructionOperand addr_reg = g.TempRegister();
        Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None), addr_reg,
             g.UseRegister(this->value(index)), g.UseRegister(base));
        // Emit desired store opcode, using temp addr_reg.
        Emit(code | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
             g.UseRegisterOrImmediateZero(value), addr_reg, g.TempImmediate(0));
      } else {
        Emit(code | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
             g.UseRegisterOrImmediateZero(value), g.UseRegister(base),
             g.UseImmediate(0));
      }
    }
  }
}

void InstructionSelectorT::VisitProtectedLoad(OpIndex node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitProtectedStore(OpIndex node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitWord32And(OpIndex node) {
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvAnd, true, kRiscvAnd);
}

void InstructionSelectorT::VisitWord32Or(OpIndex node) {
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvOr, true, kRiscvOr);
}

void InstructionSelectorT::VisitWord32Xor(OpIndex node) {
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvXor, true, kRiscvXor);
}

void InstructionSelectorT::VisitWord32Rol(OpIndex node) { UNIMPLEMENTED(); }

void InstructionSelectorT::VisitWord32Ror(OpIndex node) {
  VisitRRO(this, kRiscvRor32, node);
}

void InstructionSelectorT::VisitWord32ReverseBits(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitWord64ReverseBytes(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitWord32ReverseBytes(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  if (CpuFeatures::IsSupported(ZBB)) {
    Emit(kRiscvRev8, g.DefineAsRegister(node),
         g.UseRegister(this->input_at(node, 0)));
  } else {
    Emit(kRiscvByteSwap32, g.DefineAsRegister(node),
         g.UseRegister(this->input_at(node, 0)));
  }
}

void InstructionSelectorT::VisitSimd128ReverseBytes(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitWord32Ctz(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  Emit(kRiscvCtz, g.DefineAsRegister(node),
       g.UseRegister(this->input_at(node, 0)));
}

void InstructionSelectorT::VisitWord32Popcnt(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  Emit(kRiscvCpop, g.DefineAsRegister(node),
       g.UseRegister(this->input_at(node, 0)));
}

void InstructionSelectorT::VisitInt32Add(OpIndex node) {
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvAdd32, true, kRiscvAdd32);
}

void InstructionSelectorT::VisitInt32Sub(OpIndex node) {
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvSub32);
}

void InstructionSelectorT::VisitInt32Mul(OpIndex node) {
  VisitRRR(this, kRiscvMul32, node);
}

void InstructionSelectorT::VisitInt32MulHigh(OpIndex node) {
  VisitRRR(this, kRiscvMulHigh32, node);
}

void InstructionSelectorT::VisitUint32MulHigh(OpIndex node) {
  VisitRRR(this, kRiscvMulHighU32, node);
}

void InstructionSelectorT::VisitInt32Div(OpIndex node) {
  VisitRRR(this, kRiscvDiv32, node,
           OperandGenerator::RegisterUseKind::kUseUniqueRegister);
}

void InstructionSelectorT::VisitUint32Div(OpIndex node) {
  VisitRRR(this, kRiscvDivU32, node,
           OperandGenerator::RegisterUseKind::kUseUniqueRegister);
}

void InstructionSelectorT::VisitInt32Mod(OpIndex node) {
  VisitRRR(this, kRiscvMod32, node);
}

void InstructionSelectorT::VisitUint32Mod(OpIndex node) {
  VisitRRR(this, kRiscvModU32, node);
}

void InstructionSelectorT::VisitChangeFloat32ToFloat64(OpIndex node) {
  VisitRR(this, kRiscvCvtDS, node);
}

void InstructionSelectorT::VisitRoundInt32ToFloat32(OpIndex node) {
  VisitRR(this, kRiscvCvtSW, node);
}

void InstructionSelectorT::VisitRoundUint32ToFloat32(OpIndex node) {
  VisitRR(this, kRiscvCvtSUw, node);
}

void InstructionSelectorT::VisitChangeInt32ToFloat64(OpIndex node) {
  VisitRR(this, kRiscvCvtDW, node);
}

void InstructionSelectorT::VisitChangeUint32ToFloat64(OpIndex node) {
  VisitRR(this, kRiscvCvtDUw, node);
}

void InstructionSelectorT::VisitTruncateFloat32ToInt32(OpIndex node) {
  RiscvOperandGeneratorT g(this);

  const Operation& op = this->Get(node);
  InstructionCode opcode = kRiscvTruncWS;
  if (op.Is<Opmask::kTruncateFloat32ToInt32OverflowToMin>()) {
    opcode |= MiscField::encode(true);
  }
  Emit(opcode, g.DefineAsRegister(node),
       g.UseRegister(this->input_at(node, 0)));
}

void InstructionSelectorT::VisitTruncateFloat32ToUint32(OpIndex node) {
  RiscvOperandGeneratorT g(this);

  const Operation& op = this->Get(node);
  InstructionCode opcode = kRiscvTruncUwS;
  if (op.Is<Opmask::kTruncateFloat32ToUint32OverflowToMin>()) {
    opcode |= MiscField::encode(true);
  }

  Emit(opcode, g.DefineAsRegister(node), g.UseRegister(op.input(0)));
}

void InstructionSelectorT::VisitChangeFloat64ToInt32(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  OpIndex value = this->input_at(node, 0);
  using Rep = turboshaft::RegisterRepresentation;
  if (CanCover(node, value)) {
    const turboshaft::Operation& op = this->Get(value);
    if (op.Is<turboshaft::ChangeOp>()) {
      const turboshaft::ChangeOp& change = op.Cast<turboshaft::ChangeOp>();
      if (change.kind == turboshaft::ChangeOp::Kind::kFloatConversion) {
        if (change.from == Rep::Float32() && change.to == Rep::Float64()) {
          Emit(kRiscvTruncWS, g.DefineAsRegister(node),
               g.UseRegister(this->input_at(value, 0)));
          return;
        }
      }
    }
  }
  VisitRR(this, kRiscvTruncWD, node);
}

void InstructionSelectorT::VisitChangeFloat64ToUint32(OpIndex node) {
  VisitRR(this, kRiscvTruncUwD, node);
}

void InstructionSelectorT::VisitTruncateFloat64ToUint32(OpIndex node) {
  VisitRR(this, kRiscvTruncUwD, node);
}

void InstructionSelectorT::VisitBitcastFloat32ToInt32(OpIndex node) {
  VisitRR(this, kRiscvBitcastFloat32ToInt32, node);
}

void InstructionSelectorT::VisitBitcastInt32ToFloat32(OpIndex node) {
  VisitRR(this, kRiscvBitcastInt32ToFloat32, node);
}

void InstructionSelectorT::VisitFloat64RoundDown(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitFloat32RoundUp(OpIndex node) {
  VisitRR(this, kRiscvFloat32RoundUp, node);
}

void InstructionSelectorT::VisitFloat64RoundUp(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitFloat32RoundTruncate(OpIndex node) {
  VisitRR(this, kRiscvFloat32RoundTruncate, node);
}

void InstructionSelectorT::VisitFloat64RoundTruncate(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitFloat64RoundTiesAway(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitFloat32RoundTiesEven(OpIndex node) {
  VisitRR(this, kRiscvFloat32RoundTiesEven, node);
}

void InstructionSelectorT::VisitFloat64RoundTiesEven(OpIndex node) {
  UNIMPLEMENTED();
}

void InstructionSelectorT::VisitFloat32Neg(OpIndex node) {
  VisitRR(this, kRiscvNegS, node);
}

void InstructionSelectorT::VisitFloat64Neg(OpIndex node) {
  VisitRR(this, kRiscvNegD, node);
}

void InstructionSelectorT::VisitFloat64Ieee754Binop(OpIndex node,
                                                    InstructionCode opcode) {
  RiscvOperandGeneratorT g(this);
  Emit(opcode, g.DefineAsFixed(node, fa0),
       g.UseFixed(this->input_at(node, 0), fa0),
       g.UseFixed(this->input_at(node, 1), fa1))
      ->MarkAsCall();
}

void InstructionSelectorT::VisitFloat64Ieee754Unop(OpIndex node,
                                                   InstructionCode opcode) {
  RiscvOperandGeneratorT g(this);
  Emit(opcode, g.DefineAsFixed(node, fa0),
       g.UseFixed(this->input_at(node, 0), fa1))
      ->MarkAsCall();
}

void InstructionSelectorT::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* call_descriptor,
    OpIndex node) {
  RiscvOperandGeneratorT g(this);

  // Prepare for C function call.
  if (call_descriptor->IsCFunctionCall()) {
    Emit(kArchPrepareCallCFunction | MiscField::encode(static_cast<int>(
                                         call_descriptor->ParameterCount())),
         0, nullptr, 0, nullptr);

    // Poke any stack arguments.
    int slot = kCArgSlotCount;
    for (PushParameter input : (*arguments)) {
      Emit(kRiscvStoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
           g.TempImmediate(slot << kSystemPointerSizeLog2));
      ++slot;
    }
  } else {
    int push_count = static_cast<int>(call_descriptor->ParameterSlotCount());
    if (push_count > 0) {
      Emit(kRiscvStackClaim, g.NoOutput(),
           g.TempImmediate(arguments->size() << kSystemPointerSizeLog2));
    }
    for (size_t n = 0; n < arguments->size(); ++n) {
      PushParameter input = (*arguments)[n];
      if (input.node.valid()) {
        Emit(kRiscvStoreToStackSlot, g.NoOutput(), g.UseRegister(input.node),
             g.TempImmediate(static_cast<int>(n << kSystemPointerSizeLog2)));
      }
    }
  }
}

void InstructionSelectorT::VisitUnalignedLoad(OpIndex node) {
  auto load = this->load_view(node);
  LoadRepresentation load_rep = load.loaded_rep();
  RiscvOperandGeneratorT g(this);
  OpIndex base = load.base();
  OpIndex index = load.index();

  ArchOpcode opcode;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kRiscvULoadFloat;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kRiscvULoadDouble;
      break;
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsUnsigned() ? kRiscvLbu : kRiscvLb;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsUnsigned() ? kRiscvUlhu : kRiscvUlh;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord32:
      opcode = kRiscvUlw;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kRiscvRvvLd;
      break;
    case MachineRepresentation::kSimd256:            // Fall through.
    case MachineRepresentation::kBit:                // Fall through.
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:         // Fall through.
    case MachineRepresentation::kSandboxedPointer:   // Fall through.
    case MachineRepresentation::kMapWord:            // Fall through.
    case MachineRepresentation::kProtectedPointer:   // Fall through.
    case MachineRepresentation::kWord64:
    case MachineRepresentation::kNone:
    case MachineRepresentation::kIndirectPointer:
    case MachineRepresentation::kFloat16:
    case MachineRepresentation::kFloat16RawBits:
      UNREACHABLE();
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI),
         g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

void InstructionSelectorT::VisitUnalignedStore(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  OpIndex base = this->input_at(node, 0);
  OpIndex index = this->input_at(node, 1);
  OpIndex value = this->input_at(node, 2);
  UnalignedStoreRepresentation rep = UnalignedStoreRepresentationOf(node->op());
  ArchOpcode opcode;
  switch (rep) {
    case MachineRepresentation::kFloat32:
      opcode = kRiscvUStoreFloat;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kRiscvUStoreDouble;
      break;
    case MachineRepresentation::kWord8:
      opcode = kRiscvSb;
      break;
    case MachineRepresentation::kWord16:
      opcode = kRiscvUsh;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord32:
      opcode = kRiscvUsw;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kRiscvRvvSt;
      break;
    case MachineRepresentation::kSimd256:            // Fall through.
    case MachineRepresentation::kBit:                // Fall through.
    case MachineRepresentation::kCompressedPointer:  // Fall through.
    case MachineRepresentation::kCompressed:         // Fall through.
    case MachineRepresentation::kSandboxedPointer:
    case MachineRepresentation::kMapWord:           // Fall through.
    case MachineRepresentation::kProtectedPointer:  // Fall through.
    case MachineRepresentation::kNone:
    case MachineRepresentation::kWord64:
    case MachineRepresentation::kIndirectPointer:
    case MachineRepresentation::kFloat16:
    case MachineRepresentation::kFloat16RawBits:
      UNREACHABLE();
  }

  if (g.CanBeImmediate(index, opcode)) {
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         g.UseRegister(base), g.UseImmediate(index),
         g.UseRegisterOrImmediateZero(value));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None), addr_reg,
         g.UseRegister(index), g.UseRegister(base));
    // Emit desired store opcode, using temp addr_reg.
    Emit(opcode | AddressingModeField::encode(kMode_MRI), g.NoOutput(),
         addr_reg, g.TempImmediate(0), g.UseRegisterOrImmediateZero(value));
  }
}

namespace {

void VisitWordCompare(InstructionSelectorT* selector, OpIndex node,
                      FlagsContinuationT* cont) {
  VisitWordCompare(selector, node, kRiscvCmp, cont, false);
}

void VisitAtomicLoad(InstructionSelectorT* selector, OpIndex node,
                     ArchOpcode opcode, AtomicWidth width) {
  using OpIndex = OpIndex;
  RiscvOperandGeneratorT g(selector);
  auto load = selector->load_view(node);
  OpIndex base = load.base();
  OpIndex index = load.index();
  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.DefineAsRegister(node), g.UseRegister(base),
                   g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired load opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.DefineAsRegister(node), addr_reg, g.TempImmediate(0));
  }
}

void VisitAtomicStore(InstructionSelectorT* selector, OpIndex node,
                      ArchOpcode opcode, AtomicWidth width) {
  RiscvOperandGeneratorT g(selector);
  using OpIndex = OpIndex;
  auto store = selector->store_view(node);
  OpIndex base = store.base();
  OpIndex index = selector->value(store.index());
  OpIndex value = store.value();

  if (g.CanBeImmediate(index, opcode)) {
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.NoOutput(), g.UseRegisterOrImmediateZero(value),
                   g.UseRegister(base), g.UseImmediate(index));
  } else {
    InstructionOperand addr_reg = g.TempRegister();
    selector->Emit(kRiscvAdd32 | AddressingModeField::encode(kMode_None),
                   addr_reg, g.UseRegister(index), g.UseRegister(base));
    // Emit desired store opcode, using temp addr_reg.
    selector->Emit(opcode | AddressingModeField::encode(kMode_MRI) |
                       AtomicWidthField::encode(width),
                   g.NoOutput(), g.UseRegisterOrImmediateZero(value), addr_reg,
                   g.TempImmediate(0));
  }
}

void VisitAtomicBinop(InstructionSelectorT* selector, OpIndex node,
                      ArchOpcode opcode) {
  RiscvOperandGeneratorT g(selector);
  using OpIndex = OpIndex;
  const AtomicRMWOp& atomic_op = selector->Cast<AtomicRMWOp>(node);
  OpIndex base = atomic_op.base();
  OpIndex index = atomic_op.index();
  OpIndex value = atomic_op.value();

  AddressingMode addressing_mode = kMode_MRI;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);
  InstructionOperand outputs[1];
  outputs[0] = g.UseUniqueRegister(node);
  InstructionOperand temps[4];
  temps[0] = g.TempRegister();
  temps[1] = g.TempRegister();
  temps[2] = g.TempRegister();
  temps[3] = g.TempRegister();
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  selector->Emit(code, 1, outputs, input_count, inputs, 4, temps);
}

}  // namespace

void InstructionSelectorT::VisitStackPointerGreaterThan(
    OpIndex node, FlagsContinuationT* cont) {
  StackCheckKind kind;
  OpIndex value;
  const auto& op = this->turboshaft_graph()
                       ->Get(node)
                       .template Cast<turboshaft::StackPointerGreaterThanOp>();
  kind = op.kind;
  value = op.stack_limit();
  InstructionCode opcode =
      kArchStackPointerGreaterThan | MiscField::encode(static_cast<int>(kind));

  RiscvOperandGeneratorT g(this);

  // No outputs.
  InstructionOperand* const outputs = nullptr;
  const int output_count = 0;

  // Applying an offset to this stack check requires a temp register. Offsets
  // are only applied to the first stack check. If applying an offset, we must
  // ensure the input and temp registers do not alias, thus kUniqueRegister.
  InstructionOperand temps[] = {g.TempRegister()};
  const int temp_count = (kind == StackCheckKind::kJSFunctionEntry ? 1 : 0);
  const auto register_mode = (kind == StackCheckKind::kJSFunctionEntry)
                                 ? OperandGenerator::kUniqueRegister
                                 : OperandGenerator::kRegister;

  InstructionOperand inputs[] = {g.UseRegisterWithMode(value, register_mode)};
  static constexpr int input_count = arraysize(inputs);

  EmitWithContinuation(opcode, output_count, outputs, input_count, inputs,
                       temp_count, temps, cont);
}

// Shared routine for word comparisons against zero.
void InstructionSelectorT::VisitWordCompareZero(OpIndex user, OpIndex value,
                                                FlagsContinuation* cont) {
  // Try to combine with comparisons against 0 by simply inverting the branch.
  while (const ComparisonOp* equal =
             this->TryCast<Opmask::kWord32Equal>(value)) {
    if (!CanCover(user, value)) break;
    if (!MatchIntegralZero(equal->right())) break;

    user = value;
    value = equal->left();
    cont->Negate();
  }

  const Operation& value_op = Get(value);
  if (CanCover(user, value)) {
    if (const ComparisonOp* comparison = value_op.TryCast<ComparisonOp>()) {
      switch (comparison->rep.value()) {
        case RegisterRepresentation::Word32():
          cont->OverwriteAndNegateIfEqual(
              GetComparisonFlagCondition(*comparison));
          return VisitWordCompare(this, value, cont);
        case RegisterRepresentation::Float32():
          switch (comparison->kind) {
            case ComparisonOp::Kind::kEqual:
              cont->OverwriteAndNegateIfEqual(kEqual);
              return VisitFloat32Compare(this, value, cont);
            case ComparisonOp::Kind::kSignedLessThan:
              cont->OverwriteAndNegateIfEqual(kFloatLessThan);
              return VisitFloat32Compare(this, value, cont);
            case ComparisonOp::Kind::kSignedLessThanOrEqual:
              cont->OverwriteAndNegateIfEqual(kFloatLessThanOrEqual);
              return VisitFloat32Compare(this, value, cont);
            default:
              UNREACHABLE();
          }
        case RegisterRepresentation::Float64():
          switch (comparison->kind) {
            case ComparisonOp::Kind::kEqual:
              cont->OverwriteAndNegateIfEqual(kEqual);
              return VisitFloat64Compare(this, value, cont);
            case ComparisonOp::Kind::kSignedLessThan:
              cont->OverwriteAndNegateIfEqual(kFloatLessThan);
              return VisitFloat64Compare(this, value, cont);
            case ComparisonOp::Kind::kSignedLessThanOrEqual:
              cont->OverwriteAndNegateIfEqual(kFloatLessThanOrEqual);
              return VisitFloat64Compare(this, value, cont);
            default:
              UNREACHABLE();
          }
        default:
          break;
      }
    } else if (const ProjectionOp* projection =
                   value_op.TryCast<ProjectionOp>()) {
      // Check if this is the overflow output projection of an
      // <Operation>WithOverflow node.
      if (projection->index == 1u) {
        // We cannot combine the <Operation>WithOverflow with this branch
        // unless the 0th projection (the use of the actual value of the
        // <Operation> is either nullptr, which means there's no use of the
        // actual value, or was already defined, which means it is scheduled
        // *AFTER* this branch).
        OpIndex node = projection->input();
        if (const OverflowCheckedBinopOp* binop =
                TryCast<OverflowCheckedBinopOp>(node);
            binop && CanDoBranchIfOverflowFusion(node)) {
          const bool is64 = binop->rep == WordRepresentation::Word64();
          if (is64) {
            UNREACHABLE();
          } else {
            switch (binop->kind) {
              case OverflowCheckedBinopOp::Kind::kSignedAdd:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int32BinopMatcher>(this, node, kRiscvAddOvf,
                                                     cont);
              case OverflowCheckedBinopOp::Kind::kSignedSub:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int32BinopMatcher>(this, node, kRiscvSubOvf,
                                                     cont);
              case OverflowCheckedBinopOp::Kind::kSignedMul:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop<Int32BinopMatcher>(this, node, kRiscvMulOvf32,
                                                     cont);
            }
          }
        }
      }
    }
  }

  // Continuation could not be combined with a compare, emit compare against
  // 0.
  EmitWordCompareZero(this, value, cont);
}

void InstructionSelectorT::VisitWord32Equal(OpIndex node) {
  const Operation& equal = Get(node);
  DCHECK(equal.Is<ComparisonOp>());
  OpIndex left = equal.input(0);
  OpIndex right = equal.input(1);
  OpIndex user = node;
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);

  if (MatchZero(right)) {
    return VisitWordCompareZero(user, left, &cont);
  }
  VisitWordCompare(this, node, &cont);
}

void InstructionSelectorT::VisitInt32LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWordCompare(this, node, &cont);
}

void InstructionSelectorT::VisitInt32LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, &cont);
}

void InstructionSelectorT::VisitUint32LessThan(OpIndex node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWordCompare(this, node, &cont);
}

void InstructionSelectorT::VisitUint32LessThanOrEqual(OpIndex node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWordCompare(this, node, &cont);
}

void InstructionSelectorT::VisitInt32AddWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid() && IsUsed(ovf.value())) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop<Int32BinopMatcher>(this, node, kRiscvAddOvf, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvAddOvf, &cont);
}

void InstructionSelectorT::VisitInt32SubWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid() && IsUsed(ovf.value())) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop<Int32BinopMatcher>(this, node, kRiscvSubOvf, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvSubOvf, &cont);
}

void InstructionSelectorT::VisitInt32MulWithOverflow(OpIndex node) {
  OptionalOpIndex ovf = FindProjection(node, 1);
  if (ovf.valid() && IsUsed(ovf.value())) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf.value());
    return VisitBinop<Int32BinopMatcher>(this, node, kRiscvMulOvf32, &cont);
  }
  FlagsContinuation cont;
  VisitBinop<Int32BinopMatcher>(this, node, kRiscvMulOvf32, &cont);
}

void InstructionSelectorT::VisitWord32AtomicLoad(OpIndex node) {
  auto load = this->load_view(node);
  ArchOpcode opcode;
  LoadRepresentation load_rep = load.loaded_rep();
  switch (load_rep.representation()) {
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsSigned() ? kAtomicLoadInt8 : kAtomicLoadUint8;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsSigned() ? kAtomicLoadInt16 : kAtomicLoadUint16;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:         // Fall through.
    case MachineRepresentation::kWord32:
      opcode = kAtomicLoadWord32;
      break;
    default:
      UNREACHABLE();
  }
  VisitAtomicLoad(this, node, opcode, AtomicWidth::kWord32);
}

void InstructionSelectorT::VisitWord32AtomicStore(OpIndex node) {
  auto store = this->store_view(node);
  AtomicStoreParameters store_params(store.stored_rep().representation(),
                                     store.stored_rep().write_barrier_kind(),
                                     store.memory_order().value(),
                                     store.access_kind());
  MachineRepresentation rep = store_params.representation();
  ArchOpcode opcode;
  switch (rep) {
    case MachineRepresentation::kWord8:
      opcode = kAtomicStoreWord8;
      break;
    case MachineRepresentation::kWord16:
      opcode = kAtomicStoreWord16;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kWord32:
      opcode = kAtomicStoreWord32;
      break;
    default:
      UNREACHABLE();
  }

    VisitAtomicStore(this, node, opcode, AtomicWidth::kWord32);
}

void VisitAtomicExchange(InstructionSelectorT* selector, OpIndex node,
                         ArchOpcode opcode, AtomicWidth width) {
  RiscvOperandGeneratorT g(selector);
  using OpIndex = OpIndex;
  const AtomicRMWOp& atomic_op = selector->Cast<AtomicRMWOp>(node);
  OpIndex base = atomic_op.base();
  OpIndex index = atomic_op.index();
  OpIndex value = atomic_op.value();

  AddressingMode addressing_mode = kMode_MRI;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);
  InstructionOperand outputs[1];
  outputs[0] = g.UseUniqueRegister(node);
  InstructionOperand temp[3];
  temp[0] = g.TempRegister();
  temp[1] = g.TempRegister();
  temp[2] = g.TempRegister();
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode) |
                         AtomicWidthField::encode(width);
  selector->Emit(code, 1, outputs, input_count, inputs, 3, temp);
}

void VisitAtomicCompareExchange(InstructionSelectorT* selector, OpIndex node,
                                ArchOpcode opcode, AtomicWidth width) {
  using OpIndex = OpIndex;
  RiscvOperandGeneratorT g(selector);
  const AtomicRMWOp& atomic_op = selector->Cast<AtomicRMWOp>(node);
  OpIndex base = atomic_op.base();
  OpIndex index = atomic_op.index();
  OpIndex old_value = atomic_op.expected().value();
  OpIndex new_value = atomic_op.value();

  AddressingMode addressing_mode = kMode_MRI;
  InstructionOperand inputs[4];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(old_value);
  inputs[input_count++] = g.UseUniqueRegister(new_value);
  InstructionOperand outputs[1];
  outputs[0] = g.UseUniqueRegister(node);
  InstructionOperand temp[3];
  temp[0] = g.TempRegister();
  temp[1] = g.TempRegister();
  temp[2] = g.TempRegister();
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode) |
                         AtomicWidthField::encode(width);
  selector->Emit(code, 1, outputs, input_count, inputs, 3, temp);
}

void InstructionSelectorT::VisitWord32AtomicExchange(OpIndex node) {
  ArchOpcode opcode;

    const AtomicRMWOp& atomic_op = this->Get(node).template Cast<AtomicRMWOp>();
    if (atomic_op.memory_rep == MemoryRepresentation::Int8()) {
      opcode = kAtomicExchangeInt8;
    } else if (atomic_op.memory_rep == MemoryRepresentation::Uint8()) {
      opcode = kAtomicExchangeUint8;
    } else if (atomic_op.memory_rep == MemoryRepresentation::Int16()) {
      opcode = kAtomicExchangeInt16;
    } else if (atomic_op.memory_rep == MemoryRepresentation::Uint16()) {
      opcode = kAtomicExchangeUint16;
    } else if (atomic_op.memory_rep == MemoryRepresentation::Int32() ||
               atomic_op.memory_rep == MemoryRepresentation::Uint32()) {
      opcode = kAtomicExchangeWord32;
    } else {
      UNREACHABLE();
    }

  VisitAtomicExchange(this, node, opcode, AtomicWidth::kWord32);
}

void InstructionSelectorT::VisitWord32AtomicCompareExchange(OpIndex node) {
  ArchOpcode opcode;

    const AtomicRMWOp& atomic_op = this->Get(node).template Cast<AtomicRMWOp>();
    if (atomic_op.memory_rep == MemoryRepresentation::Int8()) {
      opcode = kAtomicCompareExchangeInt8;
    } else if (atomic_op.memory_rep == MemoryRepresentation::Uint8()) {
      opcode = kAtomicCompareExchangeUint8;
    } else if (atomic_op.memory_rep == MemoryRepresentation::Int16()) {
      opcode = kAtomicCompareExchangeInt16;
    } else if (atomic_op.memory_rep == MemoryRepresentation::Uint16()) {
      opcode = kAtomicCompareExchangeUint16;
    } else if (atomic_op.memory_rep == MemoryRepresentation::Int32() ||
               atomic_op.memory_rep == MemoryRepresentation::Uint32()) {
      opcode = kAtomicCompareExchangeWord32;
    } else {
      UNREACHABLE();
    }

    VisitAtomicCompareExchange(this, node, opcode, AtomicWidth::kWord32);
}

void InstructionSelectorT::VisitWord32AtomicBinaryOperation(
    OpIndex node, ArchOpcode int8_op, ArchOpcode uint8_op, ArchOpcode int16_op,
    ArchOpcode uint16_op, ArchOpcode word32_op) {
  ArchOpcode opcode;

    const AtomicRMWOp& atomic_op = this->Get(node).template Cast<AtomicRMWOp>();
    if (atomic_op.memory_rep == MemoryRepresentation::Int8()) {
      opcode = int8_op;
    } else if (atomic_op.memory_rep == MemoryRepresentation::Uint8()) {
      opcode = uint8_op;
    } else if (atomic_op.memory_rep == MemoryRepresentation::Int16()) {
      opcode = int16_op;
    } else if (atomic_op.memory_rep == MemoryRepresentation::Uint16()) {
      opcode = uint16_op;
    } else if (atomic_op.memory_rep == MemoryRepresentation::Int32() ||
               atomic_op.memory_rep == MemoryRepresentation::Uint32()) {
      opcode = word32_op;
    } else {
      UNREACHABLE();
    }

    VisitAtomicBinop(this, node, opcode);
}

#define VISIT_ATOMIC_BINOP(op)                                           \
                                                                         \
  void InstructionSelectorT::VisitWord32Atomic##op(OpIndex node) {       \
    VisitWord32AtomicBinaryOperation(                                    \
        node, kAtomic##op##Int8, kAtomic##op##Uint8, kAtomic##op##Int16, \
        kAtomic##op##Uint16, kAtomic##op##Word32);                       \
  }
VISIT_ATOMIC_BINOP(Add)
VISIT_ATOMIC_BINOP(Sub)
VISIT_ATOMIC_BINOP(And)
VISIT_ATOMIC_BINOP(Or)
VISIT_ATOMIC_BINOP(Xor)
#undef VISIT_ATOMIC_BINOP

void InstructionSelectorT::VisitInt32AbsWithOverflow(OpIndex node) {
  UNREACHABLE();
}

void InstructionSelectorT::VisitInt64AbsWithOverflow(OpIndex node) {
  UNREACHABLE();
}

template <unsigned N>
static void VisitInt32PairBinop(InstructionSelectorT* selector,
                                InstructionCode pair_opcode,
                                InstructionCode single_opcode, OpIndex node) {
  static_assert(N == 3 || N == 4,
                "Pair operations can only have 3 or 4 inputs");

  RiscvOperandGeneratorT g(selector);
  OptionalOpIndex projection1 = selector->FindProjection(node, 1);

  if (projection1.valid()) {
    InstructionOperand outputs[] = {g.DefineAsRegister(node),
                                    g.DefineAsRegister(projection1.value())};

    if constexpr (N == 3) {
      // We use UseUniqueRegister here to avoid register sharing with the output
      // register.
      InstructionOperand inputs[] = {
          g.UseUniqueRegister(selector->input_at(node, 0)),
          g.UseUniqueRegister(selector->input_at(node, 1)),
          g.UseUniqueRegister(selector->input_at(node, 2))};

      selector->Emit(pair_opcode, 2, outputs, N, inputs);

    } else if constexpr (N == 4) {
      // We use UseUniqueRegister here to avoid register sharing with the output
      // register.
      InstructionOperand inputs[] = {
          g.UseUniqueRegister(selector->input_at(node, 0)),
          g.UseUniqueRegister(selector->input_at(node, 1)),
          g.UseUniqueRegister(selector->input_at(node, 2)),
          g.UseUniqueRegister(selector->input_at(node, 3))};

      selector->Emit(pair_opcode, 2, outputs, N, inputs);
    }

  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    selector->Emit(single_opcode, g.DefineSameAsFirst(node),
                   g.UseRegister(selector->input_at(node, 0)),
                   g.UseRegister(selector->input_at(node, 2)));
  }
}

void InstructionSelectorT::VisitInt32PairAdd(OpIndex node) {
  VisitInt32PairBinop<4>(this, kRiscvAddPair, kRiscvAdd32, node);
}

void InstructionSelectorT::VisitInt32PairSub(OpIndex node) {
  VisitInt32PairBinop<4>(this, kRiscvSubPair, kRiscvSub32, node);
}

void InstructionSelectorT::VisitInt32PairMul(OpIndex node) {
  VisitInt32PairBinop<4>(this, kRiscvMulPair, kRiscvMul32, node);
}

void InstructionSelectorT::VisitI64x2SplatI32Pair(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand low = g.UseRegister(this->input_at(node, 0));
  InstructionOperand high = g.UseRegister(this->input_at(node, 1));
  Emit(kRiscvI64x2SplatI32Pair, g.DefineAsRegister(node), low, high);
}

void InstructionSelectorT::VisitI64x2ReplaceLaneI32Pair(OpIndex node) {
  // In turboshaft it gets lowered to an I32x4ReplaceLane.
  UNREACHABLE();
}

// Shared routine for multiple shift operations.

static void VisitWord32PairShift(InstructionSelectorT* selector,
                                 InstructionCode opcode, OpIndex node) {
  RiscvOperandGeneratorT g(selector);
  InstructionOperand shift_operand;
  OpIndex shift_by = selector->input_at(node, 2);
  if (g.IsIntegerConstant(shift_by)) {
    shift_operand = g.UseImmediate(shift_by);
  } else {
    shift_operand = g.UseUniqueRegister(shift_by);
  }

  // We use UseUniqueRegister here to avoid register sharing with the output
  // register.
  InstructionOperand inputs[] = {
      g.UseUniqueRegister(selector->input_at(node, 0)),
      g.UseUniqueRegister(selector->input_at(node, 1)), shift_operand};

  OptionalOpIndex projection1 = selector->FindProjection(node, 1);

  InstructionOperand outputs[2];
  InstructionOperand temps[1];
  int32_t output_count = 0;
  int32_t temp_count = 0;

  outputs[output_count++] = g.DefineAsRegister(node);
  if (projection1.valid()) {
    outputs[output_count++] = g.DefineAsRegister(projection1.value());
  } else {
    temps[temp_count++] = g.TempRegister();
  }

  selector->Emit(opcode, output_count, outputs, 3, inputs, temp_count, temps);
}

void InstructionSelectorT::VisitWord32PairShl(OpIndex node) {
  VisitWord32PairShift(this, kRiscvShlPair, node);
}

void InstructionSelectorT::VisitWord32PairShr(OpIndex node) {
  VisitWord32PairShift(this, kRiscvShrPair, node);
}

void InstructionSelectorT::VisitWord32PairSar(OpIndex node) {
  VisitWord32PairShift(this, kRiscvSarPair, node);
}

void InstructionSelectorT::VisitWord32AtomicPairLoad(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  OpIndex base = this->input_at(node, 0);
  OpIndex index = this->input_at(node, 1);

  ArchOpcode opcode = kRiscvWord32AtomicPairLoad;
  AddressingMode addressing_mode = kMode_MRI;
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index)};
  InstructionOperand temps[3];
  size_t temp_count = 0;
  temps[temp_count++] = g.TempRegister(t0);
  InstructionOperand outputs[2];
  size_t output_count = 0;

  OptionalOpIndex projection0 = this->FindProjection(node, 0);
  OptionalOpIndex projection1 = this->FindProjection(node, 1);
  if (projection0.valid()) {
    outputs[output_count++] = g.DefineAsFixed(projection0.value(), a0);
  } else {
    temps[temp_count++] = g.TempRegister(a0);
  }
  if (projection1.valid()) {
    outputs[output_count++] = g.DefineAsFixed(projection1.value(), a1);
  } else {
    temps[temp_count++] = g.TempRegister(a1);
  }
  Emit(code, output_count, outputs, arraysize(inputs), inputs, temp_count,
       temps);
}

void InstructionSelectorT::VisitWord32AtomicPairStore(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  const AtomicWord32PairOp& store = Cast<AtomicWord32PairOp>(node);

  OpIndex base = store.base();
  OpIndex index = store.index().value();
  OpIndex value_low = store.value_low().value();
  OpIndex value_high = store.value_high().value();

  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index),
                                 g.UseFixed(value_low, a1),
                                 g.UseFixed(value_high, a2)};
  InstructionOperand temps[] = {g.TempRegister(a0), g.TempRegister(),
                                g.TempRegister()};
  Emit(kRiscvWord32AtomicPairStore | AddressingModeField::encode(kMode_MRI), 0,
       nullptr, arraysize(inputs), inputs, arraysize(temps), temps);
}

void VisitPairAtomicBinop(InstructionSelectorT* selector, OpIndex node,
                          ArchOpcode opcode) {
  using OpIndex = OpIndex;
  RiscvOperandGeneratorT g(selector);
  OpIndex base = selector->input_at(node, 0);
  OpIndex index = selector->input_at(node, 1);
  OpIndex value = selector->input_at(node, 2);
  OpIndex value_high = selector->input_at(node, 3);

  AddressingMode addressing_mode = kMode_None;
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  InstructionOperand inputs[] = {g.UseRegister(base), g.UseRegister(index),
                                 g.UseFixed(value, a1),
                                 g.UseFixed(value_high, a2)};
  InstructionOperand outputs[2];
  size_t output_count = 0;
  InstructionOperand temps[3];
  size_t temp_count = 0;
  temps[temp_count++] = g.TempRegister(t0);

  OptionalOpIndex projection0 = selector->FindProjection(node, 0);
  OptionalOpIndex projection1 = selector->FindProjection(node, 1);
  if (projection0.valid()) {
    outputs[output_count++] = g.DefineAsFixed(projection0.value(), a0);
  } else {
    temps[temp_count++] = g.TempRegister(a0);
  }
  if (projection1.valid()) {
    outputs[output_count++] = g.DefineAsFixed(projection1.value(), a1);
  } else {
    temps[temp_count++] = g.TempRegister(a1);
  }
  selector->Emit(code, output_count, outputs, arraysize(inputs), inputs,
                 temp_count, temps);
}

void InstructionSelectorT::VisitWord32AtomicPairAdd(OpIndex node) {
  VisitPairAtomicBinop(this, node, kRiscvWord32AtomicPairAdd);
}

void InstructionSelectorT::VisitWord32AtomicPairSub(OpIndex node) {
  VisitPairAtomicBinop(this, node, kRiscvWord32AtomicPairSub);
}

void InstructionSelectorT::VisitWord32AtomicPairAnd(OpIndex node) {
  VisitPairAtomicBinop(this, node, kRiscvWord32AtomicPairAnd);
}

void InstructionSelectorT::VisitWord32AtomicPairOr(OpIndex node) {
  VisitPairAtomicBinop(this, node, kRiscvWord32AtomicPairOr);
}

void InstructionSelectorT::VisitWord32AtomicPairXor(OpIndex node) {
  VisitPairAtomicBinop(this, node, kRiscvWord32AtomicPairXor);
}

void InstructionSelectorT::VisitWord32AtomicPairExchange(OpIndex node) {
  VisitPairAtomicBinop(this, node, kRiscvWord32AtomicPairExchange);
}

void InstructionSelectorT::VisitWord32AtomicPairCompareExchange(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  // In the Turbofan and the Turboshaft graph the order of expected and value is
  // swapped.
  const size_t expected_offset = 4;
  const size_t value_offset = 2;
  InstructionOperand inputs[] = {
      g.UseRegister(this->input_at(node, 0)),
      g.UseRegister(this->input_at(node, 1)),
      g.UseFixed(this->input_at(node, expected_offset), a1),
      g.UseFixed(this->input_at(node, expected_offset + 1), a2),
      g.UseFixed(this->input_at(node, value_offset), a3),
      g.UseFixed(this->input_at(node, value_offset + 1), a4)};

  InstructionCode code = kRiscvWord32AtomicPairCompareExchange |
                         AddressingModeField::encode(kMode_MRI);
  OptionalOpIndex projection0 = this->FindProjection(node, 0);
  OptionalOpIndex projection1 = this->FindProjection(node, 1);
  InstructionOperand outputs[2];
  size_t output_count = 0;
  InstructionOperand temps[3];
  size_t temp_count = 0;
  temps[temp_count++] = g.TempRegister(t0);
  if (projection0.valid()) {
    outputs[output_count++] = g.DefineAsFixed(projection0.value(), a0);
  } else {
    temps[temp_count++] = g.TempRegister(a0);
  }
  if (projection1.valid()) {
    outputs[output_count++] = g.DefineAsFixed(projection1.value(), a1);
  } else {
    temps[temp_count++] = g.TempRegister(a1);
  }
  Emit(code, output_count, outputs, arraysize(inputs), inputs, temp_count,
       temps);
}

void InstructionSelectorT::VisitF64x2Min(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  InstructionOperand mask_reg = g.TempFpRegister(v0);
  InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);
  const int32_t kNaN = 0x7ff80000L, kNaNShift = 32;
  this->Emit(kRiscvVmfeqVv, temp1, g.UseRegister(this->input_at(node, 0)),
             g.UseRegister(this->input_at(node, 0)), g.UseImmediate(E64),
             g.UseImmediate(m1));
  this->Emit(kRiscvVmfeqVv, temp2, g.UseRegister(this->input_at(node, 1)),
             g.UseRegister(this->input_at(node, 1)), g.UseImmediate(E64),
             g.UseImmediate(m1));
  this->Emit(kRiscvVandVv, mask_reg, temp2, temp1, g.UseImmediate(E64),
             g.UseImmediate(m1));

  InstructionOperand temp3 = g.TempFpRegister(kSimd128ScratchReg);
  InstructionOperand temp4 = g.TempFpRegister(kSimd128ScratchReg);
  InstructionOperand temp5 = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, temp3, g.UseImmediate(kNaN), g.UseImmediate(E64),
             g.UseImmediate(m1));
  this->Emit(kRiscvVsll, temp4, temp3, g.UseImmediate(kNaNShift),
             g.UseImmediate(E64), g.UseImmediate(m1));
  this->Emit(kRiscvVfminVv, temp5, g.UseRegister(this->input_at(node, 1)),
             g.UseRegister(this->input_at(node, 0)), g.UseImmediate(E64),
             g.UseImmediate(m1), g.UseImmediate(Mask));
  this->Emit(kRiscvVmv, g.DefineAsRegister(node), temp5, g.UseImmediate(E64),
             g.UseImmediate(m1));
}

void InstructionSelectorT::VisitF64x2Max(OpIndex node) {
  RiscvOperandGeneratorT g(this);
  InstructionOperand temp1 = g.TempFpRegister(v0);
  InstructionOperand mask_reg = g.TempFpRegister(v0);
  InstructionOperand temp2 = g.TempFpRegister(kSimd128ScratchReg);
  const int32_t kNaN = 0x7ff80000L, kNaNShift = 32;
  this->Emit(kRiscvVmfeqVv, temp1, g.UseRegister(this->input_at(node, 0)),
             g.UseRegister(this->input_at(node, 0)), g.UseImmediate(E64),
             g.UseImmediate(m1));
  this->Emit(kRiscvVmfeqVv, temp2, g.UseRegister(this->input_at(node, 1)),
             g.UseRegister(this->input_at(node, 1)), g.UseImmediate(E64),
             g.UseImmediate(m1));
  this->Emit(kRiscvVandVv, mask_reg, temp2, temp1, g.UseImmediate(E64),
             g.UseImmediate(m1));

  InstructionOperand temp3 = g.TempFpRegister(kSimd128ScratchReg);
  InstructionOperand temp4 = g.TempFpRegister(kSimd128ScratchReg);
  InstructionOperand temp5 = g.TempFpRegister(kSimd128ScratchReg);
  this->Emit(kRiscvVmv, temp3, g.UseImmediate(kNaN), g.UseImmediate(E64),
             g.UseImmediate(m1));
  this->Emit(kRiscvVsll, temp4, temp3, g.UseImmediate(kNaNShift),
             g.UseImmediate(E64), g.UseImmediate(m1));
  this->Emit(kRiscvVfmaxVv, temp5, g.UseRegister(this->input_at(node, 1)),
             g.UseRegister(this->input_at(node, 0)), g.UseImmediate(E64),
             g.UseImmediate(m1), g.UseImmediate(Mask));
  this->Emit(kRiscvVmv, g.DefineAsRegister(node), temp5, g.UseImmediate(E64),
             g.UseImmediate(m1));
}
// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags = MachineOperatorBuilder::kNoFlags;
  flags |= MachineOperatorBuilder::kInt32DivIsSafe |
           MachineOperatorBuilder::kUint32DivIsSafe |
           MachineOperatorBuilder::kFloat32RoundDown |
           MachineOperatorBuilder::kFloat32RoundUp |
           MachineOperatorBuilder::kFloat32RoundTruncate |
           MachineOperatorBuilder::kFloat32RoundTiesEven;
  if (CpuFeatures::IsSupported(ZBB)) {
    flags |= MachineOperatorBuilder::kWord32Ctz |
             MachineOperatorBuilder::kWord32Popcnt;
  }
  return flags;
}

#undef TRACE
}  // namespace compiler
}  // namespace internal
}  // namespace v8
