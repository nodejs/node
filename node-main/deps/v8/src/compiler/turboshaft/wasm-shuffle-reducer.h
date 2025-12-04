// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_COMPILER_TURBOSHAFT_WASM_SHUFFLE_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_WASM_SHUFFLE_REDUCER_H_

#include <optional>

#include "src/base/template-utils.h"
#include "src/builtins/builtins.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/utils.h"
#include "src/zone/zone-containers.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

using SmallShuffleVector = SmallZoneVector<const Simd128ShuffleOp*, 8>;

// The aim of this reducer is to reduce the size of shuffles, by looking at
// what elements are required and we do this by looking at their users:
// - Simd128UnaryOp ConvertLow ops
// - Simd128BinaryOp ExtMulLow ops
// - Simd128ShuffleOps
// If a shuffle is only used by an operation which only reads the low half of
// shuffle input, then we can reduce the shuffle to one which shuffles fewer
// bytes. When multiple ConvertLow and/or ExtMulLow are chained, then the
// required width of the shuffle can be further reduced.
// If a shuffle is only used by a shuffle which only uses half of a shuffle
// input, that input shuffle can also reduced.

// Used by the analysis to search back from uses to their defs, looking for
// shuffles that could be reduced.
class DemandedElementAnalysis {
 public:
  static constexpr uint16_t k8x16 = 0xFFFF;
  static constexpr uint16_t k8x8Low = 0xFF;
  static constexpr uint16_t k8x4Low = 0xF;
  static constexpr uint16_t k8x2Low = 0x3;

  using LaneBitSet = std::bitset<16>;
  using DemandedElementMap =
      ZoneVector<std::pair<const Operation*, LaneBitSet>>;

  DemandedElementAnalysis(Zone* phase_zone, const Graph& input_graph)
      : phase_zone_(phase_zone), input_graph_(input_graph) {}

  void AddUnaryOp(const Simd128UnaryOp& unop, LaneBitSet lanes);
  void AddBinaryOp(const Simd128BinopOp& binop, LaneBitSet lanes);
  void RecordOp(const Operation* op, LaneBitSet lanes);

  const DemandedElementMap& demanded_elements() const {
    return demanded_elements_;
  }

  const Graph& input_graph() const { return input_graph_; }

  bool Visited(const Operation* op) const { return visited_.count(op); }

 private:
  Zone* phase_zone_;
  const Graph& input_graph_;
  DemandedElementMap demanded_elements_{phase_zone_};
  ZoneUnorderedSet<const Operation*> visited_{phase_zone_};
};

class WasmShuffleAnalyzer {
 public:
#if V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
  struct DeinterleaveLoadCandidate {
    const Simd128LoadPairDeinterleaveOp::Kind kind;
    const LoadOp& left;
    const LoadOp& right;
    const Simd128ShuffleOp& even_shfop;
    const Simd128ShuffleOp& odd_shfop;
  };
#endif  // V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS

  WasmShuffleAnalyzer(Zone* phase_zone, const Graph& input_graph)
      : phase_zone_(phase_zone), input_graph_(input_graph) {
    Run();
  }

  V8_EXPORT_PRIVATE void Run();

  void Process(const Operation& op);
  void ProcessUnary(const Simd128UnaryOp& unop);
  void ProcessBinary(const Simd128BinopOp& binop);
  void ProcessShuffle(const Simd128ShuffleOp& shuffle_op);
  void ProcessShuffleOfShuffle(const Simd128ShuffleOp& shuffle_op,
                               const Simd128ShuffleOp& shuffle,
                               uint8_t lower_limit, uint8_t upper_limit);
#if V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
  void ProcessShuffleOfLoads(const Simd128ShuffleOp& shfop, const LoadOp& left,
                             const LoadOp& right);
  bool CouldLoadPair(const LoadOp& load0, const LoadOp& load1) const;
  void AddLoadMultipleCandidate(const Simd128ShuffleOp& even_shuffle,
                                const Simd128ShuffleOp& odd_shuffle,
                                const LoadOp& left, const LoadOp& right,
                                Simd128LoadPairDeinterleaveOp::Kind kind);
#endif  // V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS

  bool ShouldReduce() const {
    return !demanded_element_analysis.demanded_elements().empty()
#if V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
           || !deinterleave_load_candidates_.empty()
#endif
        ;
  }

  const DemandedElementAnalysis::DemandedElementMap& ops_to_reduce() const {
    return demanded_element_analysis.demanded_elements();
  }

  std::optional<DemandedElementAnalysis::LaneBitSet> DemandedByteLanes(
      const Operation* op) const {
    for (const auto& [narrow_op, lanes] : ops_to_reduce()) {
      if (op == narrow_op) {
        return lanes;
      }
    }
    return {};
  }

  // Is only the top half (lanes 8...15) of the result of shuffle required?
  // If so shuffle will need to be modified so that it writes the designed data
  // into the low half lanes instead.
  bool ShouldRewriteShuffleToLow(const Simd128ShuffleOp* shuffle) const {
    for (auto shift_shuffle : shift_shuffles_) {
      if (shift_shuffle == shuffle) {
        return true;
      }
    }
    return false;
  }

#ifdef DEBUG
  bool ShouldRewriteShuffleToLow(OpIndex op) const {
    return ShouldRewriteShuffleToLow(
        &input_graph().Get(op).Cast<Simd128ShuffleOp>());
  }
#endif

  // Is the low half (lanes 0...7) result of shuffle coming exclusively from
  // the high half of one of its operands.
  bool DoesShuffleIntoLowHalf(const Simd128ShuffleOp* shuffle) const {
    for (auto half_shuffle : low_half_shuffles_) {
      if (half_shuffle == shuffle) {
        return true;
      }
    }
    return false;
  }

  // Is the high half (lanes: 8...15) result of shuffle coming exclusively from
  // the high half of its operands.
  bool DoesShuffleIntoHighHalf(const Simd128ShuffleOp* shuffle) const {
    for (auto half_shuffle : high_half_shuffles_) {
      if (half_shuffle == shuffle) {
        return true;
      }
    }
    return false;
  }

#if V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
  std::optional<const DeinterleaveLoadCandidate*> IsDeinterleaveCandidate(
      const LoadOp* load) const {
    for (auto& candidate : deinterleave_load_candidates_) {
      if (&candidate.left == load || &candidate.right == load) {
        return &candidate;
      }
    }
    return {};
  }
#endif  // V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS

  const Graph& input_graph() const { return input_graph_; }

 private:
#if V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
  std::optional<const Simd128ShuffleOp*> GetOtherShuffleUser(
      const LoadOp& left, const LoadOp& right,
      const SmallShuffleVector& shuffles) const {
    for (const auto* shuffle : shuffles) {
      const auto& shuffle_left =
          input_graph().Get(shuffle->left()).Cast<LoadOp>();
      const auto& shuffle_right =
          input_graph().Get(shuffle->right()).Cast<LoadOp>();
      if (&shuffle_left == &left && &shuffle_right == &right) {
        return shuffle;
      }
    }
    return {};
  }
#endif  // V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS

  Zone* phase_zone_;
  const Graph& input_graph_;
  DemandedElementAnalysis demanded_element_analysis{phase_zone_, input_graph_};
  SmallShuffleVector shift_shuffles_{phase_zone_};
  SmallShuffleVector low_half_shuffles_{phase_zone_};
  SmallShuffleVector high_half_shuffles_{phase_zone_};
#if V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
  SmallShuffleVector even_64x2_shuffles_{phase_zone_};
  SmallShuffleVector odd_64x2_shuffles_{phase_zone_};
  SmallShuffleVector even_32x4_shuffles_{phase_zone_};
  SmallShuffleVector odd_32x4_shuffles_{phase_zone_};
  SmallShuffleVector even_16x8_shuffles_{phase_zone_};
  SmallShuffleVector odd_16x8_shuffles_{phase_zone_};
  SmallShuffleVector even_8x16_shuffles_{phase_zone_};
  SmallShuffleVector odd_8x16_shuffles_{phase_zone_};
  base::SmallVector<DeinterleaveLoadCandidate, 8> deinterleave_load_candidates_;
#endif  // V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
};

template <class Next>
class WasmShuffleReducer : public Next {
 private:
  std::optional<WasmShuffleAnalyzer> analyzer_;

#if V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
  struct DeinterleaveLoadShuffle {
    const Simd128ShuffleOp* shuffle;
    OpIndex og_index;
    uint8_t result_index;
  };

  struct DeinterleaveLoad {
    const LoadOp* load;
    OpIndex og_index;
  };
  base::SmallVector<DeinterleaveLoad, 8> combined_loads_;
  base::SmallVector<DeinterleaveLoadShuffle, 8> deinterleave_load_shuffles_;

  void AddCombinedLoad(const LoadOp& load, OpIndex og_index) {
    combined_loads_.emplace_back(&load, og_index);
  }

  void AddDeinterleavedShuffle(const Simd128ShuffleOp& shuffle,
                               OpIndex og_index, uint8_t result_index) {
    deinterleave_load_shuffles_.emplace_back(&shuffle, og_index, result_index);
  }

  std::optional<const DeinterleaveLoadShuffle*> IsDeinterleaveLoadShuffle(
      const Simd128ShuffleOp* shuffle) const {
    for (const auto& deinterleave_load : deinterleave_load_shuffles_) {
      if (deinterleave_load.shuffle == shuffle) {
        return &deinterleave_load;
      }
    }
    return {};
  }

  std::optional<OpIndex> MaybeAlreadyCombined(const LoadOp* load) const {
    for (const auto& combined : combined_loads_) {
      if (combined.load == load) {
        return combined.og_index;
      }
    }
    return {};
  }
#endif  // V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS

 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(WasmShuffleReducer)

  void Analyze() {
    analyzer_.emplace(__ phase_zone(), __ input_graph());
    analyzer_->Run();
    Next::Analyze();
  }

#if V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
  OpIndex REDUCE_INPUT_GRAPH(Load)(OpIndex ig_index, const LoadOp& load) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceInputGraphLoad(ig_index, load);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;
    if (load.loaded_rep != MemoryRepresentation::Simd128()) goto no_change;

    if (auto maybe_combined = MaybeAlreadyCombined(&load)) {
      return maybe_combined.value();
    }

    if (auto maybe_candidate = analyzer_->IsDeinterleaveCandidate(&load)) {
      const auto* candidate = maybe_candidate.value();
      const LoadOp& left = candidate->left;
      const LoadOp& right = candidate->right;
#ifdef DEBUG
      DCHECK(!MaybeAlreadyCombined(&left));
      DCHECK(!MaybeAlreadyCombined(&right));
      CHECK_EQ(left.kind, right.kind);
      // The 'left' load of the candidate is the one shuffling the
      // even-numbered elements, which includes zero. So, the left load
      // accesses the lowest address.
      CHECK_LT(left.offset, right.offset);
#endif  // DEBUG

      V<WordPtr> base = __ MapToNewGraph(load.base());
      V<WordPtr> index;
      int offset = left.offset;
      if (load.index().has_value()) {
        index = __ MapToNewGraph(load.index().value());
        if (offset != 0) {
          index = __ WordPtrAdd(index, offset);
        }
      } else {
        index = __ IntPtrConstant(offset);
      }

      OpIndex og_index = __ Simd128LoadPairDeinterleave(base, index, load.kind,
                                                        candidate->kind);
      AddCombinedLoad(left, og_index);
      AddCombinedLoad(right, og_index);

      AddDeinterleavedShuffle(candidate->even_shfop, og_index, 0);
      AddDeinterleavedShuffle(candidate->odd_shfop, og_index, 1);
      return og_index;
    }
    goto no_change;
  }
#endif  // V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS

  OpIndex REDUCE_INPUT_GRAPH(Simd128Shuffle)(OpIndex ig_index,
                                             const Simd128ShuffleOp& shuffle) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceInputGraphSimd128Shuffle(ig_index, shuffle);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;

    if (shuffle.kind != Simd128ShuffleOp::Kind::kI8x16) goto no_change;

    auto og_left = __ MapToNewGraph(shuffle.left());
    auto og_right = __ MapToNewGraph(shuffle.right());
    std::array<uint8_t, kSimd128Size> shuffle_bytes = {0};
    std::copy(shuffle.shuffle, shuffle.shuffle + kSimd128Size,
              shuffle_bytes.begin());
#if V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS
    if (auto maybe_deinterleaved_load = IsDeinterleaveLoadShuffle(&shuffle)) {
      const auto* deinterleaved_load = maybe_deinterleaved_load.value();
      return __ Projection(deinterleaved_load->og_index,
                           deinterleaved_load->result_index,
                           RegisterRepresentation::Simd128());
    }
#endif  // V8_ENABLE_WASM_DEINTERLEAVED_MEM_OPS

    constexpr size_t half_lanes = kSimd128Size / 2;

    bool does_shuffle_into_low_half =
        analyzer_->DoesShuffleIntoLowHalf(&shuffle);
    bool does_shuffle_into_high_half =
        analyzer_->DoesShuffleIntoHighHalf(&shuffle);

    // Shuffles to adjust because one, or both, of their inputs have been
    // narrowed.
    if (does_shuffle_into_low_half && does_shuffle_into_high_half) {
      DCHECK(analyzer_->ShouldRewriteShuffleToLow(shuffle.left()));
      DCHECK(analyzer_->ShouldRewriteShuffleToLow(shuffle.right()));
      // We have a shuffle where both inputs have been reduced and shifted, so
      // something like this:
      // |--------|--------|---a1---|---b3---|  shf0 = (a, b)
      //
      // |--------|--------|---c2---|---d4---|  shf1 = (c, d)
      //
      // |---a1---|---b3---|---c2---|---d4---|  shf2 = (shf0, shf1)
      //
      // Is being changed into this:
      // |---a1---|---b3---|--------|--------|  shf0 = (a, b)
      //
      // |---c2---|---d4---|--------|--------|  shf1 = (c, d)
      //
      // |---a1---|---b3---|---c2---|---d4---|  shf2 = (shf0, shf1)
      std::transform(shuffle_bytes.begin(), shuffle_bytes.end(),
                     shuffle_bytes.begin(),
                     [](uint8_t lane) { return lane - half_lanes; });
    } else if (does_shuffle_into_low_half) {
      DCHECK(analyzer_->ShouldRewriteShuffleToLow(shuffle.left()) ||
             analyzer_->ShouldRewriteShuffleToLow(shuffle.right()));
      DCHECK_NE(analyzer_->ShouldRewriteShuffleToLow(shuffle.left()),
                analyzer_->ShouldRewriteShuffleToLow(shuffle.right()));
      // We have a shuffle where both inputs have been reduced and one has
      // been shifted, so something like this:
      // |--------|--------|---a1---|---b3---|  shf0 = (a, b)
      //
      // |---c2---|---d4---|--------|--------|  shf1 = (c, d)
      //
      // |---a1---|---b3---|---c2---|---d4---|  shf2 = (shf0, shf1)
      //
      // Is being changed into this:
      // |---a1---|---b3---|--------|--------|  shf0 = (a, b)
      //
      // |---c2---|---d4---|--------|--------|  shf1 = (c, d)
      //
      // |---a1---|---b3---|---c2---|---d4---|  shf2 = (shf0, shf1)
      //
      // Original shf2 lane-wise shuffle: [2, 3, 4, 5]
      // Needs to be converted to: [0, 1, 4, 5]
      std::transform(shuffle_bytes.begin(), shuffle_bytes.begin() + half_lanes,
                     shuffle_bytes.begin(),
                     [](uint8_t lane) { return lane - half_lanes; });
    } else if (does_shuffle_into_high_half) {
      DCHECK(analyzer_->ShouldRewriteShuffleToLow(shuffle.left()) ||
             analyzer_->ShouldRewriteShuffleToLow(shuffle.right()));
      DCHECK_NE(analyzer_->ShouldRewriteShuffleToLow(shuffle.left()),
                analyzer_->ShouldRewriteShuffleToLow(shuffle.right()));
      // We have a shuffle where both inputs have been reduced and one has
      // been shifted, so something like this:
      // |---a1---|---b3---|--------|--------|  shf0 = (a, b)
      //
      // |--------|--------|---c2---|---d4---|  shf1 = (c, d)
      //
      // |---a1---|---b3---|---c2---|---d4---|  shf2 = (shf0, shf1)
      //
      // Is being changed into this:
      // |---a1---|---b3---|--------|--------|  shf0 = (a, b)
      //
      // |---c2---|---d4---|--------|--------|  shf1 = (c, d)
      //
      // |---a1---|---b3---|---c2---|---d4---|  shf2 = (shf0, shf1)
      std::transform(shuffle_bytes.begin() + half_lanes, shuffle_bytes.end(),
                     shuffle_bytes.begin() + half_lanes,
                     [](uint8_t lane) { return lane - half_lanes; });
    }

    if (does_shuffle_into_low_half || does_shuffle_into_high_half) {
      return __ Simd128Shuffle(og_left, og_right,
                               Simd128ShuffleOp::Kind::kI8x16,
                               shuffle_bytes.data());
    }

    // Shuffles to narrow.
    if (auto maybe_lanes = analyzer_->DemandedByteLanes(&shuffle)) {
      auto lanes = maybe_lanes.value();
      if (analyzer_->ShouldRewriteShuffleToLow(&shuffle)) {
        DCHECK_EQ(lanes, DemandedElementAnalysis::k8x8Low);
        // Take the top half of the shuffle bytes and these will now write
        // those values into the low half of the result instead.
        std::copy(shuffle.shuffle + half_lanes, shuffle.shuffle + kSimd128Size,
                  shuffle_bytes.begin());
      } else {
        // Just truncate the lower half.
        std::copy(shuffle.shuffle, shuffle.shuffle + half_lanes,
                  shuffle_bytes.begin());
      }

      if (lanes == DemandedElementAnalysis::k8x2Low) {
        return __ Simd128Shuffle(og_left, og_right,
                                 Simd128ShuffleOp::Kind::kI8x2,
                                 shuffle_bytes.data());
      } else if (lanes == DemandedElementAnalysis::k8x4Low) {
        return __ Simd128Shuffle(og_left, og_right,
                                 Simd128ShuffleOp::Kind::kI8x4,
                                 shuffle_bytes.data());
      } else if (lanes == DemandedElementAnalysis::k8x8Low) {
        return __ Simd128Shuffle(og_left, og_right,
                                 Simd128ShuffleOp::Kind::kI8x8,
                                 shuffle_bytes.data());
      }
    }
    goto no_change;
  }
};

}  // namespace v8::internal::compiler::turboshaft

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

#endif  // V8_COMPILER_TURBOSHAFT_WASM_SHUFFLE_REDUCER_H_
