// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/representation-change.h"

#include <sstream>

#include "src/base/bits.h"
#include "src/code-factory.h"
#include "src/compiler/machine-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

const char* Truncation::description() const {
  switch (kind()) {
    case TruncationKind::kNone:
      return "no-value-use";
    case TruncationKind::kBool:
      return "truncate-to-bool";
    case TruncationKind::kWord32:
      return "truncate-to-word32";
    case TruncationKind::kWord64:
      return "truncate-to-word64";
    case TruncationKind::kFloat32:
      return "truncate-to-float32";
    case TruncationKind::kFloat64:
      return "truncate-to-float64";
    case TruncationKind::kAny:
      return "no-truncation";
  }
  UNREACHABLE();
  return nullptr;
}


// Partial order for truncations:
//
//  kWord64       kAny
//     ^            ^
//     \            |
//      \         kFloat64  <--+
//       \        ^    ^       |
//        \       /    |       |
//         kWord32  kFloat32  kBool
//               ^     ^      ^
//               \     |      /
//                \    |     /
//                 \   |    /
//                  \  |   /
//                   \ |  /
//                   kNone

// static
Truncation::TruncationKind Truncation::Generalize(TruncationKind rep1,
                                                  TruncationKind rep2) {
  if (LessGeneral(rep1, rep2)) return rep2;
  if (LessGeneral(rep2, rep1)) return rep1;
  // Handle the generalization of float64-representable values.
  if (LessGeneral(rep1, TruncationKind::kFloat64) &&
      LessGeneral(rep2, TruncationKind::kFloat64)) {
    return TruncationKind::kFloat64;
  }
  // All other combinations are illegal.
  FATAL("Tried to combine incompatible truncations");
  return TruncationKind::kNone;
}


// static
bool Truncation::LessGeneral(TruncationKind rep1, TruncationKind rep2) {
  switch (rep1) {
    case TruncationKind::kNone:
      return true;
    case TruncationKind::kBool:
      return rep2 == TruncationKind::kBool || rep2 == TruncationKind::kAny;
    case TruncationKind::kWord32:
      return rep2 == TruncationKind::kWord32 ||
             rep2 == TruncationKind::kWord64 ||
             rep2 == TruncationKind::kFloat64 || rep2 == TruncationKind::kAny;
    case TruncationKind::kWord64:
      return rep2 == TruncationKind::kWord64;
    case TruncationKind::kFloat32:
      return rep2 == TruncationKind::kFloat32 ||
             rep2 == TruncationKind::kFloat64 || rep2 == TruncationKind::kAny;
    case TruncationKind::kFloat64:
      return rep2 == TruncationKind::kFloat64 || rep2 == TruncationKind::kAny;
    case TruncationKind::kAny:
      return rep2 == TruncationKind::kAny;
  }
  UNREACHABLE();
  return false;
}


namespace {

// TODO(titzer): should Word64 also be implicitly convertable to others?
bool IsWord(MachineRepresentation rep) {
  return rep == MachineRepresentation::kWord8 ||
         rep == MachineRepresentation::kWord16 ||
         rep == MachineRepresentation::kWord32;
}

}  // namespace


// Changes representation from {output_rep} to {use_rep}. The {truncation}
// parameter is only used for sanity checking - if the changer cannot figure
// out signedness for the word32->float64 conversion, then we check that the
// uses truncate to word32 (so they do not care about signedness).
Node* RepresentationChanger::GetRepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type,
    MachineRepresentation use_rep, Truncation truncation) {
  if (output_rep == MachineRepresentation::kNone) {
    // The output representation should be set.
    return TypeError(node, output_rep, output_type, use_rep);
  }
  if (use_rep == output_rep) {
    // Representations are the same. That's a no-op.
    return node;
  }
  if (IsWord(use_rep) && IsWord(output_rep)) {
    // Both are words less than or equal to 32-bits.
    // Since loads of integers from memory implicitly sign or zero extend the
    // value to the full machine word size and stores implicitly truncate,
    // no representation change is necessary.
    return node;
  }
  switch (use_rep) {
    case MachineRepresentation::kTagged:
      return GetTaggedRepresentationFor(node, output_rep, output_type);
    case MachineRepresentation::kFloat32:
      return GetFloat32RepresentationFor(node, output_rep, output_type,
                                         truncation);
    case MachineRepresentation::kFloat64:
      return GetFloat64RepresentationFor(node, output_rep, output_type,
                                         truncation);
    case MachineRepresentation::kBit:
      return GetBitRepresentationFor(node, output_rep, output_type);
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
    case MachineRepresentation::kWord32:
      return GetWord32RepresentationFor(node, output_rep, output_type);
    case MachineRepresentation::kWord64:
      return GetWord64RepresentationFor(node, output_rep, output_type);
    case MachineRepresentation::kNone:
      return node;
  }
  UNREACHABLE();
  return nullptr;
}


Node* RepresentationChanger::GetTaggedRepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kNumberConstant:
    case IrOpcode::kHeapConstant:
      return node;  // No change necessary.
    case IrOpcode::kInt32Constant:
      if (output_type->Is(Type::Signed32())) {
        int32_t value = OpParameter<int32_t>(node);
        return jsgraph()->Constant(value);
      } else if (output_type->Is(Type::Unsigned32())) {
        uint32_t value = static_cast<uint32_t>(OpParameter<int32_t>(node));
        return jsgraph()->Constant(static_cast<double>(value));
      } else if (output_rep == MachineRepresentation::kBit) {
        return OpParameter<int32_t>(node) == 0 ? jsgraph()->FalseConstant()
                                               : jsgraph()->TrueConstant();
      } else {
        return TypeError(node, output_rep, output_type,
                         MachineRepresentation::kTagged);
      }
    case IrOpcode::kFloat64Constant:
      return jsgraph()->Constant(OpParameter<double>(node));
    case IrOpcode::kFloat32Constant:
      return jsgraph()->Constant(OpParameter<float>(node));
    default:
      break;
  }
  // Select the correct X -> Tagged operator.
  const Operator* op;
  if (output_rep == MachineRepresentation::kBit) {
    op = simplified()->ChangeBitToBool();
  } else if (IsWord(output_rep)) {
    if (output_type->Is(Type::Unsigned32())) {
      op = simplified()->ChangeUint32ToTagged();
    } else if (output_type->Is(Type::Signed32())) {
      op = simplified()->ChangeInt32ToTagged();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTagged);
    }
  } else if (output_rep ==
             MachineRepresentation::kFloat32) {  // float32 -> float64 -> tagged
    node = InsertChangeFloat32ToFloat64(node);
    op = simplified()->ChangeFloat64ToTagged();
  } else if (output_rep == MachineRepresentation::kFloat64) {
    op = simplified()->ChangeFloat64ToTagged();
  } else {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kTagged);
  }
  return jsgraph()->graph()->NewNode(op, node);
}


Node* RepresentationChanger::GetFloat32RepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type,
    Truncation truncation) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kFloat64Constant:
    case IrOpcode::kNumberConstant:
      return jsgraph()->Float32Constant(
          DoubleToFloat32(OpParameter<double>(node)));
    case IrOpcode::kInt32Constant:
      if (output_type->Is(Type::Unsigned32())) {
        uint32_t value = static_cast<uint32_t>(OpParameter<int32_t>(node));
        return jsgraph()->Float32Constant(static_cast<float>(value));
      } else {
        int32_t value = OpParameter<int32_t>(node);
        return jsgraph()->Float32Constant(static_cast<float>(value));
      }
    case IrOpcode::kFloat32Constant:
      return node;  // No change necessary.
    default:
      break;
  }
  // Select the correct X -> Float32 operator.
  const Operator* op;
  if (output_rep == MachineRepresentation::kBit) {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kFloat32);
  } else if (IsWord(output_rep)) {
    if (output_type->Is(Type::Signed32())) {
      op = machine()->ChangeInt32ToFloat64();
    } else {
      // Either the output is int32 or the uses only care about the
      // low 32 bits (so we can pick int32 safely).
      DCHECK(output_type->Is(Type::Unsigned32()) ||
             truncation.TruncatesToWord32());
      op = machine()->ChangeUint32ToFloat64();
    }
    // int32 -> float64 -> float32
    node = jsgraph()->graph()->NewNode(op, node);
    op = machine()->TruncateFloat64ToFloat32();
  } else if (output_rep == MachineRepresentation::kTagged) {
    op = simplified()->ChangeTaggedToFloat64();  // tagged -> float64 -> float32
    node = jsgraph()->graph()->NewNode(op, node);
    op = machine()->TruncateFloat64ToFloat32();
  } else if (output_rep == MachineRepresentation::kFloat64) {
    op = machine()->TruncateFloat64ToFloat32();
  } else {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kFloat32);
  }
  return jsgraph()->graph()->NewNode(op, node);
}


Node* RepresentationChanger::GetFloat64RepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type,
    Truncation truncation) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kNumberConstant:
      return jsgraph()->Float64Constant(OpParameter<double>(node));
    case IrOpcode::kInt32Constant:
      if (output_type->Is(Type::Signed32())) {
        int32_t value = OpParameter<int32_t>(node);
        return jsgraph()->Float64Constant(value);
      } else {
        DCHECK(output_type->Is(Type::Unsigned32()));
        uint32_t value = static_cast<uint32_t>(OpParameter<int32_t>(node));
        return jsgraph()->Float64Constant(static_cast<double>(value));
      }
    case IrOpcode::kFloat64Constant:
      return node;  // No change necessary.
    case IrOpcode::kFloat32Constant:
      return jsgraph()->Float64Constant(OpParameter<float>(node));
    default:
      break;
  }
  // Select the correct X -> Float64 operator.
  const Operator* op;
  if (output_rep == MachineRepresentation::kBit) {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kFloat64);
  } else if (IsWord(output_rep)) {
    if (output_type->Is(Type::Signed32())) {
      op = machine()->ChangeInt32ToFloat64();
    } else {
      // Either the output is int32 or the uses only care about the
      // low 32 bits (so we can pick int32 safely).
      DCHECK(output_type->Is(Type::Unsigned32()) ||
             truncation.TruncatesToWord32());
      op = machine()->ChangeUint32ToFloat64();
    }
  } else if (output_rep == MachineRepresentation::kTagged) {
    op = simplified()->ChangeTaggedToFloat64();
  } else if (output_rep == MachineRepresentation::kFloat32) {
    op = machine()->ChangeFloat32ToFloat64();
  } else {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kFloat64);
  }
  return jsgraph()->graph()->NewNode(op, node);
}


Node* RepresentationChanger::MakeTruncatedInt32Constant(double value) {
  return jsgraph()->Int32Constant(DoubleToInt32(value));
}


Node* RepresentationChanger::GetWord32RepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kInt32Constant:
      return node;  // No change necessary.
    case IrOpcode::kFloat32Constant:
      return MakeTruncatedInt32Constant(OpParameter<float>(node));
    case IrOpcode::kNumberConstant:
    case IrOpcode::kFloat64Constant:
      return MakeTruncatedInt32Constant(OpParameter<double>(node));
    default:
      break;
  }
  // Select the correct X -> Word32 operator.
  const Operator* op;
  Type* type = NodeProperties::GetType(node);

  if (output_rep == MachineRepresentation::kBit) {
    return node;  // Sloppy comparison -> word32
  } else if (output_rep == MachineRepresentation::kFloat64) {
    // TODO(jarin) Use only output_type here, once we intersect it with the
    // type inferred by the typer.
    if (output_type->Is(Type::Unsigned32()) || type->Is(Type::Unsigned32())) {
      op = machine()->ChangeFloat64ToUint32();
    } else if (output_type->Is(Type::Signed32()) ||
               type->Is(Type::Signed32())) {
      op = machine()->ChangeFloat64ToInt32();
    } else {
      op = machine()->TruncateFloat64ToInt32(TruncationMode::kJavaScript);
    }
  } else if (output_rep == MachineRepresentation::kFloat32) {
    node = InsertChangeFloat32ToFloat64(node);  // float32 -> float64 -> int32
    if (output_type->Is(Type::Unsigned32()) || type->Is(Type::Unsigned32())) {
      op = machine()->ChangeFloat64ToUint32();
    } else if (output_type->Is(Type::Signed32()) ||
               type->Is(Type::Signed32())) {
      op = machine()->ChangeFloat64ToInt32();
    } else {
      op = machine()->TruncateFloat64ToInt32(TruncationMode::kJavaScript);
    }
  } else if (output_rep == MachineRepresentation::kTagged) {
    if (output_type->Is(Type::Unsigned32()) || type->Is(Type::Unsigned32())) {
      op = simplified()->ChangeTaggedToUint32();
    } else if (output_type->Is(Type::Signed32()) ||
               type->Is(Type::Signed32())) {
      op = simplified()->ChangeTaggedToInt32();
    } else {
      node = InsertChangeTaggedToFloat64(node);
      op = machine()->TruncateFloat64ToInt32(TruncationMode::kJavaScript);
    }
  } else {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kWord32);
  }
  return jsgraph()->graph()->NewNode(op, node);
}


Node* RepresentationChanger::GetBitRepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kHeapConstant: {
      Handle<HeapObject> value = OpParameter<Handle<HeapObject>>(node);
      DCHECK(value.is_identical_to(factory()->true_value()) ||
             value.is_identical_to(factory()->false_value()));
      return jsgraph()->Int32Constant(
          value.is_identical_to(factory()->true_value()) ? 1 : 0);
    }
    default:
      break;
  }
  // Select the correct X -> Bit operator.
  const Operator* op;
  if (output_rep == MachineRepresentation::kTagged) {
    op = simplified()->ChangeBoolToBit();
  } else {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kBit);
  }
  return jsgraph()->graph()->NewNode(op, node);
}


Node* RepresentationChanger::GetWord64RepresentationFor(
    Node* node, MachineRepresentation output_rep, Type* output_type) {
  if (output_rep == MachineRepresentation::kBit) {
    return node;  // Sloppy comparison -> word64
  }
  // Can't really convert Word64 to anything else. Purported to be internal.
  return TypeError(node, output_rep, output_type,
                   MachineRepresentation::kWord64);
}


const Operator* RepresentationChanger::Int32OperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kNumberAdd:
      return machine()->Int32Add();
    case IrOpcode::kNumberSubtract:
      return machine()->Int32Sub();
    case IrOpcode::kNumberMultiply:
      return machine()->Int32Mul();
    case IrOpcode::kNumberDivide:
      return machine()->Int32Div();
    case IrOpcode::kNumberModulus:
      return machine()->Int32Mod();
    case IrOpcode::kNumberBitwiseOr:
      return machine()->Word32Or();
    case IrOpcode::kNumberBitwiseXor:
      return machine()->Word32Xor();
    case IrOpcode::kNumberBitwiseAnd:
      return machine()->Word32And();
    case IrOpcode::kNumberEqual:
      return machine()->Word32Equal();
    case IrOpcode::kNumberLessThan:
      return machine()->Int32LessThan();
    case IrOpcode::kNumberLessThanOrEqual:
      return machine()->Int32LessThanOrEqual();
    default:
      UNREACHABLE();
      return nullptr;
  }
}


const Operator* RepresentationChanger::Uint32OperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kNumberAdd:
      return machine()->Int32Add();
    case IrOpcode::kNumberSubtract:
      return machine()->Int32Sub();
    case IrOpcode::kNumberMultiply:
      return machine()->Int32Mul();
    case IrOpcode::kNumberDivide:
      return machine()->Uint32Div();
    case IrOpcode::kNumberModulus:
      return machine()->Uint32Mod();
    case IrOpcode::kNumberEqual:
      return machine()->Word32Equal();
    case IrOpcode::kNumberLessThan:
      return machine()->Uint32LessThan();
    case IrOpcode::kNumberLessThanOrEqual:
      return machine()->Uint32LessThanOrEqual();
    default:
      UNREACHABLE();
      return nullptr;
  }
}


const Operator* RepresentationChanger::Float64OperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kNumberAdd:
      return machine()->Float64Add();
    case IrOpcode::kNumberSubtract:
      return machine()->Float64Sub();
    case IrOpcode::kNumberMultiply:
      return machine()->Float64Mul();
    case IrOpcode::kNumberDivide:
      return machine()->Float64Div();
    case IrOpcode::kNumberModulus:
      return machine()->Float64Mod();
    case IrOpcode::kNumberEqual:
      return machine()->Float64Equal();
    case IrOpcode::kNumberLessThan:
      return machine()->Float64LessThan();
    case IrOpcode::kNumberLessThanOrEqual:
      return machine()->Float64LessThanOrEqual();
    default:
      UNREACHABLE();
      return nullptr;
  }
}


Node* RepresentationChanger::TypeError(Node* node,
                                       MachineRepresentation output_rep,
                                       Type* output_type,
                                       MachineRepresentation use) {
  type_error_ = true;
  if (!testing_type_errors_) {
    std::ostringstream out_str;
    out_str << output_rep << " (";
    output_type->PrintTo(out_str, Type::SEMANTIC_DIM);
    out_str << ")";

    std::ostringstream use_str;
    use_str << use;

    V8_Fatal(__FILE__, __LINE__,
             "RepresentationChangerError: node #%d:%s of "
             "%s cannot be changed to %s",
             node->id(), node->op()->mnemonic(), out_str.str().c_str(),
             use_str.str().c_str());
  }
  return node;
}


Node* RepresentationChanger::InsertChangeFloat32ToFloat64(Node* node) {
  return jsgraph()->graph()->NewNode(machine()->ChangeFloat32ToFloat64(), node);
}


Node* RepresentationChanger::InsertChangeTaggedToFloat64(Node* node) {
  return jsgraph()->graph()->NewNode(simplified()->ChangeTaggedToFloat64(),
                                     node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
