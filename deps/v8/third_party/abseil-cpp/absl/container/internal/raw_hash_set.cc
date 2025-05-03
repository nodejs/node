// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/container/internal/raw_hash_set.h"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/dynamic_annotations.h"
#include "absl/base/internal/endian.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/optimization.h"
#include "absl/container/internal/container_memory.h"
#include "absl/container/internal/hashtable_control_bytes.h"
#include "absl/container/internal/hashtablez_sampler.h"
#include "absl/functional/function_ref.h"
#include "absl/hash/hash.h"
#include "absl/numeric/bits.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {

// Represents a control byte corresponding to a full slot with arbitrary hash.
constexpr ctrl_t ZeroCtrlT() { return static_cast<ctrl_t>(0); }

// We have space for `growth_info` before a single block of control bytes. A
// single block of empty control bytes for tables without any slots allocated.
// This enables removing a branch in the hot path of find(). In order to ensure
// that the control bytes are aligned to 16, we have 16 bytes before the control
// bytes even though growth_info only needs 8.
alignas(16) ABSL_CONST_INIT ABSL_DLL const ctrl_t kEmptyGroup[32] = {
    ZeroCtrlT(),       ZeroCtrlT(),    ZeroCtrlT(),    ZeroCtrlT(),
    ZeroCtrlT(),       ZeroCtrlT(),    ZeroCtrlT(),    ZeroCtrlT(),
    ZeroCtrlT(),       ZeroCtrlT(),    ZeroCtrlT(),    ZeroCtrlT(),
    ZeroCtrlT(),       ZeroCtrlT(),    ZeroCtrlT(),    ZeroCtrlT(),
    ctrl_t::kSentinel, ctrl_t::kEmpty, ctrl_t::kEmpty, ctrl_t::kEmpty,
    ctrl_t::kEmpty,    ctrl_t::kEmpty, ctrl_t::kEmpty, ctrl_t::kEmpty,
    ctrl_t::kEmpty,    ctrl_t::kEmpty, ctrl_t::kEmpty, ctrl_t::kEmpty,
    ctrl_t::kEmpty,    ctrl_t::kEmpty, ctrl_t::kEmpty, ctrl_t::kEmpty};

// We need one full byte followed by a sentinel byte for iterator::operator++ to
// work. We have a full group after kSentinel to be safe (in case operator++ is
// changed to read a full group).
ABSL_CONST_INIT ABSL_DLL const ctrl_t kSooControl[17] = {
    ZeroCtrlT(),    ctrl_t::kSentinel, ZeroCtrlT(),    ctrl_t::kEmpty,
    ctrl_t::kEmpty, ctrl_t::kEmpty,    ctrl_t::kEmpty, ctrl_t::kEmpty,
    ctrl_t::kEmpty, ctrl_t::kEmpty,    ctrl_t::kEmpty, ctrl_t::kEmpty,
    ctrl_t::kEmpty, ctrl_t::kEmpty,    ctrl_t::kEmpty, ctrl_t::kEmpty,
    ctrl_t::kEmpty};
static_assert(NumControlBytes(SooCapacity()) <= 17,
              "kSooControl capacity too small");

namespace {

[[noreturn]] ABSL_ATTRIBUTE_NOINLINE void HashTableSizeOverflow() {
  ABSL_RAW_LOG(FATAL, "Hash table size overflow");
}

void ValidateMaxSize(size_t size, size_t slot_size) {
  if (IsAboveValidSize(size, slot_size)) {
    HashTableSizeOverflow();
  }
}

// Returns "random" seed.
inline size_t RandomSeed() {
#ifdef ABSL_HAVE_THREAD_LOCAL
  static thread_local size_t counter = 0;
  size_t value = ++counter;
#else   // ABSL_HAVE_THREAD_LOCAL
  static std::atomic<size_t> counter(0);
  size_t value = counter.fetch_add(1, std::memory_order_relaxed);
#endif  // ABSL_HAVE_THREAD_LOCAL
  return value ^ static_cast<size_t>(reinterpret_cast<uintptr_t>(&counter));
}

bool ShouldRehashForBugDetection(PerTableSeed seed, size_t capacity) {
  // Note: we can't use the abseil-random library because abseil-random
  // depends on swisstable. We want to return true with probability
  // `min(1, RehashProbabilityConstant() / capacity())`. In order to do this,
  // we probe based on a random hash and see if the offset is less than
  // RehashProbabilityConstant().
  return probe(seed, capacity, absl::HashOf(RandomSeed())).offset() <
         RehashProbabilityConstant();
}

// Find a non-deterministic hash for single group table.
// Last two bits are used to find a position for a newly inserted element after
// resize.
// This function is mixing all bits of hash and seed to maximize entropy.
size_t SingleGroupTableH1(size_t hash, PerTableSeed seed) {
  return static_cast<size_t>(absl::popcount(hash ^ seed.seed()));
}

// Returns the address of the slot `i` iterations after `slot` assuming each
// slot has the specified size.
inline void* NextSlot(void* slot, size_t slot_size, size_t i = 1) {
  return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(slot) +
                                 slot_size * i);
}

// Returns the address of the slot just before `slot` assuming each slot has the
// specified size.
inline void* PrevSlot(void* slot, size_t slot_size) {
  return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(slot) - slot_size);
}

}  // namespace

GenerationType* EmptyGeneration() {
  if (SwisstableGenerationsEnabled()) {
    constexpr size_t kNumEmptyGenerations = 1024;
    static constexpr GenerationType kEmptyGenerations[kNumEmptyGenerations]{};
    return const_cast<GenerationType*>(
        &kEmptyGenerations[RandomSeed() % kNumEmptyGenerations]);
  }
  return nullptr;
}

bool CommonFieldsGenerationInfoEnabled::
    should_rehash_for_bug_detection_on_insert(PerTableSeed seed,
                                              size_t capacity) const {
  if (reserved_growth_ == kReservedGrowthJustRanOut) return true;
  if (reserved_growth_ > 0) return false;
  return ShouldRehashForBugDetection(seed, capacity);
}

bool CommonFieldsGenerationInfoEnabled::should_rehash_for_bug_detection_on_move(
    PerTableSeed seed, size_t capacity) const {
  return ShouldRehashForBugDetection(seed, capacity);
}

bool ShouldInsertBackwardsForDebug(size_t capacity, size_t hash,
                                   PerTableSeed seed) {
  // To avoid problems with weak hashes and single bit tests, we use % 13.
  // TODO(kfm,sbenza): revisit after we do unconditional mixing
  return !is_small(capacity) && (H1(hash, seed) ^ RandomSeed()) % 13 > 6;
}

void IterateOverFullSlots(const CommonFields& c, size_t slot_size,
                          absl::FunctionRef<void(const ctrl_t*, void*)> cb) {
  const size_t cap = c.capacity();
  const ctrl_t* ctrl = c.control();
  void* slot = c.slot_array();
  if (is_small(cap)) {
    // Mirrored/cloned control bytes in small table are also located in the
    // first group (starting from position 0). We are taking group from position
    // `capacity` in order to avoid duplicates.

    // Small tables capacity fits into portable group, where
    // GroupPortableImpl::MaskFull is more efficient for the
    // capacity <= GroupPortableImpl::kWidth.
    assert(cap <= GroupPortableImpl::kWidth &&
           "unexpectedly large small capacity");
    static_assert(Group::kWidth >= GroupPortableImpl::kWidth,
                  "unexpected group width");
    // Group starts from kSentinel slot, so indices in the mask will
    // be increased by 1.
    const auto mask = GroupPortableImpl(ctrl + cap).MaskFull();
    --ctrl;
    slot = PrevSlot(slot, slot_size);
    for (uint32_t i : mask) {
      cb(ctrl + i, SlotAddress(slot, i, slot_size));
    }
    return;
  }
  size_t remaining = c.size();
  ABSL_ATTRIBUTE_UNUSED const size_t original_size_for_assert = remaining;
  while (remaining != 0) {
    for (uint32_t i : GroupFullEmptyOrDeleted(ctrl).MaskFull()) {
      assert(IsFull(ctrl[i]) && "hash table was modified unexpectedly");
      cb(ctrl + i, SlotAddress(slot, i, slot_size));
      --remaining;
    }
    ctrl += Group::kWidth;
    slot = NextSlot(slot, slot_size, Group::kWidth);
    assert((remaining == 0 || *(ctrl - 1) != ctrl_t::kSentinel) &&
           "hash table was modified unexpectedly");
  }
  // NOTE: erasure of the current element is allowed in callback for
  // absl::erase_if specialization. So we use `>=`.
  assert(original_size_for_assert >= c.size() &&
         "hash table was modified unexpectedly");
}

size_t PrepareInsertAfterSoo(size_t hash, size_t slot_size,
                             CommonFields& common) {
  assert(common.capacity() == NextCapacity(SooCapacity()));
  // After resize from capacity 1 to 3, we always have exactly the slot with
  // index 1 occupied, so we need to insert either at index 0 or index 2.
  static_assert(SooSlotIndex() == 1, "");
  PrepareInsertCommon(common);
  const size_t offset = SingleGroupTableH1(hash, common.seed()) & 2;
  common.growth_info().OverwriteEmptyAsFull();
  SetCtrlInSingleGroupTable(common, offset, H2(hash), slot_size);
  common.infoz().RecordInsert(hash, /*distance_from_desired=*/0);
  return offset;
}

void ConvertDeletedToEmptyAndFullToDeleted(ctrl_t* ctrl, size_t capacity) {
  assert(ctrl[capacity] == ctrl_t::kSentinel);
  assert(IsValidCapacity(capacity));
  for (ctrl_t* pos = ctrl; pos < ctrl + capacity; pos += Group::kWidth) {
    Group{pos}.ConvertSpecialToEmptyAndFullToDeleted(pos);
  }
  // Copy the cloned ctrl bytes.
  std::memcpy(ctrl + capacity + 1, ctrl, NumClonedBytes());
  ctrl[capacity] = ctrl_t::kSentinel;
}
// Extern template instantiation for inline function.
template FindInfo find_first_non_full(const CommonFields&, size_t);

FindInfo find_first_non_full_outofline(const CommonFields& common,
                                       size_t hash) {
  return find_first_non_full(common, hash);
}

namespace {

void ResetGrowthLeft(CommonFields& common) {
  common.growth_info().InitGrowthLeftNoDeleted(
      CapacityToGrowth(common.capacity()) - common.size());
}

// Finds guaranteed to exists empty slot from the given position.
// NOTE: this function is almost never triggered inside of the
// DropDeletesWithoutResize, so we keep it simple.
// The table is rather sparse, so empty slot will be found very quickly.
size_t FindEmptySlot(size_t start, size_t end, const ctrl_t* ctrl) {
  for (size_t i = start; i < end; ++i) {
    if (IsEmpty(ctrl[i])) {
      return i;
    }
  }
  ABSL_UNREACHABLE();
}

// Finds guaranteed to exist full slot starting from the given position.
// NOTE: this function is only triggered for rehash(0), when we need to
// go back to SOO state, so we keep it simple.
size_t FindFirstFullSlot(size_t start, size_t end, const ctrl_t* ctrl) {
  for (size_t i = start; i < end; ++i) {
    if (IsFull(ctrl[i])) {
      return i;
    }
  }
  ABSL_UNREACHABLE();
}

size_t DropDeletesWithoutResizeAndPrepareInsert(CommonFields& common,
                                                const PolicyFunctions& policy,
                                                size_t new_hash) {
  void* set = &common;
  void* slot_array = common.slot_array();
  const size_t capacity = common.capacity();
  assert(IsValidCapacity(capacity));
  assert(!is_small(capacity));
  // Algorithm:
  // - mark all DELETED slots as EMPTY
  // - mark all FULL slots as DELETED
  // - for each slot marked as DELETED
  //     hash = Hash(element)
  //     target = find_first_non_full(hash)
  //     if target is in the same group
  //       mark slot as FULL
  //     else if target is EMPTY
  //       transfer element to target
  //       mark slot as EMPTY
  //       mark target as FULL
  //     else if target is DELETED
  //       swap current element with target element
  //       mark target as FULL
  //       repeat procedure for current slot with moved from element (target)
  ctrl_t* ctrl = common.control();
  ConvertDeletedToEmptyAndFullToDeleted(ctrl, capacity);
  const void* hash_fn = policy.hash_fn(common);
  auto hasher = policy.hash_slot;
  auto transfer = policy.transfer;
  const size_t slot_size = policy.slot_size;

  size_t total_probe_length = 0;
  void* slot_ptr = SlotAddress(slot_array, 0, slot_size);

  // The index of an empty slot that can be used as temporary memory for
  // the swap operation.
  constexpr size_t kUnknownId = ~size_t{};
  size_t tmp_space_id = kUnknownId;

  for (size_t i = 0; i != capacity;
       ++i, slot_ptr = NextSlot(slot_ptr, slot_size)) {
    assert(slot_ptr == SlotAddress(slot_array, i, slot_size));
    if (IsEmpty(ctrl[i])) {
      tmp_space_id = i;
      continue;
    }
    if (!IsDeleted(ctrl[i])) continue;
    const size_t hash = (*hasher)(hash_fn, slot_ptr);
    const FindInfo target = find_first_non_full(common, hash);
    const size_t new_i = target.offset;
    total_probe_length += target.probe_length;

    // Verify if the old and new i fall within the same group wrt the hash.
    // If they do, we don't need to move the object as it falls already in the
    // best probe we can.
    const size_t probe_offset = probe(common, hash).offset();
    const h2_t h2 = H2(hash);
    const auto probe_index = [probe_offset, capacity](size_t pos) {
      return ((pos - probe_offset) & capacity) / Group::kWidth;
    };

    // Element doesn't move.
    if (ABSL_PREDICT_TRUE(probe_index(new_i) == probe_index(i))) {
      SetCtrlInLargeTable(common, i, h2, slot_size);
      continue;
    }

    void* new_slot_ptr = SlotAddress(slot_array, new_i, slot_size);
    if (IsEmpty(ctrl[new_i])) {
      // Transfer element to the empty spot.
      // SetCtrl poisons/unpoisons the slots so we have to call it at the
      // right time.
      SetCtrlInLargeTable(common, new_i, h2, slot_size);
      (*transfer)(set, new_slot_ptr, slot_ptr, 1);
      SetCtrlInLargeTable(common, i, ctrl_t::kEmpty, slot_size);
      // Initialize or change empty space id.
      tmp_space_id = i;
    } else {
      assert(IsDeleted(ctrl[new_i]));
      SetCtrlInLargeTable(common, new_i, h2, slot_size);
      // Until we are done rehashing, DELETED marks previously FULL slots.

      if (tmp_space_id == kUnknownId) {
        tmp_space_id = FindEmptySlot(i + 1, capacity, ctrl);
      }
      void* tmp_space = SlotAddress(slot_array, tmp_space_id, slot_size);
      SanitizerUnpoisonMemoryRegion(tmp_space, slot_size);

      // Swap i and new_i elements.
      (*transfer)(set, tmp_space, new_slot_ptr, 1);
      (*transfer)(set, new_slot_ptr, slot_ptr, 1);
      (*transfer)(set, slot_ptr, tmp_space, 1);

      SanitizerPoisonMemoryRegion(tmp_space, slot_size);

      // repeat the processing of the ith slot
      --i;
      slot_ptr = PrevSlot(slot_ptr, slot_size);
    }
  }
  // Prepare insert for the new element.
  PrepareInsertCommon(common);
  ResetGrowthLeft(common);
  FindInfo find_info = find_first_non_full(common, new_hash);
  SetCtrlInLargeTable(common, find_info.offset, H2(new_hash), policy.slot_size);
  common.infoz().RecordInsert(new_hash, find_info.probe_length);
  common.infoz().RecordRehash(total_probe_length);
  return find_info.offset;
}

static bool WasNeverFull(CommonFields& c, size_t index) {
  if (is_single_group(c.capacity())) {
    return true;
  }
  const size_t index_before = (index - Group::kWidth) & c.capacity();
  const auto empty_after = Group(c.control() + index).MaskEmpty();
  const auto empty_before = Group(c.control() + index_before).MaskEmpty();

  // We count how many consecutive non empties we have to the right and to the
  // left of `it`. If the sum is >= kWidth then there is at least one probe
  // window that might have seen a full group.
  return empty_before && empty_after &&
         static_cast<size_t>(empty_after.TrailingZeros()) +
                 empty_before.LeadingZeros() <
             Group::kWidth;
}

// Initializes control bytes for single element table.
// Capacity of the table must be 1.
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void InitializeSingleElementControlBytes(
    uint64_t h2, ctrl_t* new_ctrl) {
  static constexpr uint64_t kEmptyXorSentinel =
      static_cast<uint8_t>(ctrl_t::kEmpty) ^
      static_cast<uint8_t>(ctrl_t::kSentinel);
  static constexpr uint64_t kEmpty64 = static_cast<uint8_t>(ctrl_t::kEmpty);
  // The first 8 bytes, where present slot positions are replaced with 0.
  static constexpr uint64_t kFirstCtrlBytesWithZeroes =
      k8EmptyBytes ^ kEmpty64 ^ (kEmptyXorSentinel << 8) ^ (kEmpty64 << 16);

  // Fill the original 0th and mirrored 2nd bytes with the hash.
  // Result will look like:
  // HSHEEEEE
  // Where H = h2, E = kEmpty, S = kSentinel.
  const uint64_t first_ctrl_bytes =
      (h2 | kFirstCtrlBytesWithZeroes) | (h2 << 16);
  // Fill last bytes with kEmpty.
  std::memset(new_ctrl + 1, static_cast<int8_t>(ctrl_t::kEmpty), Group::kWidth);
  // Overwrite the first 3 bytes with HSH. Other bytes will not be changed.
  absl::little_endian::Store64(new_ctrl, first_ctrl_bytes);
}

// Initializes control bytes after SOO to the next capacity.
// The table must be non-empty SOO.
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void
InitializeThreeElementsControlBytesAfterSoo(size_t hash, ctrl_t* new_ctrl) {
  static constexpr size_t kNewCapacity = NextCapacity(SooCapacity());
  static_assert(kNewCapacity == 3);
  static_assert(is_single_group(kNewCapacity));
  static_assert(SooSlotIndex() == 1);

  static constexpr uint64_t kEmptyXorSentinel =
      static_cast<uint8_t>(ctrl_t::kEmpty) ^
      static_cast<uint8_t>(ctrl_t::kSentinel);
  static constexpr uint64_t kEmpty64 = static_cast<uint8_t>(ctrl_t::kEmpty);
  static constexpr size_t kMirroredSooSlotIndex =
      SooSlotIndex() + kNewCapacity + 1;
  // The first 8 bytes, where present slot positions are replaced with 0.
  static constexpr uint64_t kFirstCtrlBytesWithZeroes =
      k8EmptyBytes ^ (kEmpty64 << (8 * SooSlotIndex())) ^
      (kEmptyXorSentinel << (8 * kNewCapacity)) ^
      (kEmpty64 << (8 * kMirroredSooSlotIndex));

  const uint64_t h2 = static_cast<uint64_t>(H2(hash));
  // Fill the original 0th and mirrored 2nd bytes with the hash.
  // Result will look like:
  // EHESEHEE
  // Where H = h2, E = kEmpty, S = kSentinel.
  const uint64_t first_ctrl_bytes =
      ((h2 << (8 * SooSlotIndex())) | kFirstCtrlBytesWithZeroes) |
      (h2 << (8 * kMirroredSooSlotIndex));

  // Fill last bytes with kEmpty.
  std::memset(new_ctrl + kNewCapacity, static_cast<int8_t>(ctrl_t::kEmpty),
              Group::kWidth);
  // Overwrite the first 8 bytes with first_ctrl_bytes.
  absl::little_endian::Store64(new_ctrl, first_ctrl_bytes);

  // Example for group size 16:
  // new_ctrl after 1st memset =      ???EEEEEEEEEEEEEEEE
  // new_ctrl after 2nd store  =      EHESEHEEEEEEEEEEEEE

  // Example for group size 8:
  // new_ctrl after 1st memset =      ???EEEEEEEE
  // new_ctrl after 2nd store  =      EHESEHEEEEE
}

}  // namespace

void EraseMetaOnly(CommonFields& c, size_t index, size_t slot_size) {
  assert(IsFull(c.control()[index]) && "erasing a dangling iterator");
  c.decrement_size();
  c.infoz().RecordErase();

  if (WasNeverFull(c, index)) {
    SetCtrl(c, index, ctrl_t::kEmpty, slot_size);
    c.growth_info().OverwriteFullAsEmpty();
    return;
  }

  c.growth_info().OverwriteFullAsDeleted();
  SetCtrlInLargeTable(c, index, ctrl_t::kDeleted, slot_size);
}

void ClearBackingArray(CommonFields& c, const PolicyFunctions& policy,
                       void* alloc, bool reuse, bool soo_enabled) {
  if (reuse) {
    c.set_size_to_zero();
    assert(!soo_enabled || c.capacity() > SooCapacity());
    ResetCtrl(c, policy.slot_size);
    ResetGrowthLeft(c);
    c.infoz().RecordStorageChanged(0, c.capacity());
  } else {
    // We need to record infoz before calling dealloc, which will unregister
    // infoz.
    c.infoz().RecordClearedReservation();
    c.infoz().RecordStorageChanged(0, soo_enabled ? SooCapacity() : 0);
    c.infoz().Unregister();
    (*policy.dealloc)(alloc, c.capacity(), c.control(), policy.slot_size,
                      policy.slot_align, c.has_infoz());
    c = soo_enabled ? CommonFields{soo_tag_t{}} : CommonFields{non_soo_tag_t{}};
  }
}

namespace {

// Poisons empty slots. It is useful when slots are transferred via memcpy.
// PRECONDITIONs: common.control() is fully initialized.
void PoisonEmptySlots(CommonFields& c, size_t slot_size) {
  for (size_t i = 0; i < c.capacity(); ++i) {
    if (!IsFull(c.control()[i])) {
      SanitizerPoisonMemoryRegion(SlotAddress(c.slot_array(), i, slot_size),
                                  slot_size);
    }
  }
}

enum class ResizeNonSooMode {
  kGuaranteedEmpty,
  kGuaranteedAllocated,
};

template <ResizeNonSooMode kMode>
void ResizeNonSooImpl(CommonFields& common, const PolicyFunctions& policy,
                      size_t new_capacity, HashtablezInfoHandle infoz) {
  assert(IsValidCapacity(new_capacity));
  assert(new_capacity > policy.soo_capacity);

  const size_t old_capacity = common.capacity();
  [[maybe_unused]] ctrl_t* old_ctrl = common.control();
  [[maybe_unused]] void* old_slots = common.slot_array();

  const size_t slot_size = policy.slot_size;
  const size_t slot_align = policy.slot_align;
  const bool has_infoz = infoz.IsSampled();

  common.set_capacity(new_capacity);
  RawHashSetLayout layout(new_capacity, slot_size, slot_align, has_infoz);
  void* alloc = policy.get_char_alloc(common);
  char* mem = static_cast<char*>(policy.alloc(alloc, layout.alloc_size()));
  const GenerationType old_generation = common.generation();
  common.set_generation_ptr(
      reinterpret_cast<GenerationType*>(mem + layout.generation_offset()));
  common.set_generation(NextGeneration(old_generation));

  common.set_control</*kGenerateSeed=*/true>(
      reinterpret_cast<ctrl_t*>(mem + layout.control_offset()));
  common.set_slots(mem + layout.slot_offset());

  size_t total_probe_length = 0;
  ResetCtrl(common, slot_size);
  assert(kMode != ResizeNonSooMode::kGuaranteedEmpty ||
         old_capacity == policy.soo_capacity);
  assert(kMode != ResizeNonSooMode::kGuaranteedAllocated || old_capacity > 0);
  if constexpr (kMode == ResizeNonSooMode::kGuaranteedAllocated) {
    total_probe_length = policy.find_new_positions_and_transfer_slots(
        common, old_ctrl, old_slots, old_capacity);
    (*policy.dealloc)(alloc, old_capacity, old_ctrl, slot_size, slot_align,
                      has_infoz);
  }

  ResetGrowthLeft(common);
  if (has_infoz) {
    common.set_has_infoz();
    infoz.RecordStorageChanged(common.size(), new_capacity);
    infoz.RecordRehash(total_probe_length);
    common.set_infoz(infoz);
  }
}

void ResizeEmptyNonAllocatedTableImpl(CommonFields& common,
                                      const PolicyFunctions& policy,
                                      size_t new_capacity, bool force_infoz) {
  assert(IsValidCapacity(new_capacity));
  assert(new_capacity > policy.soo_capacity);
  assert(!force_infoz || policy.soo_capacity > 0);
  assert(common.capacity() <= policy.soo_capacity);
  assert(common.empty());
  const size_t slot_size = policy.slot_size;
  HashtablezInfoHandle infoz;
  const bool should_sample =
      policy.is_hashtablez_eligible && (force_infoz || ShouldSampleNextTable());
  if (ABSL_PREDICT_FALSE(should_sample)) {
    infoz = ForcedTrySample(slot_size, policy.key_size, policy.value_size,
                            policy.soo_capacity);
  }
  ResizeNonSooImpl<ResizeNonSooMode::kGuaranteedEmpty>(common, policy,
                                                       new_capacity, infoz);
}

// If the table was SOO, initializes new control bytes and transfers slot.
// After transferring the slot, sets control and slots in CommonFields.
// It is rare to resize an SOO table with one element to a large size.
// Requires: `c` contains SOO data.
void InsertOldSooSlotAndInitializeControlBytes(CommonFields& c,
                                               const PolicyFunctions& policy,
                                               size_t hash, ctrl_t* new_ctrl,
                                               void* new_slots) {
  assert(c.size() == policy.soo_capacity);
  assert(policy.soo_capacity == SooCapacity());
  size_t new_capacity = c.capacity();

  c.generate_new_seed();
  size_t offset = probe(c.seed(), new_capacity, hash).offset();
  offset = offset == new_capacity ? 0 : offset;
  SanitizerPoisonMemoryRegion(new_slots, policy.slot_size * new_capacity);
  void* target_slot = SlotAddress(new_slots, offset, policy.slot_size);
  SanitizerUnpoisonMemoryRegion(target_slot, policy.slot_size);
  policy.transfer(&c, target_slot, c.soo_data(), 1);
  c.set_control</*kGenerateSeed=*/false>(new_ctrl);
  c.set_slots(new_slots);
  ResetCtrl(c, policy.slot_size);
  SetCtrl(c, offset, H2(hash), policy.slot_size);
}

enum class ResizeFullSooTableSamplingMode {
  kNoSampling,
  // Force sampling. If the table was still not sampled, do not resize.
  kForceSampleNoResizeIfUnsampled,
};

void ResizeFullSooTable(CommonFields& common, const PolicyFunctions& policy,
                        size_t new_capacity,
                        ResizeFullSooTableSamplingMode sampling_mode) {
  assert(common.capacity() == policy.soo_capacity);
  assert(common.size() == policy.soo_capacity);
  assert(policy.soo_capacity == SooCapacity());
  const size_t slot_size = policy.slot_size;
  const size_t slot_align = policy.slot_align;

  HashtablezInfoHandle infoz;
  if (sampling_mode ==
      ResizeFullSooTableSamplingMode::kForceSampleNoResizeIfUnsampled) {
    if (ABSL_PREDICT_FALSE(policy.is_hashtablez_eligible)) {
      infoz = ForcedTrySample(slot_size, policy.key_size, policy.value_size,
                              policy.soo_capacity);
    }

    if (!infoz.IsSampled()) {
      return;
    }
  }

  const bool has_infoz = infoz.IsSampled();

  common.set_capacity(new_capacity);

  RawHashSetLayout layout(new_capacity, slot_size, slot_align, has_infoz);
  void* alloc = policy.get_char_alloc(common);
  char* mem = static_cast<char*>(policy.alloc(alloc, layout.alloc_size()));
  const GenerationType old_generation = common.generation();
  common.set_generation_ptr(
      reinterpret_cast<GenerationType*>(mem + layout.generation_offset()));
  common.set_generation(NextGeneration(old_generation));

  // We do not set control and slots in CommonFields yet to avoid overriding
  // SOO data.
  ctrl_t* new_ctrl = reinterpret_cast<ctrl_t*>(mem + layout.control_offset());
  void* new_slots = mem + layout.slot_offset();

  const size_t soo_slot_hash =
      policy.hash_slot(policy.hash_fn(common), common.soo_data());

  InsertOldSooSlotAndInitializeControlBytes(common, policy, soo_slot_hash,
                                            new_ctrl, new_slots);
  ResetGrowthLeft(common);
  if (has_infoz) {
    common.set_has_infoz();
    common.set_infoz(infoz);
  }
}

void GrowIntoSingleGroupShuffleControlBytes(ctrl_t* __restrict old_ctrl,
                                            size_t old_capacity,
                                            ctrl_t* __restrict new_ctrl,
                                            size_t new_capacity) {
  assert(is_single_group(new_capacity));
  constexpr size_t kHalfWidth = Group::kWidth / 2;
  ABSL_ASSUME(old_capacity < kHalfWidth);
  ABSL_ASSUME(old_capacity > 0);
  static_assert(Group::kWidth == 8 || Group::kWidth == 16,
                "Group size is not supported.");

  // NOTE: operations are done with compile time known size = 8.
  // Compiler optimizes that into single ASM operation.

  // Load the bytes from old_capacity. This contains
  // - the sentinel byte
  // - all the old control bytes
  // - the rest is filled with kEmpty bytes
  // Example:
  // old_ctrl =     012S012EEEEEEEEE...
  // copied_bytes = S012EEEE
  uint64_t copied_bytes = absl::little_endian::Load64(old_ctrl + old_capacity);

  // We change the sentinel byte to kEmpty before storing to both the start of
  // the new_ctrl, and past the end of the new_ctrl later for the new cloned
  // bytes. Note that this is faster than setting the sentinel byte to kEmpty
  // after the copy directly in new_ctrl because we are limited on store
  // bandwidth.
  static constexpr uint64_t kEmptyXorSentinel =
      static_cast<uint8_t>(ctrl_t::kEmpty) ^
      static_cast<uint8_t>(ctrl_t::kSentinel);

  // Replace the first byte kSentinel with kEmpty.
  // Resulting bytes will be shifted by one byte old control blocks.
  // Example:
  // old_ctrl = 012S012EEEEEEEEE...
  // before =   S012EEEE
  // after  =   E012EEEE
  copied_bytes ^= kEmptyXorSentinel;

  if (Group::kWidth == 8) {
    // With group size 8, we can grow with two write operations.
    assert(old_capacity < 8 && "old_capacity is too large for group size 8");
    absl::little_endian::Store64(new_ctrl, copied_bytes);

    static constexpr uint64_t kSentinal64 =
        static_cast<uint8_t>(ctrl_t::kSentinel);

    // Prepend kSentinel byte to the beginning of copied_bytes.
    // We have maximum 3 non-empty bytes at the beginning of copied_bytes for
    // group size 8.
    // Example:
    // old_ctrl = 012S012EEEE
    // before =   E012EEEE
    // after  =   SE012EEE
    copied_bytes = (copied_bytes << 8) ^ kSentinal64;
    absl::little_endian::Store64(new_ctrl + new_capacity, copied_bytes);
    // Example for capacity 3:
    // old_ctrl = 012S012EEEE
    // After the first store:
    //           >!
    // new_ctrl = E012EEEE???????
    // After the second store:
    //                  >!
    // new_ctrl = E012EEESE012EEE
    return;
  }

  assert(Group::kWidth == 16);

  // Fill the second half of the main control bytes with kEmpty.
  // For small capacity that may write into mirrored control bytes.
  // It is fine as we will overwrite all the bytes later.
  std::memset(new_ctrl + kHalfWidth, static_cast<int8_t>(ctrl_t::kEmpty),
              kHalfWidth);
  // Fill the second half of the mirrored control bytes with kEmpty.
  std::memset(new_ctrl + new_capacity + kHalfWidth,
              static_cast<int8_t>(ctrl_t::kEmpty), kHalfWidth);
  // Copy the first half of the non-mirrored control bytes.
  absl::little_endian::Store64(new_ctrl, copied_bytes);
  new_ctrl[new_capacity] = ctrl_t::kSentinel;
  // Copy the first half of the mirrored control bytes.
  absl::little_endian::Store64(new_ctrl + new_capacity + 1, copied_bytes);

  // Example for growth capacity 1->3:
  // old_ctrl =                  0S0EEEEEEEEEEEEEE
  // new_ctrl at the end =       E0ESE0EEEEEEEEEEEEE
  //                                    >!
  // new_ctrl after 1st memset = ????????EEEEEEEE???
  //                                       >!
  // new_ctrl after 2nd memset = ????????EEEEEEEEEEE
  //                            >!
  // new_ctrl after 1st store =  E0EEEEEEEEEEEEEEEEE
  // new_ctrl after kSentinel =  E0ESEEEEEEEEEEEEEEE
  //                                >!
  // new_ctrl after 2nd store =  E0ESE0EEEEEEEEEEEEE

  // Example for growth capacity 3->7:
  // old_ctrl =                  012S012EEEEEEEEEEEE
  // new_ctrl at the end =       E012EEESE012EEEEEEEEEEE
  //                                    >!
  // new_ctrl after 1st memset = ????????EEEEEEEE???????
  //                                           >!
  // new_ctrl after 2nd memset = ????????EEEEEEEEEEEEEEE
  //                            >!
  // new_ctrl after 1st store =  E012EEEEEEEEEEEEEEEEEEE
  // new_ctrl after kSentinel =  E012EEESEEEEEEEEEEEEEEE
  //                                >!
  // new_ctrl after 2nd store =  E012EEESE012EEEEEEEEEEE

  // Example for growth capacity 7->15:
  // old_ctrl =                  0123456S0123456EEEEEEEE
  // new_ctrl at the end =       E0123456EEEEEEESE0123456EEEEEEE
  //                                    >!
  // new_ctrl after 1st memset = ????????EEEEEEEE???????????????
  //                                                   >!
  // new_ctrl after 2nd memset = ????????EEEEEEEE???????EEEEEEEE
  //                            >!
  // new_ctrl after 1st store =  E0123456EEEEEEEE???????EEEEEEEE
  // new_ctrl after kSentinel =  E0123456EEEEEEES???????EEEEEEEE
  //                                            >!
  // new_ctrl after 2nd store =  E0123456EEEEEEESE0123456EEEEEEE
}

size_t GrowToNextCapacityAndPrepareInsert(CommonFields& common,
                                          const PolicyFunctions& policy,
                                          size_t new_hash) {
  assert(common.growth_left() == 0);
  const size_t old_capacity = common.capacity();
  assert(old_capacity == 0 || old_capacity > policy.soo_capacity);

  const size_t new_capacity = NextCapacity(old_capacity);
  assert(IsValidCapacity(new_capacity));
  assert(new_capacity > policy.soo_capacity);

  ctrl_t* old_ctrl = common.control();
  void* old_slots = common.slot_array();

  common.set_capacity(new_capacity);
  const size_t slot_size = policy.slot_size;
  const size_t slot_align = policy.slot_align;
  HashtablezInfoHandle infoz;
  if (old_capacity > 0) {
    infoz = common.infoz();
  } else {
    const bool should_sample =
        policy.is_hashtablez_eligible && ShouldSampleNextTable();
    if (ABSL_PREDICT_FALSE(should_sample)) {
      infoz = ForcedTrySample(slot_size, policy.key_size, policy.value_size,
                              policy.soo_capacity);
    }
  }
  const bool has_infoz = infoz.IsSampled();

  RawHashSetLayout layout(new_capacity, slot_size, slot_align, has_infoz);
  void* alloc = policy.get_char_alloc(common);
  char* mem = static_cast<char*>(policy.alloc(alloc, layout.alloc_size()));
  const GenerationType old_generation = common.generation();
  common.set_generation_ptr(
      reinterpret_cast<GenerationType*>(mem + layout.generation_offset()));
  common.set_generation(NextGeneration(old_generation));

  ctrl_t* new_ctrl = reinterpret_cast<ctrl_t*>(mem + layout.control_offset());
  void* new_slots = mem + layout.slot_offset();
  common.set_control</*kGenerateSeed=*/false>(new_ctrl);
  common.set_slots(new_slots);

  h2_t new_h2 = H2(new_hash);
  size_t total_probe_length = 0;
  FindInfo find_info;
  if (old_capacity == 0) {
    static_assert(NextCapacity(0) == 1);
    InitializeSingleElementControlBytes(new_h2, new_ctrl);
    common.generate_new_seed();
    find_info = FindInfo{0, 0};
  } else {
    if (is_single_group(new_capacity)) {
      GrowIntoSingleGroupShuffleControlBytes(old_ctrl, old_capacity, new_ctrl,
                                             new_capacity);
      // Single group tables have no deleted slots, so we can transfer all slots
      // without checking the control bytes.
      assert(common.size() == old_capacity);
      policy.transfer(&common, NextSlot(new_slots, slot_size), old_slots,
                      old_capacity);
      PoisonEmptySlots(common, slot_size);
      // We put the new element either at the beginning or at the end of the
      // table with approximately equal probability.
      size_t offset = SingleGroupTableH1(new_hash, common.seed()) & 1
                          ? 0
                          : new_capacity - 1;

      assert(IsEmpty(new_ctrl[offset]));
      SetCtrlInSingleGroupTable(common, offset, new_h2, policy.slot_size);
      find_info = FindInfo{offset, 0};
    } else {
      ResetCtrl(common, slot_size);
      total_probe_length = policy.find_new_positions_and_transfer_slots(
          common, old_ctrl, old_slots, old_capacity);
      find_info = find_first_non_full(common, new_hash);
      SetCtrlInLargeTable(common, find_info.offset, new_h2, policy.slot_size);
    }
    assert(old_capacity > policy.soo_capacity);
    (*policy.dealloc)(alloc, old_capacity, old_ctrl, slot_size, slot_align,
                      has_infoz);
  }
  PrepareInsertCommon(common);
  ResetGrowthLeft(common);

  if (has_infoz) {
    common.set_has_infoz();
    infoz.RecordStorageChanged(common.size() - 1, new_capacity);
    infoz.RecordRehash(total_probe_length);
    infoz.RecordInsert(new_hash, find_info.probe_length);
    common.set_infoz(infoz);
  }
  return find_info.offset;
}

// Called whenever the table needs to vacate empty slots either by removing
// tombstones via rehash or growth to next capacity.
ABSL_ATTRIBUTE_NOINLINE
size_t RehashOrGrowToNextCapacityAndPrepareInsert(CommonFields& common,
                                                  const PolicyFunctions& policy,
                                                  size_t new_hash) {
  const size_t cap = common.capacity();
  ABSL_ASSUME(cap > 0);
  if (cap > Group::kWidth &&
      // Do these calculations in 64-bit to avoid overflow.
      common.size() * uint64_t{32} <= cap * uint64_t{25}) {
    // Squash DELETED without growing if there is enough capacity.
    //
    // Rehash in place if the current size is <= 25/32 of capacity.
    // Rationale for such a high factor: 1) DropDeletesWithoutResize() is
    // faster than resize, and 2) it takes quite a bit of work to add
    // tombstones.  In the worst case, seems to take approximately 4
    // insert/erase pairs to create a single tombstone and so if we are
    // rehashing because of tombstones, we can afford to rehash-in-place as
    // long as we are reclaiming at least 1/8 the capacity without doing more
    // than 2X the work.  (Where "work" is defined to be size() for rehashing
    // or rehashing in place, and 1 for an insert or erase.)  But rehashing in
    // place is faster per operation than inserting or even doubling the size
    // of the table, so we actually afford to reclaim even less space from a
    // resize-in-place.  The decision is to rehash in place if we can reclaim
    // at about 1/8th of the usable capacity (specifically 3/28 of the
    // capacity) which means that the total cost of rehashing will be a small
    // fraction of the total work.
    //
    // Here is output of an experiment using the BM_CacheInSteadyState
    // benchmark running the old case (where we rehash-in-place only if we can
    // reclaim at least 7/16*capacity) vs. this code (which rehashes in place
    // if we can recover 3/32*capacity).
    //
    // Note that although in the worst-case number of rehashes jumped up from
    // 15 to 190, but the number of operations per second is almost the same.
    //
    // Abridged output of running BM_CacheInSteadyState benchmark from
    // raw_hash_set_benchmark.   N is the number of insert/erase operations.
    //
    //      | OLD (recover >= 7/16        | NEW (recover >= 3/32)
    // size |    N/s LoadFactor NRehashes |    N/s LoadFactor NRehashes
    //  448 | 145284       0.44        18 | 140118       0.44        19
    //  493 | 152546       0.24        11 | 151417       0.48        28
    //  538 | 151439       0.26        11 | 151152       0.53        38
    //  583 | 151765       0.28        11 | 150572       0.57        50
    //  628 | 150241       0.31        11 | 150853       0.61        66
    //  672 | 149602       0.33        12 | 150110       0.66        90
    //  717 | 149998       0.35        12 | 149531       0.70       129
    //  762 | 149836       0.37        13 | 148559       0.74       190
    //  807 | 149736       0.39        14 | 151107       0.39        14
    //  852 | 150204       0.42        15 | 151019       0.42        15
    return DropDeletesWithoutResizeAndPrepareInsert(common, policy, new_hash);
  } else {
    // Otherwise grow the container.
    return GrowToNextCapacityAndPrepareInsert(common, policy, new_hash);
  }
}

// Slow path for PrepareInsertNonSoo that is called when the table has deleted
// slots or need to be resized or rehashed.
size_t PrepareInsertNonSooSlow(CommonFields& common,
                               const PolicyFunctions& policy, size_t hash) {
  const GrowthInfo growth_info = common.growth_info();
  assert(!growth_info.HasNoDeletedAndGrowthLeft());
  if (ABSL_PREDICT_TRUE(growth_info.HasNoGrowthLeftAndNoDeleted())) {
    // Table without deleted slots (>95% cases) that needs to be resized.
    assert(growth_info.HasNoDeleted() && growth_info.GetGrowthLeft() == 0);
    return GrowToNextCapacityAndPrepareInsert(common, policy, hash);
  }
  if (ABSL_PREDICT_FALSE(growth_info.HasNoGrowthLeftAssumingMayHaveDeleted())) {
    // Table with deleted slots that needs to be rehashed or resized.
    return RehashOrGrowToNextCapacityAndPrepareInsert(common, policy, hash);
  }
  // Table with deleted slots that has space for the inserting element.
  FindInfo target = find_first_non_full(common, hash);
  PrepareInsertCommon(common);
  common.growth_info().OverwriteControlAsFull(common.control()[target.offset]);
  SetCtrlInLargeTable(common, target.offset, H2(hash), policy.slot_size);
  common.infoz().RecordInsert(hash, target.probe_length);
  return target.offset;
}

}  // namespace

void* GetRefForEmptyClass(CommonFields& common) {
  // Empty base optimization typically make the empty base class address to be
  // the same as the first address of the derived class object.
  // But we generally assume that for empty classes we can return any valid
  // pointer.
  return &common;
}

void ResizeAllocatedTableWithSeedChange(CommonFields& common,
                                        const PolicyFunctions& policy,
                                        size_t new_capacity) {
  ResizeNonSooImpl<ResizeNonSooMode::kGuaranteedAllocated>(
      common, policy, new_capacity, common.infoz());
}

void ReserveEmptyNonAllocatedTableToFitNewSize(CommonFields& common,
                                               const PolicyFunctions& policy,
                                               size_t new_size) {
  ValidateMaxSize(new_size, policy.slot_size);
  ResizeEmptyNonAllocatedTableImpl(
      common, policy, NormalizeCapacity(GrowthToLowerboundCapacity(new_size)),
      /*force_infoz=*/false);
  // This is after resize, to ensure that we have completed the allocation
  // and have potentially sampled the hashtable.
  common.infoz().RecordReservation(new_size);
  common.reset_reserved_growth(new_size);
  common.set_reservation_size(new_size);
}

void ReserveEmptyNonAllocatedTableToFitBucketCount(
    CommonFields& common, const PolicyFunctions& policy, size_t bucket_count) {
  size_t new_capacity = NormalizeCapacity(bucket_count);
  ValidateMaxSize(CapacityToGrowth(new_capacity), policy.slot_size);
  ResizeEmptyNonAllocatedTableImpl(common, policy, new_capacity,
                                   /*force_infoz=*/false);
}

void GrowEmptySooTableToNextCapacityForceSampling(
    CommonFields& common, const PolicyFunctions& policy) {
  ResizeEmptyNonAllocatedTableImpl(common, policy, NextCapacity(SooCapacity()),
                                   /*force_infoz=*/true);
}

// Resizes a full SOO table to the NextCapacity(SooCapacity()).
template <size_t SooSlotMemcpySize, bool TransferUsesMemcpy>
void GrowFullSooTableToNextCapacity(CommonFields& common,
                                    const PolicyFunctions& policy,
                                    size_t soo_slot_hash) {
  assert(common.capacity() == policy.soo_capacity);
  assert(common.size() == policy.soo_capacity);
  static constexpr size_t kNewCapacity = NextCapacity(SooCapacity());
  assert(kNewCapacity > policy.soo_capacity);
  assert(policy.soo_capacity == SooCapacity());
  const size_t slot_size = policy.slot_size;
  const size_t slot_align = policy.slot_align;
  common.set_capacity(kNewCapacity);

  // Since the table is not empty, it will not be sampled.
  // The decision to sample was already made during the first insertion.
  RawHashSetLayout layout(kNewCapacity, slot_size, slot_align,
                          /*has_infoz=*/false);
  void* alloc = policy.get_char_alloc(common);
  char* mem = static_cast<char*>(policy.alloc(alloc, layout.alloc_size()));
  const GenerationType old_generation = common.generation();
  common.set_generation_ptr(
      reinterpret_cast<GenerationType*>(mem + layout.generation_offset()));
  common.set_generation(NextGeneration(old_generation));

  // We do not set control and slots in CommonFields yet to avoid overriding
  // SOO data.
  ctrl_t* new_ctrl = reinterpret_cast<ctrl_t*>(mem + layout.control_offset());
  void* new_slots = mem + layout.slot_offset();

  InitializeThreeElementsControlBytesAfterSoo(soo_slot_hash, new_ctrl);

  SanitizerPoisonMemoryRegion(new_slots, slot_size * kNewCapacity);
  void* target_slot = SlotAddress(new_slots, SooSlotIndex(), slot_size);
  SanitizerUnpoisonMemoryRegion(target_slot, slot_size);
  if constexpr (TransferUsesMemcpy) {
    // Target slot is placed at index 1, but capacity is at
    // minimum 3. So we are allowed to copy at least twice as much
    // memory.
    static_assert(SooSlotIndex() == 1);
    static_assert(SooSlotMemcpySize > 0);
    static_assert(SooSlotMemcpySize <= MaxSooSlotSize());
    assert(SooSlotMemcpySize <= 2 * slot_size);
    assert(SooSlotMemcpySize >= slot_size);
    void* next_slot = SlotAddress(target_slot, 1, slot_size);
    SanitizerUnpoisonMemoryRegion(next_slot, SooSlotMemcpySize - slot_size);
    std::memcpy(target_slot, common.soo_data(), SooSlotMemcpySize);
    SanitizerPoisonMemoryRegion(next_slot, SooSlotMemcpySize - slot_size);
  } else {
    static_assert(SooSlotMemcpySize == 0);
    policy.transfer(&common, target_slot, common.soo_data(), 1);
  }
  common.set_control</*kGenerateSeed=*/true>(new_ctrl);
  common.set_slots(new_slots);

  ResetGrowthLeft(common);
}

void GrowFullSooTableToNextCapacityForceSampling(
    CommonFields& common, const PolicyFunctions& policy) {
  assert(common.capacity() == policy.soo_capacity);
  assert(common.size() == policy.soo_capacity);
  assert(policy.soo_capacity == SooCapacity());
  ResizeFullSooTable(
      common, policy, NextCapacity(SooCapacity()),
      ResizeFullSooTableSamplingMode::kForceSampleNoResizeIfUnsampled);
}

void Rehash(CommonFields& common, const PolicyFunctions& policy, size_t n) {
  const size_t cap = common.capacity();

  auto clear_backing_array = [&]() {
    ClearBackingArray(common, policy, policy.get_char_alloc(common),
                      /*reuse=*/false, policy.soo_capacity > 0);
  };

  const size_t slot_size = policy.slot_size;

  if (n == 0) {
    if (cap <= policy.soo_capacity) return;
    if (common.empty()) {
      clear_backing_array();
      return;
    }
    if (common.size() <= policy.soo_capacity) {
      // When the table is already sampled, we keep it sampled.
      if (common.infoz().IsSampled()) {
        static constexpr size_t kInitialSampledCapacity =
            NextCapacity(SooCapacity());
        if (cap > kInitialSampledCapacity) {
          ResizeAllocatedTableWithSeedChange(common, policy,
                                             kInitialSampledCapacity);
        }
        // This asserts that we didn't lose sampling coverage in `resize`.
        assert(common.infoz().IsSampled());
        return;
      }
      assert(slot_size <= sizeof(HeapOrSoo));
      assert(policy.slot_align <= alignof(HeapOrSoo));
      HeapOrSoo tmp_slot(uninitialized_tag_t{});
      size_t begin_offset = FindFirstFullSlot(0, cap, common.control());
      policy.transfer(&common, &tmp_slot,
                      SlotAddress(common.slot_array(), begin_offset, slot_size),
                      1);
      clear_backing_array();
      policy.transfer(&common, common.soo_data(), &tmp_slot, 1);
      common.set_full_soo();
      return;
    }
  }

  // bitor is a faster way of doing `max` here. We will round up to the next
  // power-of-2-minus-1, so bitor is good enough.
  size_t new_size = n | GrowthToLowerboundCapacity(common.size());
  ValidateMaxSize(n, policy.slot_size);
  const size_t new_capacity = NormalizeCapacity(new_size);
  // n == 0 unconditionally rehashes as per the standard.
  if (n == 0 || new_capacity > cap) {
    if (cap == policy.soo_capacity) {
      if (common.empty()) {
        ResizeEmptyNonAllocatedTableImpl(common, policy, new_capacity,
                                         /*force_infoz=*/false);
      } else {
        ResizeFullSooTable(common, policy, new_capacity,
                           ResizeFullSooTableSamplingMode::kNoSampling);
      }
    } else {
      ResizeAllocatedTableWithSeedChange(common, policy, new_capacity);
    }
    // This is after resize, to ensure that we have completed the allocation
    // and have potentially sampled the hashtable.
    common.infoz().RecordReservation(n);
  }
}

void ReserveAllocatedTable(CommonFields& common, const PolicyFunctions& policy,
                           size_t n) {
  common.reset_reserved_growth(n);
  common.set_reservation_size(n);

  const size_t cap = common.capacity();
  assert(!common.empty() || cap > policy.soo_capacity);
  assert(cap > 0);
  const size_t max_size_before_growth =
      cap <= policy.soo_capacity ? policy.soo_capacity
                                 : common.size() + common.growth_left();
  if (n <= max_size_before_growth) {
    return;
  }
  ValidateMaxSize(n, policy.slot_size);
  const size_t new_capacity = NormalizeCapacity(GrowthToLowerboundCapacity(n));
  if (cap == policy.soo_capacity) {
    assert(!common.empty());
    ResizeFullSooTable(common, policy, new_capacity,
                       ResizeFullSooTableSamplingMode::kNoSampling);
  } else {
    assert(cap > policy.soo_capacity);
    ResizeAllocatedTableWithSeedChange(common, policy, new_capacity);
  }
  common.infoz().RecordReservation(n);
}

size_t PrepareInsertNonSoo(CommonFields& common, const PolicyFunctions& policy,
                           size_t hash, FindInfo target) {
  const bool rehash_for_bug_detection =
      common.should_rehash_for_bug_detection_on_insert() &&
      // Required to allow use of ResizeAllocatedTable.
      common.capacity() > 0;
  if (rehash_for_bug_detection) {
    // Move to a different heap allocation in order to detect bugs.
    const size_t cap = common.capacity();
    ResizeAllocatedTableWithSeedChange(
        common, policy, common.growth_left() > 0 ? cap : NextCapacity(cap));
    target = find_first_non_full(common, hash);
  }

  const GrowthInfo growth_info = common.growth_info();
  // When there are no deleted slots in the table
  // and growth_left is positive, we can insert at the first
  // empty slot in the probe sequence (target).
  if (ABSL_PREDICT_FALSE(!growth_info.HasNoDeletedAndGrowthLeft())) {
    return PrepareInsertNonSooSlow(common, policy, hash);
  }
  PrepareInsertCommon(common);
  common.growth_info().OverwriteEmptyAsFull();
  SetCtrl(common, target.offset, H2(hash), policy.slot_size);
  common.infoz().RecordInsert(hash, target.probe_length);
  return target.offset;
}

namespace {
// Returns true if the following is true
// 1. OptimalMemcpySizeForSooSlotTransfer(left) >
//    OptimalMemcpySizeForSooSlotTransfer(left - 1)
// 2. OptimalMemcpySizeForSooSlotTransfer(left) are equal for all i in [left,
// right].
// This function is used to verify that we have all the possible template
// instantiations for GrowFullSooTableToNextCapacity.
// With this verification the problem may be detected at compile time instead of
// link time.
constexpr bool VerifyOptimalMemcpySizeForSooSlotTransferRange(size_t left,
                                                              size_t right) {
  size_t optimal_size_for_range = OptimalMemcpySizeForSooSlotTransfer(left);
  if (optimal_size_for_range <= OptimalMemcpySizeForSooSlotTransfer(left - 1)) {
    return false;
  }
  for (size_t i = left + 1; i <= right; ++i) {
    if (OptimalMemcpySizeForSooSlotTransfer(i) != optimal_size_for_range) {
      return false;
    }
  }
  return true;
}
}  // namespace

// We need to instantiate ALL possible template combinations because we define
// the function in the cc file.
template void GrowFullSooTableToNextCapacity<0, false>(CommonFields&,
                                                       const PolicyFunctions&,
                                                       size_t);
template void
GrowFullSooTableToNextCapacity<OptimalMemcpySizeForSooSlotTransfer(1), true>(
    CommonFields&, const PolicyFunctions&, size_t);

static_assert(VerifyOptimalMemcpySizeForSooSlotTransferRange(2, 3));
template void
GrowFullSooTableToNextCapacity<OptimalMemcpySizeForSooSlotTransfer(3), true>(
    CommonFields&, const PolicyFunctions&, size_t);

static_assert(VerifyOptimalMemcpySizeForSooSlotTransferRange(4, 8));
template void
GrowFullSooTableToNextCapacity<OptimalMemcpySizeForSooSlotTransfer(8), true>(
    CommonFields&, const PolicyFunctions&, size_t);

#if UINTPTR_MAX == UINT32_MAX
static_assert(MaxSooSlotSize() == 8);
#else
static_assert(VerifyOptimalMemcpySizeForSooSlotTransferRange(9, 16));
template void
GrowFullSooTableToNextCapacity<OptimalMemcpySizeForSooSlotTransfer(16), true>(
    CommonFields&, const PolicyFunctions&, size_t);
static_assert(MaxSooSlotSize() == 16);
#endif

}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl
