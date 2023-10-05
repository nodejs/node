// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_EXPLICIT_TRUNCATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_EXPLICIT_TRUNCATION_REDUCER_H_

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/reduce-args-helper.h"
#include "src/compiler/turboshaft/uniform-reducer-adapter.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

// This reducer adds explicit int64 -> int32 truncation operations. This is
// needed as Turbofan does not have an explicit truncation operation.
// TODO(12783): Once the Turboshaft graph is not created from Turbofan, this
// reducer can be removed.
template <class Next>
class ExplicitTruncationReducer
    : public UniformReducerAdapter<ExplicitTruncationReducer, Next> {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  template <Opcode opcode, typename Continuation, typename... Ts>
  OpIndex ReduceOperation(Ts... args) {
    // Construct a temporary operation. The operation is needed for generic
    // access to the inputs and the inputs representation.
    using Op = typename opcode_to_operation_map<opcode>::Op;
    size_t size =
        Operation::StorageSlotCount(opcode, (0 + ... + input_count(args)));
    storage_.resize_no_init(size);
    Op* operation = new (storage_.data()) Op(args...);

    base::Vector<const MaybeRegisterRepresentation> reps =
        operation->inputs_rep(inputs_rep_storage_);
    base::Vector<OpIndex> inputs = operation->inputs();
    bool has_truncation = false;
    for (size_t i = 0; i < reps.size(); ++i) {
      if (reps[i] == MaybeRegisterRepresentation::Word32()) {
        base::Vector<const RegisterRepresentation> actual_inputs_rep =
            Asm().input_graph().Get(inputs[i]).outputs_rep();
        // We ignore any input operation that produces more than one value.
        // These cannot be consumed directly and therefore require a projection.
        // Assumption: A projection never performs an implicit truncation from
        // word64 to word32.
        if (actual_inputs_rep.size() == 1 &&
            actual_inputs_rep[0] == RegisterRepresentation::Word64()) {
          has_truncation = true;
          inputs[i] = Next::ReduceChange(inputs[i], ChangeOp::Kind::kTruncate,
                                         ChangeOp::Assumption::kNoAssumption,
                                         RegisterRepresentation::Word64(),
                                         RegisterRepresentation::Word32());
        }
      }
    }

    if (!has_truncation) {
      // Just call the regular Reduce without any remapped values.
      return Continuation{this}.Reduce(args...);
    }

    return CallWithReduceArgs([this](auto... args) {
      return Continuation{this}.Reduce(args...);
    })(*operation);
  }

 private:
  // Computes the number of inputs of an operation, ignoring non-OpIndex inputs
  // (which are always inlined in the operation) and `base::Vector<OpIndex>`
  // inputs.
  template <typename T>
  constexpr size_t input_count(T) {
    return 0;
  }
  constexpr size_t input_count() { return 0; }
  constexpr size_t input_count(OpIndex) { return 1; }
  constexpr size_t input_count(base::Vector<const OpIndex> inputs) {
    return inputs.size();
  }

  ZoneVector<MaybeRegisterRepresentation> inputs_rep_storage_{
      Asm().phase_zone()};
  base::SmallVector<OperationStorageSlot, 32> storage_;
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_EXPLICIT_TRUNCATION_REDUCER_H_
