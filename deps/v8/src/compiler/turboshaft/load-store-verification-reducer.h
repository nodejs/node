// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_LOAD_STORE_VERIFICATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_LOAD_STORE_VERIFICATION_REDUCER_H_

#include "src/codegen/bailout-reason.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"
#include "src/compiler/turboshaft/representations.h"
#include "v8-internal.h"

#ifdef DEBUG

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <class Next>
class StoreLoadVerificationReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(StoreLoadVerification)

  V<Any> REDUCE(Load)(OpIndex base, OptionalOpIndex index, LoadOp::Kind kind,
                      MemoryRepresentation loaded_rep,
                      RegisterRepresentation result_rep, int32_t offset,
                      uint8_t element_size_log2) {
    V<Any> loaded = Next::ReduceLoad(base, index, kind, loaded_rep, result_rep,
                                     offset, element_size_log2);

    if (v8_flags.turboshaft_verify_load_store_taggedness) {
      AbortIfWrongRepresentation(loaded, loaded_rep, "load");
    }

    return loaded;
  }

  V<None> REDUCE(Store)(OpIndex base, OptionalOpIndex index, OpIndex value,
                        StoreOp::Kind kind, MemoryRepresentation stored_rep,
                        WriteBarrierKind write_barrier, int32_t offset,
                        uint8_t element_size_log2,
                        bool maybe_initializing_or_transitioning,
                        IndirectPointerTag maybe_indirect_pointer_tag) {
    if (v8_flags.turboshaft_verify_load_store_taggedness) {
      AbortIfWrongRepresentation(value, stored_rep, "store");
    }

    return Next::ReduceStore(base, index, value, kind, stored_rep,
                             write_barrier, offset, element_size_log2,
                             maybe_initializing_or_transitioning,
                             maybe_indirect_pointer_tag);
  }

 private:
  void AbortIfWrongRepresentation(V<Any> value,
                                  MemoryRepresentation expected_repr,
                                  std::string load_or_store) {
    if (expected_repr != any_of(MemoryRepresentation::TaggedSigned(),
                                MemoryRepresentation::TaggedPointer())) {
      return;
    }

    V<Word32> is_wrong;
    AbortReason reason;
    switch (expected_repr) {
      case MemoryRepresentation::TaggedSigned(): {
        is_wrong = __ Word32Equal(__ IsSmi(V<Object>::Cast(value)), 0);
        reason = AbortReason::kOperandIsNotASmi;
        break;
      }
      case MemoryRepresentation::TaggedPointer():
        is_wrong = __ IsSmi(V<Object>::Cast(value));
        reason = AbortReason::kOperandIsASmi;
        break;
      default:
        UNREACHABLE();
    }

    IF (is_wrong) {
      if (!__ data()
               ->linkage()
               ->GetIncomingDescriptor()
               ->HasRestrictedAllocatableRegisters()) {
        // Runtime calls use fixed registers, which could clash with the
        // registers that this function is allowed to use. We could technically
        // check which specific registers are allowed to see if the runtime call
        // is doable, but it's easier to just avoid the runtime call if there
        // are any restrictions on allowed registers.

        if (v8_flags.turboshaft_enable_debug_features) {
          __ DebugPrint(std::format("{}{}{}{}", "Wrong representation in ",
                                    load_or_store,
                                    " for value with id=", value.id()));
        }
        // This phase runs after MachineLoweringReducer, so we can't use
        // `__ RunitmeAbort`, but instead we do a manual runtime call to Abort.
        __ template CallRuntime<runtime::Abort>(
            __ NoContextConstant(),
            {.messageOrMessageId = __ SmiConstant(Smi::FromEnum(reason))});
      }

      __ Unreachable();
    }
  }
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // DEBUG

#endif  // V8_COMPILER_TURBOSHAFT_LOAD_STORE_VERIFICATION_REDUCER_H_
