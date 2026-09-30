// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/safepoint-table.h"

#include <iomanip>

#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/diagnostics/disasm.h"
#include "src/execution/frames-inl.h"
#include "src/utils/ostreams.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-code-manager.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

// Chosen for convenience.
// PCs start at 1 to make deduplicated early entries count as initialized.
// Deopt indices start at -1 because a delta of 0 has special meaning (namely
// "this entry has no deopt index"), and a real index of 0 should be encodable
// (with delta=1).
static constexpr int kImplicitStartPC = 1;
static constexpr int kImplicitStartDeoptIndex = -1;
static constexpr int kImplicitStartTrampoline = 0;

SafepointTable::SafepointTable(Isolate* isolate, Address pc, Tagged<Code> code)
    : SafepointTable(code->InstructionStart(isolate, pc),
                     code->safepoint_table_address()) {
  DCHECK(code->is_turbofanned());
}

SafepointTable::SafepointTable(Isolate* isolate, Address pc,
                               Tagged<GcSafeCode> code)
    : SafepointTable(code->InstructionStart(isolate, pc),
                     code->safepoint_table_address()) {
  DCHECK(code->is_turbofanned());
}

#if V8_ENABLE_WEBASSEMBLY
SafepointTable::SafepointTable(const wasm::WasmCode* code)
    : SafepointTable(
          code->instruction_start(),
          code->instruction_start() + code->safepoint_table_offset()) {}
#endif  // V8_ENABLE_WEBASSEMBLY

SafepointTable::SafepointTable(Address instruction_start,
                               Address safepoint_table_address)
    : instruction_start_(instruction_start),
      safepoint_table_address_(
          reinterpret_cast<const uint8_t*>(safepoint_table_address)),
      stack_slots_(base::Memory<SafepointTableStackSlotsField_t>(
          safepoint_table_address + kStackSlotsOffset)),
      byte_length_(ByteLengthField::decode(
          base::Memory<uint32_t>(safepoint_table_address + kConfigOffset))),
      has_tagged_registers_(HasTaggedRegistersField::decode(
          base::Memory<uint32_t>(safepoint_table_address + kConfigOffset))),
      has_deopt_data_(HasDeoptDataField::decode(
          base::Memory<uint32_t>(safepoint_table_address + kConfigOffset))),
      end_(safepoint_table_address_ + byte_length_) {
  ResetIteration();
}

namespace {

// TODO(jkummerow): Consider a faster encoding scheme.
V8_INLINE void emit_uleb(Assembler* assembler, uint32_t value) {
  while (value >= 0x80) {
    assembler->db((value & 0x7f) | 0x80);
    value >>= 7;
  }
  assembler->db(value);
}
V8_INLINE uint32_t read_uleb(const uint8_t** ptr) {
  uint8_t byte = *(*ptr)++;
  if (!(byte & 0x80)) return byte;
  uint32_t result = byte & 0x7F;
  int shift = 7;
  do {
    byte = *(*ptr)++;
    result |= (byte & 0x7F) << shift;
    shift += 7;
  } while (byte & 0x80);
  return result;
}

// Fewer than 7 skipped bits are more efficient to store in the xor mask.
// Consequence: we can slightly extend the range of a single skip byte by
// subtracting this value from the encoded value.
constexpr int kMinSkipBits = 7;
}  // namespace

void EncodeSafepointEntry(int stack_slot_count, uint32_t common_prefix,
                          const BitVector* patch, Assembler* assembler) {
  if (patch == nullptr) {
    assembler->db(0);
    return;
  }
  DCHECK_LT(common_prefix, stack_slot_count);
  DCHECK_NE(patch->length(), 0);
  // The extra -1 ensures that the highest index is mapped to 0.
  auto flip = [stack_slot_count](int index) {
    return stack_slot_count - 1 - index;
  };
  uint32_t tail_bits = stack_slot_count - common_prefix;
  uint32_t tail_bytes = (tail_bits + kBitsPerByte - 1) / kBitsPerByte;
  uint32_t skipped_bits = tail_bits - patch->length();

  // Estimate the sizes of both encoding possibilities. These are
  // approximations because we assume that all counts fit into one LEB byte.
  // If they don't, then the overall size will be so large that the estimate
  // being off by one doesn't really change the picture.
  int byte_for_length = 1;
  int approx_bytes_full_encoding = byte_for_length + tail_bytes;
  int bytes_for_skip = 0;
  int leb_payload_bits = patch->length();
  if (skipped_bits >= kMinSkipBits) {
    bytes_for_skip = 1;
  } else {
    leb_payload_bits += skipped_bits;
  }
  int approx_bytes_compact_encoding =
      bytes_for_skip + (leb_payload_bits + 6) / 7;
  auto it = patch->rbegin();
  auto end = patch->rend();

  // Full encoding.
  if (approx_bytes_full_encoding < approx_bytes_compact_encoding) {
    emit_uleb(assembler, tail_bytes << 1);
#if DEBUG
    int debug_pos_before_stackmap = assembler->pc_offset();
#endif
    int end_of_current = kBitsPerByte;
    uint8_t current = 0;
    while (it != end) {
      DCHECK_LT(common_prefix + *it, stack_slot_count);
      DCHECK_LT(*it, tail_bits);
      int bit = flip(common_prefix + *it);
      while (bit >= end_of_current) {
        assembler->db(current);
        current = 0;
        end_of_current += kBitsPerByte;
      }
      current |= (1 << (bit % kBitsPerByte));
      ++it;
    }
    assembler->db(current);
    DCHECK_EQ(tail_bytes, assembler->pc_offset() - debug_pos_before_stackmap);
    return;
  }

  // Compact encoding.
  if (skipped_bits >= kMinSkipBits) {
    uint32_t encoded_skipped_bits = skipped_bits - kMinSkipBits;
    DCHECK_EQ(encoded_skipped_bits, (encoded_skipped_bits << 2) >> 2);
    emit_uleb(assembler, (encoded_skipped_bits << 2) | 0b11);
  } else {
    skipped_bits = 0;
  }

  uint8_t current = 0b01;
  int start_of_current = skipped_bits - 2 /* tag size */;
  int end_of_current = start_of_current + 7;
  while (it != end) {
    int bit = flip(common_prefix + *it);
    DCHECK_LT(bit, stack_slot_count);
    DCHECK_GE(bit, start_of_current);
    while (bit >= end_of_current) {
      assembler->db(0x80 | current);
      current = 0;
      start_of_current = end_of_current;
      end_of_current = start_of_current + 7;
    }
    current |= 1 << (bit - start_of_current);
    ++it;
  }
  assembler->db(current);
}

// Updates {ptr} as it consumes input.
template <bool update_tagged_slots>
void DecodeSafepointEntry(const uint8_t** ptr,
                          base::OwnedVector<uint8_t>& tagged_slots) {
  uint8_t first_byte = **ptr;
  if (first_byte == 0) {
    (*ptr)++;
    return;
  }
  uint8_t* cursor = tagged_slots.data();

  // Just skip the right number of bytes if actual decoding is not requested.
  if constexpr (!update_tagged_slots) {
    if ((first_byte & 1) == 0) {
      uint32_t num_bytes = read_uleb(ptr) >> 1;
      *ptr += num_bytes;
      return;
    }
    if ((first_byte & 0b11) == 0b11) {
      read_uleb(ptr);  // skip_bits
    }
    for (uint8_t byte = *(*ptr)++; byte & 0x80; byte = *(*ptr)++) continue;
    return;
  }

  // Full encoding.
  if ((first_byte & 1) == 0) {
    uint32_t num_bytes = read_uleb(ptr) >> 1;
    while (num_bytes > kSystemPointerSize) {
      uintptr_t current;
      memcpy(&current, cursor, kSystemPointerSize);
      uintptr_t patch;
      memcpy(&patch, *ptr, kSystemPointerSize);
      current ^= patch;
      memcpy(cursor, &current, kSystemPointerSize);
      cursor += kSystemPointerSize;
      *ptr += kSystemPointerSize;
      num_bytes -= kSystemPointerSize;
    }
    while (num_bytes > 0) {
      *cursor++ ^= *(*ptr)++;
      --num_bytes;
    }
    return;
  }

  // Compact encoding.
  uint32_t skip_bits = 0;

  if ((first_byte & 0b11) == 0b11) {
    skip_bits = (read_uleb(ptr) >> 2) + kMinSkipBits;
    if (skip_bits >= kBitsPerByte) {
      cursor += skip_bits / kBitsPerByte;
      skip_bits %= kBitsPerByte;
    }
  }
  // The first byte is special: it has the tag, so only 5 bits of payload.
  uint8_t patch_byte = *(*ptr)++;
  uint32_t patch = ((patch_byte >> 2) & 0x1F) << skip_bits;
  int bits_in_patch = 5 + skip_bits;
  while (true) {
    // If we have accumulated enough bits in {patch}, apply them.
    if (bits_in_patch >= kBitsPerByte) {
      *cursor++ ^= patch;
      patch >>= kBitsPerByte;
      bits_in_patch -= kBitsPerByte;
    }
    // If there are no more bytes to decode, apply any remaining patch bits
    // and terminate the loop...
    if (!(patch_byte & 0x80)) {
      // Checking for {patch != 0} is important: while xor-applying an all-zero
      // byte would be a no-op, it could be an out-of-bounds no-op.
      if (patch != 0) *cursor ^= patch;
      break;
    }
    // ...otherwise read the next byte.
    patch_byte = *(*ptr)++;
    patch |= (patch_byte & 0x7F) << bits_in_patch;
    bits_in_patch += 7;
  }
}

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) void DecodeSafepointEntry<
    true>(const uint8_t** ptr, base::OwnedVector<uint8_t>& tagged_slots);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) void DecodeSafepointEntry<
    false>(const uint8_t** ptr, base::OwnedVector<uint8_t>& tagged_slots);

void SafepointTable::ResetIteration() {
  ptr_ = safepoint_table_address_ + kHeaderSize;
  entry_.ResetTaggedSlots(stack_slots_);
  entry_.pc_ = kImplicitStartPC;
  entry_.deopt_index_ = SafepointEntry::kNoDeoptIndex;
  entry_.trampoline_pc_ = SafepointEntry::kNoTrampolinePC;
  last_deopt_index_ = kImplicitStartDeoptIndex;
  last_trampoline_ = kImplicitStartTrampoline;
  next_pc_ = has_more() ? kImplicitStartPC + read_uleb(&ptr_) : kMaxUInt32;
}

template <bool update_tagged_slots>
void SafepointTable::Advance() {
  DCHECK(has_more());
  entry_.pc_ = next_pc_;
  if (has_deopt_data_) {
    uint32_t deopt_index_delta = read_uleb(&ptr_);
    if (deopt_index_delta == 0) {
      entry_.deopt_index_ = SafepointEntry::kNoDeoptIndex;
      entry_.trampoline_pc_ = SafepointEntry::kNoTrampolinePC;
    } else {
      last_deopt_index_ += deopt_index_delta;
      uint32_t trampoline_delta = read_uleb(&ptr_);
      last_trampoline_ += trampoline_delta;
      entry_.deopt_index_ = last_deopt_index_;
      entry_.trampoline_pc_ = last_trampoline_;
    }
  }
  if (has_tagged_registers_) {
    entry_.tagged_register_indexes_ = read_uleb(&ptr_);
  }
  DecodeSafepointEntry<update_tagged_slots>(&ptr_, entry_.tagged_slots_);
  next_pc_ = has_more() ? next_pc_ + read_uleb(&ptr_) : kMaxUInt32;
}

int SafepointTable::FindReturnPC(int pc_offset) {
  while (has_more()) {
    Advance<false>();
    if (entry_.trampoline_pc() == pc_offset || entry_.pc() == pc_offset) {
      return entry_.pc();
    }
  }
  UNREACHABLE();
}

template <bool update_tagged_slots>
void SafepointTable::FindEntryImpl(Address pc) {
  // Special case: if all entries were identical to the implicit default
  // (except for their pc), they won't be stored, so we have no bytes to
  // decode. We'll take this early exit when {ResetIteration()} found
  // {!has_more()} even before decoding the first pc; we use an alternative
  // phrasing here to protect against API misuse: if some future caller calls
  // {FindEntry()} multiple times on the same {SafepointTable}, the DCHECK
  // below will tell them that they're holding it wrong, whereas if we had
  // "if (!has_more()) return" here, that would silently give wrong results.
  if (ptr_ == safepoint_table_address_ + kHeaderSize) return;
  // We could support multiple searches, but currently we have no reason to.
  DCHECK(has_more());
  int pc_offset = static_cast<int>(pc - instruction_start_);

  // This iteration is a bit tricky because:
  // (1) we can only step forward
  // (2) each step is relatively costly
  // (3) trampoline PCs are interleaved with regular PCs
  // (4) we can only return the current entry, which might already be too far.
  // For regular pc hits, we address this by decoding {next_pc_} as part of
  // the previous step. For trampoline hits, we have to restart from the
  // beginning to get back to the entry we need to return.
  // Consider the following example:
  //   pc = 10
  //   pc = 12, trampoline = 20
  //   pc = 14
  //   pc = 16, trampoline = 22
  //   pc = 18
  // When we enter this function, {next_pc_ == 10}.
  // If we're looking for:
  //   - 8: we should return immediately.
  //   - 10: we should return when entry_.pc_ == 10.
  //   - 11: we should return when next_pc_ == 12. Can be folded with case "10"
  //         into one condition: return when next_pc_ > entry_.pc_.
  //   - 19: we should return when we reach the end, because we haven't found
  //         any trampoline candidates.
  //   - 20: we should return when trampoline == 20.
  //   - 21: when we reach "trampoline == 22", we realize that we're too far,
  //         so we must restart and iterate until "trampoline == 20".
  //   - 25: when we reach the end, we realize that we're too far, so we must
  //         restart and iterate until "trampoline == 22".

  int trampoline_candidate = 0;
  while (next_pc_ <= pc_offset && has_more()) {
    Advance<update_tagged_slots>();
    int trampoline_pc = entry_.trampoline_pc();
    if (trampoline_pc != SafepointEntry::kNoTrampolinePC) {
      // In the "20" example, return now.
      if (pc_offset == trampoline_pc) return;
      // In the "21" example, remember "trampoline == 20".
      if (pc_offset >= trampoline_pc) {
        trampoline_candidate = entry_.trampoline_pc_;
      }
      // In the "21" example, stop searching at "trampoline == 22".
      if (trampoline_candidate && trampoline_pc > pc_offset) break;
    }
  }
  // The "8", "10", "11", "19" examples return here.
  if (trampoline_candidate == 0) return;
  // The "21" and "25" examples must restart the iteration in order to go back
  // to the correct entry.
  ResetIteration();
  while (entry_.trampoline_pc() != trampoline_candidate) {
    Advance<update_tagged_slots>();
  }
}

SafepointEntry& SafepointTable::FindEntry(Address pc) {
  FindEntryImpl<true>(pc);
  CHECK(entry_.is_initialized());
  return entry_;
}

SafepointEntry& SafepointTable::FindEntry_NoStackSlots(Address pc) {
  FindEntryImpl<false>(pc);
  CHECK(entry_.is_initialized());
  return entry_;
}

void SafepointTable::Print(std::ostream& os) {
  os << "Safepoints (stack slots = " << stack_slots_
     << ", byte size = " << byte_length_ << ")\n";
  if (!has_more()) {
    os << "All bits in all entries are zero, no data was stored.\n";
    return;
  }

  while (has_more()) {
    Advance<true>();
    os << reinterpret_cast<const void*>(instruction_start_ + entry_.pc()) << " "
       << std::setw(6) << std::hex << entry_.pc() << std::dec;

    if (!entry_.tagged_slots().empty()) {
      os << "  slots (sp->fp): ";
      uint32_t i = 0;
      for (uint8_t bits : entry_.tagged_slots()) {
        for (int bit = 0; bit < kBitsPerByte && i < stack_slots_; ++bit, ++i) {
          os << ((bits >> bit) & 1);
        }
      }
      // The tagged slots bitfield ends at the min stack slot (rounded up to the
      // nearest byte) -- we might have some slots left over in the stack frame
      // before the fp, so print zeros for those.
      for (; i < stack_slots_; ++i) {
        os << 0;
      }
    }

    if (entry_.tagged_register_indexes() != 0) {
      os << "  registers: ";
      uint32_t register_bits = entry_.tagged_register_indexes();
      int bits = 32 - base::bits::CountLeadingZeros32(register_bits);
      for (int j = bits - 1; j >= 0; --j) {
        os << ((register_bits >> j) & 1);
      }
    }

    if (entry_.has_deoptimization_index()) {
      os << "  deopt " << std::setw(6) << entry_.deoptimization_index()
         << " trampoline: " << std::setw(6) << std::hex
         << entry_.trampoline_pc();
    }
    os << "\n";
  }
}

SafepointTableBuilder::Safepoint SafepointTableBuilder::DefineSafepoint(
    Assembler* assembler, int pc_offset) {
  pc_offset = pc_offset ? pc_offset : assembler->pc_offset_for_safepoint();
  std::swap(current_entry_, previous_entry_);
  current_entry_->Reset(pc_offset);
  return SafepointTableBuilder::Safepoint(current_entry_, this);
}

void SafepointTableBuilder::Commit(Safepoint* safepoint) {
  uint32_t common_bits;
  BitVector* stack_indexes =
      CompareAndCreateXorPatch(zone_, current_entry_->stack_indexes,
                               previous_entry_->stack_indexes, &common_bits);
  // There's no differing suffix if all bits were identical.
  DCHECK_IMPLIES(stack_indexes == nullptr, common_bits == kMaxUInt32);

  entries_.emplace_back(current_entry_->pc, current_entry_->register_indexes,
                        common_bits, stack_indexes);
}

int SafepointTableBuilder::UpdateDeoptimizationInfo(int pc, int trampoline,
                                                    int start,
                                                    int deopt_index) {
  DCHECK_NE(SafepointEntry::kNoTrampolinePC, trampoline);
  DCHECK_NE(SafepointEntry::kNoDeoptIndex, deopt_index);
  has_deopt_data_ = true;
  auto it = entries_.begin() + start;
  DCHECK(std::any_of(it, entries_.end(),
                     [pc](auto& entry) { return entry.pc == pc; }));
  int index = start;
  while (it->pc != pc) ++it, ++index;
  it->trampoline = trampoline;
  it->deopt_index = deopt_index;
  return index;
}

void SafepointTableBuilder::Emit(Assembler* assembler, int stack_slot_count) {
#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64
  // We cannot emit a const pool within the safepoint table.
  Assembler::BlockConstPoolScope block_const_pool(assembler);
#endif

  // Make sure the safepoint table is properly aligned. Pad with nops.
  assembler->Align(InstructionStream::kMetadataAlignment);
  assembler->RecordComment(";;; Safepoint table.");
  const int start_offset = assembler->pc_offset();
  set_safepoint_table_offset(start_offset);

  // Emit the table header.
  static_assert(SafepointTable::kStackSlotsOffset == 0 * kIntSize);
  static_assert(SafepointTable::kConfigOffset == 1 * kIntSize);
  static_assert(SafepointTable::kHeaderSize == 2 * kIntSize);
  assembler->dd(stack_slot_count);
  int config_offset = assembler->pc_offset();
  assembler->dd(0u);  // For "config", will be patched later.

  // Emit entries, sorted by pc offsets.
  CompressedEntry implicit_start(kImplicitStartPC, 0, 0, nullptr);
  CompressedEntry& previous = implicit_start;
  // These may be different from the respective {previous.foo} field because
  // they are optional fields. Here we store the last value that was present.
  int last_deopt_index = kImplicitStartDeoptIndex;
  int last_trampoline = kImplicitStartTrampoline;
  for (const CompressedEntry& entry : entries_) {
    // Entries are ordered by PC.
    DCHECK_GT(entry.pc, previous.pc);

    // Skip identical (except for PC) entries. Note: this could end up
    // skipping all entries, if they're all identical to {implicit_start}!
    // The {SafepointTable} decoder can handle that.
    if (entry.deopt_index == previous.deopt_index &&
        entry.register_indexes == previous.register_indexes &&
        entry.stack_indexes_tail == nullptr) {
      DCHECK_EQ(entry.trampoline, previous.trampoline);
      continue;
    }

    emit_uleb(assembler, entry.pc - previous.pc);
    if (has_deopt_data_) {
      if (entry.trampoline == SafepointEntry::kNoTrampolinePC) {
        // An entry either has trampoline and deopt index, or none of the two.
        DCHECK(entry.deopt_index == SafepointEntry::kNoDeoptIndex);
        assembler->db(0);
      } else {
        DCHECK_NE(entry.deopt_index, SafepointEntry::kNoDeoptIndex);
        // Deopt indices and trampoline PCs are increasing.
        DCHECK_GT(entry.deopt_index, previous.deopt_index);
        DCHECK_GT(entry.trampoline, previous.trampoline);
        // Trampoline PCs are larger than regular PCs.
        DCHECK_GT(entry.trampoline, entries_.back().pc);

        emit_uleb(assembler, entry.deopt_index - last_deopt_index);
        last_deopt_index = entry.deopt_index;
        emit_uleb(assembler, entry.trampoline - last_trampoline);
        last_trampoline = entry.trampoline;
      }
    } else {
      DCHECK_EQ(SafepointEntry::kNoDeoptIndex, entry.deopt_index);
      DCHECK_EQ(SafepointEntry::kNoTrampolinePC, entry.trampoline);
    }
    if (has_tagged_registers_) {
      emit_uleb(assembler, entry.register_indexes);
    }

    // Emit bitmaps of tagged stack slots. Note the slot list is reversed in the
    // encoding.
    EncodeSafepointEntry(stack_slot_count, entry.common_bits,
                         entry.stack_indexes_tail, assembler);
    previous = entry;
  }
  uint32_t byte_length = assembler->pc_offset() - start_offset;
  uint32_t config =
      SafepointTable::ByteLengthField::encode(byte_length) |
      SafepointTable::HasTaggedRegistersField::encode(has_tagged_registers_) |
      SafepointTable::HasDeoptDataField::encode(has_deopt_data_);
  WriteUnalignedValue(
      reinterpret_cast<Address>(assembler->buffer_start() + config_offset),
      config);
}

// Contract: Xoring the result of this function onto {a}, starting at
// {common_prefix_bits}, yields a vector with the same bits set as {b}.
// We consider the vectors to be sets, i.e. they have no upper bound, instead
// they are assumed to continue with 0-bits to infinity.
// This implies that both {a} and {b} may be empty ({length() == 0}), which
// is treated the same as an arbitrary-length vector full of 0-bits.
// Example:
//   other:  110010100 (UsedLength() == 7, length() doesn't matter)
//   this:   110000000 (UsedLength() == 2, length() doesn't matter)
//   result:     101   (length() == 3), common_prefix_bits=4
// For identical sets we return {nullptr} and, as an approximation of
// "infinity", {common_prefix_bits} = kMaxUInt32.
// For non-nullptr results, {common_prefix_bits} is a meaningful value, and
// the {length()} of the result indicates the range containing all differing
// bits, starting at that position.
BitVector* CompareAndCreateXorPatch(Zone* zone, const GrowableBitVector& v1,
                                    const GrowableBitVector& v2,
                                    uint32_t* common_prefix_bits) {
  // This function is prepared to work on overallocated GrowableBitVectors,
  // so rather than relying on {length()} we compute the actual length (i.e.
  // position of the last bit).
  const BitVector& a = v1.bits_;
  const BitVector& b = v2.bits_;
  constexpr int kDataBits = BitVector::kDataBits;
  constexpr int kDataBitShift = BitVector::kDataBitShift;

  int a_length = a.UsedLength();
  int b_length = b.UsedLength();
  int a_word_length = (a_length + kDataBits - 1) >> kDataBitShift;
  int b_word_length = (b_length + kDataBits - 1) >> kDataBitShift;
  int max_common_bits = std::min(a_length, b_length);
  int max_common_words = (max_common_bits + kDataBits - 1) >> kDataBitShift;
  int different_word = 0;
  int different_bit = 0;
  while (different_word < max_common_words &&
         a.data_begin_[different_word] == b.data_begin_[different_word]) {
    ++different_word;
  }
  // We may have found a difference already. Otherwise, if we reached the
  // end of one of the vectors, see if the other has any non-zero bits.
  if (different_word == max_common_words) {
    while (different_word < b_word_length &&
           b.data_begin_[different_word] == 0) {
      ++different_word;
    }
    while (different_word < a_word_length &&
           a.data_begin_[different_word] == 0) {
      ++different_word;
    }
  }
  // If the overlapping part was identical and only zeros followed in the
  // longer vector, they count as identical.
  if (different_word >= b_word_length && different_word >= a_word_length) {
    *common_prefix_bits = kMaxUInt32;
    return nullptr;
  }
  // Otherwise we must have found a word that differs.
  uintptr_t a_word =
      different_word < a_word_length ? a.data_begin_[different_word] : 0;
  uintptr_t b_word =
      different_word < b_word_length ? b.data_begin_[different_word] : 0;
  DCHECK_NE(a_word, b_word);
  uintptr_t diff = a_word ^ b_word;
  different_bit = base::bits::CountTrailingZeros(diff);
  *common_prefix_bits = different_word * kDataBits + different_bit;

  // Find the last difference.
  // If the vectors have different lengths, then the end of the longer one
  // is the last difference. Otherwise, skip any identical tails.
  int result_end;
  if (a_length != b_length) {
    result_end = std::max(a_length, b_length);
  } else {
    int highest_bit = a_length - 1;
    int result_end_word = highest_bit >> kDataBitShift;
    while (a.data_begin_[result_end_word] == b.data_begin_[result_end_word]) {
      result_end_word--;
    }
    a_word = a.data_begin_[result_end_word];
    b_word = b.data_begin_[result_end_word];
    DCHECK_NE(a_word, b_word);
    diff = a_word ^ b_word;
    int identical_tail = base::bits::CountLeadingZeros(diff);
    result_end = (result_end_word + 1) * kDataBits - identical_tail;
    DCHECK_GE(result_end, *common_prefix_bits);
  }

  // Allocate and populate the result.
  int suffix_length = result_end - *common_prefix_bits;
  DCHECK_NE(suffix_length, 0);
  BitVector* result = zone->New<BitVector>(suffix_length, zone);
  int result_words = result->data_length();
  if (different_bit == 0) {
    for (int i = 0; i < result_words; i++) {
      int read_i = different_word + i;
      a_word = read_i < a_word_length ? a.data_begin_[read_i] : 0;
      b_word = read_i < b_word_length ? b.data_begin_[read_i] : 0;
      result->data_begin_[i] = a_word ^ b_word;
    }
  } else {
    a_word = different_word < a_word_length ? a.data_begin_[different_word] : 0;
    b_word = different_word < b_word_length ? b.data_begin_[different_word] : 0;
    uintptr_t carry = (a_word ^ b_word) >> different_bit;
    int left_shift = kDataBits - different_bit;
    for (int i = 0; i < result_words; i++) {
      int read_i = different_word + i + 1;
      a_word = read_i < a_word_length ? a.data_begin_[read_i] : 0;
      b_word = read_i < b_word_length ? b.data_begin_[read_i] : 0;
      uintptr_t word = a_word ^ b_word;
      result->data_begin_[i] = carry | (word << left_shift);
      carry = word >> different_bit;
    }
  }
#if DEBUG
  // The patch always begins and ends with a bit that needs to be flipped.
  DCHECK(result->Contains(0));
  DCHECK(result->Contains(suffix_length - 1));
  // Any trailing bits in the backing store are unset.
  if (suffix_length != result_words * kDataBits) {
    uintptr_t last_word = result->data_begin_[result_words - 1];
    DCHECK_EQ(0, last_word >> (suffix_length % kDataBits));
  }
#endif  // DEBUG
  return result;
}

}  // namespace internal
}  // namespace v8
