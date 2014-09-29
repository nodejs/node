// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MACHINE_NODE_FACTORY_H_
#define V8_COMPILER_MACHINE_NODE_FACTORY_H_

#ifdef USE_SIMULATOR
#define MACHINE_ASSEMBLER_SUPPORTS_CALL_C 0
#else
#define MACHINE_ASSEMBLER_SUPPORTS_CALL_C 1
#endif

#include "src/v8.h"

#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

class MachineCallDescriptorBuilder : public ZoneObject {
 public:
  MachineCallDescriptorBuilder(MachineType return_type, int parameter_count,
                               const MachineType* parameter_types)
      : return_type_(return_type),
        parameter_count_(parameter_count),
        parameter_types_(parameter_types) {}

  int parameter_count() const { return parameter_count_; }
  const MachineType* parameter_types() const { return parameter_types_; }

  CallDescriptor* BuildCallDescriptor(Zone* zone) {
    return Linkage::GetSimplifiedCDescriptor(zone, parameter_count_,
                                             return_type_, parameter_types_);
  }

 private:
  const MachineType return_type_;
  const int parameter_count_;
  const MachineType* const parameter_types_;
};


#define ZONE() static_cast<NodeFactory*>(this)->zone()
#define COMMON() static_cast<NodeFactory*>(this)->common()
#define MACHINE() static_cast<NodeFactory*>(this)->machine()
#define NEW_NODE_0(op) static_cast<NodeFactory*>(this)->NewNode(op)
#define NEW_NODE_1(op, a) static_cast<NodeFactory*>(this)->NewNode(op, a)
#define NEW_NODE_2(op, a, b) static_cast<NodeFactory*>(this)->NewNode(op, a, b)
#define NEW_NODE_3(op, a, b, c) \
  static_cast<NodeFactory*>(this)->NewNode(op, a, b, c)

template <typename NodeFactory>
class MachineNodeFactory {
 public:
  // Constants.
  Node* PointerConstant(void* value) {
    return IntPtrConstant(reinterpret_cast<intptr_t>(value));
  }
  Node* IntPtrConstant(intptr_t value) {
    // TODO(dcarney): mark generated code as unserializable if value != 0.
    return kPointerSize == 8 ? Int64Constant(value)
                             : Int32Constant(static_cast<int>(value));
  }
  Node* Int32Constant(int32_t value) {
    return NEW_NODE_0(COMMON()->Int32Constant(value));
  }
  Node* Int64Constant(int64_t value) {
    return NEW_NODE_0(COMMON()->Int64Constant(value));
  }
  Node* NumberConstant(double value) {
    return NEW_NODE_0(COMMON()->NumberConstant(value));
  }
  Node* Float64Constant(double value) {
    return NEW_NODE_0(COMMON()->Float64Constant(value));
  }
  Node* HeapConstant(Handle<Object> object) {
    PrintableUnique<Object> val =
        PrintableUnique<Object>::CreateUninitialized(ZONE(), object);
    return NEW_NODE_0(COMMON()->HeapConstant(val));
  }

  Node* Projection(int index, Node* a) {
    return NEW_NODE_1(COMMON()->Projection(index), a);
  }

  // Memory Operations.
  Node* Load(MachineType rep, Node* base) {
    return Load(rep, base, Int32Constant(0));
  }
  Node* Load(MachineType rep, Node* base, Node* index) {
    return NEW_NODE_2(MACHINE()->Load(rep), base, index);
  }
  void Store(MachineType rep, Node* base, Node* value) {
    Store(rep, base, Int32Constant(0), value);
  }
  void Store(MachineType rep, Node* base, Node* index, Node* value) {
    NEW_NODE_3(MACHINE()->Store(rep, kNoWriteBarrier), base, index, value);
  }
  // Arithmetic Operations.
  Node* WordAnd(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->WordAnd(), a, b);
  }
  Node* WordOr(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->WordOr(), a, b);
  }
  Node* WordXor(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->WordXor(), a, b);
  }
  Node* WordShl(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->WordShl(), a, b);
  }
  Node* WordShr(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->WordShr(), a, b);
  }
  Node* WordSar(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->WordSar(), a, b);
  }
  Node* WordEqual(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->WordEqual(), a, b);
  }
  Node* WordNotEqual(Node* a, Node* b) {
    return WordBinaryNot(WordEqual(a, b));
  }
  Node* WordNot(Node* a) {
    if (MACHINE()->is32()) {
      return Word32Not(a);
    } else {
      return Word64Not(a);
    }
  }
  Node* WordBinaryNot(Node* a) {
    if (MACHINE()->is32()) {
      return Word32BinaryNot(a);
    } else {
      return Word64BinaryNot(a);
    }
  }

  Node* Word32And(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word32And(), a, b);
  }
  Node* Word32Or(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word32Or(), a, b);
  }
  Node* Word32Xor(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word32Xor(), a, b);
  }
  Node* Word32Shl(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word32Shl(), a, b);
  }
  Node* Word32Shr(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word32Shr(), a, b);
  }
  Node* Word32Sar(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word32Sar(), a, b);
  }
  Node* Word32Equal(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word32Equal(), a, b);
  }
  Node* Word32NotEqual(Node* a, Node* b) {
    return Word32BinaryNot(Word32Equal(a, b));
  }
  Node* Word32Not(Node* a) { return Word32Xor(a, Int32Constant(-1)); }
  Node* Word32BinaryNot(Node* a) { return Word32Equal(a, Int32Constant(0)); }

  Node* Word64And(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word64And(), a, b);
  }
  Node* Word64Or(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word64Or(), a, b);
  }
  Node* Word64Xor(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word64Xor(), a, b);
  }
  Node* Word64Shl(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word64Shl(), a, b);
  }
  Node* Word64Shr(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word64Shr(), a, b);
  }
  Node* Word64Sar(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word64Sar(), a, b);
  }
  Node* Word64Equal(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Word64Equal(), a, b);
  }
  Node* Word64NotEqual(Node* a, Node* b) {
    return Word64BinaryNot(Word64Equal(a, b));
  }
  Node* Word64Not(Node* a) { return Word64Xor(a, Int64Constant(-1)); }
  Node* Word64BinaryNot(Node* a) { return Word64Equal(a, Int64Constant(0)); }

  Node* Int32Add(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int32Add(), a, b);
  }
  Node* Int32AddWithOverflow(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int32AddWithOverflow(), a, b);
  }
  Node* Int32Sub(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int32Sub(), a, b);
  }
  Node* Int32SubWithOverflow(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int32SubWithOverflow(), a, b);
  }
  Node* Int32Mul(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int32Mul(), a, b);
  }
  Node* Int32Div(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int32Div(), a, b);
  }
  Node* Int32UDiv(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int32UDiv(), a, b);
  }
  Node* Int32Mod(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int32Mod(), a, b);
  }
  Node* Int32UMod(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int32UMod(), a, b);
  }
  Node* Int32LessThan(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int32LessThan(), a, b);
  }
  Node* Int32LessThanOrEqual(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int32LessThanOrEqual(), a, b);
  }
  Node* Uint32LessThan(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Uint32LessThan(), a, b);
  }
  Node* Uint32LessThanOrEqual(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Uint32LessThanOrEqual(), a, b);
  }
  Node* Int32GreaterThan(Node* a, Node* b) { return Int32LessThan(b, a); }
  Node* Int32GreaterThanOrEqual(Node* a, Node* b) {
    return Int32LessThanOrEqual(b, a);
  }
  Node* Int32Neg(Node* a) { return Int32Sub(Int32Constant(0), a); }

  Node* Int64Add(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int64Add(), a, b);
  }
  Node* Int64Sub(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int64Sub(), a, b);
  }
  Node* Int64Mul(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int64Mul(), a, b);
  }
  Node* Int64Div(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int64Div(), a, b);
  }
  Node* Int64UDiv(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int64UDiv(), a, b);
  }
  Node* Int64Mod(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int64Mod(), a, b);
  }
  Node* Int64UMod(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int64UMod(), a, b);
  }
  Node* Int64Neg(Node* a) { return Int64Sub(Int64Constant(0), a); }
  Node* Int64LessThan(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int64LessThan(), a, b);
  }
  Node* Int64LessThanOrEqual(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Int64LessThanOrEqual(), a, b);
  }
  Node* Int64GreaterThan(Node* a, Node* b) { return Int64LessThan(b, a); }
  Node* Int64GreaterThanOrEqual(Node* a, Node* b) {
    return Int64LessThanOrEqual(b, a);
  }

  Node* ConvertIntPtrToInt32(Node* a) {
    return kPointerSize == 8 ? NEW_NODE_1(MACHINE()->ConvertInt64ToInt32(), a)
                             : a;
  }
  Node* ConvertInt32ToIntPtr(Node* a) {
    return kPointerSize == 8 ? NEW_NODE_1(MACHINE()->ConvertInt32ToInt64(), a)
                             : a;
  }

#define INTPTR_BINOP(prefix, name)                     \
  Node* IntPtr##name(Node* a, Node* b) {               \
    return kPointerSize == 8 ? prefix##64##name(a, b)  \
                             : prefix##32##name(a, b); \
  }

  INTPTR_BINOP(Int, Add);
  INTPTR_BINOP(Int, Sub);
  INTPTR_BINOP(Int, LessThan);
  INTPTR_BINOP(Int, LessThanOrEqual);
  INTPTR_BINOP(Word, Equal);
  INTPTR_BINOP(Word, NotEqual);
  INTPTR_BINOP(Int, GreaterThanOrEqual);
  INTPTR_BINOP(Int, GreaterThan);

#undef INTPTR_BINOP

  Node* Float64Add(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Float64Add(), a, b);
  }
  Node* Float64Sub(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Float64Sub(), a, b);
  }
  Node* Float64Mul(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Float64Mul(), a, b);
  }
  Node* Float64Div(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Float64Div(), a, b);
  }
  Node* Float64Mod(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Float64Mod(), a, b);
  }
  Node* Float64Equal(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Float64Equal(), a, b);
  }
  Node* Float64NotEqual(Node* a, Node* b) {
    return WordBinaryNot(Float64Equal(a, b));
  }
  Node* Float64LessThan(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Float64LessThan(), a, b);
  }
  Node* Float64LessThanOrEqual(Node* a, Node* b) {
    return NEW_NODE_2(MACHINE()->Float64LessThanOrEqual(), a, b);
  }
  Node* Float64GreaterThan(Node* a, Node* b) { return Float64LessThan(b, a); }
  Node* Float64GreaterThanOrEqual(Node* a, Node* b) {
    return Float64LessThanOrEqual(b, a);
  }

  // Conversions.
  Node* ConvertInt32ToInt64(Node* a) {
    return NEW_NODE_1(MACHINE()->ConvertInt32ToInt64(), a);
  }
  Node* ConvertInt64ToInt32(Node* a) {
    return NEW_NODE_1(MACHINE()->ConvertInt64ToInt32(), a);
  }
  Node* ChangeInt32ToFloat64(Node* a) {
    return NEW_NODE_1(MACHINE()->ChangeInt32ToFloat64(), a);
  }
  Node* ChangeUint32ToFloat64(Node* a) {
    return NEW_NODE_1(MACHINE()->ChangeUint32ToFloat64(), a);
  }
  Node* ChangeFloat64ToInt32(Node* a) {
    return NEW_NODE_1(MACHINE()->ChangeFloat64ToInt32(), a);
  }
  Node* ChangeFloat64ToUint32(Node* a) {
    return NEW_NODE_1(MACHINE()->ChangeFloat64ToUint32(), a);
  }

#ifdef MACHINE_ASSEMBLER_SUPPORTS_CALL_C
  // Call to C.
  Node* CallC(Node* function_address, MachineType return_type,
              MachineType* arg_types, Node** args, int n_args) {
    CallDescriptor* descriptor = Linkage::GetSimplifiedCDescriptor(
        ZONE(), n_args, return_type, arg_types);
    Node** passed_args =
        static_cast<Node**>(alloca((n_args + 1) * sizeof(args[0])));
    passed_args[0] = function_address;
    for (int i = 0; i < n_args; ++i) {
      passed_args[i + 1] = args[i];
    }
    return NEW_NODE_2(COMMON()->Call(descriptor), n_args + 1, passed_args);
  }
#endif
};

#undef NEW_NODE_0
#undef NEW_NODE_1
#undef NEW_NODE_2
#undef NEW_NODE_3
#undef MACHINE
#undef COMMON
#undef ZONE

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MACHINE_NODE_FACTORY_H_
