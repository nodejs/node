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
#include "src/compiler/turboshaft/type-inference-reducer.h"
#include "src/compiler/turboshaft/types.h"
#include "src/compiler/turboshaft/uniform-reducer-adapter.h"
#include "src/heap/parked-scope.h"

namespace v8::internal::compiler::turboshaft {

struct AssertTypesReducerArgs {
  Isolate* isolate;
};

template <class Next>
class AssertTypesReducer
    : public UniformReducerAdapter<AssertTypesReducer, Next> {
  // TODO(nicohartmann@): Reenable this in a way that compiles with msvc light.
  // static_assert(next_contains_reducer<Next, TypeInferenceReducer>::value);

 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  using Adapter = UniformReducerAdapter<AssertTypesReducer, Next>;
  using ArgT =
      base::append_tuple_type<typename Next::ArgT, AssertTypesReducerArgs>;

  template <typename... Args>
  explicit AssertTypesReducer(const std::tuple<Args...>& args)
      : Adapter(args),
        isolate_(std::get<AssertTypesReducerArgs>(args).isolate) {}

  uint32_t NoContextConstant() { return IntToSmi(Context::kNoContext); }

  template <typename Op, typename Continuation>
  OpIndex ReduceInputGraphOperation(OpIndex ig_index, const Op& operation) {
    OpIndex og_index = Continuation{this}.ReduceInputGraph(ig_index, operation);
    if (!og_index.valid()) return og_index;
    if (!CanBeTyped(operation)) return og_index;
    // Unfortunately, we cannot insert assertions after block terminators, so we
    // skip them here.
    if (operation.Properties().is_block_terminator) return og_index;

    auto reps = operation.outputs_rep();
    DCHECK_GT(reps.size(), 0);
    if (reps.size() == 1) {
      Type type = Asm().GetInputGraphType(ig_index);
      InsertTypeAssert(reps[0], og_index, type);
    }
    return og_index;
  }

  void InsertTypeAssert(RegisterRepresentation rep, OpIndex value,
                        const Type& type) {
    DCHECK(!type.IsInvalid());
    if (type.IsNone()) {
      Asm().Unreachable();
      return;
    }

    if (type.IsAny()) {
      // Ignore any typed for now.
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
#ifdef DEBUG
          // Used for debugging
          if (v8_flags.turboshaft_trace_typing) {
            PrintF("Inserted assert for %3d:%-40s (%s)\n", original_value.id(),
                   Asm().output_graph().Get(original_value).ToString().c_str(),
                   type.ToString().c_str());
          }
#endif
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

 private:
  Factory* factory() { return isolate_->factory(); }
  Isolate* isolate_;
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_ASSERT_TYPES_REDUCER_H_
