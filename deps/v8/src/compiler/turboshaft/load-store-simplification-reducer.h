// Copyright 2023 the V8 project authors. All rights reserved.

// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_LOAD_STORE_SIMPLIFICATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_LOAD_STORE_SIMPLIFICATION_REDUCER_H_

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/operation-matcher.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/phase.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

// This reducer simplifies Turboshaft's "complex" loads and stores into
// simplified ones that are supported on the given target architecture.
template <class Next>
class LoadStoreSimplificationReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  // Turboshaft's loads and stores follow the pattern of
  // *(base + index * element_size_log2 + displacement), but architectures
  // typically support only a limited `element_size_log2`.
#if V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_RISCV64 || \
    V8_TARGET_ARCH_LOONG64 || V8_TARGET_ARCH_MIPS64
  static constexpr int kMaxElementSizeLog2 = 0;
#else
  static constexpr int kMaxElementSizeLog2 = 3;
#endif

  OpIndex REDUCE(Load)(OpIndex base, OptionalOpIndex index, LoadOp::Kind kind,
                       MemoryRepresentation loaded_rep,
                       RegisterRepresentation result_rep, int32_t offset,
                       uint8_t element_size_log2) {
    SimplifyLoadStore(base, index, kind, offset, element_size_log2);
    return Next::ReduceLoad(base, index, kind, loaded_rep, result_rep, offset,
                            element_size_log2);
  }

  OpIndex REDUCE(Store)(OpIndex base, OptionalOpIndex index, OpIndex value,
                        StoreOp::Kind kind, MemoryRepresentation stored_rep,
                        WriteBarrierKind write_barrier, int32_t offset,
                        uint8_t element_size_log2,
                        bool maybe_initializing_or_transitioning,
                        IndirectPointerTag maybe_indirect_pointer_tag) {
    SimplifyLoadStore(base, index, kind, offset, element_size_log2);
    return Next::ReduceStore(base, index, value, kind, stored_rep,
                             write_barrier, offset, element_size_log2,
                             maybe_initializing_or_transitioning,
                             maybe_indirect_pointer_tag);
  }

  OpIndex REDUCE(AtomicWord32Pair)(V<WordPtr> base, OptionalV<WordPtr> index,
                                   OptionalV<Word32> value_low,
                                   OptionalV<Word32> value_high,
                                   OptionalV<Word32> expected_low,
                                   OptionalV<Word32> expected_high,
                                   AtomicWord32PairOp::Kind kind,
                                   int32_t offset) {
    if (kind == AtomicWord32PairOp::Kind::kStore ||
        kind == AtomicWord32PairOp::Kind::kLoad) {
      if (!index.valid()) {
        index = __ IntPtrConstant(offset);
        offset = 0;
      } else if (offset != 0) {
        index = __ WordPtrAdd(index.value(), offset);
        offset = 0;
      }
    }
    return Next::ReduceAtomicWord32Pair(base, index, value_low, value_high,
                                        expected_low, expected_high, kind,
                                        offset);
  }

 private:
  void SimplifyLoadStore(OpIndex& base, OptionalOpIndex& index,
                         LoadOp::Kind& kind, int32_t& offset,
                         uint8_t& element_size_log2) {
    if (lowering_enabled_) {
      if (element_size_log2 > kMaxElementSizeLog2) {
        DCHECK(index.valid());
        index = __ WordPtrShiftLeft(index.value(), element_size_log2);
        element_size_log2 = 0;
      }

      // TODO(12783): This needs to be extended for all architectures that don't
      // have loads with the base + index * element_size + offset pattern.
#if V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_RISCV64 || \
    V8_TARGET_ARCH_LOONG64 || V8_TARGET_ARCH_MIPS64
      // If an index is present, the element_size_log2 is changed to zero
      // (above). So any load follows the form *(base + offset). To simplify
      // instruction selection, both static and dynamic offsets are stored in
      // the index input.
      // As tagged loads result in modfiying the offset by -1, those loads are
      // converted into raw loads.

      if (kind.tagged_base) {
        kind.tagged_base = false;
        offset -= kHeapObjectTag;
        base = __ BitcastHeapObjectToWordPtr(base);
      }

      DCHECK_EQ(element_size_log2, 0);
      if (!index.has_value() || matcher_.MatchIntegralZero(index.value())) {
        index = __ IntPtrConstant(offset);
      } else if (offset != 0) {
        index = __ WordPtrAdd(index.value(), offset);
      }
      // Lowered loads always have an index. The offset is 0.
      offset = 0;
      DCHECK(index.has_value());
      // If it has an index, the "element size" has to be 1 Byte.
      // Note that the element size does not encode the size of the loaded value
      // as that is encoded by the MemoryRepresentation, it only specifies a
      // factor as a power of 2 to multiply the index with.
      DCHECK_IMPLIES(index.has_value(), element_size_log2 == 0);
#elif V8_TARGET_ARCH_X64
      if (kind.is_atomic && index.valid() && offset != 0) {
        // Atomic stores/loads should not have both an index and an offset. We
        // fold the offset into the index when this happens.
        if (element_size_log2 != 0) {
          index = __ WordPtrShiftLeft(index.value(), element_size_log2);
          element_size_log2 = 0;
        }
        index = __ WordPtrAdd(index.value(), offset);
        offset = 0;
      }
#endif
    }
  }

  bool is_wasm_ = PipelineData::Get().is_wasm();
  // TODO(12783): Remove this flag once the Turbofan instruction selection has
  // been replaced.
#if V8_TARGET_ARCH_X64 or V8_TARGET_ARCH_ARM64 or V8_TARGET_ARCH_ARM or \
    V8_TARGET_ARCH_IA32
  bool lowering_enabled_ =
      (is_wasm_ && v8_flags.turboshaft_wasm_instruction_selection_staged) ||
      (!is_wasm_ && v8_flags.turboshaft_instruction_selection);
#else
  bool lowering_enabled_ =
      (is_wasm_ &&
       v8_flags.turboshaft_wasm_instruction_selection_experimental) ||
      (!is_wasm_ && v8_flags.turboshaft_instruction_selection);
#endif
  OperationMatcher matcher_{__ output_graph()};
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_LOAD_STORE_SIMPLIFICATION_REDUCER_H_
