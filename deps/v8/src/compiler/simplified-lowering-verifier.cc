// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-lowering-verifier.h"

#include "src/compiler/operation-typer.h"
#include "src/compiler/type-cache.h"

namespace v8 {
namespace internal {
namespace compiler {

Truncation LeastGeneralTruncation(const Truncation& t1, const Truncation& t2) {
  if (t1.IsLessGeneralThan(t2)) return t1;
  CHECK(t2.IsLessGeneralThan(t1));
  return t2;
}

Truncation LeastGeneralTruncation(const Truncation& t1, const Truncation& t2,
                                  const Truncation& t3) {
  return LeastGeneralTruncation(LeastGeneralTruncation(t1, t2), t3);
}

bool IsNonTruncatingMachineTypeFor(const MachineType& mt, const Type& type) {
  if (type.IsNone()) return true;
  // TODO(nicohartmann@): Add more cases here.
  if (type.Is(Type::BigInt())) {
    return mt.representation() == MachineRepresentation::kTaggedPointer ||
           mt.representation() == MachineRepresentation::kTagged;
  }
  switch (mt.representation()) {
    case MachineRepresentation::kBit:
      CHECK(mt.semantic() == MachineSemantic::kBool ||
            mt.semantic() == MachineSemantic::kAny);
      return type.Is(Type::Boolean());
    default:
      return true;
  }
}

void SimplifiedLoweringVerifier::CheckType(Node* node, const Type& type) {
  CHECK(NodeProperties::IsTyped(node));
  Type node_type = NodeProperties::GetType(node);
  if (!type.Is(node_type)) {
    std::ostringstream type_str;
    type.PrintTo(type_str);
    std::ostringstream node_type_str;
    node_type.PrintTo(node_type_str);

    FATAL(
        "SimplifiedLoweringVerifierError: verified type %s of node #%d:%s "
        "does not match with type %s assigned during lowering",
        type_str.str().c_str(), node->id(), node->op()->mnemonic(),
        node_type_str.str().c_str());
  }
}

void SimplifiedLoweringVerifier::CheckAndSet(Node* node, const Type& type,
                                             const Truncation& trunc) {
  DCHECK(!type.IsInvalid());

  if (NodeProperties::IsTyped(node)) {
    CheckType(node, type);
  } else {
    // We store the type inferred by the verification pass. We do not update
    // the node's type directly, because following phases might encounter
    // unsound types as long as the verification is not complete.
    SetType(node, type);
  }
  SetTruncation(node, GeneralizeTruncation(trunc, type));
}

bool IsModuloTruncation(const Truncation& truncation) {
  return truncation.IsUsedAsWord32() || truncation.IsUsedAsWord64() ||
         Truncation::Any().IsLessGeneralThan(truncation);
}

Truncation SimplifiedLoweringVerifier::GeneralizeTruncation(
    const Truncation& truncation, const Type& type) const {
  IdentifyZeros identify_zeros = truncation.identify_zeros();
  if (!type.Maybe(Type::MinusZero())) {
    identify_zeros = IdentifyZeros::kDistinguishZeros;
  }

  switch (truncation.kind()) {
    case Truncation::TruncationKind::kAny: {
      return Truncation::Any(identify_zeros);
    }
    case Truncation::TruncationKind::kBool: {
      if (type.Is(Type::Boolean())) return Truncation::Any();
      return Truncation(Truncation::TruncationKind::kBool, identify_zeros);
    }
    case Truncation::TruncationKind::kWord32: {
      if (type.Is(Type::Signed32OrMinusZero()) ||
          type.Is(Type::Unsigned32OrMinusZero())) {
        return Truncation::Any(identify_zeros);
      }
      return Truncation(Truncation::TruncationKind::kWord32, identify_zeros);
    }
    case Truncation::TruncationKind::kWord64: {
      if (type.Is(Type::BigInt())) {
        DCHECK_EQ(identify_zeros, IdentifyZeros::kDistinguishZeros);
        if (type.Is(Type::SignedBigInt64()) ||
            type.Is(Type::UnsignedBigInt64())) {
          return Truncation::Any(IdentifyZeros::kDistinguishZeros);
        }
      } else if (type.Is(TypeCache::Get()->kSafeIntegerOrMinusZero)) {
        return Truncation::Any(identify_zeros);
      }
      return Truncation(Truncation::TruncationKind::kWord64, identify_zeros);
    }

    default:
      // TODO(nicohartmann): Support remaining truncations.
      UNREACHABLE();
  }
}

void SimplifiedLoweringVerifier::VisitNode(Node* node,
                                           OperationTyper& op_typer) {
  switch (node->opcode()) {
    case IrOpcode::kStart:
    case IrOpcode::kIfTrue:
    case IrOpcode::kIfFalse:
    case IrOpcode::kMerge:
    case IrOpcode::kEnd:
    case IrOpcode::kEffectPhi:
    case IrOpcode::kCheckpoint:
    case IrOpcode::kFrameState:
    case IrOpcode::kJSStackCheck:
      break;
    case IrOpcode::kInt32Constant:
    case IrOpcode::kInt64Constant:
    case IrOpcode::kFloat64Constant: {
      // Constants might be untyped, because they are cached in the graph and
      // used in different contexts such that no single type can be assigned.
      // Their type is provided by an introduced TypeGuard where necessary.
      break;
    }
    case IrOpcode::kHeapConstant:
      break;
    case IrOpcode::kCheckedFloat64ToInt32: {
      Type input_type = InputType(node, 0);
      DCHECK(input_type.Is(Type::Number()));

      const auto& p = CheckMinusZeroParametersOf(node->op());
      if (p.mode() == CheckForMinusZeroMode::kCheckForMinusZero) {
        // Remove -0 from input_type.
        input_type =
            Type::Intersect(input_type, Type::Signed32(), graph_zone());
      } else {
        input_type = Type::Intersect(input_type, Type::Signed32OrMinusZero(),
                                     graph_zone());
      }
      CheckAndSet(node, input_type, Truncation::Word32());
      break;
    }
    case IrOpcode::kCheckedTaggedToTaggedSigned: {
      Type input_type = InputType(node, 0);
      Type output_type =
          Type::Intersect(input_type, Type::SignedSmall(), graph_zone());
      Truncation output_trunc = InputTruncation(node, 0);
      CheckAndSet(node, output_type, output_trunc);
      break;
    }
    case IrOpcode::kCheckedTaggedToTaggedPointer:
      CheckAndSet(node, InputType(node, 0), InputTruncation(node, 0));
      break;
    case IrOpcode::kTruncateTaggedToBit: {
      Type input_type = InputType(node, 0);
      Truncation input_trunc = InputTruncation(node, 0);
      // Cannot have other truncation here, because identified values lead to
      // different results when converted to bit.
      CHECK(input_trunc == Truncation::Bool() ||
            input_trunc == Truncation::Any());
      Type output_type = op_typer.ToBoolean(input_type);
      CheckAndSet(node, output_type, Truncation::Bool());
      break;
    }
    case IrOpcode::kInt32Add: {
      Type output_type =
          op_typer.NumberAdd(InputType(node, 0), InputType(node, 1));
      Truncation output_trunc = LeastGeneralTruncation(InputTruncation(node, 0),
                                                       InputTruncation(node, 1),
                                                       Truncation::Word32());
      CHECK(IsModuloTruncation(output_trunc));
      CheckAndSet(node, output_type, output_trunc);
      break;
    }
    case IrOpcode::kInt32Sub: {
      Type output_type =
          op_typer.NumberSubtract(InputType(node, 0), InputType(node, 1));
      Truncation output_trunc = LeastGeneralTruncation(InputTruncation(node, 0),
                                                       InputTruncation(node, 1),
                                                       Truncation::Word32());
      CHECK(IsModuloTruncation(output_trunc));
      CheckAndSet(node, output_type, output_trunc);
      break;
    }
    case IrOpcode::kChangeInt31ToTaggedSigned:
    case IrOpcode::kChangeInt32ToTagged:
    case IrOpcode::kChangeFloat32ToFloat64:
    case IrOpcode::kChangeInt32ToInt64:
    case IrOpcode::kChangeUint32ToUint64:
    case IrOpcode::kChangeUint64ToTagged: {
      // These change operators do not truncate any values and can simply
      // forward input type and truncation.
      CheckAndSet(node, InputType(node, 0), InputTruncation(node, 0));
      break;
    }
    case IrOpcode::kChangeFloat64ToInt64: {
      Truncation output_trunc = LeastGeneralTruncation(InputTruncation(node, 0),
                                                       Truncation::Word64());
      CheckAndSet(node, InputType(node, 0), output_trunc);
      break;
    }
    case IrOpcode::kInt64Add: {
      Type left_type = InputType(node, 0);
      Type right_type = InputType(node, 1);

      Type output_type;
      if (left_type.Is(Type::BigInt()) && right_type.Is(Type::BigInt())) {
        // BigInt x BigInt -> BigInt
        output_type = op_typer.BigIntAdd(left_type, right_type);
      } else if (left_type.Is(Type::Number()) &&
                 right_type.Is(Type::Number())) {
        // Number x Number -> Number
        output_type = op_typer.NumberAdd(left_type, right_type);
      } else {
        // Invalid type combination.
        std::ostringstream left_str, right_str;
        left_type.PrintTo(left_str);
        right_type.PrintTo(right_str);
        FATAL(
            "SimplifiedLoweringVerifierError: invalid combination of input "
            "types "
            "%s and %s for node #%d:%s",
            left_str.str().c_str(), right_str.str().c_str(), node->id(),
            node->op()->mnemonic());
      }

      Truncation output_trunc = LeastGeneralTruncation(InputTruncation(node, 0),
                                                       InputTruncation(node, 1),
                                                       Truncation::Word64());
      CHECK(IsModuloTruncation(output_trunc));
      CheckAndSet(node, output_type, output_trunc);
      break;
    }
    case IrOpcode::kDeadValue: {
      CheckAndSet(node, Type::None(), Truncation::Any());
      break;
    }
    case IrOpcode::kTypeGuard: {
      Type output_type = op_typer.TypeTypeGuard(node->op(), InputType(node, 0));
      // TypeGuard has no effect on trunction, but the restricted type may help
      // generalize it.
      CheckAndSet(node, output_type, InputTruncation(node, 0));
      break;
    }
    case IrOpcode::kTruncateBigIntToWord64: {
      Type input_type = InputType(node, 0);
      CHECK(input_type.Is(Type::BigInt()));
      CHECK(Truncation::Word64().IsLessGeneralThan(InputTruncation(node, 0)));
      CheckAndSet(node, input_type, Truncation::Word64());
      break;
    }
    case IrOpcode::kChangeTaggedSignedToInt64: {
      Type input_type = InputType(node, 0);
      CHECK(input_type.Is(Type::Number()));
      Truncation output_trunc = LeastGeneralTruncation(InputTruncation(node, 0),
                                                       Truncation::Word64());
      CheckAndSet(node, input_type, output_trunc);
      break;
    }
    case IrOpcode::kCheckBigInt: {
      Type input_type = InputType(node, 0);
      input_type = Type::Intersect(input_type, Type::BigInt(), graph_zone());
      CheckAndSet(node, input_type, InputTruncation(node, 0));
      break;
    }
    case IrOpcode::kReturn: {
      const int return_value_count = ValueInputCountOfReturn(node->op());
      for (int i = 0; i < return_value_count; ++i) {
        Type input_type = InputType(node, 1 + i);
        Truncation input_trunc = InputTruncation(node, 1 + i);
        input_trunc = GeneralizeTruncation(input_trunc, input_type);
        // No values must be lost due to truncation.
        CHECK_EQ(input_trunc, Truncation::Any());
      }
      break;
    }
    case IrOpcode::kSLVerifierHint: {
      Type output_type = InputType(node, 0);
      Truncation output_trunc = InputTruncation(node, 0);
      const auto& p = SLVerifierHintParametersOf(node->op());

      if (const Operator* semantics = p.semantics()) {
        switch (semantics->opcode()) {
          case IrOpcode::kPlainPrimitiveToNumber:
            output_type = op_typer.ToNumber(output_type);
            break;
          default:
            UNREACHABLE();
        }
        CheckType(node, output_type);
      }

      if (p.override_output_type()) {
        output_type = *p.override_output_type();
      }

      SetType(node, output_type);
      SetTruncation(node, GeneralizeTruncation(output_trunc, output_type));
      break;
    }
    case IrOpcode::kBranch: {
      CHECK(InputType(node, 0).Is(Type::Boolean()));
      CHECK_EQ(InputTruncation(node, 0), Truncation::Any());
      break;
    }
    case IrOpcode::kTypedStateValues: {
      const ZoneVector<MachineType>* machine_types = MachineTypesOf(node->op());
      for (int i = 0; i < static_cast<int>(machine_types->size()); ++i) {
        // Inputs must not be truncated.
        CHECK_EQ(InputTruncation(node, i), Truncation::Any());
        CHECK(IsNonTruncatingMachineTypeFor(machine_types->at(i),
                                            InputType(node, i)));
      }
      break;
    }
    case IrOpcode::kParameter: {
      CHECK(NodeProperties::IsTyped(node));
      SetTruncation(node, Truncation::Any());
      break;
    }

#define CASE(code, ...) case IrOpcode::k##code:
      // Control operators
      CASE(Loop)
      CASE(Switch)
      CASE(IfSuccess)
      CASE(IfException)
      CASE(IfValue)
      CASE(IfDefault)
      CASE(Deoptimize)
      CASE(DeoptimizeIf)
      CASE(DeoptimizeUnless)
      CASE(TrapIf)
      CASE(TrapUnless)
      CASE(TailCall)
      CASE(Terminate)
      CASE(Throw)
      CASE(TraceInstruction)
      // Constant operators
      CASE(TaggedIndexConstant)
      CASE(Float32Constant)
      CASE(ExternalConstant)
      CASE(NumberConstant)
      CASE(PointerConstant)
      CASE(CompressedHeapConstant)
      CASE(RelocatableInt32Constant)
      CASE(RelocatableInt64Constant)
      // Inner operators
      CASE(Select)
      CASE(Phi)
      CASE(InductionVariablePhi)
      CASE(BeginRegion)
      CASE(FinishRegion)
      CASE(StateValues)
      CASE(ArgumentsElementsState)
      CASE(ArgumentsLengthState)
      CASE(ObjectState)
      CASE(ObjectId)
      CASE(TypedObjectState)
      CASE(Call)
      CASE(OsrValue)
      CASE(LoopExit)
      CASE(LoopExitValue)
      CASE(LoopExitEffect)
      CASE(Projection)
      CASE(Retain)
      CASE(MapGuard)
      CASE(FoldConstant)
      CASE(Unreachable)
      CASE(Dead)
      CASE(Plug)
      CASE(StaticAssert)
      // Simplified change operators
      CASE(ChangeTaggedSignedToInt32)
      CASE(ChangeTaggedToInt32)
      CASE(ChangeTaggedToInt64)
      CASE(ChangeTaggedToUint32)
      CASE(ChangeTaggedToFloat64)
      CASE(ChangeTaggedToTaggedSigned)
      CASE(ChangeInt64ToTagged)
      CASE(ChangeUint32ToTagged)
      CASE(ChangeFloat64ToTagged)
      CASE(ChangeFloat64ToTaggedPointer)
      CASE(ChangeTaggedToBit)
      CASE(ChangeBitToTagged)
      CASE(ChangeInt64ToBigInt)
      CASE(ChangeUint64ToBigInt)
      CASE(TruncateTaggedToWord32)
      CASE(TruncateTaggedToFloat64)
      CASE(TruncateTaggedPointerToBit)
      // Simplified checked operators
      CASE(CheckedInt32Add)
      CASE(CheckedInt32Sub)
      CASE(CheckedInt32Div)
      CASE(CheckedInt32Mod)
      CASE(CheckedUint32Div)
      CASE(CheckedUint32Mod)
      CASE(CheckedInt32Mul)
      CASE(CheckedInt32ToTaggedSigned)
      CASE(CheckedInt64ToInt32)
      CASE(CheckedInt64ToTaggedSigned)
      CASE(CheckedUint32Bounds)
      CASE(CheckedUint32ToInt32)
      CASE(CheckedUint32ToTaggedSigned)
      CASE(CheckedUint64Bounds)
      CASE(CheckedUint64ToInt32)
      CASE(CheckedUint64ToTaggedSigned)
      CASE(CheckedFloat64ToInt64)
      CASE(CheckedTaggedSignedToInt32)
      CASE(CheckedTaggedToInt32)
      CASE(CheckedTaggedToArrayIndex)
      CASE(CheckedTruncateTaggedToWord32)
      CASE(CheckedTaggedToFloat64)
      CASE(CheckedTaggedToInt64)
      SIMPLIFIED_COMPARE_BINOP_LIST(CASE)
      SIMPLIFIED_NUMBER_BINOP_LIST(CASE)
      SIMPLIFIED_BIGINT_BINOP_LIST(CASE)
      SIMPLIFIED_SPECULATIVE_NUMBER_BINOP_LIST(CASE)
      SIMPLIFIED_NUMBER_UNOP_LIST(CASE)
      // Simplified unary bigint operators
      CASE(BigIntNegate)
      SIMPLIFIED_SPECULATIVE_NUMBER_UNOP_LIST(CASE)
      SIMPLIFIED_SPECULATIVE_BIGINT_UNOP_LIST(CASE)
      SIMPLIFIED_SPECULATIVE_BIGINT_BINOP_LIST(CASE)
      SIMPLIFIED_OTHER_OP_LIST(CASE)
      MACHINE_COMPARE_BINOP_LIST(CASE)
      MACHINE_UNOP_32_LIST(CASE)
      // Binary 32bit machine operators
      CASE(Word32And)
      CASE(Word32Or)
      CASE(Word32Xor)
      CASE(Word32Shl)
      CASE(Word32Shr)
      CASE(Word32Sar)
      CASE(Word32Rol)
      CASE(Word32Ror)
      CASE(Int32AddWithOverflow)
      CASE(Int32SubWithOverflow)
      CASE(Int32Mul)
      CASE(Int32MulWithOverflow)
      CASE(Int32MulHigh)
      CASE(Int32Div)
      CASE(Int32Mod)
      CASE(Uint32Div)
      CASE(Uint32Mod)
      CASE(Uint32MulHigh)
      // Binary 64bit machine operators
      CASE(Word64And)
      CASE(Word64Or)
      CASE(Word64Xor)
      CASE(Word64Shl)
      CASE(Word64Shr)
      CASE(Word64Sar)
      CASE(Word64Rol)
      CASE(Word64Ror)
      CASE(Word64RolLowerable)
      CASE(Word64RorLowerable)
      CASE(Int64AddWithOverflow)
      CASE(Int64Sub)
      CASE(Int64SubWithOverflow)
      CASE(Int64Mul)
      CASE(Int64Div)
      CASE(Int64Mod)
      CASE(Uint64Div)
      CASE(Uint64Mod)
      MACHINE_FLOAT32_UNOP_LIST(CASE)
      MACHINE_FLOAT32_BINOP_LIST(CASE)
      MACHINE_FLOAT64_UNOP_LIST(CASE)
      MACHINE_FLOAT64_BINOP_LIST(CASE)
      MACHINE_ATOMIC_OP_LIST(CASE)
      CASE(AbortCSADcheck)
      CASE(DebugBreak)
      CASE(Comment)
      CASE(Load)
      CASE(LoadImmutable)
      CASE(Store)
      CASE(StackSlot)
      CASE(Word32Popcnt)
      CASE(Word64Popcnt)
      CASE(Word64Clz)
      CASE(Word64Ctz)
      CASE(Word64ClzLowerable)
      CASE(Word64CtzLowerable)
      CASE(Word64ReverseBits)
      CASE(Word64ReverseBytes)
      CASE(Simd128ReverseBytes)
      CASE(Int64AbsWithOverflow)
      CASE(BitcastTaggedToWord)
      CASE(BitcastTaggedToWordForTagAndSmiBits)
      CASE(BitcastWordToTagged)
      CASE(BitcastWordToTaggedSigned)
      CASE(TruncateFloat64ToWord32)
      CASE(ChangeFloat64ToInt32)
      CASE(ChangeFloat64ToUint32)
      CASE(ChangeFloat64ToUint64)
      CASE(Float64SilenceNaN)
      CASE(TruncateFloat64ToInt64)
      CASE(TruncateFloat64ToUint32)
      CASE(TruncateFloat32ToInt32)
      CASE(TruncateFloat32ToUint32)
      CASE(TryTruncateFloat32ToInt64)
      CASE(TryTruncateFloat64ToInt64)
      CASE(TryTruncateFloat32ToUint64)
      CASE(TryTruncateFloat64ToUint64)
      CASE(TryTruncateFloat64ToInt32)
      CASE(TryTruncateFloat64ToUint32)
      CASE(ChangeInt32ToFloat64)
      CASE(BitcastWord32ToWord64)
      CASE(ChangeInt64ToFloat64)
      CASE(ChangeUint32ToFloat64)
      CASE(TruncateFloat64ToFloat32)
      CASE(TruncateInt64ToInt32)
      CASE(RoundFloat64ToInt32)
      CASE(RoundInt32ToFloat32)
      CASE(RoundInt64ToFloat32)
      CASE(RoundInt64ToFloat64)
      CASE(RoundUint32ToFloat32)
      CASE(RoundUint64ToFloat32)
      CASE(RoundUint64ToFloat64)
      CASE(BitcastFloat32ToInt32)
      CASE(BitcastFloat64ToInt64)
      CASE(BitcastInt32ToFloat32)
      CASE(BitcastInt64ToFloat64)
      CASE(Float64ExtractLowWord32)
      CASE(Float64ExtractHighWord32)
      CASE(Float64InsertLowWord32)
      CASE(Float64InsertHighWord32)
      CASE(Word32Select)
      CASE(Word64Select)
      CASE(Float32Select)
      CASE(Float64Select)
      CASE(LoadStackCheckOffset)
      CASE(LoadFramePointer)
      CASE(LoadParentFramePointer)
      CASE(UnalignedLoad)
      CASE(UnalignedStore)
      CASE(Int32PairAdd)
      CASE(Int32PairSub)
      CASE(Int32PairMul)
      CASE(Word32PairShl)
      CASE(Word32PairShr)
      CASE(Word32PairSar)
      CASE(ProtectedLoad)
      CASE(ProtectedStore)
      CASE(MemoryBarrier)
      CASE(SignExtendWord8ToInt32)
      CASE(SignExtendWord16ToInt32)
      CASE(SignExtendWord8ToInt64)
      CASE(SignExtendWord16ToInt64)
      CASE(SignExtendWord32ToInt64)
      CASE(StackPointerGreaterThan)
      JS_SIMPLE_BINOP_LIST(CASE)
      JS_SIMPLE_UNOP_LIST(CASE)
      JS_OBJECT_OP_LIST(CASE)
      JS_CONTEXT_OP_LIST(CASE)
      JS_CALL_OP_LIST(CASE)
      JS_CONSTRUCT_OP_LIST(CASE)
      CASE(JSAsyncFunctionEnter)
      CASE(JSAsyncFunctionReject)
      CASE(JSAsyncFunctionResolve)
      CASE(JSCallRuntime)
      CASE(JSForInEnumerate)
      CASE(JSForInNext)
      CASE(JSForInPrepare)
      CASE(JSGetIterator)
      CASE(JSLoadMessage)
      CASE(JSStoreMessage)
      CASE(JSLoadModule)
      CASE(JSStoreModule)
      CASE(JSGetImportMeta)
      CASE(JSGeneratorStore)
      CASE(JSGeneratorRestoreContinuation)
      CASE(JSGeneratorRestoreContext)
      CASE(JSGeneratorRestoreRegister)
      CASE(JSGeneratorRestoreInputOrDebugPos)
      CASE(JSFulfillPromise)
      CASE(JSPerformPromiseThen)
      CASE(JSPromiseResolve)
      CASE(JSRejectPromise)
      CASE(JSResolvePromise)
      CASE(JSObjectIsArray)
      CASE(JSRegExpTest)
      CASE(JSDebugger) {
        // TODO(nicohartmann@): These operators might need to be supported.
        break;
      }
      MACHINE_SIMD_OP_LIST(CASE)
      IF_WASM(SIMPLIFIED_WASM_OP_LIST, CASE) {
        // SIMD operators should not be in the graph, yet.
        UNREACHABLE();
      }
#undef CASE
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
