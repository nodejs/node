// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_ASSERT_TYPES_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_ASSERT_TYPES_REDUCER_H_

#include <limits>

#include "src/base/logging.h"
#include "src/base/template-utils.h"
#include "src/base/vector.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/frame.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/sidetable.h"
#include "src/compiler/turboshaft/types.h"
#include "src/heap/parked-scope.h"

namespace v8::internal::compiler::turboshaft {

class DetectReentranceScope {
 public:
  explicit DetectReentranceScope(bool* flag)
      : is_reentrant_(*flag), flag_(flag) {
    *flag_ = true;
  }
  ~DetectReentranceScope() { *flag_ = is_reentrant_; }
  bool IsReentrant() const { return is_reentrant_; }

 private:
  bool is_reentrant_;
  bool* flag_;
};

struct AssertTypesReducerArgs {
  Isolate* isolate;
};

template <class Next>
class AssertTypesReducer : public Next {
 public:
  using Next::Asm;
  using ArgT =
      base::append_tuple_type<typename Next::ArgT, AssertTypesReducerArgs>;

  template <typename... Args>
  explicit AssertTypesReducer(const std::tuple<Args...>& args)
      : Next(args), isolate_(std::get<AssertTypesReducerArgs>(args).isolate) {}

  uint32_t NoContextConstant() { return IntToSmi(Context::kNoContext); }

  OpIndex ReducePhi(base::Vector<const OpIndex> inputs,
                    RegisterRepresentation rep) {
    OpIndex index = Next::ReducePhi(inputs, rep);
    if (!index.valid()) return index;

    Type type = TypeOf(index);
    if (type.IsInvalid()) return index;
    // For now allow Type::Any().
    // TODO(nicohartmann@): Remove this once all operations are supported.
    if (type.IsAny()) return index;

    DetectReentranceScope reentrance_scope(&emitting_asserts_);
    DCHECK(!reentrance_scope.IsReentrant());

    InsertTypeAssert(rep, index, type);
    return index;
  }

  OpIndex ReduceConstant(ConstantOp::Kind kind, ConstantOp::Storage value) {
    OpIndex index = Next::ReduceConstant(kind, value);
    if (!index.valid()) return index;

    Type type = TypeOf(index);
    if (type.IsInvalid()) return index;

    DetectReentranceScope reentrance_scope(&emitting_asserts_);
    if (reentrance_scope.IsReentrant()) return index;

    RegisterRepresentation rep = ConstantOp::Representation(kind);
    switch (kind) {
      case ConstantOp::Kind::kWord32:
      case ConstantOp::Kind::kWord64:
      case ConstantOp::Kind::kFloat32:
      case ConstantOp::Kind::kFloat64:
        InsertTypeAssert(rep, index, type);
        break;
      case ConstantOp::Kind::kNumber:
      case ConstantOp::Kind::kTaggedIndex:
      case ConstantOp::Kind::kExternal:
      case ConstantOp::Kind::kHeapObject:
      case ConstantOp::Kind::kCompressedHeapObject:
      case ConstantOp::Kind::kRelocatableWasmCall:
      case ConstantOp::Kind::kRelocatableWasmStubCall:
        // TODO(nicohartmann@): Support remaining {kind}s.
        UNIMPLEMENTED();
    }
    return index;
  }

  OpIndex ReduceWordBinop(OpIndex left, OpIndex right, WordBinopOp::Kind kind,
                          WordRepresentation rep) {
    OpIndex index = Next::ReduceWordBinop(left, right, kind, rep);
    if (!index.valid()) return index;

    Type type = TypeOf(index);
    if (type.IsInvalid()) return index;

    DetectReentranceScope reentrance_scope(&emitting_asserts_);
    DCHECK(!reentrance_scope.IsReentrant());

    InsertTypeAssert(rep, index, type);
    return index;
  }

  OpIndex ReduceFloatBinop(OpIndex left, OpIndex right, FloatBinopOp::Kind kind,
                           FloatRepresentation rep) {
    OpIndex index = Next::ReduceFloatBinop(left, right, kind, rep);
    if (!index.valid()) return index;

    Type type = TypeOf(index);
    if (type.IsInvalid()) return index;

    DetectReentranceScope reentrance_scope(&emitting_asserts_);
    DCHECK(!reentrance_scope.IsReentrant());

    InsertTypeAssert(rep, index, type);
    return index;
  }

  void InsertTypeAssert(RegisterRepresentation rep, OpIndex value,
                        const Type& type) {
    DCHECK(!type.IsInvalid());
    if (type.IsNone()) {
      Asm().Unreachable();
      return;
    }

    auto GenerateBuiltinCall =
        [this](Builtin builtin, OpIndex original_value,
               base::SmallVector<OpIndex, 6> actual_value_indices,
               const Type& type) {
          uint32_t op_id = static_cast<uint32_t>(IntToSmi(original_value.id()));
          // Add expected type and operation id.
          Handle<TurboshaftType> expected_type = type.AllocateOnHeap(factory());
          actual_value_indices.push_back(Asm().HeapConstant(expected_type));
          actual_value_indices.push_back(Asm().Word32Constant(op_id));
          actual_value_indices.push_back(
              Asm().Word32Constant(NoContextConstant()));
          Asm().CallBuiltin(
              builtin, OpIndex::Invalid(),
              {actual_value_indices.data(), actual_value_indices.size()},
              isolate_);
          // Used for debugging
          // PrintF("Inserted assert for %3d:%-40s (%s)\n", original_value.id(),
          //        Asm().output_graph().Get(original_value).ToString().c_str(),
          //        type.ToString().c_str());
        };

    switch (rep.value()) {
      case RegisterRepresentation::Word32(): {
        DCHECK(type.IsWord32());
        base::SmallVector<OpIndex, 6> actual_value_indices = {value};
        GenerateBuiltinCall(Builtin::kCheckTurboshaftWord32Type, value,
                            std::move(actual_value_indices), type);
        break;
      }
      case RegisterRepresentation::Word64(): {
        DCHECK(type.IsWord64());
        OpIndex value_high = Asm().Word64ShiftRightLogical(
            value, Asm().Word64Constant(static_cast<uint64_t>(32)));
        OpIndex value_low = value;  // Use implicit truncation to word32.
        base::SmallVector<OpIndex, 6> actual_value_indices = {value_high,
                                                              value_low};
        GenerateBuiltinCall(Builtin::kCheckTurboshaftWord64Type, value,
                            std::move(actual_value_indices), type);
        break;
      }
      case RegisterRepresentation::Float32(): {
        DCHECK(type.IsFloat32());
        base::SmallVector<OpIndex, 6> actual_value_indices = {value};
        GenerateBuiltinCall(Builtin::kCheckTurboshaftFloat32Type, value,
                            std::move(actual_value_indices), type);
        break;
      }
      case RegisterRepresentation::Float64(): {
        DCHECK(type.IsFloat64());
        base::SmallVector<OpIndex, 6> actual_value_indices = {value};
        GenerateBuiltinCall(Builtin::kCheckTurboshaftFloat64Type, value,
                            std::move(actual_value_indices), type);
        break;
      }
      case RegisterRepresentation::Tagged():
      case RegisterRepresentation::Compressed():
        // TODO(nicohartmann@): Handle remaining cases.
        break;
    }
  }

  Type TypeOf(const OpIndex index) {
    return Asm().output_graph().operation_types()[index];
  }

 private:
  Factory* factory() { return isolate_->factory(); }
  Isolate* isolate_;
  bool emitting_asserts_ = false;
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_ASSERT_TYPES_REDUCER_H_
