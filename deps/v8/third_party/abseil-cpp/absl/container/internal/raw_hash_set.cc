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
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/dynamic_annotations.h"
#include "absl/base/internal/endian.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/optimization.h"
#include "absl/container/internal/container_memory.h"
#include "absl/container/internal/hashtable_control_bytes.h"
#include "absl/container/internal/hashtablez_sampler.h"
#include "absl/container/internal/raw_hash_set_resize_impl.h"
#include "absl/functional/function_ref.h"
#include "absl/hash/hash.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {

// Represents a control byte corresponding to a full slot with arbitrary hash.
constexpr ctrl_t ZeroCtrlT() { return static_cast<ctrl_t>(0); }

// A single control byte for default-constructed iterators. We leave it
// uninitialized because reading this memory is a bug.
ABSL_DLL ctrl_t kDefaultIterControl;

// We need one full byte followed by a sentinel byte for iterator::operator++ to
// work. We have a full group after kSentinel to be safe (in case operator++ is
// changed to read a full group).
ABSL_CONST_INIT ABSL_DLL const ctrl_t kSooControl[17] = {
    ZeroCtrlT(),    ctrl_t::kSentinel, ZeroCtrlT(),    ctrl_t::kEmpty,
    ctrl_t::kEmpty, ctrl_t::kEmpty,    ctrl_t::kEmpty, ctrl_t::kEmpty,
    ctrl_t::kEmpty, ctrl_t::kEmpty,    ctrl_t::kEmpty, ctrl_t::kEmpty,
    ctrl_t::kEmpty, ctrl_t::kEmpty,    ctrl_t::kEmpty, ctrl_t::kEmpty,
    ctrl_t::kEmpty};

namespace {

#ifdef ABSL_SWISSTABLE_ASSERT
#error ABSL_SWISSTABLE_ASSERT cannot be directly set
#else
// We use this macro for assertions that users may see when the table is in an
// invalid state that sanitizers may help diagnose.
#define ABSL_SWISSTABLE_ASSERT(CONDITION) \
  assert((CONDITION) && "Try enabling sanitizers.")
#endif

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
// This function basically using H2 last bits to save on shift operation.
size_t SingleGroupTableH1(size_t hash, PerTableSeed seed) {
  return hash ^ seed.seed();
}

// Returns the offset of the new element after resize from capacity 1 to 3.
size_t Resize1To3NewOffset(size_t hash, PerTableSeed seed) {
  // After resize from capacity 1 to 3, we always have exactly the slot with
  // index 1 occupied, so we need to insert either at index 0 or index 2.
  static_assert(SooSlotIndex() == 1);
  return SingleGroupTableH1(hash, seed) & 2;
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

namespace {

FindInfo find_first_non_full_from_h1(const ctrl_t* ctrl, size_t h1,
                                     size_t capacity) {
  auto seq = probe(h1, capacity);
  if (IsEmptyOrDeleted(ctrl[seq.offset()])) {
    return {seq.offset(), /*probe_length=*/0};
  }
  while (true) {
    GroupFullEmptyOrDeleted g{ctrl + seq.offset()};
    auto mask = g.MaskEmptyOrDeleted();
    if (mask) {
      return {seq.offset(mask.LowestBitSet()), seq.index()};
    }
    seq.next();
    ABSL_SWISSTABLE_ASSERT(seq.index() <= capacity && "full table!");
  }
}

// Whether a table fits in half a group. A half-group table fits entirely into a
// probing group, i.e., has a capacity < `Group::kWidth`.
//
// In half-group mode we are able to use the whole capacity. The extra control
// bytes give us at least one "empty" control byte to stop the iteration.
// This is important to make 1 a valid capacity.
//
// In half-group mode only the first `capacity` control bytes after the sentinel
// are valid. The rest contain dummy ctrl_t::kEmpty values that do not
// represent a real slot.
constexpr bool is_half_group(size_t capacity) {
  return capacity < Group::kWidth - 1;
}

template <class Fn>
void IterateOverFullSlotsImpl(const CommonFields& c, size_t slot_size, Fn cb) {
  const size_t cap = c.capacity();
  ABSL_SWISSTABLE_ASSERT(!IsSmallCapacity(cap));
  const ctrl_t* ctrl = c.control();
  void* slot = c.slot_array();
  if (is_half_group(cap)) {
    // Mirrored/cloned control bytes in half-group table are also located in the
    // first group (starting from position 0). We are taking group from position
    // `capacity` in order to avoid duplicates.

    // Half-group tables capacity fits into portable group, where
    // GroupPortableImpl::MaskFull is more efficient for the
    // capacity <= GroupPortableImpl::kWidth.
    ABSL_SWISSTABLE_ASSERT(cap <= GroupPortableImpl::kWidth &&
                           "unexpectedly large half-group capacity");
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
      ABSL_SWISSTABLE_ASSERT(IsFull(ctrl[i]) &&
                             "hash table was modified unexpectedly");
      cb(ctrl + i, SlotAddress(slot, i, slot_size));
      --remaining;
    }
    ctrl += Group::kWidth;
    slot = NextSlot(slot, slot_size, Group::kWidth);
    ABSL_SWISSTABLE_ASSERT(
        (remaining == 0 || *(ctrl - 1) != ctrl_t::kSentinel) &&
        "hash table was modified unexpectedly");
  }
  // NOTE: erasure of the current element is allowed in callback for
  // absl::erase_if specialization. So we use `>=`.
  ABSL_SWISSTABLE_ASSERT(original_size_for_assert >= c.size() &&
                         "hash table was modified unexpectedly");
}

}  // namespace

void ConvertDeletedToEmptyAndFullToDeleted(ctrl_t* ctrl, size_t capacity) {
  ABSL_SWISSTABLE_ASSERT(ctrl[capacity] == ctrl_t::kSentinel);
  ABSL_SWISSTABLE_ASSERT(IsValidCapacity(capacity));
  for (ctrl_t* pos = ctrl; pos < ctrl + capacity; pos += Group::kWidth) {
    Group{pos}.ConvertSpecialToEmptyAndFullToDeleted(pos);
  }
  // Copy the cloned ctrl bytes.
  std::memcpy(ctrl + capacity + 1, ctrl, NumClonedBytes());
  ctrl[capacity] = ctrl_t::kSentinel;
}

FindInfo find_first_non_full(const CommonFields& common, size_t hash) {
  return find_first_non_full_from_h1(common.control(), H1(hash, common.seed()),
                                     common.capacity());
}

void IterateOverFullSlots(const CommonFields& c, size_t slot_size,
                          absl::FunctionRef<void(const ctrl_t*, void*)> cb) {
  IterateOverFullSlotsImpl(c, slot_size, cb);
}

namespace {

void ResetGrowthLeft(GrowthInfo& growth_info, size_t capacity, size_t size) {
  growth_info.InitGrowthLeftNoDeleted(CapacityToGrowth(capacity) - size);
}

void ResetGrowthLeft(CommonFields& common) {
  ResetGrowthLeft(common.growth_info(), common.capacity(), common.size());
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

void PrepareInsertCommon(CommonFields& common) {
  common.increment_size();
  common.maybe_increment_generation_on_insert();
}

size_t DropDeletesWithoutResizeAndPrepareInsert(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    size_t new_hash) {
  void* set = &common;
  void* slot_array = common.slot_array();
  const size_t capacity = common.capacity();
  ABSL_SWISSTABLE_ASSERT(IsValidCapacity(capacity));
  ABSL_SWISSTABLE_ASSERT(!is_single_group(capacity));
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
  auto transfer_n = policy.transfer_n;
  const size_t slot_size = policy.slot_size;

  size_t total_probe_length = 0;
  void* slot_ptr = SlotAddress(slot_array, 0, slot_size);

  // The index of an empty slot that can be used as temporary memory for
  // the swap operation.
  constexpr size_t kUnknownId = ~size_t{};
  size_t tmp_space_id = kUnknownId;

  for (size_t i = 0; i != capacity;
       ++i, slot_ptr = NextSlot(slot_ptr, slot_size)) {
    ABSL_SWISSTABLE_ASSERT(slot_ptr == SlotAddress(slot_array, i, slot_size));
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
      (*transfer_n)(set, new_slot_ptr, slot_ptr, 1);
      SetCtrlInLargeTable(common, i, ctrl_t::kEmpty, slot_size);
      // Initialize or change empty space id.
      tmp_space_id = i;
    } else {
      ABSL_SWISSTABLE_ASSERT(IsDeleted(ctrl[new_i]));
      SetCtrlInLargeTable(common, new_i, h2, slot_size);
      // Until we are done rehashing, DELETED marks previously FULL slots.

      if (tmp_space_id == kUnknownId) {
        tmp_space_id = FindEmptySlot(i + 1, capacity, ctrl);
      }
      void* tmp_space = SlotAddress(slot_array, tmp_space_id, slot_size);
      SanitizerUnpoisonMemoryRegion(tmp_space, slot_size);

      // Swap i and new_i elements.
      (*transfer_n)(set, tmp_space, new_slot_ptr, 1);
      (*transfer_n)(set, new_slot_ptr, slot_ptr, 1);
      (*transfer_n)(set, slot_ptr, tmp_space, 1);

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
  SetCtrlInLargeTable(common, find_info.offset, H2(new_hash), slot_size);
  common.infoz().RecordInsert(new_hash, find_info.probe_length);
  common.infoz().RecordRehash(total_probe_length);
  return find_info.offset;
}

bool WasNeverFull(CommonFields& c, size_t index) {
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

// Updates the control bytes to indicate a completely empty table such that all
// control bytes are kEmpty except for the kSentinel byte.
void ResetCtrl(CommonFields& common, size_t slot_size) {
  const size_t capacity = common.capacity();
  ctrl_t* ctrl = common.control();
  static constexpr size_t kTwoGroupCapacity = 2 * Group::kWidth - 1;
  if (ABSL_PREDICT_TRUE(capacity <= kTwoGroupCapacity)) {
    if (IsSmallCapacity(capacity)) return;
    std::memset(ctrl, static_cast<int8_t>(ctrl_t::kEmpty), Group::kWidth);
    std::memset(ctrl + capacity, static_cast<int8_t>(ctrl_t::kEmpty),
                Group::kWidth);
    if (capacity == kTwoGroupCapacity) {
      std::memset(ctrl + Group::kWidth, static_cast<int8_t>(ctrl_t::kEmpty),
                  Group::kWidth);
    }
  } else {
    std::memset(ctrl, static_cast<int8_t>(ctrl_t::kEmpty),
                capacity + 1 + NumClonedBytes());
  }
  ctrl[capacity] = ctrl_t::kSentinel;
  SanitizerPoisonMemoryRegion(common.slot_array(), slot_size * capacity);
}

// Initializes control bytes for growing from capacity 1 to 3.
// `orig_h2` is placed in the position `SooSlotIndex()`.
// `new_h2` is placed in the position `new_offset`.
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void InitializeThreeElementsControlBytes(
    h2_t orig_h2, h2_t new_h2, size_t new_offset, ctrl_t* new_ctrl) {
  static constexpr size_t kNewCapacity = NextCapacity(SooCapacity());
  static_assert(kNewCapacity == 3);
  static_assert(is_single_group(kNewCapacity));
  static_assert(SooSlotIndex() == 1);
  ABSL_SWISSTABLE_ASSERT(new_offset == 0 || new_offset == 2);

  static constexpr uint64_t kEmptyXorSentinel =
      static_cast<uint8_t>(ctrl_t::kEmpty) ^
      static_cast<uint8_t>(ctrl_t::kSentinel);
  static constexpr uint64_t kEmpty64 = static_cast<uint8_t>(ctrl_t::kEmpty);
  static constexpr size_t kMirroredSooSlotIndex =
      SooSlotIndex() + kNewCapacity + 1;
  // The first 8 bytes, where SOO slot original and mirrored positions are
  // replaced with 0.
  // Result will look like: E0ESE0EE
  static constexpr uint64_t kFirstCtrlBytesWithZeroes =
      k8EmptyBytes ^ (kEmpty64 << (8 * SooSlotIndex())) ^
      (kEmptyXorSentinel << (8 * kNewCapacity)) ^
      (kEmpty64 << (8 * kMirroredSooSlotIndex));

  const uint64_t soo_h2 = static_cast<uint64_t>(orig_h2);
  const uint64_t new_h2_xor_empty =
      static_cast<uint64_t>(new_h2 ^ static_cast<uint8_t>(ctrl_t::kEmpty));
  // Fill the original and mirrored bytes for SOO slot.
  // Result will look like:
  // EHESEHEE
  // Where H = soo_h2, E = kEmpty, S = kSentinel.
  uint64_t first_ctrl_bytes =
      ((soo_h2 << (8 * SooSlotIndex())) | kFirstCtrlBytesWithZeroes) |
      (soo_h2 << (8 * kMirroredSooSlotIndex));
  // Replace original and mirrored empty bytes for the new position.
  // Result for new_offset 0 will look like:
  // NHESNHEE
  // Where H = soo_h2, N = H2(new_hash), E = kEmpty, S = kSentinel.
  // Result for new_offset 2 will look like:
  // EHNSEHNE
  first_ctrl_bytes ^= (new_h2_xor_empty << (8 * new_offset));
  size_t new_mirrored_offset = new_offset + kNewCapacity + 1;
  first_ctrl_bytes ^= (new_h2_xor_empty << (8 * new_mirrored_offset));

  // Fill last bytes with kEmpty.
  std::memset(new_ctrl + kNewCapacity, static_cast<int8_t>(ctrl_t::kEmpty),
              Group::kWidth);
  // Overwrite the first 8 bytes with first_ctrl_bytes.
  absl::little_endian::Store64(new_ctrl, first_ctrl_bytes);

  // Example for group size 16:
  // new_ctrl after 1st memset =      ???EEEEEEEEEEEEEEEE
  // new_offset 0:
  // new_ctrl after 2nd store  =      NHESNHEEEEEEEEEEEEE
  // new_offset 2:
  // new_ctrl after 2nd store  =      EHNSEHNEEEEEEEEEEEE

  // Example for group size 8:
  // new_ctrl after 1st memset =      ???EEEEEEEE
  // new_offset 0:
  // new_ctrl after 2nd store  =      NHESNHEEEEE
  // new_offset 2:
  // new_ctrl after 2nd store  =      EHNSEHNEEEE
}

}  // namespace

void EraseMetaOnly(CommonFields& c, const ctrl_t* ctrl, size_t slot_size) {
  ABSL_SWISSTABLE_ASSERT(IsFull(*ctrl) && "erasing a dangling iterator");
  c.decrement_size();
  c.infoz().RecordErase();

  if (c.is_small()) {
    SanitizerPoisonMemoryRegion(c.slot_array(), slot_size);
    c.growth_info().OverwriteFullAsEmpty();
    return;
  }

  size_t index = static_cast<size_t>(ctrl - c.control());

  if (WasNeverFull(c, index)) {
    SetCtrl(c, index, ctrl_t::kEmpty, slot_size);
    c.growth_info().OverwriteFullAsEmpty();
    return;
  }

  c.growth_info().OverwriteFullAsDeleted();
  SetCtrlInLargeTable(c, index, ctrl_t::kDeleted, slot_size);
}

void ClearBackingArray(CommonFields& c,
                       const PolicyFunctions& __restrict policy, void* alloc,
                       bool reuse, bool soo_enabled) {
  if (reuse) {
    c.set_size_to_zero();
    ABSL_SWISSTABLE_ASSERT(!soo_enabled || c.capacity() > SooCapacity());
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

enum class ResizeNonSooMode {
  kGuaranteedEmpty,
  kGuaranteedAllocated,
};

// Iterates over full slots in old table, finds new positions for them and
// transfers the slots.
// This function is used for reserving or rehashing non-empty tables.
// This use case is rare so the function is type erased.
// Returns the total probe length.
size_t FindNewPositionsAndTransferSlots(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    ctrl_t* old_ctrl, void* old_slots, size_t old_capacity) {
  void* new_slots = common.slot_array();
  const void* hash_fn = policy.hash_fn(common);
  const size_t slot_size = policy.slot_size;

  const auto insert_slot = [&](void* slot) {
    size_t hash = policy.hash_slot(hash_fn, slot);
    FindInfo target =
        common.is_small() ? FindInfo{0, 0} : find_first_non_full(common, hash);
    SetCtrl(common, target.offset, H2(hash), slot_size);
    policy.transfer_n(&common, SlotAddress(new_slots, target.offset, slot_size),
                      slot, 1);
    return target.probe_length;
  };
  if (IsSmallCapacity(old_capacity)) {
    if (common.size() == 1) insert_slot(old_slots);
    return 0;
  }
  size_t total_probe_length = 0;
  for (size_t i = 0; i < old_capacity; ++i) {
    if (IsFull(old_ctrl[i])) {
      total_probe_length += insert_slot(old_slots);
    }
    old_slots = NextSlot(old_slots, slot_size);
  }
  return total_probe_length;
}

void ReportGrowthToInfozImpl(CommonFields& common, HashtablezInfoHandle infoz,
                             size_t hash, size_t total_probe_length,
                             size_t distance_from_desired) {
  ABSL_SWISSTABLE_ASSERT(infoz.IsSampled());
  infoz.RecordStorageChanged(common.size() - 1, common.capacity());
  infoz.RecordRehash(total_probe_length);
  infoz.RecordInsert(hash, distance_from_desired);
  common.set_has_infoz();
  // TODO(b/413062340): we could potentially store infoz in place of the
  // control pointer for the capacity 1 case.
  common.set_infoz(infoz);
}

// Specialization to avoid passing two 0s from hot function.
ABSL_ATTRIBUTE_NOINLINE void ReportSingleGroupTableGrowthToInfoz(
    CommonFields& common, HashtablezInfoHandle infoz, size_t hash) {
  ReportGrowthToInfozImpl(common, infoz, hash, /*total_probe_length=*/0,
                          /*distance_from_desired=*/0);
}

ABSL_ATTRIBUTE_NOINLINE void ReportGrowthToInfoz(CommonFields& common,
                                                 HashtablezInfoHandle infoz,
                                                 size_t hash,
                                                 size_t total_probe_length,
                                                 size_t distance_from_desired) {
  ReportGrowthToInfozImpl(common, infoz, hash, total_probe_length,
                          distance_from_desired);
}

ABSL_ATTRIBUTE_NOINLINE void ReportResizeToInfoz(CommonFields& common,
                                                 HashtablezInfoHandle infoz,
                                                 size_t total_probe_length) {
  ABSL_SWISSTABLE_ASSERT(infoz.IsSampled());
  infoz.RecordStorageChanged(common.size(), common.capacity());
  infoz.RecordRehash(total_probe_length);
  common.set_has_infoz();
  common.set_infoz(infoz);
}

struct BackingArrayPtrs {
  ctrl_t* ctrl;
  void* slots;
};

BackingArrayPtrs AllocBackingArray(CommonFields& common,
                                   const PolicyFunctions& __restrict policy,
                                   size_t new_capacity, bool has_infoz,
                                   void* alloc) {
  RawHashSetLayout layout(new_capacity, policy.slot_size, policy.slot_align,
                          has_infoz);
  char* mem = static_cast<char*>(policy.alloc(alloc, layout.alloc_size()));
  const GenerationType old_generation = common.generation();
  common.set_generation_ptr(
      reinterpret_cast<GenerationType*>(mem + layout.generation_offset()));
  common.set_generation(NextGeneration(old_generation));

  return {reinterpret_cast<ctrl_t*>(mem + layout.control_offset()),
          mem + layout.slot_offset()};
}

template <ResizeNonSooMode kMode>
void ResizeNonSooImpl(CommonFields& common,
                      const PolicyFunctions& __restrict policy,
                      size_t new_capacity, HashtablezInfoHandle infoz) {
  ABSL_SWISSTABLE_ASSERT(IsValidCapacity(new_capacity));
  ABSL_SWISSTABLE_ASSERT(new_capacity > policy.soo_capacity());

  const size_t old_capacity = common.capacity();
  [[maybe_unused]] ctrl_t* old_ctrl = common.control();
  [[maybe_unused]] void* old_slots = common.slot_array();

  const size_t slot_size = policy.slot_size;
  const size_t slot_align = policy.slot_align;
  const bool has_infoz = infoz.IsSampled();
  void* alloc = policy.get_char_alloc(common);

  common.set_capacity(new_capacity);
  const auto [new_ctrl, new_slots] =
      AllocBackingArray(common, policy, new_capacity, has_infoz, alloc);
  common.set_control</*kGenerateSeed=*/true>(new_ctrl);
  common.set_slots(new_slots);

  size_t total_probe_length = 0;
  ResetCtrl(common, slot_size);
  ABSL_SWISSTABLE_ASSERT(kMode != ResizeNonSooMode::kGuaranteedEmpty ||
                         old_capacity == policy.soo_capacity());
  ABSL_SWISSTABLE_ASSERT(kMode != ResizeNonSooMode::kGuaranteedAllocated ||
                         old_capacity > 0);
  if constexpr (kMode == ResizeNonSooMode::kGuaranteedAllocated) {
    total_probe_length = FindNewPositionsAndTransferSlots(
        common, policy, old_ctrl, old_slots, old_capacity);
    (*policy.dealloc)(alloc, old_capacity, old_ctrl, slot_size, slot_align,
                      has_infoz);
    ResetGrowthLeft(GetGrowthInfoFromControl(new_ctrl), new_capacity,
                    common.size());
  } else {
    GetGrowthInfoFromControl(new_ctrl).InitGrowthLeftNoDeleted(
        CapacityToGrowth(new_capacity));
  }

  if (ABSL_PREDICT_FALSE(has_infoz)) {
    ReportResizeToInfoz(common, infoz, total_probe_length);
  }
}

void ResizeEmptyNonAllocatedTableImpl(CommonFields& common,
                                      const PolicyFunctions& __restrict policy,
                                      size_t new_capacity, bool force_infoz) {
  ABSL_SWISSTABLE_ASSERT(IsValidCapacity(new_capacity));
  ABSL_SWISSTABLE_ASSERT(new_capacity > policy.soo_capacity());
  ABSL_SWISSTABLE_ASSERT(!force_infoz || policy.soo_enabled);
  ABSL_SWISSTABLE_ASSERT(common.capacity() <= policy.soo_capacity());
  ABSL_SWISSTABLE_ASSERT(common.empty());
  const size_t slot_size = policy.slot_size;
  HashtablezInfoHandle infoz;
  const bool should_sample =
      policy.is_hashtablez_eligible && (force_infoz || ShouldSampleNextTable());
  if (ABSL_PREDICT_FALSE(should_sample)) {
    infoz = ForcedTrySample(slot_size, policy.key_size, policy.value_size,
                            policy.soo_capacity());
  }
  ResizeNonSooImpl<ResizeNonSooMode::kGuaranteedEmpty>(common, policy,
                                                       new_capacity, infoz);
}

// If the table was SOO, initializes new control bytes and transfers slot.
// After transferring the slot, sets control and slots in CommonFields.
// It is rare to resize an SOO table with one element to a large size.
// Requires: `c` contains SOO data.
void InsertOldSooSlotAndInitializeControlBytes(
    CommonFields& c, const PolicyFunctions& __restrict policy, size_t hash,
    ctrl_t* new_ctrl, void* new_slots) {
  ABSL_SWISSTABLE_ASSERT(c.size() == policy.soo_capacity());
  ABSL_SWISSTABLE_ASSERT(policy.soo_enabled);
  size_t new_capacity = c.capacity();

  c.generate_new_seed();
  size_t offset = probe(c.seed(), new_capacity, hash).offset();
  offset = offset == new_capacity ? 0 : offset;
  SanitizerPoisonMemoryRegion(new_slots, policy.slot_size * new_capacity);
  void* target_slot = SlotAddress(new_slots, offset, policy.slot_size);
  SanitizerUnpoisonMemoryRegion(target_slot, policy.slot_size);
  policy.transfer_n(&c, target_slot, c.soo_data(), 1);
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

void AssertSoo([[maybe_unused]] CommonFields& common,
               [[maybe_unused]] const PolicyFunctions& policy) {
  ABSL_SWISSTABLE_ASSERT(policy.soo_enabled);
  ABSL_SWISSTABLE_ASSERT(common.capacity() == policy.soo_capacity());
}
void AssertFullSoo([[maybe_unused]] CommonFields& common,
                   [[maybe_unused]] const PolicyFunctions& policy) {
  AssertSoo(common, policy);
  ABSL_SWISSTABLE_ASSERT(common.size() == policy.soo_capacity());
}

void ResizeFullSooTable(CommonFields& common,
                        const PolicyFunctions& __restrict policy,
                        size_t new_capacity,
                        ResizeFullSooTableSamplingMode sampling_mode) {
  AssertFullSoo(common, policy);
  const size_t slot_size = policy.slot_size;
  void* alloc = policy.get_char_alloc(common);

  HashtablezInfoHandle infoz;
  if (sampling_mode ==
      ResizeFullSooTableSamplingMode::kForceSampleNoResizeIfUnsampled) {
    if (ABSL_PREDICT_FALSE(policy.is_hashtablez_eligible)) {
      infoz = ForcedTrySample(slot_size, policy.key_size, policy.value_size,
                              policy.soo_capacity());
    }

    if (!infoz.IsSampled()) {
      return;
    }
  }

  const bool has_infoz = infoz.IsSampled();

  common.set_capacity(new_capacity);

  // We do not set control and slots in CommonFields yet to avoid overriding
  // SOO data.
  const auto [new_ctrl, new_slots] =
      AllocBackingArray(common, policy, new_capacity, has_infoz, alloc);

  const size_t soo_slot_hash =
      policy.hash_slot(policy.hash_fn(common), common.soo_data());

  InsertOldSooSlotAndInitializeControlBytes(common, policy, soo_slot_hash,
                                            new_ctrl, new_slots);
  ResetGrowthLeft(common);
  if (has_infoz) {
    common.set_has_infoz();
    common.set_infoz(infoz);
    infoz.RecordStorageChanged(common.size(), new_capacity);
  }
}

void GrowIntoSingleGroupShuffleControlBytes(ctrl_t* __restrict old_ctrl,
                                            size_t old_capacity,
                                            ctrl_t* __restrict new_ctrl,
                                            size_t new_capacity) {
  ABSL_SWISSTABLE_ASSERT(is_single_group(new_capacity));
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
    ABSL_SWISSTABLE_ASSERT(old_capacity < 8 &&
                           "old_capacity is too large for group size 8");
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

  ABSL_SWISSTABLE_ASSERT(Group::kWidth == 16);

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

// Size of the buffer we allocate on stack for storing probed elements in
// GrowToNextCapacity algorithm.
constexpr size_t kProbedElementsBufferSize = 512;

// Decodes information about probed elements from contiguous memory.
// Finds new position for each element and transfers it to the new slots.
// Returns the total probe length.
template <typename ProbedItem>
ABSL_ATTRIBUTE_NOINLINE size_t DecodeAndInsertImpl(
    CommonFields& c, const PolicyFunctions& __restrict policy,
    const ProbedItem* start, const ProbedItem* end, void* old_slots) {
  const size_t new_capacity = c.capacity();

  void* new_slots = c.slot_array();
  ctrl_t* new_ctrl = c.control();
  size_t total_probe_length = 0;

  const size_t slot_size = policy.slot_size;
  auto transfer_n = policy.transfer_n;

  for (; start < end; ++start) {
    const FindInfo target = find_first_non_full_from_h1(
        new_ctrl, static_cast<size_t>(start->h1), new_capacity);
    total_probe_length += target.probe_length;
    const size_t old_index = static_cast<size_t>(start->source_offset);
    const size_t new_i = target.offset;
    ABSL_SWISSTABLE_ASSERT(old_index < new_capacity / 2);
    ABSL_SWISSTABLE_ASSERT(new_i < new_capacity);
    ABSL_SWISSTABLE_ASSERT(IsEmpty(new_ctrl[new_i]));
    void* src_slot = SlotAddress(old_slots, old_index, slot_size);
    void* dst_slot = SlotAddress(new_slots, new_i, slot_size);
    SanitizerUnpoisonMemoryRegion(dst_slot, slot_size);
    transfer_n(&c, dst_slot, src_slot, 1);
    SetCtrlInLargeTable(c, new_i, static_cast<h2_t>(start->h2), slot_size);
  }
  return total_probe_length;
}

// Sentinel value for the start of marked elements.
// Signals that there are no marked elements.
constexpr size_t kNoMarkedElementsSentinel = ~size_t{};

// Process probed elements that did not fit into available buffers.
// We marked them in control bytes as kSentinel.
// Hash recomputation and full probing is done here.
// This use case should be extremely rare.
ABSL_ATTRIBUTE_NOINLINE size_t ProcessProbedMarkedElements(
    CommonFields& c, const PolicyFunctions& __restrict policy, ctrl_t* old_ctrl,
    void* old_slots, size_t start) {
  size_t old_capacity = PreviousCapacity(c.capacity());
  const size_t slot_size = policy.slot_size;
  void* new_slots = c.slot_array();
  size_t total_probe_length = 0;
  const void* hash_fn = policy.hash_fn(c);
  auto hash_slot = policy.hash_slot;
  auto transfer_n = policy.transfer_n;
  for (size_t old_index = start; old_index < old_capacity; ++old_index) {
    if (old_ctrl[old_index] != ctrl_t::kSentinel) {
      continue;
    }
    void* src_slot = SlotAddress(old_slots, old_index, slot_size);
    const size_t hash = hash_slot(hash_fn, src_slot);
    const FindInfo target = find_first_non_full(c, hash);
    total_probe_length += target.probe_length;
    const size_t new_i = target.offset;
    void* dst_slot = SlotAddress(new_slots, new_i, slot_size);
    SetCtrlInLargeTable(c, new_i, H2(hash), slot_size);
    transfer_n(&c, dst_slot, src_slot, 1);
  }
  return total_probe_length;
}

// The largest old capacity for which it is guaranteed that all probed elements
// fit in ProbedItemEncoder's local buffer.
// For such tables, `encode_probed_element` is trivial.
constexpr size_t kMaxLocalBufferOldCapacity =
    kProbedElementsBufferSize / sizeof(ProbedItem4Bytes) - 1;
static_assert(IsValidCapacity(kMaxLocalBufferOldCapacity));
constexpr size_t kMaxLocalBufferNewCapacity =
    NextCapacity(kMaxLocalBufferOldCapacity);
static_assert(kMaxLocalBufferNewCapacity <= ProbedItem4Bytes::kMaxNewCapacity);
static_assert(NextCapacity(kMaxLocalBufferNewCapacity) <=
              ProbedItem4Bytes::kMaxNewCapacity);

// Initializes mirrored control bytes after
// transfer_unprobed_elements_to_next_capacity.
void InitializeMirroredControlBytes(ctrl_t* new_ctrl, size_t new_capacity) {
  std::memcpy(new_ctrl + new_capacity,
              // We own GrowthInfo just before control bytes. So it is ok
              // to read one byte from it.
              new_ctrl - 1, Group::kWidth);
  new_ctrl[new_capacity] = ctrl_t::kSentinel;
}

// Encodes probed elements into available memory.
// At first, a local (on stack) buffer is used. The size of the buffer is
// kProbedElementsBufferSize bytes.
// When the local buffer is full, we switch to `control_` buffer. We are allowed
// to overwrite `control_` buffer till the `source_offset` byte. In case we have
// no space in `control_` buffer, we fallback to a naive algorithm for all the
// rest of the probed elements. We mark elements as kSentinel in control bytes
// and later process them fully. See ProcessMarkedElements for details. It
// should be extremely rare.
template <typename ProbedItemType,
          // If true, we only use the local buffer and never switch to the
          // control buffer.
          bool kGuaranteedFitToBuffer = false>
class ProbedItemEncoder {
 public:
  using ProbedItem = ProbedItemType;
  explicit ProbedItemEncoder(ctrl_t* control) : control_(control) {}

  // Encode item into the best available location.
  void EncodeItem(ProbedItem item) {
    if (ABSL_PREDICT_FALSE(!kGuaranteedFitToBuffer && pos_ >= end_)) {
      return ProcessEncodeWithOverflow(item);
    }
    ABSL_SWISSTABLE_ASSERT(pos_ < end_);
    *pos_ = item;
    ++pos_;
  }

  // Decodes information about probed elements from all available sources.
  // Finds new position for each element and transfers it to the new slots.
  // Returns the total probe length.
  size_t DecodeAndInsertToTable(CommonFields& common,
                                const PolicyFunctions& __restrict policy,
                                void* old_slots) const {
    if (pos_ == buffer_) {
      return 0;
    }
    if constexpr (kGuaranteedFitToBuffer) {
      return DecodeAndInsertImpl(common, policy, buffer_, pos_, old_slots);
    }
    size_t total_probe_length = DecodeAndInsertImpl(
        common, policy, buffer_,
        local_buffer_full_ ? buffer_ + kBufferSize : pos_, old_slots);
    if (!local_buffer_full_) {
      return total_probe_length;
    }
    total_probe_length +=
        DecodeAndInsertToTableOverflow(common, policy, old_slots);
    return total_probe_length;
  }

 private:
  static ProbedItem* AlignToNextItem(void* ptr) {
    return reinterpret_cast<ProbedItem*>(AlignUpTo(
        reinterpret_cast<uintptr_t>(ptr), alignof(ProbedItem)));
  }

  ProbedItem* OverflowBufferStart() const {
    // We reuse GrowthInfo memory as well.
    return AlignToNextItem(control_ - ControlOffset(/*has_infoz=*/false));
  }

  // Encodes item when previously allocated buffer is full.
  // At first that happens when local buffer is full.
  // We switch from the local buffer to the control buffer.
  // Every time this function is called, the available buffer is extended till
  // `item.source_offset` byte in the control buffer.
  // After the buffer is extended, this function wouldn't be called till the
  // buffer is exhausted.
  //
  // If there's no space in the control buffer, we fallback to naive algorithm
  // and mark probed elements as kSentinel in the control buffer. In this case,
  // we will call this function for every subsequent probed element.
  ABSL_ATTRIBUTE_NOINLINE void ProcessEncodeWithOverflow(ProbedItem item) {
    if (!local_buffer_full_) {
      local_buffer_full_ = true;
      pos_ = OverflowBufferStart();
    }
    const size_t source_offset = static_cast<size_t>(item.source_offset);
    // We are in fallback mode so we can't reuse control buffer anymore.
    // Probed elements are marked as kSentinel in the control buffer.
    if (ABSL_PREDICT_FALSE(marked_elements_starting_position_ !=
                           kNoMarkedElementsSentinel)) {
      control_[source_offset] = ctrl_t::kSentinel;
      return;
    }
    // Refresh the end pointer to the new available position.
    // Invariant: if pos < end, then we have at least sizeof(ProbedItem) bytes
    // to write.
    end_ = control_ + source_offset + 1 - sizeof(ProbedItem);
    if (ABSL_PREDICT_TRUE(pos_ < end_)) {
      *pos_ = item;
      ++pos_;
      return;
    }
    control_[source_offset] = ctrl_t::kSentinel;
    marked_elements_starting_position_ = source_offset;
    // Now we will always fall down to `ProcessEncodeWithOverflow`.
    ABSL_SWISSTABLE_ASSERT(pos_ >= end_);
  }

  // Decodes information about probed elements from control buffer and processes
  // marked elements.
  // Finds new position for each element and transfers it to the new slots.
  // Returns the total probe length.
  ABSL_ATTRIBUTE_NOINLINE size_t DecodeAndInsertToTableOverflow(
      CommonFields& common, const PolicyFunctions& __restrict policy,
      void* old_slots) const {
    ABSL_SWISSTABLE_ASSERT(local_buffer_full_ &&
                           "must not be called when local buffer is not full");
    size_t total_probe_length = DecodeAndInsertImpl(
        common, policy, OverflowBufferStart(), pos_, old_slots);
    if (ABSL_PREDICT_TRUE(marked_elements_starting_position_ ==
                          kNoMarkedElementsSentinel)) {
      return total_probe_length;
    }
    total_probe_length +=
        ProcessProbedMarkedElements(common, policy, control_, old_slots,
                                    marked_elements_starting_position_);
    return total_probe_length;
  }

  static constexpr size_t kBufferSize =
      kProbedElementsBufferSize / sizeof(ProbedItem);
  ProbedItem buffer_[kBufferSize];
  // If local_buffer_full_ is false, then pos_/end_ are in the local buffer,
  // otherwise, they're in the overflow buffer.
  ProbedItem* pos_ = buffer_;
  const void* end_ = buffer_ + kBufferSize;
  ctrl_t* const control_;
  size_t marked_elements_starting_position_ = kNoMarkedElementsSentinel;
  bool local_buffer_full_ = false;
};

// Grows to next capacity with specified encoder type.
// Encoder is used to store probed elements that are processed later.
// Different encoder is used depending on the capacity of the table.
// Returns total probe length.
template <typename Encoder>
size_t GrowToNextCapacity(CommonFields& common,
                          const PolicyFunctions& __restrict policy,
                          ctrl_t* old_ctrl, void* old_slots) {
  using ProbedItem = typename Encoder::ProbedItem;
  ABSL_SWISSTABLE_ASSERT(common.capacity() <= ProbedItem::kMaxNewCapacity);
  Encoder encoder(old_ctrl);
  policy.transfer_unprobed_elements_to_next_capacity(
      common, old_ctrl, old_slots, &encoder,
      [](void* probed_storage, h2_t h2, size_t source_offset, size_t h1) {
        auto encoder_ptr = static_cast<Encoder*>(probed_storage);
        encoder_ptr->EncodeItem(ProbedItem(h2, source_offset, h1));
      });
  InitializeMirroredControlBytes(common.control(), common.capacity());
  return encoder.DecodeAndInsertToTable(common, policy, old_slots);
}

// Grows to next capacity for relatively small tables so that even if all
// elements are probed, we don't need to overflow the local buffer.
// Returns total probe length.
size_t GrowToNextCapacityThatFitsInLocalBuffer(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    ctrl_t* old_ctrl, void* old_slots) {
  ABSL_SWISSTABLE_ASSERT(common.capacity() <= kMaxLocalBufferNewCapacity);
  return GrowToNextCapacity<
      ProbedItemEncoder<ProbedItem4Bytes, /*kGuaranteedFitToBuffer=*/true>>(
      common, policy, old_ctrl, old_slots);
}

// Grows to next capacity with different encodings. Returns total probe length.
// These functions are useful to simplify profile analysis.
size_t GrowToNextCapacity4BytesEncoder(CommonFields& common,
                                       const PolicyFunctions& __restrict policy,
                                       ctrl_t* old_ctrl, void* old_slots) {
  return GrowToNextCapacity<ProbedItemEncoder<ProbedItem4Bytes>>(
      common, policy, old_ctrl, old_slots);
}
size_t GrowToNextCapacity8BytesEncoder(CommonFields& common,
                                       const PolicyFunctions& __restrict policy,
                                       ctrl_t* old_ctrl, void* old_slots) {
  return GrowToNextCapacity<ProbedItemEncoder<ProbedItem8Bytes>>(
      common, policy, old_ctrl, old_slots);
}
size_t GrowToNextCapacity16BytesEncoder(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    ctrl_t* old_ctrl, void* old_slots) {
  return GrowToNextCapacity<ProbedItemEncoder<ProbedItem16Bytes>>(
      common, policy, old_ctrl, old_slots);
}

// Grows to next capacity for tables with relatively large capacity so that we
// can't guarantee that all probed elements fit in the local buffer. Returns
// total probe length.
size_t GrowToNextCapacityOverflowLocalBuffer(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    ctrl_t* old_ctrl, void* old_slots) {
  const size_t new_capacity = common.capacity();
  if (ABSL_PREDICT_TRUE(new_capacity <= ProbedItem4Bytes::kMaxNewCapacity)) {
    return GrowToNextCapacity4BytesEncoder(common, policy, old_ctrl, old_slots);
  }
  if (ABSL_PREDICT_TRUE(new_capacity <= ProbedItem8Bytes::kMaxNewCapacity)) {
    return GrowToNextCapacity8BytesEncoder(common, policy, old_ctrl, old_slots);
  }
  // 16 bytes encoding supports the maximum swisstable capacity.
  return GrowToNextCapacity16BytesEncoder(common, policy, old_ctrl, old_slots);
}

// Dispatches to the appropriate `GrowToNextCapacity*` function based on the
// capacity of the table. Returns total probe length.
ABSL_ATTRIBUTE_NOINLINE
size_t GrowToNextCapacityDispatch(CommonFields& common,
                                  const PolicyFunctions& __restrict policy,
                                  ctrl_t* old_ctrl, void* old_slots) {
  const size_t new_capacity = common.capacity();
  if (ABSL_PREDICT_TRUE(new_capacity <= kMaxLocalBufferNewCapacity)) {
    return GrowToNextCapacityThatFitsInLocalBuffer(common, policy, old_ctrl,
                                                   old_slots);
  } else {
    return GrowToNextCapacityOverflowLocalBuffer(common, policy, old_ctrl,
                                                 old_slots);
  }
}

void IncrementSmallSizeNonSoo(CommonFields& common,
                              const PolicyFunctions& __restrict policy) {
  ABSL_SWISSTABLE_ASSERT(common.is_small());
  common.increment_size();
  SanitizerUnpoisonMemoryRegion(common.slot_array(), policy.slot_size);
}

void IncrementSmallSize(CommonFields& common,
                        const PolicyFunctions& __restrict policy) {
  ABSL_SWISSTABLE_ASSERT(common.is_small());
  if (policy.soo_enabled) {
    common.set_full_soo();
  } else {
    IncrementSmallSizeNonSoo(common, policy);
  }
}

std::pair<ctrl_t*, void*> Grow1To3AndPrepareInsert(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    absl::FunctionRef<size_t()> get_hash) {
  // TODO(b/413062340): Refactor to reuse more code with
  // GrowSooTableToNextCapacityAndPrepareInsert.
  ABSL_SWISSTABLE_ASSERT(common.capacity() == 1);
  ABSL_SWISSTABLE_ASSERT(!common.empty());
  ABSL_SWISSTABLE_ASSERT(!policy.soo_enabled);
  constexpr size_t kOldCapacity = 1;
  constexpr size_t kNewCapacity = NextCapacity(kOldCapacity);
  ctrl_t* old_ctrl = common.control();
  void* old_slots = common.slot_array();

  common.set_capacity(kNewCapacity);
  const size_t slot_size = policy.slot_size;
  const size_t slot_align = policy.slot_align;
  void* alloc = policy.get_char_alloc(common);
  HashtablezInfoHandle infoz = common.infoz();
  const bool has_infoz = infoz.IsSampled();

  const auto [new_ctrl, new_slots] =
      AllocBackingArray(common, policy, kNewCapacity, has_infoz, alloc);
  common.set_control</*kGenerateSeed=*/true>(new_ctrl);
  common.set_slots(new_slots);
  SanitizerPoisonMemoryRegion(new_slots, kNewCapacity * slot_size);

  const size_t new_hash = get_hash();
  h2_t new_h2 = H2(new_hash);
  size_t orig_hash = policy.hash_slot(policy.hash_fn(common), old_slots);
  size_t offset = Resize1To3NewOffset(new_hash, common.seed());
  InitializeThreeElementsControlBytes(H2(orig_hash), new_h2, offset, new_ctrl);

  void* old_element_target = NextSlot(new_slots, slot_size);
  SanitizerUnpoisonMemoryRegion(old_element_target, slot_size);
  policy.transfer_n(&common, old_element_target, old_slots, 1);

  void* new_element_target_slot = SlotAddress(new_slots, offset, slot_size);
  SanitizerUnpoisonMemoryRegion(new_element_target_slot, slot_size);

  policy.dealloc(alloc, kOldCapacity, old_ctrl, slot_size, slot_align,
                 has_infoz);
  PrepareInsertCommon(common);
  ABSL_SWISSTABLE_ASSERT(common.size() == 2);
  GetGrowthInfoFromControl(new_ctrl).InitGrowthLeftNoDeleted(kNewCapacity - 2);

  if (ABSL_PREDICT_FALSE(has_infoz)) {
    ReportSingleGroupTableGrowthToInfoz(common, infoz, new_hash);
  }
  return {new_ctrl + offset, new_element_target_slot};
}

// Grows to next capacity and prepares insert for the given new_hash.
// Returns the offset of the new element.
size_t GrowToNextCapacityAndPrepareInsert(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    size_t new_hash) {
  ABSL_SWISSTABLE_ASSERT(common.growth_left() == 0);
  const size_t old_capacity = common.capacity();
  ABSL_SWISSTABLE_ASSERT(old_capacity > policy.soo_capacity());
  ABSL_SWISSTABLE_ASSERT(!IsSmallCapacity(old_capacity));

  const size_t new_capacity = NextCapacity(old_capacity);
  ctrl_t* old_ctrl = common.control();
  void* old_slots = common.slot_array();

  common.set_capacity(new_capacity);
  const size_t slot_size = policy.slot_size;
  const size_t slot_align = policy.slot_align;
  void* alloc = policy.get_char_alloc(common);
  HashtablezInfoHandle infoz = common.infoz();
  const bool has_infoz = infoz.IsSampled();

  const auto [new_ctrl, new_slots] =
      AllocBackingArray(common, policy, new_capacity, has_infoz, alloc);
  common.set_control</*kGenerateSeed=*/false>(new_ctrl);
  common.set_slots(new_slots);
  SanitizerPoisonMemoryRegion(new_slots, new_capacity * slot_size);

  h2_t new_h2 = H2(new_hash);
  size_t total_probe_length = 0;
  FindInfo find_info;
  if (ABSL_PREDICT_TRUE(is_single_group(new_capacity))) {
    size_t offset;
    GrowIntoSingleGroupShuffleControlBytes(old_ctrl, old_capacity, new_ctrl,
                                           new_capacity);
    // We put the new element either at the beginning or at the end of the
    // table with approximately equal probability.
    offset =
        SingleGroupTableH1(new_hash, common.seed()) & 1 ? 0 : new_capacity - 1;

    ABSL_SWISSTABLE_ASSERT(IsEmpty(new_ctrl[offset]));
    SetCtrlInSingleGroupTable(common, offset, new_h2, policy.slot_size);
    find_info = FindInfo{offset, 0};
    // Single group tables have all slots full on resize. So we can transfer
    // all slots without checking the control bytes.
    ABSL_SWISSTABLE_ASSERT(common.size() == old_capacity);
    void* target = NextSlot(new_slots, slot_size);
    SanitizerUnpoisonMemoryRegion(target, old_capacity * slot_size);
    policy.transfer_n(&common, target, old_slots, old_capacity);
  } else {
    total_probe_length =
        GrowToNextCapacityDispatch(common, policy, old_ctrl, old_slots);
    find_info = find_first_non_full(common, new_hash);
    SetCtrlInLargeTable(common, find_info.offset, new_h2, policy.slot_size);
  }
  ABSL_SWISSTABLE_ASSERT(old_capacity > policy.soo_capacity());
  (*policy.dealloc)(alloc, old_capacity, old_ctrl, slot_size, slot_align,
                    has_infoz);
  PrepareInsertCommon(common);
  ResetGrowthLeft(GetGrowthInfoFromControl(new_ctrl), new_capacity,
                  common.size());

  if (ABSL_PREDICT_FALSE(has_infoz)) {
    ReportGrowthToInfoz(common, infoz, new_hash, total_probe_length,
                        find_info.probe_length);
  }
  return find_info.offset;
}

}  // namespace

std::pair<ctrl_t*, void*> SmallNonSooPrepareInsert(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    absl::FunctionRef<size_t()> get_hash) {
  ABSL_SWISSTABLE_ASSERT(common.is_small());
  ABSL_SWISSTABLE_ASSERT(!policy.soo_enabled);
  if (common.capacity() == 1) {
    if (common.empty()) {
      IncrementSmallSizeNonSoo(common, policy);
      return {SooControl(), common.slot_array()};
    } else {
      return Grow1To3AndPrepareInsert(common, policy, get_hash);
    }
  }

  // Growing from 0 to 1 capacity.
  ABSL_SWISSTABLE_ASSERT(common.capacity() == 0);
  constexpr size_t kNewCapacity = 1;

  common.set_capacity(kNewCapacity);
  HashtablezInfoHandle infoz;
  const bool should_sample =
      policy.is_hashtablez_eligible && ShouldSampleNextTable();
  if (ABSL_PREDICT_FALSE(should_sample)) {
    infoz = ForcedTrySample(policy.slot_size, policy.key_size,
                            policy.value_size, policy.soo_capacity());
  }
  const bool has_infoz = infoz.IsSampled();
  void* alloc = policy.get_char_alloc(common);

  const auto [new_ctrl, new_slots] =
      AllocBackingArray(common, policy, kNewCapacity, has_infoz, alloc);
  // In small tables seed is not needed.
  common.set_control</*kGenerateSeed=*/false>(new_ctrl);
  common.set_slots(new_slots);

  static_assert(NextCapacity(0) == 1);
  PrepareInsertCommon(common);
  // TODO(b/413062340): maybe don't allocate growth info for capacity 1 tables.
  // Doing so may require additional branches/complexity so it might not be
  // worth it.
  GetGrowthInfoFromControl(new_ctrl).InitGrowthLeftNoDeleted(0);

  if (ABSL_PREDICT_FALSE(has_infoz)) {
    ReportSingleGroupTableGrowthToInfoz(common, infoz, get_hash());
  }
  return {SooControl(), new_slots};
}

namespace {

// Called whenever the table needs to vacate empty slots either by removing
// tombstones via rehash or growth to next capacity.
ABSL_ATTRIBUTE_NOINLINE
size_t RehashOrGrowToNextCapacityAndPrepareInsert(
    CommonFields& common, const PolicyFunctions& __restrict policy,
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
                               const PolicyFunctions& __restrict policy,
                               size_t hash) {
  const GrowthInfo growth_info = common.growth_info();
  ABSL_SWISSTABLE_ASSERT(!growth_info.HasNoDeletedAndGrowthLeft());
  if (ABSL_PREDICT_TRUE(growth_info.HasNoGrowthLeftAndNoDeleted())) {
    // Table without deleted slots (>95% cases) that needs to be resized.
    ABSL_SWISSTABLE_ASSERT(growth_info.HasNoDeleted() &&
                           growth_info.GetGrowthLeft() == 0);
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

// Resizes empty non-allocated SOO table to NextCapacity(SooCapacity()),
// forces the table to be sampled and prepares the insert.
// SOO tables need to switch from SOO to heap in order to store the infoz.
// Requires:
//   1. `c.capacity() == SooCapacity()`.
//   2. `c.empty()`.
ABSL_ATTRIBUTE_NOINLINE size_t
GrowEmptySooTableToNextCapacityForceSamplingAndPrepareInsert(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    size_t new_hash) {
  ResizeEmptyNonAllocatedTableImpl(common, policy, NextCapacity(SooCapacity()),
                                   /*force_infoz=*/true);
  PrepareInsertCommon(common);
  common.growth_info().OverwriteEmptyAsFull();
  SetCtrlInSingleGroupTable(common, SooSlotIndex(), H2(new_hash),
                            policy.slot_size);
  common.infoz().RecordInsert(new_hash, /*distance_from_desired=*/0);
  return SooSlotIndex();
}

// Resizes empty non-allocated table to the capacity to fit new_size elements.
// Requires:
//   1. `c.capacity() == policy.soo_capacity()`.
//   2. `c.empty()`.
//   3. `new_size > policy.soo_capacity()`.
// The table will be attempted to be sampled.
void ReserveEmptyNonAllocatedTableToFitNewSize(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    size_t new_size) {
  ValidateMaxSize(new_size, policy.slot_size);
  ABSL_ASSUME(new_size > 0);
  ResizeEmptyNonAllocatedTableImpl(common, policy, SizeToCapacity(new_size),
                                   /*force_infoz=*/false);
  // This is after resize, to ensure that we have completed the allocation
  // and have potentially sampled the hashtable.
  common.infoz().RecordReservation(new_size);
}

// Type erased version of raw_hash_set::reserve for tables that have an
// allocated backing array.
//
// Requires:
//   1. `c.capacity() > policy.soo_capacity()` OR `!c.empty()`.
// Reserving already allocated tables is considered to be a rare case.
ABSL_ATTRIBUTE_NOINLINE void ReserveAllocatedTable(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    size_t new_size) {
  const size_t cap = common.capacity();
  ValidateMaxSize(new_size, policy.slot_size);
  ABSL_ASSUME(new_size > 0);
  const size_t new_capacity = SizeToCapacity(new_size);
  if (cap == policy.soo_capacity()) {
    ABSL_SWISSTABLE_ASSERT(!common.empty());
    ResizeFullSooTable(common, policy, new_capacity,
                       ResizeFullSooTableSamplingMode::kNoSampling);
  } else {
    ABSL_SWISSTABLE_ASSERT(cap > policy.soo_capacity());
    // TODO(b/382423690): consider using GrowToNextCapacity, when applicable.
    ResizeAllocatedTableWithSeedChange(common, policy, new_capacity);
  }
  common.infoz().RecordReservation(new_size);
}

// As `ResizeFullSooTableToNextCapacity`, except that we also force the SOO
// table to be sampled. SOO tables need to switch from SOO to heap in order to
// store the infoz. No-op if sampling is disabled or not possible.
void GrowFullSooTableToNextCapacityForceSampling(
    CommonFields& common, const PolicyFunctions& __restrict policy) {
  AssertFullSoo(common, policy);
  ResizeFullSooTable(
      common, policy, NextCapacity(SooCapacity()),
      ResizeFullSooTableSamplingMode::kForceSampleNoResizeIfUnsampled);
}

}  // namespace

void* GetRefForEmptyClass(CommonFields& common) {
  // Empty base optimization typically make the empty base class address to be
  // the same as the first address of the derived class object.
  // But we generally assume that for empty classes we can return any valid
  // pointer.
  return &common;
}

void ResizeAllocatedTableWithSeedChange(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    size_t new_capacity) {
  ResizeNonSooImpl<ResizeNonSooMode::kGuaranteedAllocated>(
      common, policy, new_capacity, common.infoz());
}

void ReserveEmptyNonAllocatedTableToFitBucketCount(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    size_t bucket_count) {
  size_t new_capacity = NormalizeCapacity(bucket_count);
  ValidateMaxSize(CapacityToGrowth(new_capacity), policy.slot_size);
  ResizeEmptyNonAllocatedTableImpl(common, policy, new_capacity,
                                   /*force_infoz=*/false);
}

// Resizes a full SOO table to the NextCapacity(SooCapacity()).
template <size_t SooSlotMemcpySize, bool TransferUsesMemcpy>
size_t GrowSooTableToNextCapacityAndPrepareInsert(
    CommonFields& common, const PolicyFunctions& __restrict policy,
    size_t new_hash, ctrl_t soo_slot_ctrl) {
  AssertSoo(common, policy);
  if (ABSL_PREDICT_FALSE(soo_slot_ctrl == ctrl_t::kEmpty)) {
    // The table is empty, it is only used for forced sampling of SOO tables.
    return GrowEmptySooTableToNextCapacityForceSamplingAndPrepareInsert(
        common, policy, new_hash);
  }
  ABSL_SWISSTABLE_ASSERT(common.size() == policy.soo_capacity());
  static constexpr size_t kNewCapacity = NextCapacity(SooCapacity());
  const size_t slot_size = policy.slot_size;
  void* alloc = policy.get_char_alloc(common);
  common.set_capacity(kNewCapacity);

  // Since the table is not empty, it will not be sampled.
  // The decision to sample was already made during the first insertion.
  //
  // We do not set control and slots in CommonFields yet to avoid overriding
  // SOO data.
  const auto [new_ctrl, new_slots] = AllocBackingArray(
      common, policy, kNewCapacity, /*has_infoz=*/false, alloc);

  PrepareInsertCommon(common);
  ABSL_SWISSTABLE_ASSERT(common.size() == 2);
  GetGrowthInfoFromControl(new_ctrl).InitGrowthLeftNoDeleted(kNewCapacity - 2);
  common.generate_new_seed();

  const size_t offset = Resize1To3NewOffset(new_hash, common.seed());
  InitializeThreeElementsControlBytes(static_cast<h2_t>(soo_slot_ctrl),
                                      H2(new_hash), offset, new_ctrl);

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
    ABSL_SWISSTABLE_ASSERT(SooSlotMemcpySize <= 2 * slot_size);
    ABSL_SWISSTABLE_ASSERT(SooSlotMemcpySize >= slot_size);
    void* next_slot = SlotAddress(target_slot, 1, slot_size);
    SanitizerUnpoisonMemoryRegion(next_slot, SooSlotMemcpySize - slot_size);
    std::memcpy(target_slot, common.soo_data(), SooSlotMemcpySize);
    SanitizerPoisonMemoryRegion(next_slot, SooSlotMemcpySize - slot_size);
  } else {
    static_assert(SooSlotMemcpySize == 0);
    policy.transfer_n(&common, target_slot, common.soo_data(), 1);
  }
  // Seed was already generated above.
  common.set_control</*kGenerateSeed=*/false>(new_ctrl);
  common.set_slots(new_slots);

  // Full SOO table couldn't be sampled. If SOO table is sampled, it would
  // have been resized to the next capacity.
  ABSL_SWISSTABLE_ASSERT(!common.infoz().IsSampled());
  SanitizerUnpoisonMemoryRegion(SlotAddress(new_slots, offset, slot_size),
                                slot_size);
  return offset;
}

void Rehash(CommonFields& common, const PolicyFunctions& __restrict policy,
            size_t n) {
  const size_t cap = common.capacity();

  auto clear_backing_array = [&]() {
    ClearBackingArray(common, policy, policy.get_char_alloc(common),
                      /*reuse=*/false, policy.soo_enabled);
  };

  const size_t slot_size = policy.slot_size;

  if (n == 0) {
    if (cap <= policy.soo_capacity()) return;
    if (common.empty()) {
      clear_backing_array();
      return;
    }
    if (common.size() <= policy.soo_capacity()) {
      // When the table is already sampled, we keep it sampled.
      if (common.infoz().IsSampled()) {
        static constexpr size_t kInitialSampledCapacity =
            NextCapacity(SooCapacity());
        if (cap > kInitialSampledCapacity) {
          ResizeAllocatedTableWithSeedChange(common, policy,
                                             kInitialSampledCapacity);
        }
        // This asserts that we didn't lose sampling coverage in `resize`.
        ABSL_SWISSTABLE_ASSERT(common.infoz().IsSampled());
        return;
      }
      ABSL_SWISSTABLE_ASSERT(slot_size <= sizeof(HeapOrSoo));
      ABSL_SWISSTABLE_ASSERT(policy.slot_align <= alignof(HeapOrSoo));
      HeapOrSoo tmp_slot;
      size_t begin_offset = FindFirstFullSlot(0, cap, common.control());
      policy.transfer_n(
          &common, &tmp_slot,
          SlotAddress(common.slot_array(), begin_offset, slot_size), 1);
      clear_backing_array();
      policy.transfer_n(&common, common.soo_data(), &tmp_slot, 1);
      common.set_full_soo();
      return;
    }
  }

  ValidateMaxSize(n, policy.slot_size);
  // bitor is a faster way of doing `max` here. We will round up to the next
  // power-of-2-minus-1, so bitor is good enough.
  const size_t new_capacity =
      NormalizeCapacity(n | SizeToCapacity(common.size()));
  // n == 0 unconditionally rehashes as per the standard.
  if (n == 0 || new_capacity > cap) {
    if (cap == policy.soo_capacity()) {
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

void Copy(CommonFields& common, const PolicyFunctions& __restrict policy,
          const CommonFields& other,
          absl::FunctionRef<void(void*, const void*)> copy_fn) {
  const size_t size = other.size();
  ABSL_SWISSTABLE_ASSERT(size > 0);
  const size_t soo_capacity = policy.soo_capacity();
  const size_t slot_size = policy.slot_size;
  const bool soo_enabled = policy.soo_enabled;
  if (size == 1) {
    if (!soo_enabled) ReserveTableToFitNewSize(common, policy, 1);
    IncrementSmallSize(common, policy);
    const size_t other_capacity = other.capacity();
    const void* other_slot =
        other_capacity <= soo_capacity ? other.soo_data()
        : other.is_small()
            ? other.slot_array()
            : SlotAddress(other.slot_array(),
                          FindFirstFullSlot(0, other_capacity, other.control()),
                          slot_size);
    copy_fn(soo_enabled ? common.soo_data() : common.slot_array(), other_slot);

    if (soo_enabled && policy.is_hashtablez_eligible &&
        ShouldSampleNextTable()) {
      GrowFullSooTableToNextCapacityForceSampling(common, policy);
    }
    return;
  }

  ReserveTableToFitNewSize(common, policy, size);
  auto infoz = common.infoz();
  ABSL_SWISSTABLE_ASSERT(other.capacity() > soo_capacity);
  const size_t cap = common.capacity();
  ABSL_SWISSTABLE_ASSERT(cap > soo_capacity);
  // Note about single group tables:
  // 1. It is correct to have any order of elements.
  // 2. Order has to be non deterministic.
  // 3. We are assigning elements with arbitrary `shift` starting from
  //    `capacity + shift` position.
  // 4. `shift` must be coprime with `capacity + 1` in order to be able to use
  //     modular arithmetic to traverse all positions, instead of cycling
  //     through a subset of positions. Odd numbers are coprime with any
  //     `capacity + 1` (2^N).
  size_t offset = cap;
  const size_t shift = is_single_group(cap) ? (common.seed().seed() | 1) : 0;
  const void* hash_fn = policy.hash_fn(common);
  auto hasher = policy.hash_slot;
  IterateOverFullSlotsImpl(
      other, slot_size, [&](const ctrl_t* that_ctrl, void* that_slot) {
        if (shift == 0) {
          // Big tables case. Position must be searched via probing.
          // The table is guaranteed to be empty, so we can do faster than
          // a full `insert`.
          const size_t hash = (*hasher)(hash_fn, that_slot);
          FindInfo target = find_first_non_full(common, hash);
          infoz.RecordInsert(hash, target.probe_length);
          offset = target.offset;
        } else {
          // Small tables case. Next position is computed via shift.
          offset = (offset + shift) & cap;
        }
        const h2_t h2 = static_cast<h2_t>(*that_ctrl);
        // We rely on the hash not changing for small tables.
        ABSL_SWISSTABLE_ASSERT(
            H2((*hasher)(hash_fn, that_slot)) == h2 &&
            "hash function value changed unexpectedly during the copy");
        SetCtrl(common, offset, h2, slot_size);
        copy_fn(SlotAddress(common.slot_array(), offset, slot_size), that_slot);
        common.maybe_increment_generation_on_insert();
      });
  if (shift != 0) {
    // On small table copy we do not record individual inserts.
    // RecordInsert requires hash, but it is unknown for small tables.
    infoz.RecordStorageChanged(size, cap);
  }
  common.increment_size(size);
  common.growth_info().OverwriteManyEmptyAsFull(size);
}

void ReserveTableToFitNewSize(CommonFields& common,
                              const PolicyFunctions& __restrict policy,
                              size_t new_size) {
  common.reset_reserved_growth(new_size);
  common.set_reservation_size(new_size);
  ABSL_SWISSTABLE_ASSERT(new_size > policy.soo_capacity());
  const size_t cap = common.capacity();
  if (ABSL_PREDICT_TRUE(common.empty() && cap <= policy.soo_capacity())) {
    return ReserveEmptyNonAllocatedTableToFitNewSize(common, policy, new_size);
  }

  ABSL_SWISSTABLE_ASSERT(!common.empty() || cap > policy.soo_capacity());
  ABSL_SWISSTABLE_ASSERT(cap > 0);
  const size_t max_size_before_growth =
      cap <= policy.soo_capacity() ? policy.soo_capacity()
                                   : common.size() + common.growth_left();
  if (new_size <= max_size_before_growth) {
    return;
  }
  ReserveAllocatedTable(common, policy, new_size);
}

size_t PrepareInsertNonSoo(CommonFields& common,
                           const PolicyFunctions& __restrict policy,
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

// Extern template instantiation for inline function.
template size_t TryFindNewIndexWithoutProbing(size_t h1, size_t old_index,
                                              size_t old_capacity,
                                              ctrl_t* new_ctrl,
                                              size_t new_capacity);

// We need to instantiate ALL possible template combinations because we define
// the function in the cc file.
template size_t GrowSooTableToNextCapacityAndPrepareInsert<0, false>(
    CommonFields&, const PolicyFunctions&, size_t, ctrl_t);
template size_t GrowSooTableToNextCapacityAndPrepareInsert<
    OptimalMemcpySizeForSooSlotTransfer(1), true>(CommonFields&,
                                                  const PolicyFunctions&,
                                                  size_t, ctrl_t);

static_assert(VerifyOptimalMemcpySizeForSooSlotTransferRange(2, 3));
template size_t GrowSooTableToNextCapacityAndPrepareInsert<
    OptimalMemcpySizeForSooSlotTransfer(3), true>(CommonFields&,
                                                  const PolicyFunctions&,
                                                  size_t, ctrl_t);

static_assert(VerifyOptimalMemcpySizeForSooSlotTransferRange(4, 8));
template size_t GrowSooTableToNextCapacityAndPrepareInsert<
    OptimalMemcpySizeForSooSlotTransfer(8), true>(CommonFields&,
                                                  const PolicyFunctions&,
                                                  size_t, ctrl_t);

#if UINTPTR_MAX == UINT32_MAX
static_assert(MaxSooSlotSize() == 8);
#else
static_assert(VerifyOptimalMemcpySizeForSooSlotTransferRange(9, 16));
template size_t GrowSooTableToNextCapacityAndPrepareInsert<
    OptimalMemcpySizeForSooSlotTransfer(16), true>(CommonFields&,
                                                   const PolicyFunctions&,
                                                   size_t, ctrl_t);
static_assert(MaxSooSlotSize() == 16);
#endif

}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl
