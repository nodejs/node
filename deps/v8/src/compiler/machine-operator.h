// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MACHINE_OPERATOR_H_
#define V8_COMPILER_MACHINE_OPERATOR_H_

#include "src/compiler/machine-type.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

// TODO(turbofan): other write barriers are possible based on type
enum WriteBarrierKind { kNoWriteBarrier, kFullWriteBarrier };


// A Store needs a MachineType and a WriteBarrierKind
// in order to emit the correct write barrier.
struct StoreRepresentation {
  MachineType rep;
  WriteBarrierKind write_barrier_kind;
};


// Interface for building machine-level operators. These operators are
// machine-level but machine-independent and thus define a language suitable
// for generating code to run on architectures such as ia32, x64, arm, etc.
class MachineOperatorBuilder {
 public:
  explicit MachineOperatorBuilder(Zone* zone, MachineType word = pointer_rep())
      : zone_(zone), word_(word) {
    CHECK(word == kMachineWord32 || word == kMachineWord64);
  }

#define SIMPLE(name, properties, inputs, outputs) \
  return new (zone_)                              \
      SimpleOperator(IrOpcode::k##name, properties, inputs, outputs, #name);

#define OP1(name, ptype, pname, properties, inputs, outputs)               \
  return new (zone_)                                                       \
      Operator1<ptype>(IrOpcode::k##name, properties | Operator::kNoThrow, \
                       inputs, outputs, #name, pname)

#define BINOP(name) SIMPLE(name, Operator::kPure, 2, 1)
#define BINOP_O(name) SIMPLE(name, Operator::kPure, 2, 2)
#define BINOP_C(name) \
  SIMPLE(name, Operator::kCommutative | Operator::kPure, 2, 1)
#define BINOP_AC(name)                                                         \
  SIMPLE(name,                                                                 \
         Operator::kAssociative | Operator::kCommutative | Operator::kPure, 2, \
         1)
#define BINOP_ACO(name)                                                        \
  SIMPLE(name,                                                                 \
         Operator::kAssociative | Operator::kCommutative | Operator::kPure, 2, \
         2)
#define UNOP(name) SIMPLE(name, Operator::kPure, 1, 1)

#define WORD_SIZE(x) return is64() ? Word64##x() : Word32##x()

  Operator* Load(MachineType rep) {  // load [base + index]
    OP1(Load, MachineType, rep, Operator::kNoWrite, 2, 1);
  }
  // store [base + index], value
  Operator* Store(MachineType rep, WriteBarrierKind kind) {
    StoreRepresentation store_rep = {rep, kind};
    OP1(Store, StoreRepresentation, store_rep, Operator::kNoRead, 3, 0);
  }

  Operator* WordAnd() { WORD_SIZE(And); }
  Operator* WordOr() { WORD_SIZE(Or); }
  Operator* WordXor() { WORD_SIZE(Xor); }
  Operator* WordShl() { WORD_SIZE(Shl); }
  Operator* WordShr() { WORD_SIZE(Shr); }
  Operator* WordSar() { WORD_SIZE(Sar); }
  Operator* WordEqual() { WORD_SIZE(Equal); }

  Operator* Word32And() { BINOP_AC(Word32And); }
  Operator* Word32Or() { BINOP_AC(Word32Or); }
  Operator* Word32Xor() { BINOP_AC(Word32Xor); }
  Operator* Word32Shl() { BINOP(Word32Shl); }
  Operator* Word32Shr() { BINOP(Word32Shr); }
  Operator* Word32Sar() { BINOP(Word32Sar); }
  Operator* Word32Equal() { BINOP_C(Word32Equal); }

  Operator* Word64And() { BINOP_AC(Word64And); }
  Operator* Word64Or() { BINOP_AC(Word64Or); }
  Operator* Word64Xor() { BINOP_AC(Word64Xor); }
  Operator* Word64Shl() { BINOP(Word64Shl); }
  Operator* Word64Shr() { BINOP(Word64Shr); }
  Operator* Word64Sar() { BINOP(Word64Sar); }
  Operator* Word64Equal() { BINOP_C(Word64Equal); }

  Operator* Int32Add() { BINOP_AC(Int32Add); }
  Operator* Int32AddWithOverflow() { BINOP_ACO(Int32AddWithOverflow); }
  Operator* Int32Sub() { BINOP(Int32Sub); }
  Operator* Int32SubWithOverflow() { BINOP_O(Int32SubWithOverflow); }
  Operator* Int32Mul() { BINOP_AC(Int32Mul); }
  Operator* Int32Div() { BINOP(Int32Div); }
  Operator* Int32UDiv() { BINOP(Int32UDiv); }
  Operator* Int32Mod() { BINOP(Int32Mod); }
  Operator* Int32UMod() { BINOP(Int32UMod); }
  Operator* Int32LessThan() { BINOP(Int32LessThan); }
  Operator* Int32LessThanOrEqual() { BINOP(Int32LessThanOrEqual); }
  Operator* Uint32LessThan() { BINOP(Uint32LessThan); }
  Operator* Uint32LessThanOrEqual() { BINOP(Uint32LessThanOrEqual); }

  Operator* Int64Add() { BINOP_AC(Int64Add); }
  Operator* Int64Sub() { BINOP(Int64Sub); }
  Operator* Int64Mul() { BINOP_AC(Int64Mul); }
  Operator* Int64Div() { BINOP(Int64Div); }
  Operator* Int64UDiv() { BINOP(Int64UDiv); }
  Operator* Int64Mod() { BINOP(Int64Mod); }
  Operator* Int64UMod() { BINOP(Int64UMod); }
  Operator* Int64LessThan() { BINOP(Int64LessThan); }
  Operator* Int64LessThanOrEqual() { BINOP(Int64LessThanOrEqual); }

  Operator* ConvertInt32ToInt64() { UNOP(ConvertInt32ToInt64); }
  Operator* ConvertInt64ToInt32() { UNOP(ConvertInt64ToInt32); }

  // Convert representation of integers between float64 and int32/uint32.
  // The precise rounding mode and handling of out of range inputs are *not*
  // defined for these operators, since they are intended only for use with
  // integers.
  // TODO(titzer): rename ConvertXXX to ChangeXXX in machine operators.
  Operator* ChangeInt32ToFloat64() { UNOP(ChangeInt32ToFloat64); }
  Operator* ChangeUint32ToFloat64() { UNOP(ChangeUint32ToFloat64); }
  Operator* ChangeFloat64ToInt32() { UNOP(ChangeFloat64ToInt32); }
  Operator* ChangeFloat64ToUint32() { UNOP(ChangeFloat64ToUint32); }

  // Floating point operators always operate with IEEE 754 round-to-nearest.
  Operator* Float64Add() { BINOP_C(Float64Add); }
  Operator* Float64Sub() { BINOP(Float64Sub); }
  Operator* Float64Mul() { BINOP_C(Float64Mul); }
  Operator* Float64Div() { BINOP(Float64Div); }
  Operator* Float64Mod() { BINOP(Float64Mod); }

  // Floating point comparisons complying to IEEE 754.
  Operator* Float64Equal() { BINOP_C(Float64Equal); }
  Operator* Float64LessThan() { BINOP(Float64LessThan); }
  Operator* Float64LessThanOrEqual() { BINOP(Float64LessThanOrEqual); }

  inline bool is32() const { return word_ == kMachineWord32; }
  inline bool is64() const { return word_ == kMachineWord64; }
  inline MachineType word() const { return word_; }

  static inline MachineType pointer_rep() {
    return kPointerSize == 8 ? kMachineWord64 : kMachineWord32;
  }

#undef WORD_SIZE
#undef UNOP
#undef BINOP
#undef OP1
#undef SIMPLE

 private:
  Zone* zone_;
  MachineType word_;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_MACHINE_OPERATOR_H_
