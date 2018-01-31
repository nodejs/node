// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_RAW_MACHINE_ASSEMBLER_H_
#define V8_COMPILER_RAW_MACHINE_ASSEMBLER_H_

#include "src/assembler.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/factory.h"
#include "src/globals.h"

namespace v8 {
namespace internal {
namespace compiler {

class BasicBlock;
class RawMachineLabel;
class Schedule;


// The RawMachineAssembler produces a low-level IR graph. All nodes are wired
// into a graph and also placed into a schedule immediately, hence subsequent
// code generation can happen without the need for scheduling.
//
// In order to create a schedule on-the-fly, the assembler keeps track of basic
// blocks by having one current basic block being populated and by referencing
// other basic blocks through the use of labels.
//
// Also note that the generated graph is only valid together with the generated
// schedule, using one without the other is invalid as the graph is inherently
// non-schedulable due to missing control and effect dependencies.
class V8_EXPORT_PRIVATE RawMachineAssembler {
 public:
  RawMachineAssembler(
      Isolate* isolate, Graph* graph, CallDescriptor* call_descriptor,
      MachineRepresentation word = MachineType::PointerRepresentation(),
      MachineOperatorBuilder::Flags flags =
          MachineOperatorBuilder::Flag::kNoFlags,
      MachineOperatorBuilder::AlignmentRequirements alignment_requirements =
          MachineOperatorBuilder::AlignmentRequirements::
              FullUnalignedAccessSupport());
  ~RawMachineAssembler() {}

  Isolate* isolate() const { return isolate_; }
  Graph* graph() const { return graph_; }
  Zone* zone() const { return graph()->zone(); }
  MachineOperatorBuilder* machine() { return &machine_; }
  CommonOperatorBuilder* common() { return &common_; }
  CallDescriptor* call_descriptor() const { return call_descriptor_; }

  // Finalizes the schedule and exports it to be used for code generation. Note
  // that this RawMachineAssembler becomes invalid after export.
  Schedule* Export();

  // ===========================================================================
  // The following utility methods create new nodes with specific operators and
  // place them into the current basic block. They don't perform control flow,
  // hence will not switch the current basic block.

  Node* NullConstant();
  Node* UndefinedConstant();

  // Constants.
  Node* PointerConstant(void* value) {
    return IntPtrConstant(reinterpret_cast<intptr_t>(value));
  }
  Node* IntPtrConstant(intptr_t value) {
    // TODO(dcarney): mark generated code as unserializable if value != 0.
    return kPointerSize == 8 ? Int64Constant(value)
                             : Int32Constant(static_cast<int>(value));
  }
  Node* RelocatableIntPtrConstant(intptr_t value, RelocInfo::Mode rmode);
  Node* Int32Constant(int32_t value) {
    return AddNode(common()->Int32Constant(value));
  }
  Node* StackSlot(MachineRepresentation rep, int alignment = 0) {
    return AddNode(machine()->StackSlot(rep, alignment));
  }
  Node* Int64Constant(int64_t value) {
    return AddNode(common()->Int64Constant(value));
  }
  Node* NumberConstant(double value) {
    return AddNode(common()->NumberConstant(value));
  }
  Node* Float32Constant(float value) {
    return AddNode(common()->Float32Constant(value));
  }
  Node* Float64Constant(double value) {
    return AddNode(common()->Float64Constant(value));
  }
  Node* HeapConstant(Handle<HeapObject> object) {
    return AddNode(common()->HeapConstant(object));
  }
  Node* BooleanConstant(bool value) {
    Handle<Object> object = isolate()->factory()->ToBoolean(value);
    return HeapConstant(Handle<HeapObject>::cast(object));
  }
  Node* ExternalConstant(ExternalReference address) {
    return AddNode(common()->ExternalConstant(address));
  }
  Node* RelocatableInt32Constant(int32_t value, RelocInfo::Mode rmode) {
    return AddNode(common()->RelocatableInt32Constant(value, rmode));
  }
  Node* RelocatableInt64Constant(int64_t value, RelocInfo::Mode rmode) {
    return AddNode(common()->RelocatableInt64Constant(value, rmode));
  }

  Node* Projection(int index, Node* a) {
    return AddNode(common()->Projection(index), a);
  }

  // Memory Operations.
  Node* Load(MachineType rep, Node* base) {
    return Load(rep, base, IntPtrConstant(0));
  }
  Node* Load(MachineType rep, Node* base, Node* index) {
    return AddNode(machine()->Load(rep), base, index);
  }
  Node* Store(MachineRepresentation rep, Node* base, Node* value,
              WriteBarrierKind write_barrier) {
    return Store(rep, base, IntPtrConstant(0), value, write_barrier);
  }
  Node* Store(MachineRepresentation rep, Node* base, Node* index, Node* value,
              WriteBarrierKind write_barrier) {
    return AddNode(machine()->Store(StoreRepresentation(rep, write_barrier)),
                   base, index, value);
  }
  Node* Retain(Node* value) { return AddNode(common()->Retain(), value); }

  // Unaligned memory operations
  Node* UnalignedLoad(MachineType type, Node* base) {
    return UnalignedLoad(type, base, IntPtrConstant(0));
  }
  Node* UnalignedLoad(MachineType type, Node* base, Node* index) {
    if (machine()->UnalignedLoadSupported(type.representation())) {
      return AddNode(machine()->Load(type), base, index);
    } else {
      return AddNode(machine()->UnalignedLoad(type), base, index);
    }
  }
  Node* UnalignedStore(MachineRepresentation rep, Node* base, Node* value) {
    return UnalignedStore(rep, base, IntPtrConstant(0), value);
  }
  Node* UnalignedStore(MachineRepresentation rep, Node* base, Node* index,
                       Node* value) {
    if (machine()->UnalignedStoreSupported(rep)) {
      return AddNode(machine()->Store(StoreRepresentation(
                         rep, WriteBarrierKind::kNoWriteBarrier)),
                     base, index, value);
    } else {
      return AddNode(
          machine()->UnalignedStore(UnalignedStoreRepresentation(rep)), base,
          index, value);
    }
  }

  // Atomic memory operations.
  Node* AtomicLoad(MachineType type, Node* base, Node* index) {
    return AddNode(machine()->AtomicLoad(type), base, index);
  }
  Node* AtomicStore(MachineRepresentation rep, Node* base, Node* index,
                    Node* value) {
    return AddNode(machine()->AtomicStore(rep), base, index, value);
  }
#define ATOMIC_FUNCTION(name)                                                 \
  Node* Atomic##name(MachineType rep, Node* base, Node* index, Node* value) { \
    return AddNode(machine()->Atomic##name(rep), base, index, value);         \
  }
  ATOMIC_FUNCTION(Exchange);
  ATOMIC_FUNCTION(Add);
  ATOMIC_FUNCTION(Sub);
  ATOMIC_FUNCTION(And);
  ATOMIC_FUNCTION(Or);
  ATOMIC_FUNCTION(Xor);
#undef ATOMIC_FUNCTION

  Node* AtomicCompareExchange(MachineType rep, Node* base, Node* index,
                              Node* old_value, Node* new_value) {
    return AddNode(machine()->AtomicCompareExchange(rep), base, index,
                   old_value, new_value);
  }

  // Arithmetic Operations.
  Node* WordAnd(Node* a, Node* b) {
    return AddNode(machine()->WordAnd(), a, b);
  }
  Node* WordOr(Node* a, Node* b) { return AddNode(machine()->WordOr(), a, b); }
  Node* WordXor(Node* a, Node* b) {
    return AddNode(machine()->WordXor(), a, b);
  }
  Node* WordShl(Node* a, Node* b) {
    return AddNode(machine()->WordShl(), a, b);
  }
  Node* WordShr(Node* a, Node* b) {
    return AddNode(machine()->WordShr(), a, b);
  }
  Node* WordSar(Node* a, Node* b) {
    return AddNode(machine()->WordSar(), a, b);
  }
  Node* WordRor(Node* a, Node* b) {
    return AddNode(machine()->WordRor(), a, b);
  }
  Node* WordEqual(Node* a, Node* b) {
    return AddNode(machine()->WordEqual(), a, b);
  }
  Node* WordNotEqual(Node* a, Node* b) {
    return Word32BinaryNot(WordEqual(a, b));
  }
  Node* WordNot(Node* a) {
    if (machine()->Is32()) {
      return Word32Not(a);
    } else {
      return Word64Not(a);
    }
  }

  Node* Word32And(Node* a, Node* b) {
    return AddNode(machine()->Word32And(), a, b);
  }
  Node* Word32Or(Node* a, Node* b) {
    return AddNode(machine()->Word32Or(), a, b);
  }
  Node* Word32Xor(Node* a, Node* b) {
    return AddNode(machine()->Word32Xor(), a, b);
  }
  Node* Word32Shl(Node* a, Node* b) {
    return AddNode(machine()->Word32Shl(), a, b);
  }
  Node* Word32Shr(Node* a, Node* b) {
    return AddNode(machine()->Word32Shr(), a, b);
  }
  Node* Word32Sar(Node* a, Node* b) {
    return AddNode(machine()->Word32Sar(), a, b);
  }
  Node* Word32Ror(Node* a, Node* b) {
    return AddNode(machine()->Word32Ror(), a, b);
  }
  Node* Word32Clz(Node* a) { return AddNode(machine()->Word32Clz(), a); }
  Node* Word32Equal(Node* a, Node* b) {
    return AddNode(machine()->Word32Equal(), a, b);
  }
  Node* Word32NotEqual(Node* a, Node* b) {
    return Word32BinaryNot(Word32Equal(a, b));
  }
  Node* Word32Not(Node* a) { return Word32Xor(a, Int32Constant(-1)); }
  Node* Word32BinaryNot(Node* a) { return Word32Equal(a, Int32Constant(0)); }

  Node* Word64And(Node* a, Node* b) {
    return AddNode(machine()->Word64And(), a, b);
  }
  Node* Word64Or(Node* a, Node* b) {
    return AddNode(machine()->Word64Or(), a, b);
  }
  Node* Word64Xor(Node* a, Node* b) {
    return AddNode(machine()->Word64Xor(), a, b);
  }
  Node* Word64Shl(Node* a, Node* b) {
    return AddNode(machine()->Word64Shl(), a, b);
  }
  Node* Word64Shr(Node* a, Node* b) {
    return AddNode(machine()->Word64Shr(), a, b);
  }
  Node* Word64Sar(Node* a, Node* b) {
    return AddNode(machine()->Word64Sar(), a, b);
  }
  Node* Word64Ror(Node* a, Node* b) {
    return AddNode(machine()->Word64Ror(), a, b);
  }
  Node* Word64Clz(Node* a) { return AddNode(machine()->Word64Clz(), a); }
  Node* Word64Equal(Node* a, Node* b) {
    return AddNode(machine()->Word64Equal(), a, b);
  }
  Node* Word64NotEqual(Node* a, Node* b) {
    return Word32BinaryNot(Word64Equal(a, b));
  }
  Node* Word64Not(Node* a) { return Word64Xor(a, Int64Constant(-1)); }

  Node* Int32Add(Node* a, Node* b) {
    return AddNode(machine()->Int32Add(), a, b);
  }
  Node* Int32AddWithOverflow(Node* a, Node* b) {
    return AddNode(machine()->Int32AddWithOverflow(), a, b);
  }
  Node* Int32Sub(Node* a, Node* b) {
    return AddNode(machine()->Int32Sub(), a, b);
  }
  Node* Int32SubWithOverflow(Node* a, Node* b) {
    return AddNode(machine()->Int32SubWithOverflow(), a, b);
  }
  Node* Int32Mul(Node* a, Node* b) {
    return AddNode(machine()->Int32Mul(), a, b);
  }
  Node* Int32MulHigh(Node* a, Node* b) {
    return AddNode(machine()->Int32MulHigh(), a, b);
  }
  Node* Int32MulWithOverflow(Node* a, Node* b) {
    return AddNode(machine()->Int32MulWithOverflow(), a, b);
  }
  Node* Int32Div(Node* a, Node* b) {
    return AddNode(machine()->Int32Div(), a, b);
  }
  Node* Int32Mod(Node* a, Node* b) {
    return AddNode(machine()->Int32Mod(), a, b);
  }
  Node* Int32LessThan(Node* a, Node* b) {
    return AddNode(machine()->Int32LessThan(), a, b);
  }
  Node* Int32LessThanOrEqual(Node* a, Node* b) {
    return AddNode(machine()->Int32LessThanOrEqual(), a, b);
  }
  Node* Uint32Div(Node* a, Node* b) {
    return AddNode(machine()->Uint32Div(), a, b);
  }
  Node* Uint32LessThan(Node* a, Node* b) {
    return AddNode(machine()->Uint32LessThan(), a, b);
  }
  Node* Uint32LessThanOrEqual(Node* a, Node* b) {
    return AddNode(machine()->Uint32LessThanOrEqual(), a, b);
  }
  Node* Uint32Mod(Node* a, Node* b) {
    return AddNode(machine()->Uint32Mod(), a, b);
  }
  Node* Uint32MulHigh(Node* a, Node* b) {
    return AddNode(machine()->Uint32MulHigh(), a, b);
  }
  Node* Int32GreaterThan(Node* a, Node* b) { return Int32LessThan(b, a); }
  Node* Int32GreaterThanOrEqual(Node* a, Node* b) {
    return Int32LessThanOrEqual(b, a);
  }
  Node* Uint32GreaterThan(Node* a, Node* b) { return Uint32LessThan(b, a); }
  Node* Uint32GreaterThanOrEqual(Node* a, Node* b) {
    return Uint32LessThanOrEqual(b, a);
  }
  Node* Int32Neg(Node* a) { return Int32Sub(Int32Constant(0), a); }

  Node* Int64Add(Node* a, Node* b) {
    return AddNode(machine()->Int64Add(), a, b);
  }
  Node* Int64AddWithOverflow(Node* a, Node* b) {
    return AddNode(machine()->Int64AddWithOverflow(), a, b);
  }
  Node* Int64Sub(Node* a, Node* b) {
    return AddNode(machine()->Int64Sub(), a, b);
  }
  Node* Int64SubWithOverflow(Node* a, Node* b) {
    return AddNode(machine()->Int64SubWithOverflow(), a, b);
  }
  Node* Int64Mul(Node* a, Node* b) {
    return AddNode(machine()->Int64Mul(), a, b);
  }
  Node* Int64Div(Node* a, Node* b) {
    return AddNode(machine()->Int64Div(), a, b);
  }
  Node* Int64Mod(Node* a, Node* b) {
    return AddNode(machine()->Int64Mod(), a, b);
  }
  Node* Int64Neg(Node* a) { return Int64Sub(Int64Constant(0), a); }
  Node* Int64LessThan(Node* a, Node* b) {
    return AddNode(machine()->Int64LessThan(), a, b);
  }
  Node* Int64LessThanOrEqual(Node* a, Node* b) {
    return AddNode(machine()->Int64LessThanOrEqual(), a, b);
  }
  Node* Uint64LessThan(Node* a, Node* b) {
    return AddNode(machine()->Uint64LessThan(), a, b);
  }
  Node* Uint64LessThanOrEqual(Node* a, Node* b) {
    return AddNode(machine()->Uint64LessThanOrEqual(), a, b);
  }
  Node* Int64GreaterThan(Node* a, Node* b) { return Int64LessThan(b, a); }
  Node* Int64GreaterThanOrEqual(Node* a, Node* b) {
    return Int64LessThanOrEqual(b, a);
  }
  Node* Uint64GreaterThan(Node* a, Node* b) { return Uint64LessThan(b, a); }
  Node* Uint64GreaterThanOrEqual(Node* a, Node* b) {
    return Uint64LessThanOrEqual(b, a);
  }
  Node* Uint64Div(Node* a, Node* b) {
    return AddNode(machine()->Uint64Div(), a, b);
  }
  Node* Uint64Mod(Node* a, Node* b) {
    return AddNode(machine()->Uint64Mod(), a, b);
  }
  Node* Int32PairAdd(Node* a_low, Node* a_high, Node* b_low, Node* b_high) {
    return AddNode(machine()->Int32PairAdd(), a_low, a_high, b_low, b_high);
  }
  Node* Int32PairSub(Node* a_low, Node* a_high, Node* b_low, Node* b_high) {
    return AddNode(machine()->Int32PairSub(), a_low, a_high, b_low, b_high);
  }
  Node* Int32PairMul(Node* a_low, Node* a_high, Node* b_low, Node* b_high) {
    return AddNode(machine()->Int32PairMul(), a_low, a_high, b_low, b_high);
  }
  Node* Word32PairShl(Node* low_word, Node* high_word, Node* shift) {
    return AddNode(machine()->Word32PairShl(), low_word, high_word, shift);
  }
  Node* Word32PairShr(Node* low_word, Node* high_word, Node* shift) {
    return AddNode(machine()->Word32PairShr(), low_word, high_word, shift);
  }
  Node* Word32PairSar(Node* low_word, Node* high_word, Node* shift) {
    return AddNode(machine()->Word32PairSar(), low_word, high_word, shift);
  }

#define INTPTR_BINOP(prefix, name)                     \
  Node* IntPtr##name(Node* a, Node* b) {               \
    return kPointerSize == 8 ? prefix##64##name(a, b)  \
                             : prefix##32##name(a, b); \
  }

  INTPTR_BINOP(Int, Add);
  INTPTR_BINOP(Int, AddWithOverflow);
  INTPTR_BINOP(Int, Sub);
  INTPTR_BINOP(Int, SubWithOverflow);
  INTPTR_BINOP(Int, Mul);
  INTPTR_BINOP(Int, Div);
  INTPTR_BINOP(Int, LessThan);
  INTPTR_BINOP(Int, LessThanOrEqual);
  INTPTR_BINOP(Word, Equal);
  INTPTR_BINOP(Word, NotEqual);
  INTPTR_BINOP(Int, GreaterThanOrEqual);
  INTPTR_BINOP(Int, GreaterThan);

#undef INTPTR_BINOP

#define UINTPTR_BINOP(prefix, name)                    \
  Node* UintPtr##name(Node* a, Node* b) {              \
    return kPointerSize == 8 ? prefix##64##name(a, b)  \
                             : prefix##32##name(a, b); \
  }

  UINTPTR_BINOP(Uint, LessThan);
  UINTPTR_BINOP(Uint, LessThanOrEqual);
  UINTPTR_BINOP(Uint, GreaterThanOrEqual);
  UINTPTR_BINOP(Uint, GreaterThan);

#undef UINTPTR_BINOP

  Node* Int32AbsWithOverflow(Node* a) {
    return AddNode(machine()->Int32AbsWithOverflow().op(), a);
  }

  Node* Int64AbsWithOverflow(Node* a) {
    return AddNode(machine()->Int64AbsWithOverflow().op(), a);
  }

  Node* IntPtrAbsWithOverflow(Node* a) {
    return kPointerSize == 8 ? Int64AbsWithOverflow(a)
                             : Int32AbsWithOverflow(a);
  }

  Node* Float32Add(Node* a, Node* b) {
    return AddNode(machine()->Float32Add(), a, b);
  }
  Node* Float32Sub(Node* a, Node* b) {
    return AddNode(machine()->Float32Sub(), a, b);
  }
  Node* Float32Mul(Node* a, Node* b) {
    return AddNode(machine()->Float32Mul(), a, b);
  }
  Node* Float32Div(Node* a, Node* b) {
    return AddNode(machine()->Float32Div(), a, b);
  }
  Node* Float32Abs(Node* a) { return AddNode(machine()->Float32Abs(), a); }
  Node* Float32Neg(Node* a) { return AddNode(machine()->Float32Neg(), a); }
  Node* Float32Sqrt(Node* a) { return AddNode(machine()->Float32Sqrt(), a); }
  Node* Float32Equal(Node* a, Node* b) {
    return AddNode(machine()->Float32Equal(), a, b);
  }
  Node* Float32NotEqual(Node* a, Node* b) {
    return Word32BinaryNot(Float32Equal(a, b));
  }
  Node* Float32LessThan(Node* a, Node* b) {
    return AddNode(machine()->Float32LessThan(), a, b);
  }
  Node* Float32LessThanOrEqual(Node* a, Node* b) {
    return AddNode(machine()->Float32LessThanOrEqual(), a, b);
  }
  Node* Float32GreaterThan(Node* a, Node* b) { return Float32LessThan(b, a); }
  Node* Float32GreaterThanOrEqual(Node* a, Node* b) {
    return Float32LessThanOrEqual(b, a);
  }
  Node* Float32Max(Node* a, Node* b) {
    return AddNode(machine()->Float32Max(), a, b);
  }
  Node* Float32Min(Node* a, Node* b) {
    return AddNode(machine()->Float32Min(), a, b);
  }
  Node* Float64Add(Node* a, Node* b) {
    return AddNode(machine()->Float64Add(), a, b);
  }
  Node* Float64Sub(Node* a, Node* b) {
    return AddNode(machine()->Float64Sub(), a, b);
  }
  Node* Float64Mul(Node* a, Node* b) {
    return AddNode(machine()->Float64Mul(), a, b);
  }
  Node* Float64Div(Node* a, Node* b) {
    return AddNode(machine()->Float64Div(), a, b);
  }
  Node* Float64Mod(Node* a, Node* b) {
    return AddNode(machine()->Float64Mod(), a, b);
  }
  Node* Float64Max(Node* a, Node* b) {
    return AddNode(machine()->Float64Max(), a, b);
  }
  Node* Float64Min(Node* a, Node* b) {
    return AddNode(machine()->Float64Min(), a, b);
  }
  Node* Float64Abs(Node* a) { return AddNode(machine()->Float64Abs(), a); }
  Node* Float64Neg(Node* a) { return AddNode(machine()->Float64Neg(), a); }
  Node* Float64Acos(Node* a) { return AddNode(machine()->Float64Acos(), a); }
  Node* Float64Acosh(Node* a) { return AddNode(machine()->Float64Acosh(), a); }
  Node* Float64Asin(Node* a) { return AddNode(machine()->Float64Asin(), a); }
  Node* Float64Asinh(Node* a) { return AddNode(machine()->Float64Asinh(), a); }
  Node* Float64Atan(Node* a) { return AddNode(machine()->Float64Atan(), a); }
  Node* Float64Atanh(Node* a) { return AddNode(machine()->Float64Atanh(), a); }
  Node* Float64Atan2(Node* a, Node* b) {
    return AddNode(machine()->Float64Atan2(), a, b);
  }
  Node* Float64Cbrt(Node* a) { return AddNode(machine()->Float64Cbrt(), a); }
  Node* Float64Cos(Node* a) { return AddNode(machine()->Float64Cos(), a); }
  Node* Float64Cosh(Node* a) { return AddNode(machine()->Float64Cosh(), a); }
  Node* Float64Exp(Node* a) { return AddNode(machine()->Float64Exp(), a); }
  Node* Float64Expm1(Node* a) { return AddNode(machine()->Float64Expm1(), a); }
  Node* Float64Log(Node* a) { return AddNode(machine()->Float64Log(), a); }
  Node* Float64Log1p(Node* a) { return AddNode(machine()->Float64Log1p(), a); }
  Node* Float64Log10(Node* a) { return AddNode(machine()->Float64Log10(), a); }
  Node* Float64Log2(Node* a) { return AddNode(machine()->Float64Log2(), a); }
  Node* Float64Pow(Node* a, Node* b) {
    return AddNode(machine()->Float64Pow(), a, b);
  }
  Node* Float64Sin(Node* a) { return AddNode(machine()->Float64Sin(), a); }
  Node* Float64Sinh(Node* a) { return AddNode(machine()->Float64Sinh(), a); }
  Node* Float64Sqrt(Node* a) { return AddNode(machine()->Float64Sqrt(), a); }
  Node* Float64Tan(Node* a) { return AddNode(machine()->Float64Tan(), a); }
  Node* Float64Tanh(Node* a) { return AddNode(machine()->Float64Tanh(), a); }
  Node* Float64Equal(Node* a, Node* b) {
    return AddNode(machine()->Float64Equal(), a, b);
  }
  Node* Float64NotEqual(Node* a, Node* b) {
    return Word32BinaryNot(Float64Equal(a, b));
  }
  Node* Float64LessThan(Node* a, Node* b) {
    return AddNode(machine()->Float64LessThan(), a, b);
  }
  Node* Float64LessThanOrEqual(Node* a, Node* b) {
    return AddNode(machine()->Float64LessThanOrEqual(), a, b);
  }
  Node* Float64GreaterThan(Node* a, Node* b) { return Float64LessThan(b, a); }
  Node* Float64GreaterThanOrEqual(Node* a, Node* b) {
    return Float64LessThanOrEqual(b, a);
  }

  // Conversions.
  Node* BitcastTaggedToWord(Node* a) {
#ifdef ENABLE_VERIFY_CSA
    return AddNode(machine()->BitcastTaggedToWord(), a);
#else
    return a;
#endif
  }
  Node* BitcastWordToTagged(Node* a) {
    return AddNode(machine()->BitcastWordToTagged(), a);
  }
  Node* BitcastWordToTaggedSigned(Node* a) {
#ifdef ENABLE_VERIFY_CSA
    return AddNode(machine()->BitcastWordToTaggedSigned(), a);
#else
    return a;
#endif
  }
  Node* TruncateFloat64ToWord32(Node* a) {
    return AddNode(machine()->TruncateFloat64ToWord32(), a);
  }
  Node* ChangeFloat32ToFloat64(Node* a) {
    return AddNode(machine()->ChangeFloat32ToFloat64(), a);
  }
  Node* ChangeInt32ToFloat64(Node* a) {
    return AddNode(machine()->ChangeInt32ToFloat64(), a);
  }
  Node* ChangeUint32ToFloat64(Node* a) {
    return AddNode(machine()->ChangeUint32ToFloat64(), a);
  }
  Node* ChangeFloat64ToInt32(Node* a) {
    return AddNode(machine()->ChangeFloat64ToInt32(), a);
  }
  Node* ChangeFloat64ToUint32(Node* a) {
    return AddNode(machine()->ChangeFloat64ToUint32(), a);
  }
  Node* ChangeFloat64ToUint64(Node* a) {
    return AddNode(machine()->ChangeFloat64ToUint64(), a);
  }
  Node* TruncateFloat64ToUint32(Node* a) {
    return AddNode(machine()->TruncateFloat64ToUint32(), a);
  }
  Node* TruncateFloat32ToInt32(Node* a) {
    return AddNode(machine()->TruncateFloat32ToInt32(), a);
  }
  Node* TruncateFloat32ToUint32(Node* a) {
    return AddNode(machine()->TruncateFloat32ToUint32(), a);
  }
  Node* TryTruncateFloat32ToInt64(Node* a) {
    return AddNode(machine()->TryTruncateFloat32ToInt64(), a);
  }
  Node* TryTruncateFloat64ToInt64(Node* a) {
    return AddNode(machine()->TryTruncateFloat64ToInt64(), a);
  }
  Node* TryTruncateFloat32ToUint64(Node* a) {
    return AddNode(machine()->TryTruncateFloat32ToUint64(), a);
  }
  Node* TryTruncateFloat64ToUint64(Node* a) {
    return AddNode(machine()->TryTruncateFloat64ToUint64(), a);
  }
  Node* ChangeInt32ToInt64(Node* a) {
    return AddNode(machine()->ChangeInt32ToInt64(), a);
  }
  Node* ChangeUint32ToUint64(Node* a) {
    return AddNode(machine()->ChangeUint32ToUint64(), a);
  }
  Node* TruncateFloat64ToFloat32(Node* a) {
    return AddNode(machine()->TruncateFloat64ToFloat32(), a);
  }
  Node* TruncateInt64ToInt32(Node* a) {
    return AddNode(machine()->TruncateInt64ToInt32(), a);
  }
  Node* RoundFloat64ToInt32(Node* a) {
    return AddNode(machine()->RoundFloat64ToInt32(), a);
  }
  Node* RoundInt32ToFloat32(Node* a) {
    return AddNode(machine()->RoundInt32ToFloat32(), a);
  }
  Node* RoundInt64ToFloat32(Node* a) {
    return AddNode(machine()->RoundInt64ToFloat32(), a);
  }
  Node* RoundInt64ToFloat64(Node* a) {
    return AddNode(machine()->RoundInt64ToFloat64(), a);
  }
  Node* RoundUint32ToFloat32(Node* a) {
    return AddNode(machine()->RoundUint32ToFloat32(), a);
  }
  Node* RoundUint64ToFloat32(Node* a) {
    return AddNode(machine()->RoundUint64ToFloat32(), a);
  }
  Node* RoundUint64ToFloat64(Node* a) {
    return AddNode(machine()->RoundUint64ToFloat64(), a);
  }
  Node* BitcastFloat32ToInt32(Node* a) {
    return AddNode(machine()->BitcastFloat32ToInt32(), a);
  }
  Node* BitcastFloat64ToInt64(Node* a) {
    return AddNode(machine()->BitcastFloat64ToInt64(), a);
  }
  Node* BitcastInt32ToFloat32(Node* a) {
    return AddNode(machine()->BitcastInt32ToFloat32(), a);
  }
  Node* BitcastInt64ToFloat64(Node* a) {
    return AddNode(machine()->BitcastInt64ToFloat64(), a);
  }
  Node* Float32RoundDown(Node* a) {
    return AddNode(machine()->Float32RoundDown().op(), a);
  }
  Node* Float64RoundDown(Node* a) {
    return AddNode(machine()->Float64RoundDown().op(), a);
  }
  Node* Float32RoundUp(Node* a) {
    return AddNode(machine()->Float32RoundUp().op(), a);
  }
  Node* Float64RoundUp(Node* a) {
    return AddNode(machine()->Float64RoundUp().op(), a);
  }
  Node* Float32RoundTruncate(Node* a) {
    return AddNode(machine()->Float32RoundTruncate().op(), a);
  }
  Node* Float64RoundTruncate(Node* a) {
    return AddNode(machine()->Float64RoundTruncate().op(), a);
  }
  Node* Float64RoundTiesAway(Node* a) {
    return AddNode(machine()->Float64RoundTiesAway().op(), a);
  }
  Node* Float32RoundTiesEven(Node* a) {
    return AddNode(machine()->Float32RoundTiesEven().op(), a);
  }
  Node* Float64RoundTiesEven(Node* a) {
    return AddNode(machine()->Float64RoundTiesEven().op(), a);
  }
  Node* Word32ReverseBytes(Node* a) {
    return AddNode(machine()->Word32ReverseBytes().op(), a);
  }
  Node* Word64ReverseBytes(Node* a) {
    return AddNode(machine()->Word64ReverseBytes().op(), a);
  }

  // Float64 bit operations.
  Node* Float64ExtractLowWord32(Node* a) {
    return AddNode(machine()->Float64ExtractLowWord32(), a);
  }
  Node* Float64ExtractHighWord32(Node* a) {
    return AddNode(machine()->Float64ExtractHighWord32(), a);
  }
  Node* Float64InsertLowWord32(Node* a, Node* b) {
    return AddNode(machine()->Float64InsertLowWord32(), a, b);
  }
  Node* Float64InsertHighWord32(Node* a, Node* b) {
    return AddNode(machine()->Float64InsertHighWord32(), a, b);
  }
  Node* Float64SilenceNaN(Node* a) {
    return AddNode(machine()->Float64SilenceNaN(), a);
  }

  // Stack operations.
  Node* LoadStackPointer() { return AddNode(machine()->LoadStackPointer()); }
  Node* LoadFramePointer() { return AddNode(machine()->LoadFramePointer()); }
  Node* LoadParentFramePointer() {
    return AddNode(machine()->LoadParentFramePointer());
  }

  // Parameters.
  Node* Parameter(size_t index);

  // Pointer utilities.
  Node* LoadFromPointer(void* address, MachineType rep, int32_t offset = 0) {
    return Load(rep, PointerConstant(address), Int32Constant(offset));
  }
  Node* StoreToPointer(void* address, MachineRepresentation rep, Node* node) {
    return Store(rep, PointerConstant(address), node, kNoWriteBarrier);
  }
  Node* UnalignedLoadFromPointer(void* address, MachineType rep,
                                 int32_t offset = 0) {
    return UnalignedLoad(rep, PointerConstant(address), Int32Constant(offset));
  }
  Node* UnalignedStoreToPointer(void* address, MachineRepresentation rep,
                                Node* node) {
    return UnalignedStore(rep, PointerConstant(address), node);
  }
  Node* StringConstant(const char* string) {
    return HeapConstant(isolate()->factory()->InternalizeUtf8String(string));
  }

  // Call a given call descriptor and the given arguments.
  // The call target is passed as part of the {inputs} array.
  Node* CallN(CallDescriptor* desc, int input_count, Node* const* inputs);

  // Call a given call descriptor and the given arguments and frame-state.
  // The call target and frame state are passed as part of the {inputs} array.
  Node* CallNWithFrameState(CallDescriptor* desc, int input_count,
                            Node* const* inputs);

  // Tail call a given call descriptor and the given arguments.
  // The call target is passed as part of the {inputs} array.
  Node* TailCallN(CallDescriptor* desc, int input_count, Node* const* inputs);

  // Call to a C function with zero arguments.
  Node* CallCFunction0(MachineType return_type, Node* function);
  // Call to a C function with one parameter.
  Node* CallCFunction1(MachineType return_type, MachineType arg0_type,
                       Node* function, Node* arg0);
  // Call to a C function with one argument, while saving/restoring caller
  // registers.
  Node* CallCFunction1WithCallerSavedRegisters(
      MachineType return_type, MachineType arg0_type, Node* function,
      Node* arg0, SaveFPRegsMode mode = kSaveFPRegs);
  // Call to a C function with two arguments.
  Node* CallCFunction2(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, Node* function, Node* arg0,
                       Node* arg1);
  // Call to a C function with three arguments.
  Node* CallCFunction3(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, MachineType arg2_type,
                       Node* function, Node* arg0, Node* arg1, Node* arg2);
  // Call to a C function with three arguments, while saving/restoring caller
  // registers.
  Node* CallCFunction3WithCallerSavedRegisters(
      MachineType return_type, MachineType arg0_type, MachineType arg1_type,
      MachineType arg2_type, Node* function, Node* arg0, Node* arg1, Node* arg2,
      SaveFPRegsMode mode = kSaveFPRegs);
  // Call to a C function with four arguments.
  Node* CallCFunction4(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, MachineType arg2_type,
                       MachineType arg3_type, Node* function, Node* arg0,
                       Node* arg1, Node* arg2, Node* arg3);
  // Call to a C function with five arguments.
  Node* CallCFunction5(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, MachineType arg2_type,
                       MachineType arg3_type, MachineType arg4_type,
                       Node* function, Node* arg0, Node* arg1, Node* arg2,
                       Node* arg3, Node* arg4);
  // Call to a C function with six arguments.
  Node* CallCFunction6(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, MachineType arg2_type,
                       MachineType arg3_type, MachineType arg4_type,
                       MachineType arg5_type, Node* function, Node* arg0,
                       Node* arg1, Node* arg2, Node* arg3, Node* arg4,
                       Node* arg5);
  // Call to a C function with eight arguments.
  Node* CallCFunction8(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, MachineType arg2_type,
                       MachineType arg3_type, MachineType arg4_type,
                       MachineType arg5_type, MachineType arg6_type,
                       MachineType arg7_type, Node* function, Node* arg0,
                       Node* arg1, Node* arg2, Node* arg3, Node* arg4,
                       Node* arg5, Node* arg6, Node* arg7);
  // Call to a C function with nine arguments.
  Node* CallCFunction9(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, MachineType arg2_type,
                       MachineType arg3_type, MachineType arg4_type,
                       MachineType arg5_type, MachineType arg6_type,
                       MachineType arg7_type, MachineType arg8_type,
                       Node* function, Node* arg0, Node* arg1, Node* arg2,
                       Node* arg3, Node* arg4, Node* arg5, Node* arg6,
                       Node* arg7, Node* arg8);

  // ===========================================================================
  // The following utility methods deal with control flow, hence might switch
  // the current basic block or create new basic blocks for labels.

  // Control flow.
  void Goto(RawMachineLabel* label);
  void Branch(Node* condition, RawMachineLabel* true_val,
              RawMachineLabel* false_val);
  void Switch(Node* index, RawMachineLabel* default_label,
              const int32_t* case_values, RawMachineLabel** case_labels,
              size_t case_count);
  void Return(Node* value);
  void Return(Node* v1, Node* v2);
  void Return(Node* v1, Node* v2, Node* v3);
  void PopAndReturn(Node* pop, Node* value);
  void PopAndReturn(Node* pop, Node* v1, Node* v2);
  void PopAndReturn(Node* pop, Node* v1, Node* v2, Node* v3);
  void Bind(RawMachineLabel* label);
  void Deoptimize(Node* state);
  void DebugAbort(Node* message);
  void DebugBreak();
  void Unreachable();
  void Comment(const char* msg);

#if DEBUG
  void Bind(RawMachineLabel* label, AssemblerDebugInfo info);
  void SetInitialDebugInformation(AssemblerDebugInfo info);
  void PrintCurrentBlock(std::ostream& os);
  bool InsideBlock();
#endif  // DEBUG

  // Add success / exception successor blocks and ends the current block ending
  // in a potentially throwing call node.
  void Continuations(Node* call, RawMachineLabel* if_success,
                     RawMachineLabel* if_exception);

  // Variables.
  Node* Phi(MachineRepresentation rep, Node* n1, Node* n2) {
    return AddNode(common()->Phi(rep, 2), n1, n2, graph()->start());
  }
  Node* Phi(MachineRepresentation rep, Node* n1, Node* n2, Node* n3) {
    return AddNode(common()->Phi(rep, 3), n1, n2, n3, graph()->start());
  }
  Node* Phi(MachineRepresentation rep, Node* n1, Node* n2, Node* n3, Node* n4) {
    return AddNode(common()->Phi(rep, 4), n1, n2, n3, n4, graph()->start());
  }
  Node* Phi(MachineRepresentation rep, int input_count, Node* const* inputs);
  void AppendPhiInput(Node* phi, Node* new_input);

  // ===========================================================================
  // The following generic node creation methods can be used for operators that
  // are not covered by the above utility methods. There should rarely be a need
  // to do that outside of testing though.

  Node* AddNode(const Operator* op, int input_count, Node* const* inputs);

  Node* AddNode(const Operator* op) {
    return AddNode(op, 0, static_cast<Node* const*>(nullptr));
  }

  template <class... TArgs>
  Node* AddNode(const Operator* op, Node* n1, TArgs... args) {
    Node* buffer[] = {n1, args...};
    return AddNode(op, sizeof...(args) + 1, buffer);
  }

 private:
  Node* MakeNode(const Operator* op, int input_count, Node* const* inputs);
  BasicBlock* Use(RawMachineLabel* label);
  BasicBlock* EnsureBlock(RawMachineLabel* label);
  BasicBlock* CurrentBlock();

  Schedule* schedule() { return schedule_; }
  size_t parameter_count() const { return call_descriptor_->ParameterCount(); }

  Isolate* isolate_;
  Graph* graph_;
  Schedule* schedule_;
  MachineOperatorBuilder machine_;
  CommonOperatorBuilder common_;
  CallDescriptor* call_descriptor_;
  NodeVector parameters_;
  BasicBlock* current_block_;

  DISALLOW_COPY_AND_ASSIGN(RawMachineAssembler);
};

class V8_EXPORT_PRIVATE RawMachineLabel final {
 public:
  enum Type { kDeferred, kNonDeferred };

  explicit RawMachineLabel(Type type = kNonDeferred)
      : deferred_(type == kDeferred) {}
  ~RawMachineLabel();

  BasicBlock* block() const { return block_; }

 private:
  BasicBlock* block_ = nullptr;
  bool used_ = false;
  bool bound_ = false;
  bool deferred_;
  friend class RawMachineAssembler;
  DISALLOW_COPY_AND_ASSIGN(RawMachineLabel);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_RAW_MACHINE_ASSEMBLER_H_
