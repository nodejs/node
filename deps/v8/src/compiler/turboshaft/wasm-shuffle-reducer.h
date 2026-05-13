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

class DemandedBytes {
 public:
  template <uint8_t num_bytes>
  static DemandedBytes Low() {
    static_assert(base::bits::IsPowerOfTwo(num_bytes));
    static_assert(num_bytes >= 1);
    static_assert(num_bytes <= kSimd128Size);
    return DemandedBytes(num_bytes);
  }

  static DemandedBytes All() { return Low<kSimd128Size>(); }

  static DemandedBytes Low(uint8_t num_bytes) {
    DCHECK(base::bits::IsPowerOfTwo(num_bytes));
    DCHECK_GE(num_bytes, 1);
    DCHECK_LE(num_bytes, kSimd128Size);
    switch (num_bytes) {
      default:
        UNREACHABLE();
      case 1:
        return Low<1>();
      case 2:
        return Low<2>();
      case 4:
        return Low<4>();
      case 8:
        return Low<8>();
      case 16:
        return Low<16>();
    }
  }

  static DemandedBytes LowFromTotalBytes(uint8_t total_bytes) {
    return Low(std::bit_ceil(total_bytes));
  }

  static DemandedBytes LowFromLane(uint8_t bytes_per_lane, uint8_t lane_index) {
    uint8_t total_bytes = bytes_per_lane * (lane_index + 1);
    return LowFromTotalBytes(total_bytes);
  }

  static DemandedBytes LowFromMaxShuffleIndex(uint8_t index) {
    return Low(std::bit_ceil(static_cast<uint8_t>(1 + (index % kSimd128Size))));
  }

  // Halve the number of demanded low bytes, to a limit.
  void HalveWithLimit(uint8_t min_bytes) {
    DCHECK_GE(min_bytes, 1);
    DCHECK(base::bits::IsPowerOfTwo(min_bytes));
    if (bytes() <= min_bytes) return;
    bytes_ >>= 1;
  }

  void Max(const DemandedBytes& demanded) {
    if (demanded.bytes() > bytes()) {
      bytes_ = demanded.bytes();
      DCHECK(base::bits::IsPowerOfTwo(bytes()));
      DCHECK_GE(bytes(), 1);
      DCHECK_LE(bytes(), kSimd128Size);
    }
  }

  bool IsLessThanOrEqual(const DemandedBytes& demanded) const {
    return bytes() <= demanded.bytes();
  }

  bool IsLow(uint8_t num_bytes) const { return bytes() == num_bytes; }
  bool IsAll() const { return IsLow(kSimd128Size); }

  Simd128ShuffleOp::Kind GetShuffleKind() const {
    if (IsLow(1)) return Simd128ShuffleOp::Kind::kI8x1;
    if (IsLow(2)) return Simd128ShuffleOp::Kind::kI8x2;
    if (IsLow(4)) return Simd128ShuffleOp::Kind::kI8x4;
    if (IsLow(8)) return Simd128ShuffleOp::Kind::kI8x8;
    if (IsLow(16)) return Simd128ShuffleOp::Kind::kI8x16;
    UNREACHABLE();
  }

  uint8_t bytes() const { return bytes_; }

 private:
  explicit DemandedBytes(uint8_t bytes) : bytes_(bytes) {}

  uint8_t bytes_;
};

class DemandedByteAnalysis {
 public:
  static constexpr int kMaxNumOperations = 150;

  // TODO(sparker): Add floating-point conversions:
  // - PromoteLow
  // - ConvertLow
  static constexpr std::array unary_low_half_ops = {
      Simd128UnaryOp::Kind::kI16x8SConvertI8x16Low,
      Simd128UnaryOp::Kind::kI16x8UConvertI8x16Low,
      Simd128UnaryOp::Kind::kI32x4SConvertI16x8Low,
      Simd128UnaryOp::Kind::kI32x4UConvertI16x8Low,
      Simd128UnaryOp::Kind::kI64x2SConvertI32x4Low,
      Simd128UnaryOp::Kind::kI64x2UConvertI32x4Low,
  };
  static constexpr std::array binary_low_half_ops = {
      Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16S,
      Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16U,
      Simd128BinopOp::Kind::kI32x4ExtMulLowI16x8S,
      Simd128BinopOp::Kind::kI32x4ExtMulLowI16x8U,
      Simd128BinopOp::Kind::kI64x2ExtMulLowI32x4S,
      Simd128BinopOp::Kind::kI64x2ExtMulLowI32x4U,
  };

  static bool IsUnaryLowHalfOp(Simd128UnaryOp::Kind kind) {
    return std::find(unary_low_half_ops.begin(), unary_low_half_ops.end(),
                     kind) != unary_low_half_ops.end();
  }
  static bool IsBinaryLowHalfOp(Simd128BinopOp::Kind kind) {
    return std::find(binary_low_half_ops.begin(), binary_low_half_ops.end(),
                     kind) != binary_low_half_ops.end();
  }

  static uint8_t GetInputElementSizeInBytes(Simd128UnaryOp::Kind kind) {
    switch (kind) {
      default:
        UNREACHABLE();
      case Simd128UnaryOp::Kind::kI16x8SConvertI8x16Low:
      case Simd128UnaryOp::Kind::kI16x8UConvertI8x16Low:
        return 1;
      case Simd128UnaryOp::Kind::kI32x4SConvertI16x8Low:
      case Simd128UnaryOp::Kind::kI32x4UConvertI16x8Low:
        return 2;
      case Simd128UnaryOp::Kind::kI64x2SConvertI32x4Low:
      case Simd128UnaryOp::Kind::kI64x2UConvertI32x4Low:
        return 4;
    }
  }
  static uint8_t GetInputElementSizeInBytes(Simd128BinopOp::Kind kind) {
    switch (kind) {
      default:
        UNREACHABLE();
      case Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16S:
      case Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16U:
        return 1;
      case Simd128BinopOp::Kind::kI32x4ExtMulLowI16x8S:
      case Simd128BinopOp::Kind::kI32x4ExtMulLowI16x8U:
        return 2;
      case Simd128BinopOp::Kind::kI64x2ExtMulLowI32x4S:
      case Simd128BinopOp::Kind::kI64x2ExtMulLowI32x4U:
        return 4;
    }
  }

  using DemandedByteMap =
      ZoneVector<std::pair<const Operation*, DemandedBytes>>;

  DemandedByteAnalysis(Zone* phase_zone, const Graph& input_graph)
      : phase_zone_(phase_zone), input_graph_(input_graph) {}

  void Add(OpIndex node, DemandedBytes demanded);
  void AddOp(const Simd128UnaryOp& unop, DemandedBytes demanded);
  void AddOp(const Simd128BinopOp& binop, DemandedBytes demanded);
  void AddOp(const Simd128ExtractLaneOp& extract_op, DemandedBytes demanded);
  void AddOp(const Simd128LaneMemoryOp& lane_op, DemandedBytes demanded);
  void RecordOp(const Operation& op, DemandedBytes demanded);
  void Revisit();

  const DemandedByteMap& demanded_bytes() const { return demanded_bytes_; }

  const Graph& input_graph() const { return input_graph_; }

  bool Visited(const Operation* op) const { return visited_.count(op); }

 private:
  class MultiUserBits {
   public:
    MultiUserBits(const Operation* op, DemandedBytes demanded,
                  uint32_t num_users)
        : op_(op), demanded_(demanded), num_users_(num_users) {}

    const Operation* op() const { return op_; }
    DemandedBytes demanded() const { return demanded_; }
    uint32_t num_users() const { return num_users_; }

    void Add(const DemandedBytes& bytes) {
      demanded_.Max(bytes);
      ++num_users_;
    }

    bool FoundAllUsers() const {
      return !op()->saturated_use_count.IsSaturated() &&
             op()->saturated_use_count.Is(num_users());
    }

   private:
    const Operation* op_;
    DemandedBytes demanded_;
    uint32_t num_users_;
  };

  // For the given op, return whether we have now visited it from all of its
  // users and so we know all of the used bytes.
  std::optional<DemandedBytes> AddUserAndCheckFoundAll(
      const Operation& op, const DemandedBytes& demanded);
  void RecordPartialOp(const Operation& op, DemandedBytes demanded);
  void RecordPartialOp(const Simd128UnaryOp& unop, DemandedBytes demanded);
  void RecordPartialOp(const Simd128BinopOp& binop, DemandedBytes demanded);
  void RevisitShuffle(const Simd128ShuffleOp& shuffle, DemandedBytes demanded);

  Zone* phase_zone_;
  const Graph& input_graph_;
  DemandedByteMap demanded_bytes_{phase_zone_};
  ZoneVector<MultiUserBits> to_revisit_{phase_zone_};
  ZoneUnorderedSet<const Operation*> visited_{phase_zone_};
  bool demanded_limit_reached_ = false;
  bool revisit_limit_reached_ = false;
};

class WasmShuffleAnalyzer {
 public:
  struct DeinterleaveLoadCandidate {
    const Simd128LoadPairDeinterleaveOp::Kind kind;
    const LoadOp& left;
    const LoadOp& right;
    const Simd128ShuffleOp& even_shfop;
    const Simd128ShuffleOp& odd_shfop;
  };

  struct ShuffleToShift {
    const Simd128ShuffleOp* shuffle;
    uint8_t begin_index;
    uint8_t end_index;
  };

  struct ShuffleToReadShifted {
    const Simd128ShuffleOp* shuffle;
    uint8_t begin_index;
    uint8_t end_index;
    uint8_t shuffle_in_begin;
  };

  void ProcessShuffleOfLoads(const Simd128ShuffleOp& shfop, const LoadOp& left,
                             const LoadOp& right);
  bool CouldLoadPair(const LoadOp& load0, const LoadOp& load1) const;
  void AddLoadMultipleCandidate(const Simd128ShuffleOp& even_shuffle,
                                const Simd128ShuffleOp& odd_shuffle,
                                const LoadOp& left, const LoadOp& right,
                                Simd128LoadPairDeinterleaveOp::Kind kind);

  std::optional<DeinterleaveLoadCandidate> GetDeinterleaveCandidate(
      const LoadOp* load) const {
    for (auto& candidate : deinterleave_load_candidates_) {
      if (&candidate.left == load || &candidate.right == load) {
        return candidate;
      }
    }
    return {};
  }

  WasmShuffleAnalyzer(Zone* phase_zone, const Graph& input_graph)
      : phase_zone_(phase_zone), input_graph_(input_graph) {
    Run();
  }

  V8_EXPORT_PRIVATE void Run();

  void Process(const Operation& op);
  void ProcessUnary(const Simd128UnaryOp& unop);
  void ProcessBinary(const Simd128BinopOp& binop);
  void ProcessReplaceLane(const Simd128ReplaceLaneOp& replace_op);
  void ProcessExtractLane(const Simd128ExtractLaneOp& extract_op);
  void ProcessLaneMemory(const Simd128LaneMemoryOp& lane_op);
  void ProcessShuffle(const Simd128ShuffleOp& shuffle_op);
  void TryReduceFromMSB(OpIndex input, const Simd128ShuffleOp& shuffle,
                        uint8_t lower_limit, uint8_t upper_limit);
  // Return true if shuffle_op will be reduced.
  bool ProcessShuffleOfShuffle(const Simd128ShuffleOp& shuffle_op,
                               const Simd128ShuffleOp& shuffle,
                               uint8_t lower_limit, uint8_t upper_limit);

  bool ShouldReduce() const {
    return !demanded_byte_analysis_.demanded_bytes().empty() ||
           !deinterleave_load_candidates_.empty() ||
           !load_lane_candidates_.empty() ||
           !shuffles_to_read_shifted_.empty() || !shuffles_to_shift_.empty();
  }

  const DemandedByteAnalysis::DemandedByteMap& ops_to_reduce() const {
    return demanded_byte_analysis_.demanded_bytes();
  }

  DemandedBytes GetDemandedBytes(const Operation* op) const {
    for (const auto& [narrow_op, bytes] : ops_to_reduce()) {
      if (op == narrow_op) {
        return bytes;
      }
    }
    return DemandedBytes::All();
  }

  void AddLoadLaneCandidate(const LoadOp* load,
                            const Simd128ReplaceLaneOp& replace) {
    load_lane_candidates_[load] = &replace;
  }

  std::optional<const Simd128ReplaceLaneOp*> IsLoadLaneCandidate(
      const LoadOp& op) {
    auto it = load_lane_candidates_.find(&op);
    if (it != load_lane_candidates_.end()) return it->second;
    return {};
  }

  std::optional<OpIndex> GetMergedLoadReplaceLane(
      const Simd128ReplaceLaneOp& op) {
    auto it = load_lanes_.find(&op);
    if (it != load_lanes_.end()) return it->second;
    return {};
  }

  void RecordLoadLane(const Simd128ReplaceLaneOp* replace, OpIndex load_lane) {
    load_lanes_[replace] = load_lane;
  }

  const SmallZoneVector<ShuffleToShift, 8>& shuffles_to_shift() const {
    return shuffles_to_shift_;
  }

  const SmallZoneVector<ShuffleToReadShifted, 8> shuffles_to_read_shifted()
      const {
    return shuffles_to_read_shifted_;
  }

  const Graph& input_graph() const { return input_graph_; }

 private:
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

  Zone* phase_zone_;
  const Graph& input_graph_;
  DemandedByteAnalysis demanded_byte_analysis_{phase_zone_, input_graph_};
  SmallZoneVector<ShuffleToShift, 8> shuffles_to_shift_{phase_zone_};
  SmallZoneVector<ShuffleToReadShifted, 8> shuffles_to_read_shifted_{
      phase_zone_};
  ZoneUnorderedMap<const LoadOp*, const Simd128ReplaceLaneOp*>
      load_lane_candidates_{phase_zone_};
  ZoneUnorderedMap<const Simd128ReplaceLaneOp*, OpIndex> load_lanes_{
      phase_zone_};
  SmallShuffleVector even_64x2_shuffles_{phase_zone_};
  SmallShuffleVector odd_64x2_shuffles_{phase_zone_};
  SmallShuffleVector even_32x4_shuffles_{phase_zone_};
  SmallShuffleVector odd_32x4_shuffles_{phase_zone_};
  SmallShuffleVector even_16x8_shuffles_{phase_zone_};
  SmallShuffleVector odd_16x8_shuffles_{phase_zone_};
  SmallShuffleVector even_8x16_shuffles_{phase_zone_};
  SmallShuffleVector odd_8x16_shuffles_{phase_zone_};
  base::SmallVector<DeinterleaveLoadCandidate, 8> deinterleave_load_candidates_;
};

template <class Next>
class WasmShuffleReducer : public Next {
 private:
  std::optional<WasmShuffleAnalyzer> analyzer_;

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

 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(WasmShuffleReducer)

  void Analyze() {
    analyzer_.emplace(__ phase_zone(), __ input_graph());
    analyzer_->Run();
    Next::Analyze();
  }

  OpIndex REDUCE_INPUT_GRAPH(Simd128ReplaceLane)(
      OpIndex ig_index, const Simd128ReplaceLaneOp& replace_lane) {
    if (!ShouldSkipOptimizationStep()) {
      if (auto load_lane = analyzer_->GetMergedLoadReplaceLane(replace_lane)) {
        return load_lane.value();
      }
    }
    return Next::ReduceInputGraphSimd128ReplaceLane(ig_index, replace_lane);
  }

  OpIndex REDUCE_INPUT_GRAPH(Load)(OpIndex ig_index, const LoadOp& load) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceInputGraphLoad(ig_index, load);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;

    if (auto maybe_replace_lane = analyzer_->IsLoadLaneCandidate(load)) {
      const Simd128ReplaceLaneOp* replace_lane = maybe_replace_lane.value();
      OpIndex og_into = __ template MapToNewGraph<true>(replace_lane->into());
      if (og_into.valid()) {
        V<WordPtr> base = __ MapToNewGraph(load.base());
        V<WordPtr> index;
        if (load.index().has_value()) {
          index = __ MapToNewGraph(load.index().value());
          if (load.offset != 0) {
            index = __ WordPtrAdd(index, load.offset);
          }
        } else {
          index = __ IntPtrConstant(load.offset);
        }

        Simd128LaneMemoryOp::LaneKind lane_kind =
            Simd128LaneMemoryOp::LaneKindFromBytes(
                load.loaded_rep.SizeInBytes());
        OpIndex load_lane = __ Simd128LaneMemory(
            base, index, og_into, Simd128LaneMemoryOp::Mode::kLoad, load.kind,
            lane_kind, replace_lane->lane, 0);
        analyzer_->RecordLoadLane(replace_lane, load_lane);
        return load_lane;
      }
    }

#if V8_TARGET_ARCH_ARM64
    if (!v8_flags.experimental_wasm_deinterleave_loads) goto no_change;

    if (load.loaded_rep != MemoryRepresentation::Simd128()) goto no_change;

    if (auto maybe_combined = MaybeAlreadyCombined(&load)) {
      return maybe_combined.value();
    }

    if (auto maybe_candidate = analyzer_->GetDeinterleaveCandidate(&load)) {
      WasmShuffleAnalyzer::DeinterleaveLoadCandidate candidate =
          maybe_candidate.value();
      const LoadOp& left = candidate.left;
      const LoadOp& right = candidate.right;
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
                                                        candidate.kind);
      AddCombinedLoad(left, og_index);
      AddCombinedLoad(right, og_index);

      AddDeinterleavedShuffle(candidate.even_shfop, og_index, 0);
      AddDeinterleavedShuffle(candidate.odd_shfop, og_index, 1);
      return og_index;
    }
#endif  // V8_TARGET_ARCH_ARM64
    goto no_change;
  }

  OpIndex REDUCE_INPUT_GRAPH(Simd128Shuffle)(OpIndex ig_index,
                                             const Simd128ShuffleOp& shuffle) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceInputGraphSimd128Shuffle(ig_index, shuffle);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;

    if (shuffle.kind != Simd128ShuffleOp::Kind::kI8x16) goto no_change;

    auto og_left = __ MapToNewGraph(shuffle.left());
    auto og_right = __ MapToNewGraph(shuffle.right());
#if V8_TARGET_ARCH_ARM64
    if (auto maybe_deinterleaved_load = IsDeinterleaveLoadShuffle(&shuffle)) {
      DCHECK(v8_flags.experimental_wasm_deinterleave_loads);
      const auto* deinterleaved_load = maybe_deinterleaved_load.value();
      return __ Projection(deinterleaved_load->og_index,
                           deinterleaved_load->result_index,
                           RegisterRepresentation::Simd128());
    }
#endif  // V8_TARGET_ARCH_ARM64

    std::array<uint8_t, kSimd128Size> shuffle_bytes = {0};
    std::copy(shuffle.shuffle, shuffle.shuffle + kSimd128Size,
              shuffle_bytes.begin());
    DemandedBytes demanded_bytes = analyzer_->GetDemandedBytes(&shuffle);
    bool emit_new_shuffle = !demanded_bytes.IsLow(kSimd128Size);

    // For 'shifted' shuffles, the ones that are operands to other shuffles,
    // we move the demanded elements into the LSBs.
    for (const auto& shuffle_to_shift : analyzer_->shuffles_to_shift()) {
      if (shuffle_to_shift.shuffle == &shuffle) {
        const uint8_t begin_index = shuffle_to_shift.begin_index;
        const uint8_t end_index = shuffle_to_shift.end_index;
        DCHECK(begin_index <= kSimd128Size);
        DCHECK(end_index <= kSimd128Size);
        DCHECK_LT(begin_index, end_index);
        std::copy(shuffle.shuffle + begin_index, shuffle.shuffle + end_index,
                  shuffle_bytes.begin());
        emit_new_shuffle = true;
      }
    }

    // For the shuffles that have shifted shuffles as operands, we need to
    // update the shuffle to read from the LSBs.
    for (const auto& shuffle_to_read_shifted :
         analyzer_->shuffles_to_read_shifted()) {
      if (shuffle_to_read_shifted.shuffle == &shuffle) {
        const uint8_t begin_index = shuffle_to_read_shifted.begin_index;
        const uint8_t end_index = shuffle_to_read_shifted.end_index;
        const uint8_t shuffle_in_begin =
            shuffle_to_read_shifted.shuffle_in_begin;
        DCHECK(begin_index <= kSimd128Size);
        DCHECK(end_index <= kSimd128Size);
        DCHECK_LT(begin_index, end_index);

        uint8_t base =
            shuffle.shuffle[begin_index] >= kSimd128Size ? kSimd128Size : 0;
        for (uint8_t i = begin_index; i < end_index; ++i) {
          uint8_t original = shuffle.shuffle[i];
          DCHECK_GE(original, shuffle_in_begin);
          shuffle_bytes[i] = base + (original - shuffle_in_begin);
        }
        emit_new_shuffle = true;
      }
    }

    if (emit_new_shuffle) {
      return __ Simd128Shuffle(og_left, og_right,
                               demanded_bytes.GetShuffleKind(),
                               shuffle_bytes.data());
    }

    goto no_change;
  }
};

}  // namespace v8::internal::compiler::turboshaft

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

#endif  // V8_COMPILER_TURBOSHAFT_WASM_SHUFFLE_REDUCER_H_
