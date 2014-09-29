// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_REPRESENTATION_CHANGE_H_
#define V8_COMPILER_REPRESENTATION_CHANGE_H_

#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// The types and representations tracked during representation inference
// and change insertion.
// TODO(titzer): First, merge MachineType and RepType.
// TODO(titzer): Second, Use the real type system instead of RepType.
enum RepType {
  // Representations.
  rBit = 1 << 0,
  rWord32 = 1 << 1,
  rWord64 = 1 << 2,
  rFloat64 = 1 << 3,
  rTagged = 1 << 4,

  // Types.
  tBool = 1 << 5,
  tInt32 = 1 << 6,
  tUint32 = 1 << 7,
  tInt64 = 1 << 8,
  tUint64 = 1 << 9,
  tNumber = 1 << 10,
  tAny = 1 << 11
};

#define REP_TYPE_STRLEN 24

typedef uint16_t RepTypeUnion;


inline void RenderRepTypeUnion(char* buf, RepTypeUnion info) {
  base::OS::SNPrintF(buf, REP_TYPE_STRLEN, "{%s%s%s%s%s %s%s%s%s%s%s%s}",
                     (info & rBit) ? "k" : " ", (info & rWord32) ? "w" : " ",
                     (info & rWord64) ? "q" : " ",
                     (info & rFloat64) ? "f" : " ",
                     (info & rTagged) ? "t" : " ", (info & tBool) ? "Z" : " ",
                     (info & tInt32) ? "I" : " ", (info & tUint32) ? "U" : " ",
                     (info & tInt64) ? "L" : " ", (info & tUint64) ? "J" : " ",
                     (info & tNumber) ? "N" : " ", (info & tAny) ? "*" : " ");
}


const RepTypeUnion rMask = rBit | rWord32 | rWord64 | rFloat64 | rTagged;
const RepTypeUnion tMask =
    tBool | tInt32 | tUint32 | tInt64 | tUint64 | tNumber | tAny;
const RepType rPtr = kPointerSize == 4 ? rWord32 : rWord64;

// Contains logic related to changing the representation of values for constants
// and other nodes, as well as lowering Simplified->Machine operators.
// Eagerly folds any representation changes for constants.
class RepresentationChanger {
 public:
  RepresentationChanger(JSGraph* jsgraph, SimplifiedOperatorBuilder* simplified,
                        MachineOperatorBuilder* machine, Isolate* isolate)
      : jsgraph_(jsgraph),
        simplified_(simplified),
        machine_(machine),
        isolate_(isolate),
        testing_type_errors_(false),
        type_error_(false) {}


  Node* GetRepresentationFor(Node* node, RepTypeUnion output_type,
                             RepTypeUnion use_type) {
    if (!IsPowerOf2(output_type & rMask)) {
      // There should be only one output representation.
      return TypeError(node, output_type, use_type);
    }
    if ((use_type & rMask) == (output_type & rMask)) {
      // Representations are the same. That's a no-op.
      return node;
    }
    if (use_type & rTagged) {
      return GetTaggedRepresentationFor(node, output_type);
    } else if (use_type & rFloat64) {
      return GetFloat64RepresentationFor(node, output_type);
    } else if (use_type & rWord32) {
      return GetWord32RepresentationFor(node, output_type, use_type & tUint32);
    } else if (use_type & rBit) {
      return GetBitRepresentationFor(node, output_type);
    } else if (use_type & rWord64) {
      return GetWord64RepresentationFor(node, output_type);
    } else {
      return node;
    }
  }

  Node* GetTaggedRepresentationFor(Node* node, RepTypeUnion output_type) {
    // Eagerly fold representation changes for constants.
    switch (node->opcode()) {
      case IrOpcode::kNumberConstant:
      case IrOpcode::kHeapConstant:
        return node;  // No change necessary.
      case IrOpcode::kInt32Constant:
        if (output_type & tUint32) {
          uint32_t value = ValueOf<uint32_t>(node->op());
          return jsgraph()->Constant(static_cast<double>(value));
        } else if (output_type & tInt32) {
          int32_t value = ValueOf<int32_t>(node->op());
          return jsgraph()->Constant(value);
        } else if (output_type & rBit) {
          return ValueOf<int32_t>(node->op()) == 0 ? jsgraph()->FalseConstant()
                                                   : jsgraph()->TrueConstant();
        } else {
          return TypeError(node, output_type, rTagged);
        }
      case IrOpcode::kFloat64Constant:
        return jsgraph()->Constant(ValueOf<double>(node->op()));
      default:
        break;
    }
    // Select the correct X -> Tagged operator.
    Operator* op;
    if (output_type & rBit) {
      op = simplified()->ChangeBitToBool();
    } else if (output_type & rWord32) {
      if (output_type & tUint32) {
        op = simplified()->ChangeUint32ToTagged();
      } else if (output_type & tInt32) {
        op = simplified()->ChangeInt32ToTagged();
      } else {
        return TypeError(node, output_type, rTagged);
      }
    } else if (output_type & rFloat64) {
      op = simplified()->ChangeFloat64ToTagged();
    } else {
      return TypeError(node, output_type, rTagged);
    }
    return jsgraph()->graph()->NewNode(op, node);
  }

  Node* GetFloat64RepresentationFor(Node* node, RepTypeUnion output_type) {
    // Eagerly fold representation changes for constants.
    switch (node->opcode()) {
      case IrOpcode::kNumberConstant:
        return jsgraph()->Float64Constant(ValueOf<double>(node->op()));
      case IrOpcode::kInt32Constant:
        if (output_type & tUint32) {
          uint32_t value = ValueOf<uint32_t>(node->op());
          return jsgraph()->Float64Constant(static_cast<double>(value));
        } else {
          int32_t value = ValueOf<int32_t>(node->op());
          return jsgraph()->Float64Constant(value);
        }
      case IrOpcode::kFloat64Constant:
        return node;  // No change necessary.
      default:
        break;
    }
    // Select the correct X -> Float64 operator.
    Operator* op;
    if (output_type & rWord32) {
      if (output_type & tUint32) {
        op = machine()->ChangeUint32ToFloat64();
      } else {
        op = machine()->ChangeInt32ToFloat64();
      }
    } else if (output_type & rTagged) {
      op = simplified()->ChangeTaggedToFloat64();
    } else {
      return TypeError(node, output_type, rFloat64);
    }
    return jsgraph()->graph()->NewNode(op, node);
  }

  Node* GetWord32RepresentationFor(Node* node, RepTypeUnion output_type,
                                   bool use_unsigned) {
    // Eagerly fold representation changes for constants.
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant:
        return node;  // No change necessary.
      case IrOpcode::kNumberConstant:
      case IrOpcode::kFloat64Constant: {
        double value = ValueOf<double>(node->op());
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
      default:
        break;
    }
    // Select the correct X -> Word32 operator.
    Operator* op = NULL;
    if (output_type & rFloat64) {
      if (output_type & tUint32 || use_unsigned) {
        op = machine()->ChangeFloat64ToUint32();
      } else {
        op = machine()->ChangeFloat64ToInt32();
      }
    } else if (output_type & rTagged) {
      if (output_type & tUint32 || use_unsigned) {
        op = simplified()->ChangeTaggedToUint32();
      } else {
        op = simplified()->ChangeTaggedToInt32();
      }
    } else if (output_type & rBit) {
      return node;  // Sloppy comparison -> word32.
    } else {
      return TypeError(node, output_type, rWord32);
    }
    return jsgraph()->graph()->NewNode(op, node);
  }

  Node* GetBitRepresentationFor(Node* node, RepTypeUnion output_type) {
    // Eagerly fold representation changes for constants.
    switch (node->opcode()) {
      case IrOpcode::kInt32Constant: {
        int32_t value = ValueOf<int32_t>(node->op());
        if (value == 0 || value == 1) return node;
        return jsgraph()->OneConstant();  // value != 0
      }
      case IrOpcode::kHeapConstant: {
        Handle<Object> handle = ValueOf<Handle<Object> >(node->op());
        DCHECK(*handle == isolate()->heap()->true_value() ||
               *handle == isolate()->heap()->false_value());
        return jsgraph()->Int32Constant(
            *handle == isolate()->heap()->true_value() ? 1 : 0);
      }
      default:
        break;
    }
    // Select the correct X -> Bit operator.
    Operator* op;
    if (output_type & rWord32) {
      return node;  // No change necessary.
    } else if (output_type & rWord64) {
      return node;  // TODO(titzer): No change necessary, on 64-bit.
    } else if (output_type & rTagged) {
      op = simplified()->ChangeBoolToBit();
    } else {
      return TypeError(node, output_type, rBit);
    }
    return jsgraph()->graph()->NewNode(op, node);
  }

  Node* GetWord64RepresentationFor(Node* node, RepTypeUnion output_type) {
    if (output_type & rBit) {
      return node;  // Sloppy comparison -> word64
    }
    // Can't really convert Word64 to anything else. Purported to be internal.
    return TypeError(node, output_type, rWord64);
  }

  static RepType TypeForMachineType(MachineType rep) {
    // TODO(titzer): merge MachineType and RepType.
    switch (rep) {
      case kMachineWord8:
        return rWord32;
      case kMachineWord16:
        return rWord32;
      case kMachineWord32:
        return rWord32;
      case kMachineWord64:
        return rWord64;
      case kMachineFloat64:
        return rFloat64;
      case kMachineTagged:
        return rTagged;
      default:
        UNREACHABLE();
        return static_cast<RepType>(0);
    }
  }

  Operator* Int32OperatorFor(IrOpcode::Value opcode) {
    switch (opcode) {
      case IrOpcode::kNumberAdd:
        return machine()->Int32Add();
      case IrOpcode::kNumberSubtract:
        return machine()->Int32Sub();
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

  Operator* Uint32OperatorFor(IrOpcode::Value opcode) {
    switch (opcode) {
      case IrOpcode::kNumberAdd:
        return machine()->Int32Add();
      case IrOpcode::kNumberSubtract:
        return machine()->Int32Sub();
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

  Operator* Float64OperatorFor(IrOpcode::Value opcode) {
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

  RepType TypeForField(const FieldAccess& access) {
    RepType tElement = static_cast<RepType>(0);  // TODO(titzer)
    RepType rElement = TypeForMachineType(access.representation);
    return static_cast<RepType>(tElement | rElement);
  }

  RepType TypeForElement(const ElementAccess& access) {
    RepType tElement = static_cast<RepType>(0);  // TODO(titzer)
    RepType rElement = TypeForMachineType(access.representation);
    return static_cast<RepType>(tElement | rElement);
  }

  RepType TypeForBasePointer(const FieldAccess& access) {
    if (access.tag() != 0) return static_cast<RepType>(tAny | rTagged);
    return kPointerSize == 8 ? rWord64 : rWord32;
  }

  RepType TypeForBasePointer(const ElementAccess& access) {
    if (access.tag() != 0) return static_cast<RepType>(tAny | rTagged);
    return kPointerSize == 8 ? rWord64 : rWord32;
  }

  RepType TypeFromUpperBound(Type* type) {
    if (type->Is(Type::None()))
      return tAny;  // TODO(titzer): should be an error
    if (type->Is(Type::Signed32())) return tInt32;
    if (type->Is(Type::Unsigned32())) return tUint32;
    if (type->Is(Type::Number())) return tNumber;
    if (type->Is(Type::Boolean())) return tBool;
    return tAny;
  }

 private:
  JSGraph* jsgraph_;
  SimplifiedOperatorBuilder* simplified_;
  MachineOperatorBuilder* machine_;
  Isolate* isolate_;

  friend class RepresentationChangerTester;  // accesses the below fields.

  bool testing_type_errors_;  // If {true}, don't abort on a type error.
  bool type_error_;           // Set when a type error is detected.

  Node* TypeError(Node* node, RepTypeUnion output_type, RepTypeUnion use) {
    type_error_ = true;
    if (!testing_type_errors_) {
      char buf1[REP_TYPE_STRLEN];
      char buf2[REP_TYPE_STRLEN];
      RenderRepTypeUnion(buf1, output_type);
      RenderRepTypeUnion(buf2, use);
      V8_Fatal(__FILE__, __LINE__,
               "RepresentationChangerError: node #%d:%s of rep"
               "%s cannot be changed to rep%s",
               node->id(), node->op()->mnemonic(), buf1, buf2);
    }
    return node;
  }

  JSGraph* jsgraph() { return jsgraph_; }
  Isolate* isolate() { return isolate_; }
  SimplifiedOperatorBuilder* simplified() { return simplified_; }
  MachineOperatorBuilder* machine() { return machine_; }
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_REPRESENTATION_CHANGE_H_
