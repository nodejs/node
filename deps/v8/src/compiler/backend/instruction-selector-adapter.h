// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_ADAPTER_H_
#define V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_ADAPTER_H_

#include <optional>

#include "src/codegen/machine-type.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operation-matcher.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/use-map.h"

namespace v8::internal::compiler {

struct TurboshaftAdapter : public turboshaft::OperationMatcher {
  static constexpr bool IsTurbofan = false;
  static constexpr bool IsTurboshaft = true;
  static constexpr bool AllowsImplicitWord64ToWord32Truncation = true;

  explicit TurboshaftAdapter(turboshaft::Graph* graph)
      : turboshaft::OperationMatcher(*graph), graph_(graph) {}

  class LoadView {
   public:
    LoadView(turboshaft::Graph* graph, turboshaft::OpIndex node) : node_(node) {
      switch (graph->Get(node_).opcode) {
        case turboshaft::Opcode::kLoad:
          load_ = &graph->Get(node_).Cast<turboshaft::LoadOp>();
          break;
#if V8_ENABLE_WEBASSEMBLY
        case turboshaft::Opcode::kSimd128LoadTransform:
          load_transform_ =
              &graph->Get(node_).Cast<turboshaft::Simd128LoadTransformOp>();
          break;
#if V8_ENABLE_WASM_SIMD256_REVEC
        case turboshaft::Opcode::kSimd256LoadTransform:
          load_transform256_ =
              &graph->Get(node_).Cast<turboshaft::Simd256LoadTransformOp>();
          break;
#endif  // V8_ENABLE_WASM_SIMD256_REVEC
#endif  // V8_ENABLE_WEBASSEMBLY
        default:
          UNREACHABLE();
      }
    }
    LoadRepresentation loaded_rep() const {
      DCHECK_NOT_NULL(load_);
      return load_->machine_type();
    }
    turboshaft::MemoryRepresentation ts_loaded_rep() const {
      DCHECK_NOT_NULL(load_);
      return load_->loaded_rep;
    }
    turboshaft::RegisterRepresentation ts_result_rep() const {
      DCHECK_NOT_NULL(load_);
      return load_->result_rep;
    }
    bool is_protected(bool* traps_on_null) const {
      if (kind().with_trap_handler) {
        if (load_) {
          *traps_on_null = load_->kind.trap_on_null;
#if V8_ENABLE_WEBASSEMBLY
        } else {
#if V8_ENABLE_WASM_SIMD256_REVEC
          DCHECK(
              (load_transform_ && !load_transform_->load_kind.trap_on_null) ||
              (load_transform256_ &&
               !load_transform256_->load_kind.trap_on_null));
#else
          DCHECK(load_transform_);
          DCHECK(!load_transform_->load_kind.trap_on_null);
#endif  // V8_ENABLE_WASM_SIMD256_REVEC
          *traps_on_null = false;
#endif  // V8_ENABLE_WEBASSEMBLY
        }
        return true;
      }
      return false;
    }
    bool is_atomic() const { return kind().is_atomic; }

    turboshaft::OpIndex base() const {
      if (load_) return load_->base();
#if V8_ENABLE_WEBASSEMBLY
      if (load_transform_) return load_transform_->base();
#if V8_ENABLE_WASM_SIMD256_REVEC
      if (load_transform256_) return load_transform256_->base();
#endif  // V8_ENABLE_WASM_SIMD256_REVEC
#endif
      UNREACHABLE();
    }
    turboshaft::OpIndex index() const {
      if (load_) return load_->index().value_or_invalid();
#if V8_ENABLE_WEBASSEMBLY
      if (load_transform_) return load_transform_->index();
#if V8_ENABLE_WASM_SIMD256_REVEC
      if (load_transform256_) return load_transform256_->index();
#endif  // V8_ENABLE_WASM_SIMD256_REVEC
#endif
      UNREACHABLE();
    }
    int32_t displacement() const {
      static_assert(
          std::is_same_v<decltype(turboshaft::StoreOp::offset), int32_t>);
      if (load_) {
        int32_t offset = load_->offset;
        if (load_->kind.tagged_base) {
          CHECK_GE(offset,
                   std::numeric_limits<int32_t>::min() + kHeapObjectTag);
          offset -= kHeapObjectTag;
        }
        return offset;
#if V8_ENABLE_WEBASSEMBLY
      } else if (load_transform_) {
        int32_t offset = load_transform_->offset;
        DCHECK(!load_transform_->load_kind.tagged_base);
        return offset;
#if V8_ENABLE_WASM_SIMD256_REVEC
      } else if (load_transform256_) {
        int32_t offset = load_transform256_->offset;
        DCHECK(!load_transform256_->load_kind.tagged_base);
        return offset;
#endif  // V8_ENABLE_WASM_SIMD256_REVEC
#endif
      }
      UNREACHABLE();
    }
    uint8_t element_size_log2() const {
      static_assert(
          std::is_same_v<decltype(turboshaft::StoreOp::element_size_log2),
                         uint8_t>);
      if (load_) return load_->element_size_log2;
#if V8_ENABLE_WEBASSEMBLY
      if (load_transform_) return 0;
#if V8_ENABLE_WASM_SIMD256_REVEC
      if (load_transform256_) return 0;
#endif  // V8_ENABLE_WASM_SIMD256_REVEC
#endif
      UNREACHABLE();
    }

    operator turboshaft::OpIndex() const { return node_; }

   private:
    turboshaft::LoadOp::Kind kind() const {
      if (load_) return load_->kind;
#if V8_ENABLE_WEBASSEMBLY
      if (load_transform_) return load_transform_->load_kind;
#if V8_ENABLE_WASM_SIMD256_REVEC
      if (load_transform256_) return load_transform256_->load_kind;
#endif  // V8_ENABLE_WASM_SIMD256_REVEC
#endif
      UNREACHABLE();
    }

    turboshaft::OpIndex node_;
    const turboshaft::LoadOp* load_ = nullptr;
#if V8_ENABLE_WEBASSEMBLY
    const turboshaft::Simd128LoadTransformOp* load_transform_ = nullptr;
#if V8_ENABLE_WASM_SIMD256_REVEC
    const turboshaft::Simd256LoadTransformOp* load_transform256_ = nullptr;
#endif  // V8_ENABLE_WASM_SIMD256_REVEC
#endif
  };

  class StoreView {
   public:
    StoreView(turboshaft::Graph* graph, turboshaft::OpIndex node)
        : node_(node) {
      op_ = &graph->Get(node_).Cast<turboshaft::StoreOp>();
    }

    StoreRepresentation stored_rep() const {
      return {op_->stored_rep.ToMachineType().representation(),
              op_->write_barrier};
    }
    turboshaft::MemoryRepresentation ts_stored_rep() const {
      return op_->stored_rep;
    }
    std::optional<AtomicMemoryOrder> memory_order() const {
      // TODO(nicohartmann@): Currently we don't support memory orders.
      if (op_->kind.is_atomic) return AtomicMemoryOrder::kSeqCst;
      return std::nullopt;
    }
    MemoryAccessKind access_kind() const {
      return op_->kind.with_trap_handler
                 ? MemoryAccessKind::kProtectedByTrapHandler
                 : MemoryAccessKind::kNormal;
    }
    bool is_atomic() const { return op_->kind.is_atomic; }

    turboshaft::OpIndex base() const { return op_->base(); }
    turboshaft::OptionalOpIndex index() const { return op_->index(); }
    turboshaft::OpIndex value() const { return op_->value(); }
    IndirectPointerTag indirect_pointer_tag() const {
      return static_cast<IndirectPointerTag>(op_->indirect_pointer_tag());
    }
    int32_t displacement() const {
      static_assert(
          std::is_same_v<decltype(turboshaft::StoreOp::offset), int32_t>);
      int32_t offset = op_->offset;
      if (op_->kind.tagged_base) {
        CHECK_GE(offset, std::numeric_limits<int32_t>::min() + kHeapObjectTag);
        offset -= kHeapObjectTag;
      }
      return offset;
    }
    uint8_t element_size_log2() const {
      static_assert(
          std::is_same_v<decltype(turboshaft::StoreOp::element_size_log2),
                         uint8_t>);
      return op_->element_size_log2;
    }

    bool is_store_trap_on_null() const {
      return op_->kind.with_trap_handler && op_->kind.trap_on_null;
    }

    operator turboshaft::OpIndex() const { return node_; }

   private:
    turboshaft::OpIndex node_;
    const turboshaft::StoreOp* op_;
  };

#if V8_ENABLE_WEBASSEMBLY
  // TODO(391750831): Inline this.
  class SimdShuffleView {
   public:
    explicit SimdShuffleView(const turboshaft::Graph* graph,
                             turboshaft::OpIndex node)
        : node_(node) {
      op128_ = &graph->Get(node).Cast<turboshaft::Simd128ShuffleOp>();
      // Initialize input mapping.
      for (int i = 0; i < op128_->input_count; ++i) {
        input_mapping_.push_back(i);
      }
    }

    bool isSimd128() const {
      // TODO(nicohartmann@): Extend when we add support for Simd256.
      return true;
    }

    const uint8_t* data() const { return op128_->shuffle; }

    turboshaft::OpIndex input(int index) const {
      DCHECK_LT(index, op128_->input_count);
      return op128_->input(input_mapping_[index]);
    }

    void SwapInputs() { std::swap(input_mapping_[0], input_mapping_[1]); }

    void DuplicateFirstInput() {
      DCHECK_LE(2, input_mapping_.size());
      input_mapping_[1] = input_mapping_[0];
    }

    operator turboshaft::OpIndex() const { return node_; }

   private:
    turboshaft::OpIndex node_;
    base::SmallVector<int, 2> input_mapping_;
    const turboshaft::Simd128ShuffleOp* op128_;
  };
#endif

  bool is_load(turboshaft::OpIndex node) const {
    return graph_->Get(node).Is<turboshaft::LoadOp>()
#if V8_ENABLE_WEBASSEMBLY
           || graph_->Get(node).Is<turboshaft::Simd128LoadTransformOp>()
#if V8_ENABLE_WASM_SIMD256_REVEC
           || graph_->Get(node).Is<turboshaft::Simd256LoadTransformOp>()
#endif  // V8_ENABLE_WASM_SIMD256_REVEC
#endif
        ;
  }
  bool is_load_root_register(turboshaft::OpIndex node) const {
    return graph_->Get(node).Is<turboshaft::LoadRootRegisterOp>();
  }
  LoadView load_view(turboshaft::OpIndex node) {
    DCHECK(is_load(node));
    return LoadView(graph_, node);
  }
  StoreView store_view(turboshaft::OpIndex node) {
    return StoreView(graph_, node);
  }

#if V8_ENABLE_WEBASSEMBLY
  SimdShuffleView simd_shuffle_view(turboshaft::OpIndex node) {
    return SimdShuffleView(graph_, node);
  }
#endif

  turboshaft::Graph* turboshaft_graph() const { return graph_; }

  turboshaft::Block* block(turboshaft::Graph* schedule,
                           turboshaft::OpIndex node) const {
    // TODO(nicohartmann@): This might be too slow and we should consider
    // precomputing.
    return &schedule->Get(schedule->BlockOf(node));
  }

  RpoNumber rpo_number(const turboshaft::Block* block) const {
    return RpoNumber::FromInt(block->index().id());
  }

  const ZoneVector<turboshaft::Block*>& rpo_order(turboshaft::Graph* schedule) {
    return schedule->blocks_vector();
  }

  bool IsLoopHeader(const turboshaft::Block* block) const {
    return block->IsLoop();
  }

  size_t PredecessorCount(const turboshaft::Block* block) const {
    return block->PredecessorCount();
  }
  turboshaft::Block* PredecessorAt(const turboshaft::Block* block,
                                   size_t index) const {
    return block->Predecessors()[index];
  }

  base::iterator_range<turboshaft::Graph::OpIndexIterator> nodes(
      const turboshaft::Block* block) {
    return graph_->OperationIndices(*block);
  }

  bool IsRetain(turboshaft::OpIndex node) const {
    return graph_->Get(node).Is<turboshaft::RetainOp>();
  }
  bool IsHeapConstant(turboshaft::OpIndex node) const {
    turboshaft::ConstantOp* constant =
        graph_->Get(node).TryCast<turboshaft::ConstantOp>();
    if (constant == nullptr) return false;
    return constant->kind == turboshaft::ConstantOp::Kind::kHeapObject;
  }
  bool IsExternalConstant(turboshaft::OpIndex node) const {
    turboshaft::ConstantOp* constant =
        graph_->Get(node).TryCast<turboshaft::ConstantOp>();
    if (constant == nullptr) return false;
    return constant->kind == turboshaft::ConstantOp::Kind::kExternal;
  }
  bool IsRelocatableWasmConstant(turboshaft::OpIndex node) const {
    turboshaft::ConstantOp* constant =
        graph_->Get(node).TryCast<turboshaft::ConstantOp>();
    if (constant == nullptr) return false;
    return constant->kind ==
           turboshaft::any_of(
               turboshaft::ConstantOp::Kind::kRelocatableWasmCall,
               turboshaft::ConstantOp::Kind::kRelocatableWasmStubCall);
  }
  bool IsLoadOrLoadImmutable(turboshaft::OpIndex node) const {
    return graph_->Get(node).opcode == turboshaft::Opcode::kLoad;
  }
  bool IsProtectedLoad(turboshaft::OpIndex node) const {
#if V8_ENABLE_WEBASSEMBLY
    if (graph_->Get(node).opcode == turboshaft::Opcode::kSimd128LoadTransform) {
      return true;
    }
#if V8_ENABLE_WASM_SIMD256_REVEC
    if (graph_->Get(node).opcode == turboshaft::Opcode::kSimd256LoadTransform) {
      return true;
    }
#endif  // V8_ENABLE_WASM_SIMD256_REVEC
#endif  // V8_ENABLE_WEBASSEMBLY

    if (!IsLoadOrLoadImmutable(node)) return false;

    bool traps_on_null;
    return LoadView(graph_, node).is_protected(&traps_on_null);
  }

#if !(defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_IA32))
  int value_input_count(turboshaft::OpIndex node) const {
    return graph_->Get(node).input_count;
  }
  turboshaft::OpIndex input_at(turboshaft::OpIndex node, size_t index) const {
    return graph_->Get(node).input(index);
  }
  base::Vector<const turboshaft::OpIndex> inputs(
      turboshaft::OpIndex node) const {
    return graph_->Get(node).inputs();
  }
  turboshaft::Opcode opcode(turboshaft::OpIndex node) const {
    return graph_->Get(node).opcode;
  }
#endif

  uint32_t id(turboshaft::OpIndex node) const { return node.id(); }
  static turboshaft::OpIndex value(turboshaft::OptionalOpIndex node) {
    DCHECK(node.valid());
    return node.value();
  }

 private:
  turboshaft::Graph* graph_;
};

}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_BACKEND_INSTRUCTION_SELECTOR_ADAPTER_H_
