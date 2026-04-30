// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-shuffle-reducer.h"

#include "src/base/iterator.h"
#include "src/flags/flags.h"
#include "src/wasm/simd-shuffle.h"

namespace v8::internal::compiler::turboshaft {

#define TRACE(...)                                             \
  do {                                                         \
    if (v8_flags.trace_wasm_simd_shuffle) PrintF(__VA_ARGS__); \
  } while (false)

void DemandedByteAnalysis::Add(OpIndex node, DemandedBytes demanded) {
  const Operation& op = input_graph().Get(node);
  // We're trying to find out which SIMD bytes of op are used. If op has more
  // than a single user we will have to visit it multiple times. Both `Record`
  // methods will attempt to recursively add more operations.
  if (op.saturated_use_count.Is(1)) {
    TRACE("Found Op %d, demanded bytes: %#04x\n", input_graph().Index(op).id(),
          static_cast<uint32_t>(demanded.bytes()));
    RecordOp(op, demanded);
  } else {
    RecordPartialOp(op, demanded);
  }
}

void DemandedByteAnalysis::AddOp(const Simd128UnaryOp& unop,
                                 DemandedBytes demanded) {
  if (Visited(&unop)) return;
  visited_.insert(&unop);

  if (IsUnaryLowHalfOp(unop.kind)) {
    demanded.Halve();
    Add(unop.input(), demanded);
  }
}

void DemandedByteAnalysis::AddOp(const Simd128BinopOp& binop,
                                 DemandedBytes demanded) {
  if (Visited(&binop)) return;
  visited_.insert(&binop);

  if (IsBinaryLowHalfOp(binop.kind)) {
    demanded.Halve();
    Add(binop.left(), demanded);
    Add(binop.right(), demanded);
  }
}

void DemandedByteAnalysis::AddOp(const Simd128ExtractLaneOp& extract_op,
                                 DemandedBytes demanded) {
  if (Visited(&extract_op)) return;
  visited_.insert(&extract_op);

  // F16 isn't supported yet because the element rep is Float32.
  if (extract_op.kind == Simd128ExtractLaneOp::Kind::kF16x8) return;

  uint8_t elem_size =
      ElementSizeInBytes(Simd128ExtractLaneOp::element_rep(extract_op.kind));
  demanded = DemandedBytes::LowFromLane(elem_size, extract_op.lane);
  TRACE("ExtractOp %d extracts lane %d, of size %d, so demands: %#04x\n",
        input_graph().Index(extract_op).id(), extract_op.lane, elem_size,
        static_cast<uint32_t>(demanded.bytes()));
  Add(extract_op.input(), demanded);
}

void DemandedByteAnalysis::AddOp(const Simd128LaneMemoryOp& lane_op,
                                 DemandedBytes demanded) {
  if (Visited(&lane_op)) return;
  visited_.insert(&lane_op);

  if (lane_op.mode == Simd128LaneMemoryOp::Mode::kStore) {
    demanded = DemandedBytes::LowFromLane(lane_op.lane_size(), lane_op.lane);
    TRACE("LaneMemoryOp %d stores lane %d, of size %d, so demands: %#04x\n",
          input_graph().Index(lane_op).id(), lane_op.lane, lane_op.lane_size(),
          static_cast<uint32_t>(demanded.bytes()));
    Add(lane_op.value(), demanded);
  }
}

void DemandedByteAnalysis::RecordOp(const Operation& op,
                                    DemandedBytes demanded) {
  if (auto* unop = op.TryCast<Simd128UnaryOp>()) {
    AddOp(*unop, demanded);
  } else if (auto* binop = op.TryCast<Simd128BinopOp>()) {
    AddOp(*binop, demanded);
  } else if (auto* extract_op = op.TryCast<Simd128ExtractLaneOp>()) {
    AddOp(*extract_op, demanded);
  } else if (auto* lane_op = op.TryCast<Simd128LaneMemoryOp>()) {
    AddOp(*lane_op, demanded);
  } else if (auto* shuffle = op.TryCast<Simd128ShuffleOp>()) {
    if (!demanded.IsAll()) {
      if (demanded_bytes_.size() < kMaxNumOperations) {
        demanded_bytes_.emplace_back(shuffle, demanded);
      } else if (!demanded_limit_reached_) {
        TRACE("Demanded elements limit reached in RecordOp\n");
        demanded_limit_reached_ = true;
      }
    }
  }
}

std::optional<DemandedBytes> DemandedByteAnalysis::AddUserAndCheckFoundAll(
    const Operation& op, const DemandedBytes& demanded) {
  for (MultiUserBits& multi_use : to_revisit_) {
    if (multi_use.op() == &op) {
      multi_use.Add(demanded);
      if (multi_use.FoundAllUsers()) {
        // Ensure we don't visit this again from the top-level search.
        visited_.insert(&op);
        TRACE("Found all users (%d) of Op %d, demanded bytes: %#04x\n",
              op.saturated_use_count.GetMaybeSaturated(),
              input_graph().Index(op).id(),
              static_cast<uint32_t>(multi_use.demanded().bytes()));
        return multi_use.demanded();
      }
      return {};
    }
  }
  if (to_revisit_.size() < kMaxNumOperations) {
    to_revisit_.emplace_back(&op, demanded, 1);
  } else if (!revisit_limit_reached_) {
    TRACE("Revisit limit reached\n");
    revisit_limit_reached_ = true;
  }
  return {};
}

void DemandedByteAnalysis::RecordPartialOp(const Operation& op,
                                           DemandedBytes demanded) {
  TRACE("Recording partial Op %d, demanded bytes: %#04x\n",
        input_graph().Index(op).id(), static_cast<uint32_t>(demanded.bytes()));
  if (auto* unop = op.TryCast<Simd128UnaryOp>()) {
    RecordPartialOp(*unop, demanded);
  } else if (auto* binop = op.TryCast<Simd128BinopOp>()) {
    RecordPartialOp(*binop, demanded);
  } else if (auto* shuffle = op.TryCast<Simd128ShuffleOp>()) {
    (void)AddUserAndCheckFoundAll(*shuffle, demanded);
  }
}

void DemandedByteAnalysis::RecordPartialOp(const Simd128UnaryOp& unop,
                                           DemandedBytes demanded) {
  if (IsUnaryLowHalfOp(unop.kind)) {
    // If we've now visited this unop from all of its users, visit its
    // operand.
    if (auto maybe_demanded = AddUserAndCheckFoundAll(unop, demanded)) {
      demanded = maybe_demanded.value();
      demanded.Halve();
      Add(unop.input(), demanded);
    }
  }
}

void DemandedByteAnalysis::RecordPartialOp(const Simd128BinopOp& binop,
                                           DemandedBytes demanded) {
  if (IsBinaryLowHalfOp(binop.kind)) {
    // If we've now visited this binop from all of its users, visit its
    // operands.
    if (auto maybe_demanded = AddUserAndCheckFoundAll(binop, demanded)) {
      demanded = maybe_demanded.value();
      demanded.Halve();
      Add(binop.left(), demanded);
      Add(binop.right(), demanded);
    }
  }
}

void DemandedByteAnalysis::RevisitShuffle(const Simd128ShuffleOp& shuffle,
                                          DemandedBytes demanded) {
  // The shuffle may have been recorded previously because it had a single user,
  // but that user may have been used by multiple others. Once all those users
  // have been visited, we may now know that fewer bytes are demanded.
  for (unsigned i = 0; i < demanded_bytes_.size(); ++i) {
    if (demanded_bytes_[i].first == &shuffle) {
      DCHECK(demanded.IsLessThanOrEqual(demanded_bytes_[i].second));
      demanded_bytes_[i].second = demanded;
      return;
    }
  }
  if (demanded_bytes_.size() < kMaxNumOperations) {
    demanded_bytes_.emplace_back(&shuffle, demanded);
  } else if (!demanded_limit_reached_) {
    TRACE("Demanded elements limit reached in RevisitShuffle\n");
    demanded_limit_reached_ = true;
  }
}

void DemandedByteAnalysis::Revisit() {
  for (const MultiUserBits& multi_user : to_revisit_) {
    if (multi_user.FoundAllUsers()) {
      const Operation* op = multi_user.op();
      if (auto* shuffle = op->TryCast<Simd128ShuffleOp>()) {
        RevisitShuffle(*shuffle, multi_user.demanded());
      }
    }
  }
  to_revisit_.clear();
  revisit_limit_reached_ = false;
}

void WasmShuffleAnalyzer::Run() {
  TRACE("Running WasmShuffleAnalyzer\n");
  for (uint32_t processed = input_graph().block_count(); processed > 0;
       --processed) {
    BlockIndex block_index = static_cast<BlockIndex>(processed - 1);
    TRACE("BLOCK %d\n", block_index.id());
    const Block& block = input_graph().Get(block_index);
    auto idx_range = input_graph().OperationIndices(block);
    for (OpIndex op_idx : base::Reversed(idx_range)) {
      const Operation& op = input_graph().Get(op_idx);
      Process(op);
    }
    demanded_byte_analysis_.Revisit();
    // TODO(sparker): Reset the state after each block? Currently our testing
    // requires being able to look at demanded_bytes.
  }
}

void WasmShuffleAnalyzer::Process(const Operation& op) {
  if (ShouldSkipOperation(op)) {
    return;
  }

  if (auto* unop = op.TryCast<Simd128UnaryOp>()) {
    ProcessUnary(*unop);
    return;
  }

  if (auto* binop = op.TryCast<Simd128BinopOp>()) {
    ProcessBinary(*binop);
    return;
  }

  if (auto* shuffle_op = op.TryCast<Simd128ShuffleOp>()) {
    ProcessShuffle(*shuffle_op);
    return;
  }

  if (auto* replace_op = op.TryCast<Simd128ReplaceLaneOp>()) {
    ProcessReplaceLane(*replace_op);
    return;
  }

  if (auto* extract_op = op.TryCast<Simd128ExtractLaneOp>()) {
    ProcessExtractLane(*extract_op);
    return;
  }

  if (auto* lane_op = op.TryCast<Simd128LaneMemoryOp>()) {
    ProcessLaneMemory(*lane_op);
    return;
  }
}

void WasmShuffleAnalyzer::ProcessUnary(const Simd128UnaryOp& unop) {
  demanded_byte_analysis_.AddOp(unop, DemandedBytes::All());
}

void WasmShuffleAnalyzer::ProcessBinary(const Simd128BinopOp& binop) {
  demanded_byte_analysis_.AddOp(binop, DemandedBytes::All());
}

void WasmShuffleAnalyzer::ProcessReplaceLane(
    const Simd128ReplaceLaneOp& replace_op) {
  // FP16 isn't supported.
  if (replace_op.kind == Simd128ReplaceLaneOp::Kind::kF16x8) return;

  if (const LoadOp* load =
          input_graph().Get(replace_op.new_lane()).TryCast<LoadOp>()) {
    int replace_bytes =
        ElementSizeInBytes(Simd128ReplaceLaneOp::element_rep(replace_op.kind));
    if (load->saturated_use_count.Is(1) &&
        load->loaded_rep.SizeInBytes() == replace_bytes &&
        !load->kind.tagged_base && !load->kind.is_atomic) {
      AddLoadLaneCandidate(load, replace_op);
    }
  }
}

void WasmShuffleAnalyzer::ProcessExtractLane(
    const Simd128ExtractLaneOp& extract_op) {
  demanded_byte_analysis_.AddOp(extract_op, DemandedBytes::All());
}

void WasmShuffleAnalyzer::ProcessLaneMemory(
    const Simd128LaneMemoryOp& lane_op) {
  demanded_byte_analysis_.AddOp(lane_op, DemandedBytes::All());
}

// Searches for a contiguous window of size `shuffle_in_demanded` within
// `shuffle` such that all elements in the window are sourced from the range
// `[lower_limit, upper_limit]`, and all elements outside of the window are
// sourced outside of this range. Returns the starting index of the window if it
// exists. Example of what would be accepted with lower and upper limits of 0
// and 15, respectively are: [ 9 ] [ 8, 9 ] [ 2, 3, 4, 5 ]. [ 5, 4, 3, 2 ] [ 3,
// 2, 5, 4 ] [ 3, 3, 3, 3 ] [ 0, 2, 4, 6, 8, 10, 12, 14 ] In these cases, we
// also have to ensure that there's not another instance of a value 0-15 in the
// shuffle.
std::optional<uint8_t> GetFirstIndex(const uint8_t* shuffle,
                                     uint8_t lower_limit, uint8_t upper_limit,
                                     DemandedBytes shuffle_in_demanded,
                                     DemandedBytes shuffle_out_demanded) {
  DCHECK_LE(shuffle_in_demanded.bytes(), kSimd128HalfSize);
  DCHECK_LE(shuffle_out_demanded.bytes(), kSimd128Size);

  if (shuffle_in_demanded.bytes() > shuffle_out_demanded.bytes()) return {};

  auto in_range = [&lower_limit, &upper_limit](uint8_t i) {
    return i >= lower_limit && i <= upper_limit;
  };

  // Sliding window, of shuffle_in_demanded.bytes(), over shuffle[0, N-1],
  // inclusive. Stop once a full window would exceed bounds.
  for (unsigned i = 0;
       i + shuffle_in_demanded.bytes() <= shuffle_out_demanded.bytes(); ++i) {
    const uint8_t* first = shuffle + i;
    // Test for a valid range in shuffle_in_demanded_bytes, with no other
    // instances of in_range.
    if (std::all_of(first, first + shuffle_in_demanded.bytes(), in_range) &&
        std::none_of(first + shuffle_in_demanded.bytes(),
                     shuffle + shuffle_out_demanded.bytes(), in_range) &&
        std::none_of(shuffle, first, in_range)) {
      return i;
    }
  }
  return {};
}

bool WasmShuffleAnalyzer::ProcessShuffleOfShuffle(
    const Simd128ShuffleOp& shuffle_in, const Simd128ShuffleOp& shuffle_out,
    uint8_t lower_limit, uint8_t upper_limit) {
  TRACE("Shuffling ShuffleOp %d with lower (%d) and upper (%d)\n",
        input_graph().Index(shuffle_in).id(), lower_limit, upper_limit);
  // Suppose we have two 16-byte shuffles:
  // |---a1---|---b3---|--------|--------|  shuffle_in = (a, b)
  //
  // |---a1---|---b3---|---c?---|---c?---|  shuffle_out = (shuffle_in, c)
  //
  // As only half of the shuffle_in is used, it means that half the work of
  // shuffle_in is wasted so, here, we try to reduce shuffle_in to a more narrow
  // kind. In the case above we can simply truncate shuffle_in.shuffle but
  // there are other situations which involve more work:
  //
  // In the following case, shuffle_in.shuffle needs to be shifted left so that
  // it writes the required bytes to the low half of the result. This then means
  // that shuffle_out.shuffle needs to be updated to read from the low half.
  //
  // |--------|--------|---a1---|---b3---|  shuffle_in = (a, b)
  //
  // |---a1---|---b3---|---c?---|---c?---|  shuffle_out = (shuffle_in, c)
  //

  DemandedBytes shuffle_out_demanded = GetDemandedBytes(&shuffle_out);
  std::array<DemandedBytes, 4> shuffle_in_demanded_bytes = {
      DemandedBytes::Low(8), DemandedBytes::Low(4), DemandedBytes::Low(2),
      DemandedBytes::Low(1)};
  for (auto shuffle_in_demanded : shuffle_in_demanded_bytes) {
    if (auto maybe_shuffle_out_begin =
            GetFirstIndex(shuffle_out.shuffle, lower_limit, upper_limit,
                          shuffle_in_demanded, shuffle_out_demanded)) {
      // The incoming shuffle can be reduced because, at least a part of, it
      // defines a consecutive part of shuffle_out.
      const uint8_t shuffle_out_begin = maybe_shuffle_out_begin.value();
      const uint8_t* window_begin = shuffle_out.shuffle + shuffle_out_begin;
      const uint8_t* window_end = window_begin + shuffle_in_demanded.bytes();
      const uint8_t shuffle_in_begin =
          *std::min_element(window_begin, window_end);
      const uint8_t shuffle_in_max =
          *std::max_element(window_begin, window_end);

      // Calculate the span of used indices and calculate the demanded bytes
      // from it.
      const uint8_t span = shuffle_in_max - shuffle_in_begin + 1;
      shuffle_in_demanded = DemandedBytes::LowFromTotalBytes(span);

      if (shuffle_in_demanded.IsAll()) continue;

      const uint8_t shuffle_in_end =
          shuffle_in_begin + shuffle_in_demanded.bytes() - 1;

      TRACE("Can reduce shuffle to a size of %d, with the LSB %d\n",
            shuffle_in_demanded.bytes(), shuffle_in_begin);
      // If shuffle_in_begin isn't 0 or 16, which would be the LSB of the left
      // or right input, the shuffles will need modifying.
      if (shuffle_in_begin % kSimd128Size) {
        TRACE("Need to shift ShuffleOp %d elements %d - %d down to LSBs\n",
              input_graph().Index(shuffle_in).id(), shuffle_in_begin,
              shuffle_in_end);
        TRACE("Need to modify ShuffleOp %d to read from %d\n",
              input_graph().Index(shuffle_out).id(), shuffle_out_begin);
        // shuffle_in needs to write the selected shuffle_in_demanded_bytes of
        // bytes into its LSBs.
        shuffles_to_shift_.emplace_back(&shuffle_in,
                                        shuffle_in_begin % kSimd128Size,
                                        shuffle_in_end % kSimd128Size + 1);
        // shuffle_out.shuffle will need to be updated to read the modified
        // shuffle_in.
        shuffles_to_read_shifted_.emplace_back(
            &shuffle_out, shuffle_out_begin,
            shuffle_out_begin + shuffle_out_demanded.bytes(), shuffle_in_begin);
      }
      // shuffle_in will be reduced.
      demanded_byte_analysis_.RecordOp(shuffle_in, shuffle_in_demanded);
      return true;
    }
  }
  return false;
}

bool WasmShuffleAnalyzer::CouldLoadPair(const LoadOp& load0,
                                        const LoadOp& load1) const {
  DCHECK_NE(load0, load1);
  // Before adding a candidate, ensure that the loads can be scheduled together
  // and that the accesses are consecutive.
  if (load0.kind != load1.kind) {
    return false;
  }

  // Only try this for loads in the same block.
  if (input_graph().BlockIndexOf(load0) != input_graph().BlockIndexOf(load1)) {
    return false;
  }

  if (load0.base() != load1.base()) {
    return false;
  }
  if (load0.index() != load1.index()) {
    return false;
  }

  // TODO(sparker): Can we use absolute diff..? I guess this would require
  // some permutation after the load.
  if (load1.offset - load0.offset != kSimd128Size) {
    return false;
  }

  // Calculate whether second can be moved next to first by comparing the
  // effects through the sequence of operations in between.
  auto CanReschedule = [this](OpIndex first_idx, OpIndex second_idx) {
    const Operation& second = input_graph().Get(second_idx);
    OpEffects second_effects = second.Effects();
    OpIndex prev_idx = input_graph().PreviousIndex(second_idx);
    bool second_is_trapping_load = second.IsTrappingLoad();

    while (prev_idx != first_idx) {
      const Operation& prev_op = input_graph().Get(prev_idx);
      OpEffects prev_effects = prev_op.Effects();
      bool both_protected = second_is_trapping_load && prev_op.IsTrappingLoad();
      if (both_protected &&
          CannotSwapTrappingLoads(prev_effects, second_effects)) {
        return false;
      } else if (CannotSwapOperations(prev_effects, second_effects)) {
        return false;
      }
      prev_idx = input_graph().PreviousIndex(prev_idx);
    }
    return true;
  };

  OpIndex first_idx = input_graph().Index(load0);
  OpIndex second_idx = input_graph().Index(load1);
  if (first_idx.offset() > second_idx.offset()) {
    std::swap(first_idx, second_idx);
  }

  return CanReschedule(first_idx, second_idx);
}

void WasmShuffleAnalyzer::AddLoadMultipleCandidate(
    const Simd128ShuffleOp& even_shuffle, const Simd128ShuffleOp& odd_shuffle,
    const LoadOp& left, const LoadOp& right,
    Simd128LoadPairDeinterleaveOp::Kind kind) {
  DCHECK_NE(even_shuffle, odd_shuffle);

  if (CouldLoadPair(left, right)) {
    deinterleave_load_candidates_.emplace_back(kind, left, right, even_shuffle,
                                               odd_shuffle);
  }
}

void WasmShuffleAnalyzer::ProcessShuffleOfLoads(const Simd128ShuffleOp& shuffle,
                                                const LoadOp& left,
                                                const LoadOp& right) {
  DCHECK_EQ(shuffle.left(), input_graph().Index(left));
  DCHECK_EQ(shuffle.right(), input_graph().Index(right));
  // We're looking for two shuffle users of these two loads, so ensure the
  // number of users doesn't exceed two.
  if (!left.saturated_use_count.Is(2) || !right.saturated_use_count.Is(2)) {
    return;
  }

  // Look for even and odd shuffles. If we find one, then also check if we've
  // already found another shuffle, performing the opposite action, on the same
  // two loads. And if this is true, then we add a candidate including both
  // shuffles and both loads.
  auto AddShuffle = [this, &shuffle, &left, &right](
                        SmallShuffleVector& shuffles,
                        const SmallShuffleVector& other_shuffles, bool is_even,
                        Simd128LoadPairDeinterleaveOp::Kind kind) {
    shuffles.push_back(&shuffle);
    if (auto other_shuffle = GetOtherShuffleUser(left, right, other_shuffles)) {
      if (is_even) {
        AddLoadMultipleCandidate(shuffle, *other_shuffle.value(), left, right,
                                 kind);
      } else {
        AddLoadMultipleCandidate(*other_shuffle.value(), shuffle, left, right,
                                 kind);
      }
    }
  };

  if (GetDemandedBytes(&shuffle).IsAll()) {
    // Full width shuffles.
    wasm::SimdShuffle::ShuffleArray shuffle_bytes;
    std::copy_n(shuffle.shuffle, kSimd128Size, shuffle_bytes.begin());
    auto canonical = wasm::SimdShuffle::TryMatchCanonical(shuffle_bytes);
    switch (canonical) {
      default:
        return;
      case wasm::SimdShuffle::CanonicalShuffle::kS64x2Even:
        AddShuffle(even_64x2_shuffles_, odd_64x2_shuffles_, true,
                   Simd128LoadPairDeinterleaveOp::Kind::k64x4);
        break;
      case wasm::SimdShuffle::CanonicalShuffle::kS64x2Odd:
        AddShuffle(odd_64x2_shuffles_, even_64x2_shuffles_, false,
                   Simd128LoadPairDeinterleaveOp::Kind::k64x4);
        break;
      case wasm::SimdShuffle::CanonicalShuffle::kS32x4Even:
        AddShuffle(even_32x4_shuffles_, odd_32x4_shuffles_, true,
                   Simd128LoadPairDeinterleaveOp::Kind::k32x8);
        break;
      case wasm::SimdShuffle::CanonicalShuffle::kS32x4Odd:
        AddShuffle(odd_32x4_shuffles_, even_32x4_shuffles_, false,
                   Simd128LoadPairDeinterleaveOp::Kind::k32x8);
        break;
      case wasm::SimdShuffle::CanonicalShuffle::kS16x8Even:
        AddShuffle(even_16x8_shuffles_, odd_16x8_shuffles_, true,
                   Simd128LoadPairDeinterleaveOp::Kind::k16x16);
        break;
      case wasm::SimdShuffle::CanonicalShuffle::kS16x8Odd:
        AddShuffle(odd_16x8_shuffles_, even_16x8_shuffles_, false,
                   Simd128LoadPairDeinterleaveOp::Kind::k16x16);
        break;
      case wasm::SimdShuffle::CanonicalShuffle::kS8x16Even:
        AddShuffle(even_8x16_shuffles_, odd_8x16_shuffles_, true,
                   Simd128LoadPairDeinterleaveOp::Kind::k8x32);
        break;
      case wasm::SimdShuffle::CanonicalShuffle::kS8x16Odd:
        AddShuffle(odd_8x16_shuffles_, even_8x16_shuffles_, false,
                   Simd128LoadPairDeinterleaveOp::Kind::k8x32);
        break;
    }
  }
}

// If 'input' is only used by 'shuffle' we may be able to reduce input
// based upon the most-significant byte used by shuffle.
void WasmShuffleAnalyzer::TryReduceFromMSB(OpIndex input,
                                           const Simd128ShuffleOp& shuffle,
                                           uint8_t lower_limit,
                                           uint8_t upper_limit) {
  DemandedBytes demanded = GetDemandedBytes(&shuffle);
  std::optional<uint8_t> max = {};

  for (unsigned i = 0; i < demanded.bytes(); ++i) {
    uint8_t index = shuffle.shuffle[i];
    if (index >= lower_limit && index <= upper_limit) {
      max = std::max(static_cast<uint8_t>(index % kSimd128Size),
                     max.value_or(uint8_t{0}));
    }
  }
  if (max) {
    // input can be reduced.
    TRACE("Can reduce Op %d based upon max used index: %d\n", input.id(),
          max.value());
    demanded_byte_analysis_.Add(
        input, DemandedBytes::LowFromMaxShuffleIndex(max.value()));
  }
}

void WasmShuffleAnalyzer::ProcessShuffle(const Simd128ShuffleOp& shuffle) {
  constexpr uint8_t left_lower = 0;
  constexpr uint8_t left_upper = 15;
  constexpr uint8_t right_lower = 16;
  constexpr uint8_t right_upper = 31;
  bool reduced_left = false;
  bool reduced_right = false;
  const Operation& left = input_graph().Get(shuffle.left());
  const Operation& right = input_graph().Get(shuffle.right());

#if V8_TARGET_ARCH_ARM64

  if (shuffle.kind != Simd128ShuffleOp::Kind::kI8x16) {
    return;
  }

  // Experimental flags controls the generation of deinterleaving loads.
  if (v8_flags.experimental_wasm_deinterleave_loads) {
    auto* load_left = left.TryCast<LoadOp>();
    auto* load_right = right.TryCast<LoadOp>();

    // TODO(sparker): Handle I8x8 shuffles, which will likely mean that the
    // input to the shuffles are Simd128LoadTransformOp::Load64Zero.
    if (load_left && load_right) {
      ProcessShuffleOfLoads(shuffle, *load_left, *load_right);
      return;
    }
  }

  // Experimental flags controls reducing shuffles depending on specific bit
  // patterns.
  if (v8_flags.experimental_wasm_simd_opt) {
    auto* shuffle_left = left.TryCast<Simd128ShuffleOp>();
    auto* shuffle_right = right.TryCast<Simd128ShuffleOp>();
    if (shuffle_left && shuffle_left->kind == Simd128ShuffleOp::Kind::kI8x16 &&
        shuffle_left->saturated_use_count.Is(1)) {
      reduced_left = ProcessShuffleOfShuffle(*shuffle_left, shuffle, left_lower,
                                             left_upper);
    }
    if (shuffle_right &&
        shuffle_right->kind == Simd128ShuffleOp::Kind::kI8x16 &&
        shuffle_right->saturated_use_count.Is(1)) {
      reduced_right = ProcessShuffleOfShuffle(*shuffle_right, shuffle,
                                              right_lower, right_upper);
    }
  }

#endif  // V8_TARGET_ARCH_ARM64

  uint8_t max_uses = shuffle.left() == shuffle.right() ? 2 : 1;
  if (!reduced_left && left.saturated_use_count.Is(max_uses)) {
    TryReduceFromMSB(shuffle.left(), shuffle, left_lower, left_upper);
  }
  if (!reduced_right && right.saturated_use_count.Is(max_uses)) {
    TryReduceFromMSB(shuffle.right(), shuffle, right_lower, right_upper);
  }
}

#undef TRACE

}  // namespace v8::internal::compiler::turboshaft
