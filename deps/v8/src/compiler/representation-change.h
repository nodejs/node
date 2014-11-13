// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_REPRESENTATION_CHANGE_H_
#define V8_COMPILER_REPRESENTATION_CHANGE_H_

#include <sstream>

#include "src/base/bits.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Contains logic related to changing the representation of values for constants
// and other nodes, as well as lowering Simplified->Machine operators.
// Eagerly folds any representation changes for constants.
class RepresentationChanger {
 public:
  RepresentationChanger(JSGraph* jsgraph, SimplifiedOperatorBuilder* simplified,
                        Isolate* isolate)
      : jsgraph_(jsgraph),
        simplified_(simplified),
        isolate_(isolate),
        testing_type_errors_(false),
        type_error_(false) {}

  // TODO(titzer): should Word64 also be implicitly convertable to others?
  static const MachineTypeUnion rWord =
      kRepBit | kRepWord8 | kRepWord16 | kRepWord32;

  Node* GetRepresentationFor(Node* node, MachineTypeUnion output_type,
                             MachineTypeUnion use_type) {
    if (!base::bits::IsPowerOfTwo32(output_type & kRepMask)) {
      // There should be only one output representation.
      return TypeError(node, output_type, use_type);
    }
    if ((use_type & kRepMask) == (output_type & kRepMask)) {
      // Representations are the same. That's a no-op.
      return node;
    }
    if ((use_type & rWord) && (output_type & rWord)) {
      // Both are words less than or equal to 32-bits.
      // Since loads of integers from memory implicitly sign or zero extend the
      // value to the full machine word size and stores implicitly truncate,
      // no representation change is necessary.
      return node;
    }
    if (use_type & kRepTagged) {
      return GetTaggedRepresentationFor(node, output_type);
    } else if (use_type & kRepFloat32) {
      return GetFloat32RepresentationFor(node, output_type);
    } else if (use_type & kRepFloat64) {
      return GetFloat64RepresentationFor(node, output_type);
    } else if (use_type & kRepBit) {
      return GetBitRepresentationFor(node, output_type);
    } else if (use_type & rWord) {
      return GetWord32RepresentationFor(node, output_type,
                                        use_type & kTypeUint32);
    } else if (use_type & kRepWord64) {
      return GetWord64RepresentationFor(node, output_type);
    } else {
      return node;
    }
  }

  Node* GetTaggedRepresentationFor(Node* node, MachineTypeUnion output_type) {
    // Eagerly fold representation changes for constants.
    switch (node->opcode()) {
      case IrOpcode::kNumberConstant:
      case IrOpcode::kHeapConstant:
        return node;  // No change necessary.
      case IrOpcode::kInt32Constant:
        if (output_type & kTypeUint32) {
          uint32_t value = OpParameter<uint32_t>(node);
          return jsgraph()->Constant(static_cast<double>(value));
        } else if (output_type & kTypeInt32) {
          int32_t value = OpParameter<int32_t>(node);
          return jsgraph()->Constant(value);
        } else if (output_type & kRepBit) {
          return OpParameter<int32_t>(node) == 0 ? jsgraph()->FalseConstant()
                                                 : jsgraph()->TrueConstant();
        } else {
          return TypeError(node, output_type, kRepTagged);
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
    if (output_type & kRepBit) {
      op = simplified()->ChangeBitToBool();
    } else if (output_type & rWord) {
      if (output_type & kTypeUint32) {
        op = simplified()->ChangeUint32ToTagged();
      } else if (output_type & kTypeInt32) {
        op = simplified()->ChangeInt32ToTagged();
      } else {
        return TypeError(node, output_type, kRepTagged);
      }
    } else if (output_type & kRepFloat32) {  // float32 -> float64 -> tagged
      node = InsertChangeFloat32ToFloat64(node);
      op = simplified()->ChangeFloat64ToTagged();
    } else if (output_type & kRepFloat64) {
      op = simplified()->ChangeFloat64ToTagged();
    } else {
      return TypeError(node, output_type, kRepTagged);
    }
    return jsgraph()->graph()->NewNode(op, node);
  }

  Node* GetFloat32RepresentationFor(Node* node, MachineTypeUnion output_type) {
    // Eagerly fold representation changes for constants.
    switch (node->opcode()) {
      case IrOpcode::kFloat64Constant:
      case IrOpcode::kNumberConstant:
        return jsgraph()->Float32Constant(
            DoubleToFloat32(OpParameter<double>(node)));
      case IrOpcode::kInt32Constant:
        if (output_type & kTypeUint32) {
          uint32_t value = OpParameter<uint32_t>(node);
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
    if (output_type & kRepBit) {
      return TypeError(node, output_type, kRepFloat32);
    } else if (output_type & rWord) {
      if (output_type & kTypeUint32) {
        op = machine()->ChangeUint32ToFloat64();
      } else {
        op = machine()->ChangeInt32ToFloat64();
      }
      // int32 -> float64 -> float32
      node = jsgraph()->graph()->NewNode(op, node);
      op = machine()->TruncateFloat64ToFloat32();
    } else if (output_type & kRepTagged) {
      op = simplified()
               ->ChangeTaggedToFloat64();  // tagged -> float64 -> float32
      node = jsgraph()->graph()->NewNode(op, node);
      op = machine()->TruncateFloat64ToFloat32();
    } else if (output_type & kRepFloat64) {
      op = machine()->TruncateFloat64ToFloat32();
    } else {
      return TypeError(node, output_type, kRepFloat32);
    }
    return jsgraph()->graph()->NewNode(op, node);
  }

  Node* GetFloat64RepresentationFor(Node* node, MachineTypeUnion output_type) {
    // Eagerly fold representation changes for constants.
    switch (node->opcode()) {
      case IrOpcode::kNumberConstant:
        return jsgraph()->Float64Constant(OpParameter<double>(node));
      case IrOpcode::kInt32Constant:
        if (output_type & kTypeUint32) {
          uint32_t value = OpParameter<uint32_t>(node);
          return jsgraph()->Float64Constant(static_cast<double>(value));
        } else {
          int32_t value = OpParameter<int32_t>(node);
          return jsgraph()->Float64Constant(value);
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
    if (output_type & kRepBit) {
      return TypeError(node, output_type, kRepFloat64);
    } else if (output_type & rWord) {
      if (output_type & kTypeUint32) {
        op = machine()->ChangeUint32ToFloat64();
      } else {
        op = machine()->ChangeInt32ToFloat64();
      }
    } else if (output_type & kRepTagged) {
      op = simplified()->ChangeTaggedToFloat64();
    } else if (output_type & kRepFloat32) {
      op = machine()->ChangeFloat32ToFloat64();
    } else {
      return TypeError(node, output_type, kRepFloat64);
    }
    return jsgraph()->graph()->NewNode(op, node);
  }

  Node* MakeInt32Constant(double value) {
    if (value < 0) {
      DCHECK(IsInt32Double(value));
      int32_t iv = static_cast<int32_t>(value);
      return jsgraph()->Int32Constant(iv);
    } else {
      DCHECK(IsUint32Double(value));
      int32_t iv = static_cast<int32_t>(static_cast<uint32_t>(value));
      return jsgraph()->Int32Constant(iv);
    }
  }

  Node* GetTruncatedWord32For(Node* node, MachineTypeUnion output_type) {
    // Eagerly fold truncations for constants.
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
        return node;  // No change necessary.
      case IrOpcode::kFloat32Constant:
        return jsgraph()->Int32Constant(
            DoubleToInt32(OpParameter<float>(node)));
      case IrOpcode::kNumberConstant:
      case IrOpcode::kFloat64Constant:
        return jsgraph()->Int32Constant(
            DoubleToInt32(OpParameter<double>(node)));
      default:
        break;
    }
    // Select the correct X -> Word32 truncation operator.
    const Operator* op = NULL;
    if (output_type & kRepFloat64) {
      op = machine()->TruncateFloat64ToInt32();
    } else if (output_type & kRepFloat32) {
      node = InsertChangeFloat32ToFloat64(node);
      op = machine()->TruncateFloat64ToInt32();
    } else if (output_type & kRepTagged) {
      node = InsertChangeTaggedToFloat64(node);
      op = machine()->TruncateFloat64ToInt32();
    } else {
      return TypeError(node, output_type, kRepWord32);
    }
    return jsgraph()->graph()->NewNode(op, node);
  }

  Node* GetWord32RepresentationFor(Node* node, MachineTypeUnion output_type,
                                   bool use_unsigned) {
    // Eagerly fold representation changes for constants.
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
        return node;  // No change necessary.
      case IrOpcode::kFloat32Constant:
        return MakeInt32Constant(OpParameter<float>(node));
      case IrOpcode::kNumberConstant:
      case IrOpcode::kFloat64Constant:
        return MakeInt32Constant(OpParameter<double>(node));
      default:
        break;
    }
    // Select the correct X -> Word32 operator.
    const Operator* op = NULL;
    if (output_type & kRepFloat64) {
      if (output_type & kTypeUint32 || use_unsigned) {
        op = machine()->ChangeFloat64ToUint32();
      } else {
        op = machine()->ChangeFloat64ToInt32();
      }
    } else if (output_type & kRepFloat32) {
      node = InsertChangeFloat32ToFloat64(node);  // float32 -> float64 -> int32
      if (output_type & kTypeUint32 || use_unsigned) {
        op = machine()->ChangeFloat64ToUint32();
      } else {
        op = machine()->ChangeFloat64ToInt32();
      }
    } else if (output_type & kRepTagged) {
      if (output_type & kTypeUint32 || use_unsigned) {
        op = simplified()->ChangeTaggedToUint32();
      } else {
        op = simplified()->ChangeTaggedToInt32();
      }
    } else {
      return TypeError(node, output_type, kRepWord32);
    }
    return jsgraph()->graph()->NewNode(op, node);
  }

  Node* GetBitRepresentationFor(Node* node, MachineTypeUnion output_type) {
    // Eagerly fold representation changes for constants.
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant: {
        int32_t value = OpParameter<int32_t>(node);
        if (value == 0 || value == 1) return node;
        return jsgraph()->OneConstant();  // value != 0
      }
      case IrOpcode::kHeapConstant: {
        Handle<Object> handle = OpParameter<Unique<Object> >(node).handle();
        DCHECK(*handle == isolate()->heap()->true_value() ||
               *handle == isolate()->heap()->false_value());
        return jsgraph()->Int32Constant(
            *handle == isolate()->heap()->true_value() ? 1 : 0);
      }
      default:
        break;
    }
    // Select the correct X -> Bit operator.
    const Operator* op;
    if (output_type & rWord) {
      return node;  // No change necessary.
    } else if (output_type & kRepWord64) {
      return node;  // TODO(titzer): No change necessary, on 64-bit.
    } else if (output_type & kRepTagged) {
      op = simplified()->ChangeBoolToBit();
    } else {
      return TypeError(node, output_type, kRepBit);
    }
    return jsgraph()->graph()->NewNode(op, node);
  }

  Node* GetWord64RepresentationFor(Node* node, MachineTypeUnion output_type) {
    if (output_type & kRepBit) {
      return node;  // Sloppy comparison -> word64
    }
    // Can't really convert Word64 to anything else. Purported to be internal.
    return TypeError(node, output_type, kRepWord64);
  }

  const Operator* Int32OperatorFor(IrOpcode::Value opcode) {
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
      case IrOpcode::kNumberEqual:
        return machine()->Word32Equal();
      case IrOpcode::kNumberLessThan:
        return machine()->Int32LessThan();
      case IrOpcode::kNumberLessThanOrEqual:
        return machine()->Int32LessThanOrEqual();
      default:
        UNREACHABLE();
        return NULL;
    }
  }

  const Operator* Uint32OperatorFor(IrOpcode::Value opcode) {
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
        return NULL;
    }
  }

  const Operator* Float64OperatorFor(IrOpcode::Value opcode) {
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
        return NULL;
    }
  }

  MachineType TypeForBasePointer(const FieldAccess& access) {
    return access.tag() != 0 ? kMachAnyTagged : kMachPtr;
  }

  MachineType TypeForBasePointer(const ElementAccess& access) {
    return access.tag() != 0 ? kMachAnyTagged : kMachPtr;
  }

  MachineType TypeFromUpperBound(Type* type) {
    if (type->Is(Type::None()))
      return kTypeAny;  // TODO(titzer): should be an error
    if (type->Is(Type::Signed32())) return kTypeInt32;
    if (type->Is(Type::Unsigned32())) return kTypeUint32;
    if (type->Is(Type::Number())) return kTypeNumber;
    if (type->Is(Type::Boolean())) return kTypeBool;
    return kTypeAny;
  }

 private:
  JSGraph* jsgraph_;
  SimplifiedOperatorBuilder* simplified_;
  Isolate* isolate_;

  friend class RepresentationChangerTester;  // accesses the below fields.

  bool testing_type_errors_;  // If {true}, don't abort on a type error.
  bool type_error_;           // Set when a type error is detected.

  Node* TypeError(Node* node, MachineTypeUnion output_type,
                  MachineTypeUnion use) {
    type_error_ = true;
    if (!testing_type_errors_) {
      std::ostringstream out_str;
      out_str << static_cast<MachineType>(output_type);

      std::ostringstream use_str;
      use_str << static_cast<MachineType>(use);

      V8_Fatal(__FILE__, __LINE__,
               "RepresentationChangerError: node #%d:%s of "
               "%s cannot be changed to %s",
               node->id(), node->op()->mnemonic(), out_str.str().c_str(),
               use_str.str().c_str());
    }
    return node;
  }

  Node* InsertChangeFloat32ToFloat64(Node* node) {
    return jsgraph()->graph()->NewNode(machine()->ChangeFloat32ToFloat64(),
                                       node);
  }

  Node* InsertChangeTaggedToFloat64(Node* node) {
    return jsgraph()->graph()->NewNode(simplified()->ChangeTaggedToFloat64(),
                                       node);
  }

  JSGraph* jsgraph() { return jsgraph_; }
  Isolate* isolate() { return isolate_; }
  SimplifiedOperatorBuilder* simplified() { return simplified_; }
  MachineOperatorBuilder* machine() { return jsgraph()->machine(); }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_REPRESENTATION_CHANGE_H_
