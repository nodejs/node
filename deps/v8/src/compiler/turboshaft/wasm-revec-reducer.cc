// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-revec-reducer.h"

#include "src/base/logging.h"

#define TRACE(...)                                  \
  do {                                              \
    if (v8_flags.trace_wasm_revectorize) {          \
      PrintF("Revec: %s %d: ", __func__, __LINE__); \
      PrintF(__VA_ARGS__);                          \
    }                                               \
  } while (false)

namespace v8::internal::compiler::turboshaft {

//  This class is the wrapper for StoreOp/LoadOp, which is helpful to calcualte
//  the relative offset between two StoreOp/LoadOp.
template <typename Op,
          typename = std::enable_if_t<std::is_same_v<Op, StoreOp> ||
                                      std::is_same_v<Op, LoadOp>>>
class StoreLoadInfo {
 public:
  StoreLoadInfo(const Graph* graph, const Op* op)
      : op_(op), offset_(op->offset) {
    if (!op->index().has_value()) return;
    base_ = &graph->Get(op->base());
    const ChangeOp* change =
        graph->Get(op->index().value()).template TryCast<ChangeOp>();
    if (change == nullptr) {
      SetInvalid();
      return;
    }
    DCHECK_EQ(change->kind, ChangeOp::Kind::kZeroExtend);
    const Operation* change_input = &graph->Get(change->input());
    if (const ConstantOp* const_op = change_input->TryCast<ConstantOp>()) {
      DCHECK_EQ(const_op->kind, ConstantOp::Kind::kWord32);
      int new_offset;
      if (base::bits::SignedAddOverflow32(static_cast<int>(const_op->word32()),
                                          offset_, &new_offset)) {
        // offset is overflow
        SetInvalid();
        return;
      }
      offset_ = new_offset;
      return;
    }
    index_ = change_input;
  }

  base::Optional<int> operator-(const StoreLoadInfo<Op>& rhs) const {
    DCHECK(IsValid() && rhs.IsValid());
    bool calculatable = base_ == rhs.base_ && index_ == rhs.index_ &&
                        op_->kind == rhs.op_->kind;
    if constexpr (std::is_same_v<Op, StoreOp>) {
      // TODO(v8:12716) If one store has a full write barrier and the other has
      // no write barrier, consider combine them with a full write barrier.
      calculatable &= (op_->write_barrier == rhs.op_->write_barrier);
    }

    if (calculatable) {
      return offset_ - rhs.offset_;
    }
    return {};
  }

  bool IsValid() const { return op_ != nullptr; }

  const Operation* index() const { return index_; }
  int offset() const { return offset_; }
  const Op* op() const { return op_; }

 private:
  void SetInvalid() { op_ = nullptr; }

  const Op* op_;
  const Operation* base_ = nullptr;
  const Operation* index_ = nullptr;
  int offset_;
};

struct StoreInfoCompare {
  bool operator()(const StoreLoadInfo<StoreOp>& lhs,
                  const StoreLoadInfo<StoreOp>& rhs) const {
    if (lhs.index() != rhs.index()) {
      return lhs.index() < rhs.index();
    }
    return lhs.offset() < rhs.offset();
  }
};

using StoreInfoSet = ZoneSet<StoreLoadInfo<StoreOp>, StoreInfoCompare>;

void WasmRevecAnalyzer::ProcessBlock(const Block& block) {
  StoreInfoSet simd128_stores(phase_zone_);
  for (const Operation& op : base::Reversed(graph_.operations(block))) {
    if (const StoreOp* store_op = op.TryCast<StoreOp>()) {
      if (store_op->stored_rep == MemoryRepresentation::Simd128()) {
        StoreLoadInfo<StoreOp> info(&graph_, store_op);
        if (info.IsValid()) {
          simd128_stores.insert(info);
        }
      }
    }
  }

  if (simd128_stores.size() >= 2) {
    for (auto it = std::next(simd128_stores.cbegin()),
              end = simd128_stores.cend();
         it != end;) {
      const StoreLoadInfo<StoreOp>& info0 = *std::prev(it);
      const StoreLoadInfo<StoreOp>& info1 = *it;
      auto diff = info1 - info0;

      if (diff.has_value()) {
        const int value = diff.value();
        DCHECK_GE(value, 0);
        if (value == kSimd128Size) {
          store_seeds_.push_back({info0.op(), info1.op()});
          if (std::distance(it, end) < 2) {
            break;
          }
          std::advance(it, 2);
          continue;
        }
      }
      it++;
    }
  }
}

void WasmRevecAnalyzer::Run() {
  TRACE("before collect seeds\n");
  for (Block& block : base::Reversed(graph_.blocks())) {
    ProcessBlock(block);
  }

  TRACE("after collect seed\n");
  if (store_seeds_.empty()) {
    TRACE("empty seed\n");
    return;
  }

  if (v8_flags.trace_wasm_revectorize) {
    PrintF("store seeds:\n");
    for (auto pair : store_seeds_) {
      PrintF("{\n");
      PrintF("#%u ", graph_.Index(*pair.first).id());
      Print(*pair.first);
      PrintF("#%u ", graph_.Index(*pair.second).id());
      Print(*pair.second);
      PrintF("}\n");
    }
  }
}

}  // namespace v8::internal::compiler::turboshaft
