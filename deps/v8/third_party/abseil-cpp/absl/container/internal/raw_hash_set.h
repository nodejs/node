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
//
// An open-addressing
// hashtable with quadratic probing.
//
// This is a low level hashtable on top of which different interfaces can be
// implemented, like flat_hash_set, node_hash_set, string_hash_set, etc.
//
// The table interface is similar to that of std::unordered_set. Notable
// differences are that most member functions support heterogeneous keys when
// BOTH the hash and eq functions are marked as transparent. They do so by
// providing a typedef called `is_transparent`.
//
// When heterogeneous lookup is enabled, functions that take key_type act as if
// they have an overload set like:
//
//   iterator find(const key_type& key);
//   template <class K>
//   iterator find(const K& key);
//
//   size_type erase(const key_type& key);
//   template <class K>
//   size_type erase(const K& key);
//
//   std::pair<iterator, iterator> equal_range(const key_type& key);
//   template <class K>
//   std::pair<iterator, iterator> equal_range(const K& key);
//
// When heterogeneous lookup is disabled, only the explicit `key_type` overloads
// exist.
//
// In addition the pointer to element and iterator stability guarantees are
// weaker: all iterators and pointers are invalidated after a new element is
// inserted.
//
// IMPLEMENTATION DETAILS
//
// # Table Layout
//
// A raw_hash_set's backing array consists of control bytes followed by slots
// that may or may not contain objects.
//
// The layout of the backing array, for `capacity` slots, is thus, as a
// pseudo-struct:
//
//   struct BackingArray {
//     // Sampling handler. This field isn't present when the sampling is
//     // disabled or this allocation hasn't been selected for sampling.
//     HashtablezInfoHandle infoz_;
//     // The number of elements we can insert before growing the capacity.
//     size_t growth_left;
//     // Control bytes for the "real" slots.
//     ctrl_t ctrl[capacity];
//     // Always `ctrl_t::kSentinel`. This is used by iterators to find when to
//     // stop and serves no other purpose.
//     ctrl_t sentinel;
//     // A copy of the first `kWidth - 1` elements of `ctrl`. This is used so
//     // that if a probe sequence picks a value near the end of `ctrl`,
//     // `Group` will have valid control bytes to look at.
//     ctrl_t clones[kWidth - 1];
//     // The actual slot data.
//     slot_type slots[capacity];
//   };
//
// The length of this array is computed by `RawHashSetLayout::alloc_size` below.
//
// Control bytes (`ctrl_t`) are bytes (collected into groups of a
// platform-specific size) that define the state of the corresponding slot in
// the slot array. Group manipulation is tightly optimized to be as efficient
// as possible: SSE and friends on x86, clever bit operations on other arches.
//
//      Group 1         Group 2        Group 3
// +---------------+---------------+---------------+
// | | | | | | | | | | | | | | | | | | | | | | | | |
// +---------------+---------------+---------------+
//
// Each control byte is either a special value for empty slots, deleted slots
// (sometimes called *tombstones*), and a special end-of-table marker used by
// iterators, or, if occupied, seven bits (H2) from the hash of the value in the
// corresponding slot.
//
// Storing control bytes in a separate array also has beneficial cache effects,
// since more logical slots will fit into a cache line.
//
// # Small Object Optimization (SOO)
//
// When the size/alignment of the value_type and the capacity of the table are
// small, we enable small object optimization and store the values inline in
// the raw_hash_set object. This optimization allows us to avoid
// allocation/deallocation as well as cache/dTLB misses.
//
// # Hashing
//
// We compute two separate hashes, `H1` and `H2`, from the hash of an object.
// `H1(hash(x))` is an index into `slots`, and essentially the starting point
// for the probe sequence. `H2(hash(x))` is a 7-bit value used to filter out
// objects that cannot possibly be the one we are looking for.
//
// # Table operations.
//
// The key operations are `insert`, `find`, and `erase`.
//
// Since `insert` and `erase` are implemented in terms of `find`, we describe
// `find` first. To `find` a value `x`, we compute `hash(x)`. From
// `H1(hash(x))` and the capacity, we construct a `probe_seq` that visits every
// group of slots in some interesting order.
//
// We now walk through these indices. At each index, we select the entire group
// starting with that index and extract potential candidates: occupied slots
// with a control byte equal to `H2(hash(x))`. If we find an empty slot in the
// group, we stop and return an error. Each candidate slot `y` is compared with
// `x`; if `x == y`, we are done and return `&y`; otherwise we continue to the
// next probe index. Tombstones effectively behave like full slots that never
// match the value we're looking for.
//
// The `H2` bits ensure when we compare a slot to an object with `==`, we are
// likely to have actually found the object.  That is, the chance is low that
// `==` is called and returns `false`.  Thus, when we search for an object, we
// are unlikely to call `==` many times.  This likelyhood can be analyzed as
// follows (assuming that H2 is a random enough hash function).
//
// Let's assume that there are `k` "wrong" objects that must be examined in a
// probe sequence.  For example, when doing a `find` on an object that is in the
// table, `k` is the number of objects between the start of the probe sequence
// and the final found object (not including the final found object).  The
// expected number of objects with an H2 match is then `k/128`.  Measurements
// and analysis indicate that even at high load factors, `k` is less than 32,
// meaning that the number of "false positive" comparisons we must perform is
// less than 1/8 per `find`.

// `insert` is implemented in terms of `unchecked_insert`, which inserts a
// value presumed to not be in the table (violating this requirement will cause
// the table to behave erratically). Given `x` and its hash `hash(x)`, to insert
// it, we construct a `probe_seq` once again, and use it to find the first
// group with an unoccupied (empty *or* deleted) slot. We place `x` into the
// first such slot in the group and mark it as full with `x`'s H2.
//
// To `insert`, we compose `unchecked_insert` with `find`. We compute `h(x)` and
// perform a `find` to see if it's already present; if it is, we're done. If
// it's not, we may decide the table is getting overcrowded (i.e. the load
// factor is greater than 7/8 for big tables; tables smaller than one probing
// group use a max load factor of 1); in this case, we allocate a bigger array,
// `unchecked_insert` each element of the table into the new array (we know that
// no insertion here will insert an already-present value), and discard the old
// backing array. At this point, we may `unchecked_insert` the value `x`.
//
// Below, `unchecked_insert` is partly implemented by `prepare_insert`, which
// presents a viable, initialized slot pointee to the caller.
//
// `erase` is implemented in terms of `erase_at`, which takes an index to a
// slot. Given an offset, we simply create a tombstone and destroy its contents.
// If we can prove that the slot would not appear in a probe sequence, we can
// make the slot as empty, instead. We can prove this by observing that if a
// group has any empty slots, it has never been full (assuming we never create
// an empty slot in a group with no empties, which this heuristic guarantees we
// never do) and find would stop at this group anyways (since it does not probe
// beyond groups with empties).
//
// `erase` is `erase_at` composed with `find`: if we
// have a value `x`, we can perform a `find`, and then `erase_at` the resulting
// slot.
//
// To iterate, we simply traverse the array, skipping empty and deleted slots
// and stopping when we hit a `kSentinel`.

#ifndef ABSL_CONTAINER_INTERNAL_RAW_HASH_SET_H_
#define ABSL_CONTAINER_INTERNAL_RAW_HASH_SET_H_

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/endian.h"
#include "absl/base/internal/iterator_traits.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/base/options.h"
#include "absl/base/port.h"
#include "absl/base/prefetch.h"
#include "absl/container/internal/common.h"  // IWYU pragma: export // for node_handle
#include "absl/container/internal/common_policy_traits.h"
#include "absl/container/internal/compressed_tuple.h"
#include "absl/container/internal/container_memory.h"
#include "absl/container/internal/hash_function_defaults.h"
#include "absl/container/internal/hash_policy_traits.h"
#include "absl/container/internal/hashtable_control_bytes.h"
#include "absl/container/internal/hashtable_debug_hooks.h"
#include "absl/container/internal/hashtablez_sampler.h"
#include "absl/functional/function_ref.h"
#include "absl/hash/hash.h"
#include "absl/hash/internal/weakly_mixed_integer.h"
#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"
#include "absl/numeric/bits.h"
#include "absl/utility/utility.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {

#ifdef ABSL_SWISSTABLE_ENABLE_GENERATIONS
#error ABSL_SWISSTABLE_ENABLE_GENERATIONS cannot be directly set
#elif (defined(ABSL_HAVE_ADDRESS_SANITIZER) ||   \
       defined(ABSL_HAVE_HWADDRESS_SANITIZER) || \
       defined(ABSL_HAVE_MEMORY_SANITIZER)) &&   \
    !defined(NDEBUG_SANITIZER)  // If defined, performance is important.
// When compiled in sanitizer mode, we add generation integers to the backing
// array and iterators. In the backing array, we store the generation between
// the control bytes and the slots. When iterators are dereferenced, we assert
// that the container has not been mutated in a way that could cause iterator
// invalidation since the iterator was initialized.
#define ABSL_SWISSTABLE_ENABLE_GENERATIONS
#endif

#ifdef ABSL_SWISSTABLE_ASSERT
#error ABSL_SWISSTABLE_ASSERT cannot be directly set
#else
// We use this macro for assertions that users may see when the table is in an
// invalid state that sanitizers may help diagnose.
#define ABSL_SWISSTABLE_ASSERT(CONDITION) \
  assert((CONDITION) && "Try enabling sanitizers.")
#endif

// We use uint8_t so we don't need to worry about padding.
using GenerationType = uint8_t;

// A sentinel value for empty generations. Using 0 makes it easy to constexpr
// initialize an array of this value.
constexpr GenerationType SentinelEmptyGeneration() { return 0; }

constexpr GenerationType NextGeneration(GenerationType generation) {
  return ++generation == SentinelEmptyGeneration() ? ++generation : generation;
}

#ifdef ABSL_SWISSTABLE_ENABLE_GENERATIONS
constexpr bool SwisstableGenerationsEnabled() { return true; }
constexpr size_t NumGenerationBytes() { return sizeof(GenerationType); }
#else
constexpr bool SwisstableGenerationsEnabled() { return false; }
constexpr size_t NumGenerationBytes() { return 0; }
#endif

// Returns true if we should assert that the table is not accessed after it has
// been destroyed or during the destruction of the table.
constexpr bool SwisstableAssertAccessToDestroyedTable() {
#ifndef NDEBUG
  return true;
#endif
  return SwisstableGenerationsEnabled();
}

template <typename AllocType>
void SwapAlloc(AllocType& lhs, AllocType& rhs,
               std::true_type /* propagate_on_container_swap */) {
  using std::swap;
  swap(lhs, rhs);
}
template <typename AllocType>
void SwapAlloc(AllocType& lhs, AllocType& rhs,
               std::false_type /* propagate_on_container_swap */) {
  (void)lhs;
  (void)rhs;
  assert(lhs == rhs &&
         "It's UB to call swap with unequal non-propagating allocators.");
}

template <typename AllocType>
void CopyAlloc(AllocType& lhs, AllocType& rhs,
               std::true_type /* propagate_alloc */) {
  lhs = rhs;
}
template <typename AllocType>
void CopyAlloc(AllocType&, AllocType&, std::false_type /* propagate_alloc */) {}

// The state for a probe sequence.
//
// Currently, the sequence is a triangular progression of the form
//
//   p(i) := Width * (i^2 + i)/2 + hash (mod mask + 1)
//
// The use of `Width` ensures that each probe step does not overlap groups;
// the sequence effectively outputs the addresses of *groups* (although not
// necessarily aligned to any boundary). The `Group` machinery allows us
// to check an entire group with minimal branching.
//
// Wrapping around at `mask + 1` is important, but not for the obvious reason.
// As described above, the first few entries of the control byte array
// are mirrored at the end of the array, which `Group` will find and use
// for selecting candidates. However, when those candidates' slots are
// actually inspected, there are no corresponding slots for the cloned bytes,
// so we need to make sure we've treated those offsets as "wrapping around".
//
// It turns out that this probe sequence visits every group exactly once if the
// number of groups is a power of two, since (i^2+i)/2 is a bijection in
// Z/(2^m). See https://en.wikipedia.org/wiki/Quadratic_probing
template <size_t Width>
class probe_seq {
 public:
  // Creates a new probe sequence using `hash` as the initial value of the
  // sequence and `mask` (usually the capacity of the table) as the mask to
  // apply to each value in the progression.
  probe_seq(size_t hash, size_t mask) {
    ABSL_SWISSTABLE_ASSERT(((mask + 1) & mask) == 0 && "not a mask");
    mask_ = mask;
    offset_ = hash & mask_;
  }

  // The offset within the table, i.e., the value `p(i)` above.
  size_t offset() const { return offset_; }
  size_t offset(size_t i) const { return (offset_ + i) & mask_; }

  void next() {
    index_ += Width;
    offset_ += index_;
    offset_ &= mask_;
  }
  // 0-based probe index, a multiple of `Width`.
  size_t index() const { return index_; }

 private:
  size_t mask_;
  size_t offset_;
  size_t index_ = 0;
};

template <class ContainerKey, class Hash, class Eq>
struct RequireUsableKey {
  template <class PassedKey, class... Args>
  std::pair<
      decltype(std::declval<const Hash&>()(std::declval<const PassedKey&>())),
      decltype(std::declval<const Eq&>()(std::declval<const ContainerKey&>(),
                                         std::declval<const PassedKey&>()))>*
  operator()(const PassedKey&, const Args&...) const;
};

template <class E, class Policy, class Hash, class Eq, class... Ts>
struct IsDecomposable : std::false_type {};

template <class Policy, class Hash, class Eq, class... Ts>
struct IsDecomposable<
    absl::void_t<decltype(Policy::apply(
        RequireUsableKey<typename Policy::key_type, Hash, Eq>(),
        std::declval<Ts>()...))>,
    Policy, Hash, Eq, Ts...> : std::true_type {};

// TODO(alkis): Switch to std::is_nothrow_swappable when gcc/clang supports it.
template <class T>
constexpr bool IsNoThrowSwappable(std::true_type = {} /* is_swappable */) {
  using std::swap;
  return noexcept(swap(std::declval<T&>(), std::declval<T&>()));
}
template <class T>
constexpr bool IsNoThrowSwappable(std::false_type /* is_swappable */) {
  return false;
}

ABSL_DLL extern ctrl_t kDefaultIterControl;

// We use these sentinel capacity values in debug mode to indicate different
// classes of bugs.
enum InvalidCapacity : size_t {
  kAboveMaxValidCapacity = ~size_t{} - 100,
  kReentrance,
  kDestroyed,

  // These two must be last because we use `>= kMovedFrom` to mean moved-from.
  kMovedFrom,
  kSelfMovedFrom,
};

// Returns a pointer to a control byte that can be used by default-constructed
// iterators. We don't expect this pointer to be dereferenced.
inline ctrl_t* DefaultIterControl() { return &kDefaultIterControl; }

// For use in SOO iterators.
// TODO(b/289225379): we could potentially get rid of this by adding an is_soo
// bit in iterators. This would add branches but reduce cache misses.
ABSL_DLL extern const ctrl_t kSooControl[17];

// Returns a pointer to a full byte followed by a sentinel byte.
inline ctrl_t* SooControl() {
  // Const must be cast away here; no uses of this function will actually write
  // to it because it is only used for SOO iterators.
  return const_cast<ctrl_t*>(kSooControl);
}
// Whether ctrl is from the SooControl array.
inline bool IsSooControl(const ctrl_t* ctrl) { return ctrl == SooControl(); }

// Returns a pointer to a generation to use for an empty hashtable.
GenerationType* EmptyGeneration();

// Returns whether `generation` is a generation for an empty hashtable that
// could be returned by EmptyGeneration().
inline bool IsEmptyGeneration(const GenerationType* generation) {
  return *generation == SentinelEmptyGeneration();
}

// We only allow a maximum of 1 SOO element, which makes the implementation
// much simpler. Complications with multiple SOO elements include:
// - Satisfying the guarantee that erasing one element doesn't invalidate
//   iterators to other elements means we would probably need actual SOO
//   control bytes.
// - In order to prevent user code from depending on iteration order for small
//   tables, we would need to randomize the iteration order somehow.
constexpr size_t SooCapacity() { return 1; }
// Sentinel type to indicate SOO CommonFields construction.
struct soo_tag_t {};
// Sentinel type to indicate SOO CommonFields construction with full size.
struct full_soo_tag_t {};
// Sentinel type to indicate non-SOO CommonFields construction.
struct non_soo_tag_t {};
// Sentinel value to indicate an uninitialized value explicitly.
struct uninitialized_tag_t {};
// Sentinel value to indicate creation of an empty table without a seed.
struct no_seed_empty_tag_t {};

// Per table hash salt. This gets mixed into H1 to randomize iteration order
// per-table.
// The seed is needed to ensure non-determinism of iteration order.
class PerTableSeed {
 public:
  // The number of bits in the seed.
  // It is big enough to ensure non-determinism of iteration order.
  // We store the seed inside a uint64_t together with size and other metadata.
  // Using 16 bits allows us to save one `and` instruction in H1 (we use movzwl
  // instead of movq+and).
  static constexpr size_t kBitCount = 16;

  // Returns the seed for the table. Only the lowest kBitCount are non zero.
  size_t seed() const { return seed_; }

 private:
  friend class HashtableSize;
  explicit PerTableSeed(size_t seed) : seed_(seed) {}

  const size_t seed_;
};

// Returns next per-table seed.
inline uint16_t NextSeed() {
  static_assert(PerTableSeed::kBitCount == 16);
  thread_local uint16_t seed =
      static_cast<uint16_t>(reinterpret_cast<uintptr_t>(&seed));
  seed += uint16_t{0xad53};
  return seed;
}

// The size and also has additionally
// 1) one bit that stores whether we have infoz.
// 2) PerTableSeed::kBitCount bits for the seed.
class HashtableSize {
 public:
  static constexpr size_t kSizeBitCount = 64 - PerTableSeed::kBitCount - 1;

  explicit HashtableSize(uninitialized_tag_t) {}
  explicit HashtableSize(no_seed_empty_tag_t) : data_(0) {}
  explicit HashtableSize(full_soo_tag_t) : data_(kSizeOneNoMetadata) {}

  // Returns actual size of the table.
  size_t size() const { return static_cast<size_t>(data_ >> kSizeShift); }
  void increment_size() { data_ += kSizeOneNoMetadata; }
  void increment_size(size_t size) {
    data_ += static_cast<uint64_t>(size) * kSizeOneNoMetadata;
  }
  void decrement_size() { data_ -= kSizeOneNoMetadata; }
  // Returns true if the table is empty.
  bool empty() const { return data_ < kSizeOneNoMetadata; }
  // Sets the size to zero, but keeps all the metadata bits.
  void set_size_to_zero_keep_metadata() { data_ = data_ & kMetadataMask; }

  PerTableSeed seed() const {
    return PerTableSeed(static_cast<size_t>(data_) & kSeedMask);
  }

  void generate_new_seed() {
    data_ = (data_ & ~kSeedMask) ^ uint64_t{NextSeed()};
  }

  // Returns true if the table has infoz.
  bool has_infoz() const {
    return ABSL_PREDICT_FALSE((data_ & kHasInfozMask) != 0);
  }

  // Sets the has_infoz bit.
  void set_has_infoz() { data_ |= kHasInfozMask; }

  void set_no_seed_for_testing() { data_ &= ~kSeedMask; }

 private:
  static constexpr size_t kSizeShift = 64 - kSizeBitCount;
  static constexpr uint64_t kSizeOneNoMetadata = uint64_t{1} << kSizeShift;
  static constexpr uint64_t kMetadataMask = kSizeOneNoMetadata - 1;
  static constexpr uint64_t kSeedMask =
      (uint64_t{1} << PerTableSeed::kBitCount) - 1;
  // The next bit after the seed.
  static constexpr uint64_t kHasInfozMask = kSeedMask + 1;
  uint64_t data_;
};

// Extracts the H1 portion of a hash: 57 bits mixed with a per-table seed.
inline size_t H1(size_t hash, PerTableSeed seed) {
  return (hash >> 7) ^ seed.seed();
}

// Extracts the H2 portion of a hash: the 7 bits not used for H1.
//
// These are used as an occupied control byte.
inline h2_t H2(size_t hash) { return hash & 0x7F; }

// When there is an insertion with no reserved growth, we rehash with
// probability `min(1, RehashProbabilityConstant() / capacity())`. Using a
// constant divided by capacity ensures that inserting N elements is still O(N)
// in the average case. Using the constant 16 means that we expect to rehash ~8
// times more often than when generations are disabled. We are adding expected
// rehash_probability * #insertions/capacity_growth = 16/capacity * ((7/8 -
// 7/16) * capacity)/capacity_growth = ~7 extra rehashes per capacity growth.
inline size_t RehashProbabilityConstant() { return 16; }

class CommonFieldsGenerationInfoEnabled {
  // A sentinel value for reserved_growth_ indicating that we just ran out of
  // reserved growth on the last insertion. When reserve is called and then
  // insertions take place, reserved_growth_'s state machine is N, ..., 1,
  // kReservedGrowthJustRanOut, 0.
  static constexpr size_t kReservedGrowthJustRanOut =
      (std::numeric_limits<size_t>::max)();

 public:
  CommonFieldsGenerationInfoEnabled() = default;
  CommonFieldsGenerationInfoEnabled(CommonFieldsGenerationInfoEnabled&& that)
      : reserved_growth_(that.reserved_growth_),
        reservation_size_(that.reservation_size_),
        generation_(that.generation_) {
    that.reserved_growth_ = 0;
    that.reservation_size_ = 0;
    that.generation_ = EmptyGeneration();
  }
  CommonFieldsGenerationInfoEnabled& operator=(
      CommonFieldsGenerationInfoEnabled&&) = default;

  // Whether we should rehash on insert in order to detect bugs of using invalid
  // references. We rehash on the first insertion after reserved_growth_ reaches
  // 0 after a call to reserve. We also do a rehash with low probability
  // whenever reserved_growth_ is zero.
  bool should_rehash_for_bug_detection_on_insert(PerTableSeed seed,
                                                 size_t capacity) const;
  // Similar to above, except that we don't depend on reserved_growth_.
  bool should_rehash_for_bug_detection_on_move(PerTableSeed seed,
                                               size_t capacity) const;
  void maybe_increment_generation_on_insert() {
    if (reserved_growth_ == kReservedGrowthJustRanOut) reserved_growth_ = 0;

    if (reserved_growth_ > 0) {
      if (--reserved_growth_ == 0) reserved_growth_ = kReservedGrowthJustRanOut;
    } else {
      increment_generation();
    }
  }
  void increment_generation() { *generation_ = NextGeneration(*generation_); }
  void reset_reserved_growth(size_t reservation, size_t size) {
    reserved_growth_ = reservation - size;
  }
  size_t reserved_growth() const { return reserved_growth_; }
  void set_reserved_growth(size_t r) { reserved_growth_ = r; }
  size_t reservation_size() const { return reservation_size_; }
  void set_reservation_size(size_t r) { reservation_size_ = r; }
  GenerationType generation() const { return *generation_; }
  void set_generation(GenerationType g) { *generation_ = g; }
  GenerationType* generation_ptr() const { return generation_; }
  void set_generation_ptr(GenerationType* g) { generation_ = g; }

 private:
  // The number of insertions remaining that are guaranteed to not rehash due to
  // a prior call to reserve. Note: we store reserved growth in addition to
  // reservation size because calls to erase() decrease size_ but don't decrease
  // reserved growth.
  size_t reserved_growth_ = 0;
  // The maximum argument to reserve() since the container was cleared. We need
  // to keep track of this, in addition to reserved growth, because we reset
  // reserved growth to this when erase(begin(), end()) is called.
  size_t reservation_size_ = 0;
  // Pointer to the generation counter, which is used to validate iterators and
  // is stored in the backing array between the control bytes and the slots.
  // Note that we can't store the generation inside the container itself and
  // keep a pointer to the container in the iterators because iterators must
  // remain valid when the container is moved.
  // Note: we could derive this pointer from the control pointer, but it makes
  // the code more complicated, and there's a benefit in having the sizes of
  // raw_hash_set in sanitizer mode and non-sanitizer mode a bit more different,
  // which is that tests are less likely to rely on the size remaining the same.
  GenerationType* generation_ = EmptyGeneration();
};

class CommonFieldsGenerationInfoDisabled {
 public:
  CommonFieldsGenerationInfoDisabled() = default;
  CommonFieldsGenerationInfoDisabled(CommonFieldsGenerationInfoDisabled&&) =
      default;
  CommonFieldsGenerationInfoDisabled& operator=(
      CommonFieldsGenerationInfoDisabled&&) = default;

  bool should_rehash_for_bug_detection_on_insert(PerTableSeed, size_t) const {
    return false;
  }
  bool should_rehash_for_bug_detection_on_move(PerTableSeed, size_t) const {
    return false;
  }
  void maybe_increment_generation_on_insert() {}
  void increment_generation() {}
  void reset_reserved_growth(size_t, size_t) {}
  size_t reserved_growth() const { return 0; }
  void set_reserved_growth(size_t) {}
  size_t reservation_size() const { return 0; }
  void set_reservation_size(size_t) {}
  GenerationType generation() const { return 0; }
  void set_generation(GenerationType) {}
  GenerationType* generation_ptr() const { return nullptr; }
  void set_generation_ptr(GenerationType*) {}
};

class HashSetIteratorGenerationInfoEnabled {
 public:
  HashSetIteratorGenerationInfoEnabled() = default;
  explicit HashSetIteratorGenerationInfoEnabled(
      const GenerationType* generation_ptr)
      : generation_ptr_(generation_ptr), generation_(*generation_ptr) {}

  GenerationType generation() const { return generation_; }
  void reset_generation() { generation_ = *generation_ptr_; }
  const GenerationType* generation_ptr() const { return generation_ptr_; }
  void set_generation_ptr(const GenerationType* ptr) { generation_ptr_ = ptr; }

 private:
  const GenerationType* generation_ptr_ = EmptyGeneration();
  GenerationType generation_ = *generation_ptr_;
};

class HashSetIteratorGenerationInfoDisabled {
 public:
  HashSetIteratorGenerationInfoDisabled() = default;
  explicit HashSetIteratorGenerationInfoDisabled(const GenerationType*) {}

  GenerationType generation() const { return 0; }
  void reset_generation() {}
  const GenerationType* generation_ptr() const { return nullptr; }
  void set_generation_ptr(const GenerationType*) {}
};

#ifdef ABSL_SWISSTABLE_ENABLE_GENERATIONS
using CommonFieldsGenerationInfo = CommonFieldsGenerationInfoEnabled;
using HashSetIteratorGenerationInfo = HashSetIteratorGenerationInfoEnabled;
#else
using CommonFieldsGenerationInfo = CommonFieldsGenerationInfoDisabled;
using HashSetIteratorGenerationInfo = HashSetIteratorGenerationInfoDisabled;
#endif

// Stored the information regarding number of slots we can still fill
// without needing to rehash.
//
// We want to ensure sufficient number of empty slots in the table in order
// to keep probe sequences relatively short. Empty slot in the probe group
// is required to stop probing.
//
// Tombstones (kDeleted slots) are not included in the growth capacity,
// because we'd like to rehash when the table is filled with tombstones and/or
// full slots.
//
// GrowthInfo also stores a bit that encodes whether table may have any
// deleted slots.
// Most of the tables (>95%) have no deleted slots, so some functions can
// be more efficient with this information.
//
// Callers can also force a rehash via the standard `rehash(0)`,
// which will recompute this value as a side-effect.
//
// See also `CapacityToGrowth()`.
class GrowthInfo {
 public:
  // Leaves data member uninitialized.
  GrowthInfo() = default;

  // Initializes the GrowthInfo assuming we can grow `growth_left` elements
  // and there are no kDeleted slots in the table.
  void InitGrowthLeftNoDeleted(size_t growth_left) {
    growth_left_info_ = growth_left;
  }

  // Overwrites single full slot with an empty slot.
  void OverwriteFullAsEmpty() { ++growth_left_info_; }

  // Overwrites single empty slot with a full slot.
  void OverwriteEmptyAsFull() {
    ABSL_SWISSTABLE_ASSERT(GetGrowthLeft() > 0);
    --growth_left_info_;
  }

  // Overwrites several empty slots with full slots.
  void OverwriteManyEmptyAsFull(size_t count) {
    ABSL_SWISSTABLE_ASSERT(GetGrowthLeft() >= count);
    growth_left_info_ -= count;
  }

  // Overwrites specified control element with full slot.
  void OverwriteControlAsFull(ctrl_t ctrl) {
    ABSL_SWISSTABLE_ASSERT(GetGrowthLeft() >=
                           static_cast<size_t>(IsEmpty(ctrl)));
    growth_left_info_ -= static_cast<size_t>(IsEmpty(ctrl));
  }

  // Overwrites single full slot with a deleted slot.
  void OverwriteFullAsDeleted() { growth_left_info_ |= kDeletedBit; }

  // Returns true if table satisfies two properties:
  // 1. Guaranteed to have no kDeleted slots.
  // 2. There is a place for at least one element to grow.
  bool HasNoDeletedAndGrowthLeft() const {
    return static_cast<std::make_signed_t<size_t>>(growth_left_info_) > 0;
  }

  // Returns true if the table satisfies two properties:
  // 1. Guaranteed to have no kDeleted slots.
  // 2. There is no growth left.
  bool HasNoGrowthLeftAndNoDeleted() const { return growth_left_info_ == 0; }

  // Returns true if GetGrowthLeft() == 0, but must be called only if
  // HasNoDeleted() is false. It is slightly more efficient.
  bool HasNoGrowthLeftAssumingMayHaveDeleted() const {
    ABSL_SWISSTABLE_ASSERT(!HasNoDeleted());
    return growth_left_info_ == kDeletedBit;
  }

  // Returns true if table guaranteed to have no kDeleted slots.
  bool HasNoDeleted() const {
    return static_cast<std::make_signed_t<size_t>>(growth_left_info_) >= 0;
  }

  // Returns the number of elements left to grow.
  size_t GetGrowthLeft() const { return growth_left_info_ & kGrowthLeftMask; }

 private:
  static constexpr size_t kGrowthLeftMask = ((~size_t{}) >> 1);
  static constexpr size_t kDeletedBit = ~kGrowthLeftMask;
  // Topmost bit signal whenever there are deleted slots.
  size_t growth_left_info_;
};

static_assert(sizeof(GrowthInfo) == sizeof(size_t), "");
static_assert(alignof(GrowthInfo) == alignof(size_t), "");

// Returns whether `n` is a valid capacity (i.e., number of slots).
//
// A valid capacity is a non-zero integer `2^m - 1`.
constexpr bool IsValidCapacity(size_t n) { return ((n + 1) & n) == 0 && n > 0; }

// Whether a table is small enough that we don't need to hash any keys.
constexpr bool IsSmallCapacity(size_t capacity) { return capacity <= 1; }

// Returns the number of "cloned control bytes".
//
// This is the number of control bytes that are present both at the beginning
// of the control byte array and at the end, such that we can create a
// `Group::kWidth`-width probe window starting from any control byte.
constexpr size_t NumClonedBytes() { return Group::kWidth - 1; }

// Returns the number of control bytes including cloned.
constexpr size_t NumControlBytes(size_t capacity) {
  return IsSmallCapacity(capacity) ? 0 : capacity + 1 + NumClonedBytes();
}

// Computes the offset from the start of the backing allocation of control.
// infoz and growth_info are stored at the beginning of the backing array.
constexpr size_t ControlOffset(bool has_infoz) {
  return (has_infoz ? sizeof(HashtablezInfoHandle) : 0) + sizeof(GrowthInfo);
}

// Returns the offset of the next item after `offset` that is aligned to `align`
// bytes. `align` must be a power of two.
constexpr size_t AlignUpTo(size_t offset, size_t align) {
  return (offset + align - 1) & (~align + 1);
}

// Helper class for computing offsets and allocation size of hash set fields.
class RawHashSetLayout {
 public:
  explicit RawHashSetLayout(size_t capacity, size_t slot_size,
                            size_t slot_align, bool has_infoz)
      : control_offset_(ControlOffset(has_infoz)),
        generation_offset_(control_offset_ + NumControlBytes(capacity)),
        slot_offset_(
            AlignUpTo(generation_offset_ + NumGenerationBytes(), slot_align)),
        alloc_size_(slot_offset_ + capacity * slot_size) {
    ABSL_SWISSTABLE_ASSERT(IsValidCapacity(capacity));
    ABSL_SWISSTABLE_ASSERT(
        slot_size <=
        ((std::numeric_limits<size_t>::max)() - slot_offset_) / capacity);
  }

  // Returns precomputed offset from the start of the backing allocation of
  // control.
  size_t control_offset() const { return control_offset_; }

  // Given the capacity of a table, computes the offset (from the start of the
  // backing allocation) of the generation counter (if it exists).
  size_t generation_offset() const { return generation_offset_; }

  // Given the capacity of a table, computes the offset (from the start of the
  // backing allocation) at which the slots begin.
  size_t slot_offset() const { return slot_offset_; }

  // Given the capacity of a table, computes the total size of the backing
  // array.
  size_t alloc_size() const { return alloc_size_; }

 private:
  size_t control_offset_;
  size_t generation_offset_;
  size_t slot_offset_;
  size_t alloc_size_;
};

struct HashtableFreeFunctionsAccess;

// This allows us to work around an uninitialized memory warning when
// constructing begin() iterators in empty hashtables.
template <typename T>
union MaybeInitializedPtr {
  T* get() const { ABSL_SWISSTABLE_IGNORE_UNINITIALIZED_RETURN(p); }
  void set(T* ptr) { p = ptr; }

  T* p;
};

struct HeapPtrs {
  // The control bytes (and, also, a pointer near to the base of the backing
  // array).
  //
  // This contains `capacity + 1 + NumClonedBytes()` entries.
  //
  // Note that growth_info is stored immediately before this pointer.
  // May be uninitialized for small tables.
  MaybeInitializedPtr<ctrl_t> control;

  // The beginning of the slots, located at `SlotOffset()` bytes after
  // `control`. May be uninitialized for empty tables.
  // Note: we can't use `slots` because Qt defines "slots" as a macro.
  MaybeInitializedPtr<void> slot_array;
};

// Returns the maximum size of the SOO slot.
constexpr size_t MaxSooSlotSize() { return sizeof(HeapPtrs); }

// Manages the backing array pointers or the SOO slot. When raw_hash_set::is_soo
// is true, the SOO slot is stored in `soo_data`. Otherwise, we use `heap`.
union HeapOrSoo {
  MaybeInitializedPtr<ctrl_t>& control() {
    ABSL_SWISSTABLE_IGNORE_UNINITIALIZED_RETURN(heap.control);
  }
  MaybeInitializedPtr<ctrl_t> control() const {
    ABSL_SWISSTABLE_IGNORE_UNINITIALIZED_RETURN(heap.control);
  }
  MaybeInitializedPtr<void>& slot_array() {
    ABSL_SWISSTABLE_IGNORE_UNINITIALIZED_RETURN(heap.slot_array);
  }
  MaybeInitializedPtr<void> slot_array() const {
    ABSL_SWISSTABLE_IGNORE_UNINITIALIZED_RETURN(heap.slot_array);
  }
  void* get_soo_data() {
    ABSL_SWISSTABLE_IGNORE_UNINITIALIZED_RETURN(soo_data);
  }
  const void* get_soo_data() const {
    ABSL_SWISSTABLE_IGNORE_UNINITIALIZED_RETURN(soo_data);
  }

  HeapPtrs heap;
  unsigned char soo_data[MaxSooSlotSize()];
};

// Returns a reference to the GrowthInfo object stored immediately before
// `control`.
inline GrowthInfo& GetGrowthInfoFromControl(ctrl_t* control) {
  auto* gl_ptr = reinterpret_cast<GrowthInfo*>(control) - 1;
  ABSL_SWISSTABLE_ASSERT(
      reinterpret_cast<uintptr_t>(gl_ptr) % alignof(GrowthInfo) == 0);
  return *gl_ptr;
}

// CommonFields hold the fields in raw_hash_set that do not depend
// on template parameters. This allows us to conveniently pass all
// of this state to helper functions as a single argument.
class CommonFields : public CommonFieldsGenerationInfo {
 public:
  explicit CommonFields(soo_tag_t)
      : capacity_(SooCapacity()), size_(no_seed_empty_tag_t{}) {}
  explicit CommonFields(full_soo_tag_t)
      : capacity_(SooCapacity()), size_(full_soo_tag_t{}) {}
  explicit CommonFields(non_soo_tag_t)
      : capacity_(0), size_(no_seed_empty_tag_t{}) {}
  // For use in swapping.
  explicit CommonFields(uninitialized_tag_t) : size_(uninitialized_tag_t{}) {}

  // Not copyable
  CommonFields(const CommonFields&) = delete;
  CommonFields& operator=(const CommonFields&) = delete;

  // Copy with guarantee that it is not SOO.
  CommonFields(non_soo_tag_t, const CommonFields& that)
      : capacity_(that.capacity_),
        size_(that.size_),
        heap_or_soo_(that.heap_or_soo_) {
  }

  // Movable
  CommonFields(CommonFields&& that) = default;
  CommonFields& operator=(CommonFields&&) = default;

  template <bool kSooEnabled>
  static CommonFields CreateDefault() {
    return kSooEnabled ? CommonFields{soo_tag_t{}}
                       : CommonFields{non_soo_tag_t{}};
  }

  // The inline data for SOO is written on top of control_/slots_.
  const void* soo_data() const { return heap_or_soo_.get_soo_data(); }
  void* soo_data() { return heap_or_soo_.get_soo_data(); }

  ctrl_t* control() const {
    ABSL_SWISSTABLE_IGNORE_UNINITIALIZED_RETURN(heap_or_soo_.control().get());
  }

  // When we set the control bytes, we also often want to generate a new seed.
  // So we bundle these two operations together to make sure we don't forget to
  // generate a new seed.
  // The table will be invalidated if
  // `kGenerateSeed && !empty() && !is_single_group(capacity())` because H1 is
  // being changed. In such cases, we will need to rehash the table.
  template <bool kGenerateSeed>
  void set_control(ctrl_t* c) {
    heap_or_soo_.control().set(c);
    if constexpr (kGenerateSeed) {
      generate_new_seed();
    }
  }
  void* backing_array_start() const {
    // growth_info (and maybe infoz) is stored before control bytes.
    ABSL_SWISSTABLE_ASSERT(
        reinterpret_cast<uintptr_t>(control()) % alignof(size_t) == 0);
    return control() - ControlOffset(has_infoz());
  }

  // Note: we can't use slots() because Qt defines "slots" as a macro.
  void* slot_array() const { return heap_or_soo_.slot_array().get(); }
  MaybeInitializedPtr<void> slots_union() const {
    return heap_or_soo_.slot_array();
  }
  void set_slots(void* s) { heap_or_soo_.slot_array().set(s); }

  // The number of filled slots.
  size_t size() const { return size_.size(); }
  // Sets the size to zero, but keeps hashinfoz bit and seed.
  void set_size_to_zero() { size_.set_size_to_zero_keep_metadata(); }
  void set_empty_soo() {
    AssertInSooMode();
    size_ = HashtableSize(no_seed_empty_tag_t{});
  }
  void set_full_soo() {
    AssertInSooMode();
    size_ = HashtableSize(full_soo_tag_t{});
  }
  void increment_size() {
    ABSL_SWISSTABLE_ASSERT(size() < capacity());
    size_.increment_size();
  }
  void increment_size(size_t n) {
    ABSL_SWISSTABLE_ASSERT(size() + n <= capacity());
    size_.increment_size(n);
  }
  void decrement_size() {
    ABSL_SWISSTABLE_ASSERT(!empty());
    size_.decrement_size();
  }
  bool empty() const { return size_.empty(); }

  // The seed used for the H1 part of the hash function.
  PerTableSeed seed() const { return size_.seed(); }
  // Generates a new seed for the H1 part of the hash function.
  // The table will be invalidated if
  // `kGenerateSeed && !empty() && !is_single_group(capacity())` because H1 is
  // being changed. In such cases, we will need to rehash the table.
  void generate_new_seed() { size_.generate_new_seed(); }
  void set_no_seed_for_testing() { size_.set_no_seed_for_testing(); }

  // The total number of available slots.
  size_t capacity() const { return capacity_; }
  void set_capacity(size_t c) {
    // We allow setting above the max valid capacity for debugging purposes.
    ABSL_SWISSTABLE_ASSERT(c == 0 || IsValidCapacity(c) ||
                           c > kAboveMaxValidCapacity);
    capacity_ = c;
  }
  bool is_small() const { return IsSmallCapacity(capacity_); }

  // The number of slots we can still fill without needing to rehash.
  // This is stored in the heap allocation before the control bytes.
  // TODO(b/289225379): experiment with moving growth_info back inline to
  // increase room for SOO.
  size_t growth_left() const { return growth_info().GetGrowthLeft(); }

  GrowthInfo& growth_info() {
    return GetGrowthInfoFromControl(control());
  }
  GrowthInfo growth_info() const {
    return const_cast<CommonFields*>(this)->growth_info();
  }

  bool has_infoz() const { return size_.has_infoz(); }
  void set_has_infoz() { size_.set_has_infoz(); }

  HashtablezInfoHandle infoz() {
    return has_infoz()
               ? *reinterpret_cast<HashtablezInfoHandle*>(backing_array_start())
               : HashtablezInfoHandle();
  }
  void set_infoz(HashtablezInfoHandle infoz) {
    ABSL_SWISSTABLE_ASSERT(has_infoz());
    *reinterpret_cast<HashtablezInfoHandle*>(backing_array_start()) = infoz;
  }

  bool should_rehash_for_bug_detection_on_insert() const {
    if constexpr (!SwisstableGenerationsEnabled()) {
      return false;
    }
    // As an optimization, we avoid calling ShouldRehashForBugDetection if we
    // will end up rehashing anyways.
    if (growth_left() == 0) return false;
    return CommonFieldsGenerationInfo::
        should_rehash_for_bug_detection_on_insert(seed(), capacity());
  }
  bool should_rehash_for_bug_detection_on_move() const {
    return CommonFieldsGenerationInfo::should_rehash_for_bug_detection_on_move(
        seed(), capacity());
  }
  void reset_reserved_growth(size_t reservation) {
    CommonFieldsGenerationInfo::reset_reserved_growth(reservation, size());
  }

  // The size of the backing array allocation.
  size_t alloc_size(size_t slot_size, size_t slot_align) const {
    return RawHashSetLayout(capacity(), slot_size, slot_align, has_infoz())
        .alloc_size();
  }

  // Move fields other than heap_or_soo_.
  void move_non_heap_or_soo_fields(CommonFields& that) {
    static_cast<CommonFieldsGenerationInfo&>(*this) =
        std::move(static_cast<CommonFieldsGenerationInfo&>(that));
    capacity_ = that.capacity_;
    size_ = that.size_;
  }

  // Returns the number of control bytes set to kDeleted. For testing only.
  size_t TombstonesCount() const {
    return static_cast<size_t>(
        std::count(control(), control() + capacity(), ctrl_t::kDeleted));
  }

  // Helper to enable sanitizer mode validation to protect against reentrant
  // calls during element constructor/destructor.
  template <typename F>
  void RunWithReentrancyGuard(F f) {
#ifdef NDEBUG
    f();
    return;
#endif
    const size_t cap = capacity();
    set_capacity(InvalidCapacity::kReentrance);
    f();
    set_capacity(cap);
  }

 private:
  // We store the has_infoz bit in the lowest bit of size_.
  static constexpr size_t HasInfozShift() { return 1; }
  static constexpr size_t HasInfozMask() {
    return (size_t{1} << HasInfozShift()) - 1;
  }

  // We can't assert that SOO is enabled because we don't have SooEnabled(), but
  // we assert what we can.
  void AssertInSooMode() const {
    ABSL_SWISSTABLE_ASSERT(capacity() == SooCapacity());
    ABSL_SWISSTABLE_ASSERT(!has_infoz());
  }

  // The number of slots in the backing array. This is always 2^N-1 for an
  // integer N. NOTE: we tried experimenting with compressing the capacity and
  // storing it together with size_: (a) using 6 bits to store the corresponding
  // power (N in 2^N-1), and (b) storing 2^N as the most significant bit of
  // size_ and storing size in the low bits. Both of these experiments were
  // regressions, presumably because we need capacity to do find operations.
  size_t capacity_;

  // TODO(b/289225379): we could put size_ into HeapOrSoo and make capacity_
  // encode the size in SOO case. We would be making size()/capacity() more
  // expensive in order to have more SOO space.
  HashtableSize size_;

  // Either the control/slots pointers or the SOO slot.
  HeapOrSoo heap_or_soo_;
};

template <class Policy, class Hash, class Eq, class Alloc>
class raw_hash_set;

// Returns the next valid capacity after `n`.
constexpr size_t NextCapacity(size_t n) {
  ABSL_SWISSTABLE_ASSERT(IsValidCapacity(n) || n == 0);
  return n * 2 + 1;
}

// Returns the previous valid capacity before `n`.
constexpr size_t PreviousCapacity(size_t n) {
  ABSL_SWISSTABLE_ASSERT(IsValidCapacity(n));
  return n / 2;
}

// Applies the following mapping to every byte in the control array:
//   * kDeleted -> kEmpty
//   * kEmpty -> kEmpty
//   * _ -> kDeleted
// PRECONDITION:
//   IsValidCapacity(capacity)
//   ctrl[capacity] == ctrl_t::kSentinel
//   ctrl[i] != ctrl_t::kSentinel for all i < capacity
void ConvertDeletedToEmptyAndFullToDeleted(ctrl_t* ctrl, size_t capacity);

// Converts `n` into the next valid capacity, per `IsValidCapacity`.
constexpr size_t NormalizeCapacity(size_t n) {
  return n ? ~size_t{} >> countl_zero(n) : 1;
}

// General notes on capacity/growth methods below:
// - We use 7/8th as maximum load factor. For 16-wide groups, that gives an
//   average of two empty slots per group.
// - For (capacity+1) >= Group::kWidth, growth is 7/8*capacity.
// - For (capacity+1) < Group::kWidth, growth == capacity. In this case, we
//   never need to probe (the whole table fits in one group) so we don't need a
//   load factor less than 1.

// Given `capacity`, applies the load factor; i.e., it returns the maximum
// number of values we should put into the table before a resizing rehash.
constexpr size_t CapacityToGrowth(size_t capacity) {
  ABSL_SWISSTABLE_ASSERT(IsValidCapacity(capacity));
  // `capacity*7/8`
  if (Group::kWidth == 8 && capacity == 7) {
    // x-x/8 does not work when x==7.
    return 6;
  }
  return capacity - capacity / 8;
}

// Given `size`, "unapplies" the load factor to find how large the capacity
// should be to stay within the load factor.
//
// For size == 0, returns 0.
// For other values, returns the same as `NormalizeCapacity(size*8/7)`.
constexpr size_t SizeToCapacity(size_t size) {
  if (size == 0) {
    return 0;
  }
  // The minimum possible capacity is NormalizeCapacity(size).
  // Shifting right `~size_t{}` by `leading_zeros` yields
  // NormalizeCapacity(size).
  int leading_zeros = absl::countl_zero(size);
  constexpr size_t kLast3Bits = size_t{7} << (sizeof(size_t) * 8 - 3);
  // max_size_for_next_capacity = max_load_factor * next_capacity
  //                            = (7/8) * (~size_t{} >> leading_zeros)
  //                            = (7/8*~size_t{}) >> leading_zeros
  //                            = kLast3Bits >> leading_zeros
  size_t max_size_for_next_capacity = kLast3Bits >> leading_zeros;
  // Decrease shift if size is too big for the minimum capacity.
  leading_zeros -= static_cast<int>(size > max_size_for_next_capacity);
  if constexpr (Group::kWidth == 8) {
    // Formula doesn't work when size==7 for 8-wide groups.
    leading_zeros -= (size == 7);
  }
  return (~size_t{}) >> leading_zeros;
}

template <class InputIter>
size_t SelectBucketCountForIterRange(InputIter first, InputIter last,
                                     size_t bucket_count) {
  if (bucket_count != 0) {
    return bucket_count;
  }
  if (base_internal::IsAtLeastIterator<std::random_access_iterator_tag,
                                       InputIter>()) {
    return SizeToCapacity(static_cast<size_t>(std::distance(first, last)));
  }
  return 0;
}

constexpr bool SwisstableDebugEnabled() {
#if defined(ABSL_SWISSTABLE_ENABLE_GENERATIONS) || \
    ABSL_OPTION_HARDENED == 1 || !defined(NDEBUG)
  return true;
#else
  return false;
#endif
}

inline void AssertIsFull(const ctrl_t* ctrl, GenerationType generation,
                         const GenerationType* generation_ptr,
                         const char* operation) {
  if (!SwisstableDebugEnabled()) return;
  // `SwisstableDebugEnabled()` is also true for release builds with hardening
  // enabled. To minimize their impact in those builds:
  // - use `ABSL_PREDICT_FALSE()` to provide a compiler hint for code layout
  // - use `ABSL_RAW_LOG()` with a format string to reduce code size and improve
  //   the chances that the hot paths will be inlined.
  if (ABSL_PREDICT_FALSE(ctrl == nullptr)) {
    ABSL_RAW_LOG(FATAL, "%s called on end() iterator.", operation);
  }
  if (ABSL_PREDICT_FALSE(ctrl == DefaultIterControl())) {
    ABSL_RAW_LOG(FATAL, "%s called on default-constructed iterator.",
                 operation);
  }
  if (SwisstableGenerationsEnabled()) {
    if (ABSL_PREDICT_FALSE(generation != *generation_ptr)) {
      ABSL_RAW_LOG(FATAL,
                   "%s called on invalid iterator. The table could have "
                   "rehashed or moved since this iterator was initialized.",
                   operation);
    }
    if (ABSL_PREDICT_FALSE(!IsFull(*ctrl))) {
      ABSL_RAW_LOG(
          FATAL,
          "%s called on invalid iterator. The element was likely erased.",
          operation);
    }
  } else {
    if (ABSL_PREDICT_FALSE(!IsFull(*ctrl))) {
      ABSL_RAW_LOG(
          FATAL,
          "%s called on invalid iterator. The element might have been erased "
          "or the table might have rehashed. Consider running with "
          "--config=asan to diagnose rehashing issues.",
          operation);
    }
  }
}

// Note that for comparisons, null/end iterators are valid.
inline void AssertIsValidForComparison(const ctrl_t* ctrl,
                                       GenerationType generation,
                                       const GenerationType* generation_ptr) {
  if (!SwisstableDebugEnabled()) return;
  const bool ctrl_is_valid_for_comparison =
      ctrl == nullptr || ctrl == DefaultIterControl() || IsFull(*ctrl);
  if (SwisstableGenerationsEnabled()) {
    if (ABSL_PREDICT_FALSE(generation != *generation_ptr)) {
      ABSL_RAW_LOG(FATAL,
                   "Invalid iterator comparison. The table could have rehashed "
                   "or moved since this iterator was initialized.");
    }
    if (ABSL_PREDICT_FALSE(!ctrl_is_valid_for_comparison)) {
      ABSL_RAW_LOG(
          FATAL, "Invalid iterator comparison. The element was likely erased.");
    }
  } else {
    ABSL_HARDENING_ASSERT_SLOW(
        ctrl_is_valid_for_comparison &&
        "Invalid iterator comparison. The element might have been erased or "
        "the table might have rehashed. Consider running with --config=asan to "
        "diagnose rehashing issues.");
  }
}

// If the two iterators come from the same container, then their pointers will
// interleave such that ctrl_a <= ctrl_b < slot_a <= slot_b or vice/versa.
// Note: we take slots by reference so that it's not UB if they're uninitialized
// as long as we don't read them (when ctrl is null).
inline bool AreItersFromSameContainer(const ctrl_t* ctrl_a,
                                      const ctrl_t* ctrl_b,
                                      const void* const& slot_a,
                                      const void* const& slot_b) {
  // If either control byte is null, then we can't tell.
  if (ctrl_a == nullptr || ctrl_b == nullptr) return true;
  const bool a_is_soo = IsSooControl(ctrl_a);
  if (a_is_soo != IsSooControl(ctrl_b)) return false;
  if (a_is_soo) return slot_a == slot_b;

  const void* low_slot = slot_a;
  const void* hi_slot = slot_b;
  if (ctrl_a > ctrl_b) {
    std::swap(ctrl_a, ctrl_b);
    std::swap(low_slot, hi_slot);
  }
  return ctrl_b < low_slot && low_slot <= hi_slot;
}

// Asserts that two iterators come from the same container.
// Note: we take slots by reference so that it's not UB if they're uninitialized
// as long as we don't read them (when ctrl is null).
inline void AssertSameContainer(const ctrl_t* ctrl_a, const ctrl_t* ctrl_b,
                                const void* const& slot_a,
                                const void* const& slot_b,
                                const GenerationType* generation_ptr_a,
                                const GenerationType* generation_ptr_b) {
  if (!SwisstableDebugEnabled()) return;
  // `SwisstableDebugEnabled()` is also true for release builds with hardening
  // enabled. To minimize their impact in those builds:
  // - use `ABSL_PREDICT_FALSE()` to provide a compiler hint for code layout
  // - use `ABSL_RAW_LOG()` with a format string to reduce code size and improve
  //   the chances that the hot paths will be inlined.

  // fail_if(is_invalid, message) crashes when is_invalid is true and provides
  // an error message based on `message`.
  const auto fail_if = [](bool is_invalid, const char* message) {
    if (ABSL_PREDICT_FALSE(is_invalid)) {
      ABSL_RAW_LOG(FATAL, "Invalid iterator comparison. %s", message);
    }
  };

  const bool a_is_default = ctrl_a == DefaultIterControl();
  const bool b_is_default = ctrl_b == DefaultIterControl();
  if (a_is_default && b_is_default) return;
  fail_if(a_is_default != b_is_default,
          "Comparing default-constructed hashtable iterator with a "
          "non-default-constructed hashtable iterator.");

  if (SwisstableGenerationsEnabled()) {
    if (ABSL_PREDICT_TRUE(generation_ptr_a == generation_ptr_b)) return;
    // Users don't need to know whether the tables are SOO so don't mention SOO
    // in the debug message.
    const bool a_is_soo = IsSooControl(ctrl_a);
    const bool b_is_soo = IsSooControl(ctrl_b);
    fail_if(a_is_soo != b_is_soo || (a_is_soo && b_is_soo),
            "Comparing iterators from different hashtables.");

    const bool a_is_empty = IsEmptyGeneration(generation_ptr_a);
    const bool b_is_empty = IsEmptyGeneration(generation_ptr_b);
    fail_if(a_is_empty != b_is_empty,
            "Comparing an iterator from an empty hashtable with an iterator "
            "from a non-empty hashtable.");
    fail_if(a_is_empty && b_is_empty,
            "Comparing iterators from different empty hashtables.");

    const bool a_is_end = ctrl_a == nullptr;
    const bool b_is_end = ctrl_b == nullptr;
    fail_if(a_is_end || b_is_end,
            "Comparing iterator with an end() iterator from a different "
            "hashtable.");
    fail_if(true, "Comparing non-end() iterators from different hashtables.");
  } else {
    ABSL_HARDENING_ASSERT_SLOW(
        AreItersFromSameContainer(ctrl_a, ctrl_b, slot_a, slot_b) &&
        "Invalid iterator comparison. The iterators may be from different "
        "containers or the container might have rehashed or moved. Consider "
        "running with --config=asan to diagnose issues.");
  }
}

struct FindInfo {
  size_t offset;
  size_t probe_length;
};

// Whether a table fits entirely into a probing group.
// Arbitrary order of elements in such tables is correct.
constexpr bool is_single_group(size_t capacity) {
  return capacity <= Group::kWidth;
}

// Begins a probing operation on `common.control`, using `hash`.
inline probe_seq<Group::kWidth> probe(size_t h1, size_t capacity) {
  return probe_seq<Group::kWidth>(h1, capacity);
}
inline probe_seq<Group::kWidth> probe(PerTableSeed seed, size_t capacity,
                                      size_t hash) {
  return probe(H1(hash, seed), capacity);
}
inline probe_seq<Group::kWidth> probe(const CommonFields& common, size_t hash) {
  return probe(common.seed(), common.capacity(), hash);
}

// Probes an array of control bits using a probe sequence derived from `hash`,
// and returns the offset corresponding to the first deleted or empty slot.
//
// Behavior when the entire table is full is undefined.
//
// NOTE: this function must work with tables having both empty and deleted
// slots in the same group. Such tables appear during `erase()`.
FindInfo find_first_non_full(const CommonFields& common, size_t hash);

constexpr size_t kProbedElementIndexSentinel = ~size_t{};

// Implementation detail of transfer_unprobed_elements_to_next_capacity_fn.
// Tries to find the new index for an element whose hash corresponds to
// `h1` for growth to the next capacity.
// Returns kProbedElementIndexSentinel if full probing is required.
//
// If element is located in the first probing group in the table before growth,
// returns one of two positions: `old_index` or `old_index + old_capacity + 1`.
//
// Otherwise, we will try to insert it into the first probe group of the new
// table. We only attempt to do so if the first probe group is already
// initialized.
template <typename = void>
inline size_t TryFindNewIndexWithoutProbing(size_t h1, size_t old_index,
                                            size_t old_capacity,
                                            ctrl_t* new_ctrl,
                                            size_t new_capacity) {
  size_t index_diff = old_index - h1;
  // The first probe group starts with h1 & capacity.
  // All following groups start at (h1 + Group::kWidth * K) & capacity.
  // We can find an index within the floating group as index_diff modulo
  // Group::kWidth.
  // Both old and new capacity are larger than Group::kWidth so we can avoid
  // computing `& capacity`.
  size_t in_floating_group_index = index_diff & (Group::kWidth - 1);
  // By subtracting we will get the difference between the first probe group
  // and the probe group corresponding to old_index.
  index_diff -= in_floating_group_index;
  if (ABSL_PREDICT_TRUE((index_diff & old_capacity) == 0)) {
    size_t new_index = (h1 + in_floating_group_index) & new_capacity;
    ABSL_ASSUME(new_index != kProbedElementIndexSentinel);
    return new_index;
  }
  ABSL_SWISSTABLE_ASSERT(((old_index - h1) & old_capacity) >= Group::kWidth);
  // Try to insert element into the first probe group.
  // new_ctrl is not yet fully initialized so we can't use regular search via
  // find_first_non_full.

  // We can search in the first probe group only if it is located in already
  // initialized part of the table.
  if (ABSL_PREDICT_FALSE((h1 & old_capacity) >= old_index)) {
    return kProbedElementIndexSentinel;
  }
  size_t offset = h1 & new_capacity;
  Group new_g(new_ctrl + offset);
  if (auto mask = new_g.MaskNonFull(); ABSL_PREDICT_TRUE(mask)) {
    size_t result = offset + mask.LowestBitSet();
    ABSL_ASSUME(result != kProbedElementIndexSentinel);
    return result;
  }
  return kProbedElementIndexSentinel;
}

// Extern template for inline function keeps possibility of inlining.
// When compiler decided to not inline, no symbols will be added to the
// corresponding translation unit.
extern template size_t TryFindNewIndexWithoutProbing(size_t h1,
                                                     size_t old_index,
                                                     size_t old_capacity,
                                                     ctrl_t* new_ctrl,
                                                     size_t new_capacity);

// Sets sanitizer poisoning for slot corresponding to control byte being set.
inline void DoSanitizeOnSetCtrl(const CommonFields& c, size_t i, ctrl_t h,
                                size_t slot_size) {
  ABSL_SWISSTABLE_ASSERT(i < c.capacity());
  auto* slot_i = static_cast<const char*>(c.slot_array()) + i * slot_size;
  if (IsFull(h)) {
    SanitizerUnpoisonMemoryRegion(slot_i, slot_size);
  } else {
    SanitizerPoisonMemoryRegion(slot_i, slot_size);
  }
}

// Sets `ctrl[i]` to `h`.
//
// Unlike setting it directly, this function will perform bounds checks and
// mirror the value to the cloned tail if necessary.
inline void SetCtrl(const CommonFields& c, size_t i, ctrl_t h,
                    size_t slot_size) {
  DoSanitizeOnSetCtrl(c, i, h, slot_size);
  ctrl_t* ctrl = c.control();
  ctrl[i] = h;
  ctrl[((i - NumClonedBytes()) & c.capacity()) +
       (NumClonedBytes() & c.capacity())] = h;
}
// Overload for setting to an occupied `h2_t` rather than a special `ctrl_t`.
inline void SetCtrl(const CommonFields& c, size_t i, h2_t h, size_t slot_size) {
  SetCtrl(c, i, static_cast<ctrl_t>(h), slot_size);
}

// Like SetCtrl, but in a single group table, we can save some operations when
// setting the cloned control byte.
inline void SetCtrlInSingleGroupTable(const CommonFields& c, size_t i, ctrl_t h,
                                      size_t slot_size) {
  ABSL_SWISSTABLE_ASSERT(is_single_group(c.capacity()));
  DoSanitizeOnSetCtrl(c, i, h, slot_size);
  ctrl_t* ctrl = c.control();
  ctrl[i] = h;
  ctrl[i + c.capacity() + 1] = h;
}
// Overload for setting to an occupied `h2_t` rather than a special `ctrl_t`.
inline void SetCtrlInSingleGroupTable(const CommonFields& c, size_t i, h2_t h,
                                      size_t slot_size) {
  SetCtrlInSingleGroupTable(c, i, static_cast<ctrl_t>(h), slot_size);
}

// Like SetCtrl, but in a table with capacity >= Group::kWidth - 1,
// we can save some operations when setting the cloned control byte.
inline void SetCtrlInLargeTable(const CommonFields& c, size_t i, ctrl_t h,
                                size_t slot_size) {
  ABSL_SWISSTABLE_ASSERT(c.capacity() >= Group::kWidth - 1);
  DoSanitizeOnSetCtrl(c, i, h, slot_size);
  ctrl_t* ctrl = c.control();
  ctrl[i] = h;
  ctrl[((i - NumClonedBytes()) & c.capacity()) + NumClonedBytes()] = h;
}
// Overload for setting to an occupied `h2_t` rather than a special `ctrl_t`.
inline void SetCtrlInLargeTable(const CommonFields& c, size_t i, h2_t h,
                                size_t slot_size) {
  SetCtrlInLargeTable(c, i, static_cast<ctrl_t>(h), slot_size);
}

// growth_info (which is a size_t) is stored with the backing array.
constexpr size_t BackingArrayAlignment(size_t align_of_slot) {
  return (std::max)(align_of_slot, alignof(GrowthInfo));
}

// Returns the address of the ith slot in slots where each slot occupies
// slot_size.
inline void* SlotAddress(void* slot_array, size_t slot, size_t slot_size) {
  return static_cast<void*>(static_cast<char*>(slot_array) +
                            (slot * slot_size));
}

// Iterates over all full slots and calls `cb(const ctrl_t*, void*)`.
// No insertion to the table is allowed during `cb` call.
// Erasure is allowed only for the element passed to the callback.
// The table must not be in SOO mode.
void IterateOverFullSlots(const CommonFields& c, size_t slot_size,
                          absl::FunctionRef<void(const ctrl_t*, void*)> cb);

template <typename CharAlloc>
constexpr bool ShouldSampleHashtablezInfoForAlloc() {
  // Folks with custom allocators often make unwarranted assumptions about the
  // behavior of their classes vis-a-vis trivial destructability and what
  // calls they will or won't make.  Avoid sampling for people with custom
  // allocators to get us out of this mess.  This is not a hard guarantee but
  // a workaround while we plan the exact guarantee we want to provide.
  return std::is_same_v<CharAlloc, std::allocator<char>>;
}

template <bool kSooEnabled>
bool ShouldSampleHashtablezInfoOnResize(bool force_sampling,
                                        bool is_hashtablez_eligible,
                                        size_t old_capacity, CommonFields& c) {
  if (!is_hashtablez_eligible) return false;
  // Force sampling is only allowed for SOO tables.
  ABSL_SWISSTABLE_ASSERT(kSooEnabled || !force_sampling);
  if (kSooEnabled && force_sampling) {
    return true;
  }
  // In SOO, we sample on the first insertion so if this is an empty SOO case
  // (e.g. when reserve is called), then we still need to sample.
  if (kSooEnabled && old_capacity == SooCapacity() && c.empty()) {
    return ShouldSampleNextTable();
  }
  if (!kSooEnabled && old_capacity == 0) {
    return ShouldSampleNextTable();
  }
  return false;
}

// Allocates `n` bytes for a backing array.
template <size_t AlignOfBackingArray, typename Alloc>
ABSL_ATTRIBUTE_NOINLINE void* AllocateBackingArray(void* alloc, size_t n) {
  return Allocate<AlignOfBackingArray>(static_cast<Alloc*>(alloc), n);
}

template <size_t AlignOfBackingArray, typename Alloc>
ABSL_ATTRIBUTE_NOINLINE void DeallocateBackingArray(
    void* alloc, size_t capacity, ctrl_t* ctrl, size_t slot_size,
    size_t slot_align, bool had_infoz) {
  RawHashSetLayout layout(capacity, slot_size, slot_align, had_infoz);
  void* backing_array = ctrl - layout.control_offset();
  // Unpoison before returning the memory to the allocator.
  SanitizerUnpoisonMemoryRegion(backing_array, layout.alloc_size());
  Deallocate<AlignOfBackingArray>(static_cast<Alloc*>(alloc), backing_array,
                                  layout.alloc_size());
}

// PolicyFunctions bundles together some information for a particular
// raw_hash_set<T, ...> instantiation. This information is passed to
// type-erased functions that want to do small amounts of type-specific
// work.
struct PolicyFunctions {
  uint32_t key_size;
  uint32_t value_size;
  uint32_t slot_size;
  uint16_t slot_align;
  bool soo_enabled;
  bool is_hashtablez_eligible;

  // Returns the pointer to the hash function stored in the set.
  void* (*hash_fn)(CommonFields& common);

  // Returns the hash of the pointed-to slot.
  size_t (*hash_slot)(const void* hash_fn, void* slot);

  // Transfers the contents of `count` slots from src_slot to dst_slot.
  // We use ability to transfer several slots in single group table growth.
  void (*transfer_n)(void* set, void* dst_slot, void* src_slot, size_t count);

  // Returns the pointer to the CharAlloc stored in the set.
  void* (*get_char_alloc)(CommonFields& common);

  // Allocates n bytes for the backing store for common.
  void* (*alloc)(void* alloc, size_t n);

  // Deallocates the backing store from common.
  void (*dealloc)(void* alloc, size_t capacity, ctrl_t* ctrl, size_t slot_size,
                  size_t slot_align, bool had_infoz);

  // Implementation detail of GrowToNextCapacity.
  // Iterates over all full slots and transfers unprobed elements.
  // Initializes the new control bytes except mirrored bytes and kSentinel.
  // Caller must finish the initialization.
  // All slots corresponding to the full control bytes are transferred.
  // Probed elements are reported by `encode_probed_element` callback.
  // encode_probed_element may overwrite old_ctrl buffer till source_offset.
  // Different encoding is used depending on the capacity of the table.
  // See ProbedItem*Bytes classes for details.
  void (*transfer_unprobed_elements_to_next_capacity)(
      CommonFields& common, const ctrl_t* old_ctrl, void* old_slots,
      // TODO(b/382423690): Try to use absl::FunctionRef here.
      void* probed_storage,
      void (*encode_probed_element)(void* probed_storage, h2_t h2,
                                    size_t source_offset, size_t h1));

  uint8_t soo_capacity() const {
    return static_cast<uint8_t>(soo_enabled ? SooCapacity() : 0);
  }
};

// Returns the maximum valid size for a table with 1-byte slots.
// This function is an utility shared by MaxValidSize and IsAboveValidSize.
// Template parameter is only used to enable testing.
template <size_t kSizeOfSizeT = sizeof(size_t)>
constexpr size_t MaxValidSizeFor1ByteSlot() {
  if constexpr (kSizeOfSizeT == 8) {
    return CapacityToGrowth(
        static_cast<size_t>(uint64_t{1} << HashtableSize::kSizeBitCount) - 1);
  } else {
    static_assert(kSizeOfSizeT == 4);
    return CapacityToGrowth((size_t{1} << (kSizeOfSizeT * 8 - 2)) - 1);
  }
}

// Returns the maximum valid size for a table with provided slot size.
// Template parameter is only used to enable testing.
template <size_t kSizeOfSizeT = sizeof(size_t)>
constexpr size_t MaxValidSize(size_t slot_size) {
  if constexpr (kSizeOfSizeT == 8) {
    // For small slot sizes we are limited by HashtableSize::kSizeBitCount.
    if (slot_size < size_t{1} << (64 - HashtableSize::kSizeBitCount)) {
      return MaxValidSizeFor1ByteSlot<kSizeOfSizeT>();
    }
    return (size_t{1} << (kSizeOfSizeT * 8 - 2)) / slot_size;
  } else {
    return MaxValidSizeFor1ByteSlot<kSizeOfSizeT>() / slot_size;
  }
}

// Returns true if size is larger than the maximum valid size.
// It is an optimization to avoid the division operation in the common case.
// Template parameter is only used to enable testing.
template <size_t kSizeOfSizeT = sizeof(size_t)>
constexpr bool IsAboveValidSize(size_t size, size_t slot_size) {
  if constexpr (kSizeOfSizeT == 8) {
    // For small slot sizes we are limited by HashtableSize::kSizeBitCount.
    if (ABSL_PREDICT_TRUE(slot_size <
                          (size_t{1} << (64 - HashtableSize::kSizeBitCount)))) {
      return size > MaxValidSizeFor1ByteSlot<kSizeOfSizeT>();
    }
    return size > MaxValidSize<kSizeOfSizeT>(slot_size);
  } else {
    return uint64_t{size} * slot_size >
           MaxValidSizeFor1ByteSlot<kSizeOfSizeT>();
  }
}

// Returns the index of the SOO slot when growing from SOO to non-SOO in a
// single group. See also InitializeSmallControlBytesAfterSoo(). It's important
// to use index 1 so that when resizing from capacity 1 to 3, we can still have
// random iteration order between the first two inserted elements.
// I.e. it allows inserting the second element at either index 0 or 2.
constexpr size_t SooSlotIndex() { return 1; }

// Maximum capacity for the algorithm for small table after SOO.
// Note that typical size after SOO is 3, but we allow up to 7.
// Allowing till 16 would require additional store that can be avoided.
constexpr size_t MaxSmallAfterSooCapacity() { return 7; }

// Type erased version of raw_hash_set::reserve.
// Requires: `new_size > policy.soo_capacity`.
void ReserveTableToFitNewSize(CommonFields& common,
                              const PolicyFunctions& policy, size_t new_size);

// Resizes empty non-allocated table to the next valid capacity after
// `bucket_count`. Requires:
//   1. `c.capacity() == policy.soo_capacity`.
//   2. `c.empty()`.
//   3. `new_size > policy.soo_capacity`.
// The table will be attempted to be sampled.
void ReserveEmptyNonAllocatedTableToFitBucketCount(
    CommonFields& common, const PolicyFunctions& policy, size_t bucket_count);

// Type erased version of raw_hash_set::rehash.
void Rehash(CommonFields& common, const PolicyFunctions& policy, size_t n);

// Type erased version of copy constructor.
void Copy(CommonFields& common, const PolicyFunctions& policy,
          const CommonFields& other,
          absl::FunctionRef<void(void*, const void*)> copy_fn);

// Returns the optimal size for memcpy when transferring SOO slot.
// Otherwise, returns the optimal size for memcpy SOO slot transfer
// to SooSlotIndex().
// At the destination we are allowed to copy upto twice more bytes,
// because there is at least one more slot after SooSlotIndex().
// The result must not exceed MaxSooSlotSize().
// Some of the cases are merged to minimize the number of function
// instantiations.
constexpr size_t OptimalMemcpySizeForSooSlotTransfer(
    size_t slot_size, size_t max_soo_slot_size = MaxSooSlotSize()) {
  static_assert(MaxSooSlotSize() >= 8, "unexpectedly small SOO slot size");
  if (slot_size == 1) {
    return 1;
  }
  if (slot_size <= 3) {
    return 4;
  }
  // We are merging 4 and 8 into one case because we expect them to be the
  // hottest cases. Copying 8 bytes is as fast on common architectures.
  if (slot_size <= 8) {
    return 8;
  }
  if (max_soo_slot_size <= 16) {
    return max_soo_slot_size;
  }
  if (slot_size <= 16) {
    return 16;
  }
  if (max_soo_slot_size <= 24) {
    return max_soo_slot_size;
  }
  static_assert(MaxSooSlotSize() <= 24, "unexpectedly large SOO slot size");
  return 24;
}

// Resizes SOO table to the NextCapacity(SooCapacity()) and prepares insert for
// the given new_hash. Returns the offset of the new element.
// `soo_slot_ctrl` is the control byte of the SOO slot.
// If soo_slot_ctrl is kEmpty
//   1. The table must be empty.
//   2. Table will be forced to be sampled.
// All possible template combinations are defined in cc file to improve
// compilation time.
template <size_t SooSlotMemcpySize, bool TransferUsesMemcpy>
size_t GrowSooTableToNextCapacityAndPrepareInsert(CommonFields& common,
                                                  const PolicyFunctions& policy,
                                                  size_t new_hash,
                                                  ctrl_t soo_slot_ctrl);

// PrepareInsert for small tables (is_small()==true).
// Returns the new control and the new slot.
// Hash is only computed if the table is sampled or grew to large size
// (is_small()==false).
std::pair<ctrl_t*, void*> SmallNonSooPrepareInsert(
    CommonFields& common, const PolicyFunctions& policy,
    absl::FunctionRef<size_t()> get_hash);

// Resizes table with allocated slots and change the table seed.
// Tables with SOO enabled must have capacity > policy.soo_capacity.
// No sampling will be performed since table is already allocated.
void ResizeAllocatedTableWithSeedChange(CommonFields& common,
                                        const PolicyFunctions& policy,
                                        size_t new_capacity);

// ClearBackingArray clears the backing array, either modifying it in place,
// or creating a new one based on the value of "reuse".
// REQUIRES: c.capacity > 0
void ClearBackingArray(CommonFields& c, const PolicyFunctions& policy,
                       void* alloc, bool reuse, bool soo_enabled);

// Type-erased version of raw_hash_set::erase_meta_only.
void EraseMetaOnly(CommonFields& c, const ctrl_t* ctrl, size_t slot_size);

// For trivially relocatable types we use memcpy directly. This allows us to
// share the same function body for raw_hash_set instantiations that have the
// same slot size as long as they are relocatable.
// Separate function for relocating single slot cause significant binary bloat.
template <size_t SizeOfSlot>
ABSL_ATTRIBUTE_NOINLINE void TransferNRelocatable(void*, void* dst, void* src,
                                                  size_t count) {
  // TODO(b/382423690): Experiment with making specialization for power of 2 and
  // non power of 2. This would require passing the size of the slot.
  memcpy(dst, src, SizeOfSlot * count);
}

// Returns a pointer to `common`. This is used to implement type erased
// raw_hash_set::get_hash_ref_fn and raw_hash_set::get_alloc_ref_fn for the
// empty class cases.
void* GetRefForEmptyClass(CommonFields& common);

// Given the hash of a value not currently in the table and the first empty
// slot in the probe sequence, finds a viable slot index to insert it at.
//
// In case there's no space left, the table can be resized or rehashed
// (for tables with deleted slots, see FindInsertPositionWithGrowthOrRehash).
//
// In the case of absence of deleted slots and positive growth_left, the element
// can be inserted in the provided `target` position.
//
// When the table has deleted slots (according to GrowthInfo), the target
// position will be searched one more time using `find_first_non_full`.
//
// REQUIRES: Table is not SOO.
// REQUIRES: At least one non-full slot available.
// REQUIRES: `target` is a valid empty position to insert.
size_t PrepareInsertNonSoo(CommonFields& common, const PolicyFunctions& policy,
                           size_t hash, FindInfo target);

// A SwissTable.
//
// Policy: a policy defines how to perform different operations on
// the slots of the hashtable (see hash_policy_traits.h for the full interface
// of policy).
//
// Hash: a (possibly polymorphic) functor that hashes keys of the hashtable. The
// functor should accept a key and return size_t as hash. For best performance
// it is important that the hash function provides high entropy across all bits
// of the hash.
//
// Eq: a (possibly polymorphic) functor that compares two keys for equality. It
// should accept two (of possibly different type) keys and return a bool: true
// if they are equal, false if they are not. If two keys compare equal, then
// their hash values as defined by Hash MUST be equal.
//
// Allocator: an Allocator
// [https://en.cppreference.com/w/cpp/named_req/Allocator] with which
// the storage of the hashtable will be allocated and the elements will be
// constructed and destroyed.
template <class Policy, class Hash, class Eq, class Alloc>
class raw_hash_set {
  using PolicyTraits = hash_policy_traits<Policy>;
  using KeyArgImpl =
      KeyArg<IsTransparent<Eq>::value && IsTransparent<Hash>::value>;

 public:
  using init_type = typename PolicyTraits::init_type;
  using key_type = typename PolicyTraits::key_type;
  using allocator_type = Alloc;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using hasher = Hash;
  using key_equal = Eq;
  using policy_type = Policy;
  using value_type = typename PolicyTraits::value_type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = typename absl::allocator_traits<
      allocator_type>::template rebind_traits<value_type>::pointer;
  using const_pointer = typename absl::allocator_traits<
      allocator_type>::template rebind_traits<value_type>::const_pointer;

 private:
  // Alias used for heterogeneous lookup functions.
  // `key_arg<K>` evaluates to `K` when the functors are transparent and to
  // `key_type` otherwise. It permits template argument deduction on `K` for the
  // transparent case.
  template <class K>
  using key_arg = typename KeyArgImpl::template type<K, key_type>;

  using slot_type = typename PolicyTraits::slot_type;

  // TODO(b/289225379): we could add extra SOO space inside raw_hash_set
  // after CommonFields to allow inlining larger slot_types (e.g. std::string),
  // but it's a bit complicated if we want to support incomplete mapped_type in
  // flat_hash_map. We could potentially do this for flat_hash_set and for an
  // allowlist of `mapped_type`s of flat_hash_map that includes e.g. arithmetic
  // types, strings, cords, and pairs/tuples of allowlisted types.
  constexpr static bool SooEnabled() {
    return PolicyTraits::soo_enabled() &&
           sizeof(slot_type) <= sizeof(HeapOrSoo) &&
           alignof(slot_type) <= alignof(HeapOrSoo);
  }

  constexpr static size_t DefaultCapacity() {
    return SooEnabled() ? SooCapacity() : 0;
  }

  // Whether `size` fits in the SOO capacity of this table.
  bool fits_in_soo(size_t size) const {
    return SooEnabled() && size <= SooCapacity();
  }
  // Whether this table is in SOO mode or non-SOO mode.
  bool is_soo() const { return fits_in_soo(capacity()); }
  bool is_full_soo() const { return is_soo() && !empty(); }

  bool is_small() const { return common().is_small(); }

  // Give an early error when key_type is not hashable/eq.
  auto KeyTypeCanBeHashed(const Hash& h, const key_type& k) -> decltype(h(k));
  auto KeyTypeCanBeEq(const Eq& eq, const key_type& k) -> decltype(eq(k, k));

  using AllocTraits = absl::allocator_traits<allocator_type>;
  using SlotAlloc = typename absl::allocator_traits<
      allocator_type>::template rebind_alloc<slot_type>;
  // People are often sloppy with the exact type of their allocator (sometimes
  // it has an extra const or is missing the pair, but rebinds made it work
  // anyway).
  using CharAlloc =
      typename absl::allocator_traits<Alloc>::template rebind_alloc<char>;
  using SlotAllocTraits = typename absl::allocator_traits<
      allocator_type>::template rebind_traits<slot_type>;

  static_assert(std::is_lvalue_reference<reference>::value,
                "Policy::element() must return a reference");

  // An enabler for insert(T&&): T must be convertible to init_type or be the
  // same as [cv] value_type [ref].
  template <class T>
  using Insertable = absl::disjunction<
      std::is_same<absl::remove_cvref_t<reference>, absl::remove_cvref_t<T>>,
      std::is_convertible<T, init_type>>;
  template <class T>
  using IsNotBitField = std::is_pointer<T*>;

  // RequiresNotInit is a workaround for gcc prior to 7.1.
  // See https://godbolt.org/g/Y4xsUh.
  template <class T>
  using RequiresNotInit =
      typename std::enable_if<!std::is_same<T, init_type>::value, int>::type;

  template <class... Ts>
  using IsDecomposable = IsDecomposable<void, PolicyTraits, Hash, Eq, Ts...>;

  template <class T>
  using IsDecomposableAndInsertable =
      IsDecomposable<std::enable_if_t<Insertable<T>::value, T>>;

  // Evaluates to true if an assignment from the given type would require the
  // source object to remain alive for the life of the element.
  template <class U>
  using IsLifetimeBoundAssignmentFrom = std::conditional_t<
      policy_trait_element_is_owner<Policy>::value, std::false_type,
      type_traits_internal::IsLifetimeBoundAssignment<init_type, U>>;

 public:
  static_assert(std::is_same<pointer, value_type*>::value,
                "Allocators with custom pointer types are not supported");
  static_assert(std::is_same<const_pointer, const value_type*>::value,
                "Allocators with custom pointer types are not supported");

  class iterator : private HashSetIteratorGenerationInfo {
    friend class raw_hash_set;
    friend struct HashtableFreeFunctionsAccess;

   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = typename raw_hash_set::value_type;
    using reference =
        absl::conditional_t<PolicyTraits::constant_iterators::value,
                            const value_type&, value_type&>;
    using pointer = absl::remove_reference_t<reference>*;
    using difference_type = typename raw_hash_set::difference_type;

    iterator() {}

    // PRECONDITION: not an end() iterator.
    reference operator*() const {
      AssertIsFull(ctrl_, generation(), generation_ptr(), "operator*()");
      return unchecked_deref();
    }

    // PRECONDITION: not an end() iterator.
    pointer operator->() const {
      AssertIsFull(ctrl_, generation(), generation_ptr(), "operator->");
      return &operator*();
    }

    // PRECONDITION: not an end() iterator.
    iterator& operator++() {
      AssertIsFull(ctrl_, generation(), generation_ptr(), "operator++");
      ++ctrl_;
      ++slot_;
      skip_empty_or_deleted();
      if (ABSL_PREDICT_FALSE(*ctrl_ == ctrl_t::kSentinel)) ctrl_ = nullptr;
      return *this;
    }
    // PRECONDITION: not an end() iterator.
    iterator operator++(int) {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    friend bool operator==(const iterator& a, const iterator& b) {
      AssertIsValidForComparison(a.ctrl_, a.generation(), a.generation_ptr());
      AssertIsValidForComparison(b.ctrl_, b.generation(), b.generation_ptr());
      AssertSameContainer(a.ctrl_, b.ctrl_, a.slot_, b.slot_,
                          a.generation_ptr(), b.generation_ptr());
      return a.ctrl_ == b.ctrl_;
    }
    friend bool operator!=(const iterator& a, const iterator& b) {
      return !(a == b);
    }

   private:
    iterator(ctrl_t* ctrl, slot_type* slot,
             const GenerationType* generation_ptr)
        : HashSetIteratorGenerationInfo(generation_ptr),
          ctrl_(ctrl),
          slot_(slot) {
      // This assumption helps the compiler know that any non-end iterator is
      // not equal to any end iterator.
      ABSL_ASSUME(ctrl != nullptr);
    }
    // This constructor is used in begin() to avoid an MSan
    // use-of-uninitialized-value error. Delegating from this constructor to
    // the previous one doesn't avoid the error.
    iterator(ctrl_t* ctrl, MaybeInitializedPtr<void> slot,
             const GenerationType* generation_ptr)
        : HashSetIteratorGenerationInfo(generation_ptr),
          ctrl_(ctrl),
          slot_(to_slot(slot.get())) {
      // This assumption helps the compiler know that any non-end iterator is
      // not equal to any end iterator.
      ABSL_ASSUME(ctrl != nullptr);
    }
    // For end() iterators.
    explicit iterator(const GenerationType* generation_ptr)
        : HashSetIteratorGenerationInfo(generation_ptr), ctrl_(nullptr) {}

    // Fixes up `ctrl_` to point to a full or sentinel by advancing `ctrl_` and
    // `slot_` until they reach one.
    void skip_empty_or_deleted() {
      while (IsEmptyOrDeleted(*ctrl_)) {
        uint32_t shift =
            GroupFullEmptyOrDeleted{ctrl_}.CountLeadingEmptyOrDeleted();
        ctrl_ += shift;
        slot_ += shift;
      }
    }

    ctrl_t* control() const { return ctrl_; }
    slot_type* slot() const { return slot_; }

    // We use DefaultIterControl() for default-constructed iterators so that
    // they can be distinguished from end iterators, which have nullptr ctrl_.
    ctrl_t* ctrl_ = DefaultIterControl();
    // To avoid uninitialized member warnings, put slot_ in an anonymous union.
    // The member is not initialized on singleton and end iterators.
    union {
      slot_type* slot_;
    };

    // An equality check which skips ABSL Hardening iterator invalidation
    // checks.
    // Should be used when the lifetimes of the iterators are well-enough
    // understood to prove that they cannot be invalid.
    bool unchecked_equals(const iterator& b) { return ctrl_ == b.control(); }

    // Dereferences the iterator without ABSL Hardening iterator invalidation
    // checks.
    reference unchecked_deref() const { return PolicyTraits::element(slot_); }
  };

  class const_iterator {
    friend class raw_hash_set;
    template <class Container, typename Enabler>
    friend struct absl::container_internal::hashtable_debug_internal::
        HashtableDebugAccess;

   public:
    using iterator_category = typename iterator::iterator_category;
    using value_type = typename raw_hash_set::value_type;
    using reference = typename raw_hash_set::const_reference;
    using pointer = typename raw_hash_set::const_pointer;
    using difference_type = typename raw_hash_set::difference_type;

    const_iterator() = default;
    // Implicit construction from iterator.
    const_iterator(iterator i) : inner_(std::move(i)) {}  // NOLINT

    reference operator*() const { return *inner_; }
    pointer operator->() const { return inner_.operator->(); }

    const_iterator& operator++() {
      ++inner_;
      return *this;
    }
    const_iterator operator++(int) { return inner_++; }

    friend bool operator==(const const_iterator& a, const const_iterator& b) {
      return a.inner_ == b.inner_;
    }
    friend bool operator!=(const const_iterator& a, const const_iterator& b) {
      return !(a == b);
    }

   private:
    const_iterator(const ctrl_t* ctrl, const slot_type* slot,
                   const GenerationType* gen)
        : inner_(const_cast<ctrl_t*>(ctrl), const_cast<slot_type*>(slot), gen) {
    }
    ctrl_t* control() const { return inner_.control(); }
    slot_type* slot() const { return inner_.slot(); }

    iterator inner_;

    bool unchecked_equals(const const_iterator& b) {
      return inner_.unchecked_equals(b.inner_);
    }
  };

  using node_type = node_handle<Policy, hash_policy_traits<Policy>, Alloc>;
  using insert_return_type = InsertReturnType<iterator, node_type>;

  // Note: can't use `= default` due to non-default noexcept (causes
  // problems for some compilers). NOLINTNEXTLINE
  raw_hash_set() noexcept(
      std::is_nothrow_default_constructible<hasher>::value &&
      std::is_nothrow_default_constructible<key_equal>::value &&
      std::is_nothrow_default_constructible<allocator_type>::value) {}

  explicit raw_hash_set(
      size_t bucket_count, const hasher& hash = hasher(),
      const key_equal& eq = key_equal(),
      const allocator_type& alloc = allocator_type())
      : settings_(CommonFields::CreateDefault<SooEnabled()>(), hash, eq,
                  alloc) {
    if (bucket_count > DefaultCapacity()) {
      ReserveEmptyNonAllocatedTableToFitBucketCount(
          common(), GetPolicyFunctions(), bucket_count);
    }
  }

  raw_hash_set(size_t bucket_count, const hasher& hash,
               const allocator_type& alloc)
      : raw_hash_set(bucket_count, hash, key_equal(), alloc) {}

  raw_hash_set(size_t bucket_count, const allocator_type& alloc)
      : raw_hash_set(bucket_count, hasher(), key_equal(), alloc) {}

  explicit raw_hash_set(const allocator_type& alloc)
      : raw_hash_set(0, hasher(), key_equal(), alloc) {}

  template <class InputIter>
  raw_hash_set(InputIter first, InputIter last, size_t bucket_count = 0,
               const hasher& hash = hasher(), const key_equal& eq = key_equal(),
               const allocator_type& alloc = allocator_type())
      : raw_hash_set(SelectBucketCountForIterRange(first, last, bucket_count),
                     hash, eq, alloc) {
    insert(first, last);
  }

  template <class InputIter>
  raw_hash_set(InputIter first, InputIter last, size_t bucket_count,
               const hasher& hash, const allocator_type& alloc)
      : raw_hash_set(first, last, bucket_count, hash, key_equal(), alloc) {}

  template <class InputIter>
  raw_hash_set(InputIter first, InputIter last, size_t bucket_count,
               const allocator_type& alloc)
      : raw_hash_set(first, last, bucket_count, hasher(), key_equal(), alloc) {}

  template <class InputIter>
  raw_hash_set(InputIter first, InputIter last, const allocator_type& alloc)
      : raw_hash_set(first, last, 0, hasher(), key_equal(), alloc) {}

  // Instead of accepting std::initializer_list<value_type> as the first
  // argument like std::unordered_set<value_type> does, we have two overloads
  // that accept std::initializer_list<T> and std::initializer_list<init_type>.
  // This is advantageous for performance.
  //
  //   // Turns {"abc", "def"} into std::initializer_list<std::string>, then
  //   // copies the strings into the set.
  //   std::unordered_set<std::string> s = {"abc", "def"};
  //
  //   // Turns {"abc", "def"} into std::initializer_list<const char*>, then
  //   // copies the strings into the set.
  //   absl::flat_hash_set<std::string> s = {"abc", "def"};
  //
  // The same trick is used in insert().
  //
  // The enabler is necessary to prevent this constructor from triggering where
  // the copy constructor is meant to be called.
  //
  //   absl::flat_hash_set<int> a, b{a};
  //
  // RequiresNotInit<T> is a workaround for gcc prior to 7.1.
  template <class T, RequiresNotInit<T> = 0,
            std::enable_if_t<Insertable<T>::value, int> = 0>
  raw_hash_set(std::initializer_list<T> init, size_t bucket_count = 0,
               const hasher& hash = hasher(), const key_equal& eq = key_equal(),
               const allocator_type& alloc = allocator_type())
      : raw_hash_set(init.begin(), init.end(), bucket_count, hash, eq, alloc) {}

  raw_hash_set(std::initializer_list<init_type> init, size_t bucket_count = 0,
               const hasher& hash = hasher(), const key_equal& eq = key_equal(),
               const allocator_type& alloc = allocator_type())
      : raw_hash_set(init.begin(), init.end(), bucket_count, hash, eq, alloc) {}

  template <class T, RequiresNotInit<T> = 0,
            std::enable_if_t<Insertable<T>::value, int> = 0>
  raw_hash_set(std::initializer_list<T> init, size_t bucket_count,
               const hasher& hash, const allocator_type& alloc)
      : raw_hash_set(init, bucket_count, hash, key_equal(), alloc) {}

  raw_hash_set(std::initializer_list<init_type> init, size_t bucket_count,
               const hasher& hash, const allocator_type& alloc)
      : raw_hash_set(init, bucket_count, hash, key_equal(), alloc) {}

  template <class T, RequiresNotInit<T> = 0,
            std::enable_if_t<Insertable<T>::value, int> = 0>
  raw_hash_set(std::initializer_list<T> init, size_t bucket_count,
               const allocator_type& alloc)
      : raw_hash_set(init, bucket_count, hasher(), key_equal(), alloc) {}

  raw_hash_set(std::initializer_list<init_type> init, size_t bucket_count,
               const allocator_type& alloc)
      : raw_hash_set(init, bucket_count, hasher(), key_equal(), alloc) {}

  template <class T, RequiresNotInit<T> = 0,
            std::enable_if_t<Insertable<T>::value, int> = 0>
  raw_hash_set(std::initializer_list<T> init, const allocator_type& alloc)
      : raw_hash_set(init, 0, hasher(), key_equal(), alloc) {}

  raw_hash_set(std::initializer_list<init_type> init,
               const allocator_type& alloc)
      : raw_hash_set(init, 0, hasher(), key_equal(), alloc) {}

  raw_hash_set(const raw_hash_set& that)
      : raw_hash_set(that, AllocTraits::select_on_container_copy_construction(
                               allocator_type(that.char_alloc_ref()))) {}

  raw_hash_set(const raw_hash_set& that, const allocator_type& a)
      : raw_hash_set(0, that.hash_ref(), that.eq_ref(), a) {
    that.AssertNotDebugCapacity();
    if (that.empty()) return;
    Copy(common(), GetPolicyFunctions(), that.common(),
         [this](void* dst, const void* src) {
           // TODO(b/413598253): type erase for trivially copyable types via
           // PolicyTraits.
           construct(to_slot(dst),
                     PolicyTraits::element(
                         static_cast<slot_type*>(const_cast<void*>(src))));
         });
  }

  ABSL_ATTRIBUTE_NOINLINE raw_hash_set(raw_hash_set&& that) noexcept(
      std::is_nothrow_copy_constructible<hasher>::value &&
      std::is_nothrow_copy_constructible<key_equal>::value &&
      std::is_nothrow_copy_constructible<allocator_type>::value)
      :  // Hash, equality and allocator are copied instead of moved because
         // `that` must be left valid. If Hash is std::function<Key>, moving it
         // would create a nullptr functor that cannot be called.
         // Note: we avoid using exchange for better generated code.
        settings_(PolicyTraits::transfer_uses_memcpy() || !that.is_full_soo()
                      ? std::move(that.common())
                      : CommonFields{full_soo_tag_t{}},
                  that.hash_ref(), that.eq_ref(), that.char_alloc_ref()) {
    if (!PolicyTraits::transfer_uses_memcpy() && that.is_full_soo()) {
      transfer(soo_slot(), that.soo_slot());
    }
    that.common() = CommonFields::CreateDefault<SooEnabled()>();
    annotate_for_bug_detection_on_move(that);
  }

  raw_hash_set(raw_hash_set&& that, const allocator_type& a)
      : settings_(CommonFields::CreateDefault<SooEnabled()>(), that.hash_ref(),
                  that.eq_ref(), a) {
    if (CharAlloc(a) == that.char_alloc_ref()) {
      swap_common(that);
      annotate_for_bug_detection_on_move(that);
    } else {
      move_elements_allocs_unequal(std::move(that));
    }
  }

  raw_hash_set& operator=(const raw_hash_set& that) {
    that.AssertNotDebugCapacity();
    if (ABSL_PREDICT_FALSE(this == &that)) return *this;
    constexpr bool propagate_alloc =
        AllocTraits::propagate_on_container_copy_assignment::value;
    // TODO(ezb): maybe avoid allocating a new backing array if this->capacity()
    // is an exact match for that.size(). If this->capacity() is too big, then
    // it would make iteration very slow to reuse the allocation. Maybe we can
    // do the same heuristic as clear() and reuse if it's small enough.
    allocator_type alloc(propagate_alloc ? that.char_alloc_ref()
                                         : char_alloc_ref());
    raw_hash_set tmp(that, alloc);
    // NOLINTNEXTLINE: not returning *this for performance.
    return assign_impl<propagate_alloc>(std::move(tmp));
  }

  raw_hash_set& operator=(raw_hash_set&& that) noexcept(
      absl::allocator_traits<allocator_type>::is_always_equal::value &&
      std::is_nothrow_move_assignable<hasher>::value &&
      std::is_nothrow_move_assignable<key_equal>::value) {
    // TODO(sbenza): We should only use the operations from the noexcept clause
    // to make sure we actually adhere to that contract.
    // NOLINTNEXTLINE: not returning *this for performance.
    return move_assign(
        std::move(that),
        typename AllocTraits::propagate_on_container_move_assignment());
  }

  ~raw_hash_set() {
    destructor_impl();
    if constexpr (SwisstableAssertAccessToDestroyedTable()) {
      common().set_capacity(InvalidCapacity::kDestroyed);
    }
  }

  iterator begin() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (ABSL_PREDICT_FALSE(empty())) return end();
    if (is_small()) return single_iterator();
    iterator it = {control(), common().slots_union(),
                   common().generation_ptr()};
    it.skip_empty_or_deleted();
    ABSL_SWISSTABLE_ASSERT(IsFull(*it.control()));
    return it;
  }
  iterator end() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    AssertNotDebugCapacity();
    return iterator(common().generation_ptr());
  }

  const_iterator begin() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return const_cast<raw_hash_set*>(this)->begin();
  }
  const_iterator end() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return const_cast<raw_hash_set*>(this)->end();
  }
  const_iterator cbegin() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return begin();
  }
  const_iterator cend() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return end(); }

  bool empty() const { return !size(); }
  size_t size() const {
    AssertNotDebugCapacity();
    return common().size();
  }
  size_t capacity() const {
    const size_t cap = common().capacity();
    // Compiler complains when using functions in ASSUME so use local variable.
    ABSL_ATTRIBUTE_UNUSED static constexpr size_t kDefaultCapacity =
        DefaultCapacity();
    ABSL_ASSUME(cap >= kDefaultCapacity);
    return cap;
  }
  size_t max_size() const { return MaxValidSize(sizeof(slot_type)); }

  ABSL_ATTRIBUTE_REINITIALIZES void clear() {
    if (SwisstableGenerationsEnabled() &&
        capacity() >= InvalidCapacity::kMovedFrom) {
      common().set_capacity(DefaultCapacity());
    }
    AssertNotDebugCapacity();
    // Iterating over this container is O(bucket_count()). When bucket_count()
    // is much greater than size(), iteration becomes prohibitively expensive.
    // For clear() it is more important to reuse the allocated array when the
    // container is small because allocation takes comparatively long time
    // compared to destruction of the elements of the container. So we pick the
    // largest bucket_count() threshold for which iteration is still fast and
    // past that we simply deallocate the array.
    const size_t cap = capacity();
    if (cap == 0) {
      // Already guaranteed to be empty; so nothing to do.
    } else if (is_small()) {
      if (!empty()) {
        destroy(single_slot());
        decrement_small_size();
      }
    } else {
      destroy_slots();
      clear_backing_array(/*reuse=*/cap < 128);
    }
    common().set_reserved_growth(0);
    common().set_reservation_size(0);
  }

  // This overload kicks in when the argument is an rvalue of insertable and
  // decomposable type other than init_type.
  //
  //   flat_hash_map<std::string, int> m;
  //   m.insert(std::make_pair("abc", 42));
  template <class T,
            int = std::enable_if_t<IsDecomposableAndInsertable<T>::value &&
                                       IsNotBitField<T>::value &&
                                       !IsLifetimeBoundAssignmentFrom<T>::value,
                                   int>()>
  std::pair<iterator, bool> insert(T&& value) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return emplace(std::forward<T>(value));
  }

  template <class T, int&...,
            std::enable_if_t<IsDecomposableAndInsertable<T>::value &&
                                 IsNotBitField<T>::value &&
                                 IsLifetimeBoundAssignmentFrom<T>::value,
                             int> = 0>
  std::pair<iterator, bool> insert(
      T&& value ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY(this))
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return this->template insert<T, 0>(std::forward<T>(value));
  }

  // This overload kicks in when the argument is a bitfield or an lvalue of
  // insertable and decomposable type.
  //
  //   union { int n : 1; };
  //   flat_hash_set<int> s;
  //   s.insert(n);
  //
  //   flat_hash_set<std::string> s;
  //   const char* p = "hello";
  //   s.insert(p);
  //
  template <class T, int = std::enable_if_t<
                         IsDecomposableAndInsertable<const T&>::value &&
                             !IsLifetimeBoundAssignmentFrom<const T&>::value,
                         int>()>
  std::pair<iterator, bool> insert(const T& value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return emplace(value);
  }
  template <class T, int&...,
            std::enable_if_t<IsDecomposableAndInsertable<const T&>::value &&
                                 IsLifetimeBoundAssignmentFrom<const T&>::value,
                             int> = 0>
  std::pair<iterator, bool> insert(
      const T& value ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY(this))
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return this->template insert<T, 0>(value);
  }

  // This overload kicks in when the argument is an rvalue of init_type. Its
  // purpose is to handle brace-init-list arguments.
  //
  //   flat_hash_map<std::string, int> s;
  //   s.insert({"abc", 42});
  std::pair<iterator, bool> insert(init_type&& value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND
#if __cplusplus >= 202002L
    requires(!IsLifetimeBoundAssignmentFrom<init_type>::value)
#endif
  {
    return emplace(std::move(value));
  }
#if __cplusplus >= 202002L
  std::pair<iterator, bool> insert(
      init_type&& value ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY(this))
      ABSL_ATTRIBUTE_LIFETIME_BOUND
    requires(IsLifetimeBoundAssignmentFrom<init_type>::value)
  {
    return emplace(std::move(value));
  }
#endif

  template <class T,
            int = std::enable_if_t<IsDecomposableAndInsertable<T>::value &&
                                       IsNotBitField<T>::value &&
                                       !IsLifetimeBoundAssignmentFrom<T>::value,
                                   int>()>
  iterator insert(const_iterator, T&& value) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return insert(std::forward<T>(value)).first;
  }
  template <class T, int&...,
            std::enable_if_t<IsDecomposableAndInsertable<T>::value &&
                                 IsNotBitField<T>::value &&
                                 IsLifetimeBoundAssignmentFrom<T>::value,
                             int> = 0>
  iterator insert(const_iterator hint,
                  T&& value ABSL_INTERNAL_ATTRIBUTE_CAPTURED_BY(this))
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return this->template insert<T, 0>(hint, std::forward<T>(value));
  }

  template <class T, std::enable_if_t<
                         IsDecomposableAndInsertable<const T&>::value, int> = 0>
  iterator insert(const_iterator,
                  const T& value) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return insert(value).first;
  }

  iterator insert(const_iterator,
                  init_type&& value) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return insert(std::move(value)).first;
  }

  template <class InputIt>
  void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) emplace(*first);
  }

  template <class T, RequiresNotInit<T> = 0,
            std::enable_if_t<Insertable<const T&>::value, int> = 0>
  void insert(std::initializer_list<T> ilist) {
    insert(ilist.begin(), ilist.end());
  }

  void insert(std::initializer_list<init_type> ilist) {
    insert(ilist.begin(), ilist.end());
  }

  insert_return_type insert(node_type&& node) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (!node) return {end(), false, node_type()};
    const auto& elem = PolicyTraits::element(CommonAccess::GetSlot(node));
    auto res = PolicyTraits::apply(
        InsertSlot<false>{*this, std::move(*CommonAccess::GetSlot(node))},
        elem);
    if (res.second) {
      CommonAccess::Reset(&node);
      return {res.first, true, node_type()};
    } else {
      return {res.first, false, std::move(node)};
    }
  }

  iterator insert(const_iterator,
                  node_type&& node) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto res = insert(std::move(node));
    node = std::move(res.node);
    return res.position;
  }

  // This overload kicks in if we can deduce the key from args. This enables us
  // to avoid constructing value_type if an entry with the same key already
  // exists.
  //
  // For example:
  //
  //   flat_hash_map<std::string, std::string> m = {{"abc", "def"}};
  //   // Creates no std::string copies and makes no heap allocations.
  //   m.emplace("abc", "xyz");
  template <class... Args,
            std::enable_if_t<IsDecomposable<Args...>::value, int> = 0>
  std::pair<iterator, bool> emplace(Args&&... args)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return PolicyTraits::apply(EmplaceDecomposable{*this},
                               std::forward<Args>(args)...);
  }

  // This overload kicks in if we cannot deduce the key from args. It constructs
  // value_type unconditionally and then either moves it into the table or
  // destroys.
  template <class... Args,
            std::enable_if_t<!IsDecomposable<Args...>::value, int> = 0>
  std::pair<iterator, bool> emplace(Args&&... args)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    alignas(slot_type) unsigned char raw[sizeof(slot_type)];
    slot_type* slot = to_slot(&raw);

    construct(slot, std::forward<Args>(args)...);
    const auto& elem = PolicyTraits::element(slot);
    return PolicyTraits::apply(InsertSlot<true>{*this, std::move(*slot)}, elem);
  }

  template <class... Args>
  iterator emplace_hint(const_iterator,
                        Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return emplace(std::forward<Args>(args)...).first;
  }

  // Extension API: support for lazy emplace.
  //
  // Looks up key in the table. If found, returns the iterator to the element.
  // Otherwise calls `f` with one argument of type `raw_hash_set::constructor`,
  // and returns an iterator to the new element.
  //
  // `f` must abide by several restrictions:
  //  - it MUST call `raw_hash_set::constructor` with arguments as if a
  //    `raw_hash_set::value_type` is constructed,
  //  - it MUST NOT access the container before the call to
  //    `raw_hash_set::constructor`, and
  //  - it MUST NOT erase the lazily emplaced element.
  // Doing any of these is undefined behavior.
  //
  // For example:
  //
  //   std::unordered_set<ArenaString> s;
  //   // Makes ArenaStr even if "abc" is in the map.
  //   s.insert(ArenaString(&arena, "abc"));
  //
  //   flat_hash_set<ArenaStr> s;
  //   // Makes ArenaStr only if "abc" is not in the map.
  //   s.lazy_emplace("abc", [&](const constructor& ctor) {
  //     ctor(&arena, "abc");
  //   });
  //
  // WARNING: This API is currently experimental. If there is a way to implement
  // the same thing with the rest of the API, prefer that.
  class constructor {
    friend class raw_hash_set;

   public:
    template <class... Args>
    void operator()(Args&&... args) const {
      ABSL_SWISSTABLE_ASSERT(*slot_);
      PolicyTraits::construct(alloc_, *slot_, std::forward<Args>(args)...);
      *slot_ = nullptr;
    }

   private:
    constructor(allocator_type* a, slot_type** slot) : alloc_(a), slot_(slot) {}

    allocator_type* alloc_;
    slot_type** slot_;
  };

  template <class K = key_type, class F>
  iterator lazy_emplace(const key_arg<K>& key,
                        F&& f) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto res = find_or_prepare_insert(key);
    if (res.second) {
      slot_type* slot = res.first.slot();
      allocator_type alloc(char_alloc_ref());
      std::forward<F>(f)(constructor(&alloc, &slot));
      ABSL_SWISSTABLE_ASSERT(!slot);
    }
    return res.first;
  }

  // Extension API: support for heterogeneous keys.
  //
  //   std::unordered_set<std::string> s;
  //   // Turns "abc" into std::string.
  //   s.erase("abc");
  //
  //   flat_hash_set<std::string> s;
  //   // Uses "abc" directly without copying it into std::string.
  //   s.erase("abc");
  template <class K = key_type>
  size_type erase(const key_arg<K>& key) {
    auto it = find(key);
    if (it == end()) return 0;
    erase(it);
    return 1;
  }

  // Erases the element pointed to by `it`. Unlike `std::unordered_set::erase`,
  // this method returns void to reduce algorithmic complexity to O(1). The
  // iterator is invalidated so any increment should be done before calling
  // erase (e.g. `erase(it++)`).
  void erase(const_iterator cit) { erase(cit.inner_); }

  // This overload is necessary because otherwise erase<K>(const K&) would be
  // a better match if non-const iterator is passed as an argument.
  void erase(iterator it) {
    ABSL_SWISSTABLE_ASSERT(capacity() > 0);
    AssertNotDebugCapacity();
    AssertIsFull(it.control(), it.generation(), it.generation_ptr(), "erase()");
    destroy(it.slot());
    erase_meta_only(it);
  }

  iterator erase(const_iterator first,
                 const_iterator last) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    AssertNotDebugCapacity();
    // We check for empty first because clear_backing_array requires that
    // capacity() > 0 as a precondition.
    if (empty()) return end();
    if (first == last) return last.inner_;
    if (is_small()) {
      destroy(single_slot());
      erase_meta_only(single_iterator());
      return end();
    }
    if (first == begin() && last == end()) {
      // TODO(ezb): we access control bytes in destroy_slots so it could make
      // sense to combine destroy_slots and clear_backing_array to avoid cache
      // misses when the table is large. Note that we also do this in clear().
      destroy_slots();
      clear_backing_array(/*reuse=*/true);
      common().set_reserved_growth(common().reservation_size());
      return end();
    }
    while (first != last) {
      erase(first++);
    }
    return last.inner_;
  }

  // Moves elements from `src` into `this`.
  // If the element already exists in `this`, it is left unmodified in `src`.
  template <typename H, typename E>
  void merge(raw_hash_set<Policy, H, E, Alloc>& src) {  // NOLINT
    AssertNotDebugCapacity();
    src.AssertNotDebugCapacity();
    assert(this != &src);
    // Returns whether insertion took place.
    const auto insert_slot = [this](slot_type* src_slot) {
      return PolicyTraits::apply(InsertSlot<false>{*this, std::move(*src_slot)},
                                 PolicyTraits::element(src_slot))
          .second;
    };

    if (src.is_small()) {
      if (src.empty()) return;
      if (insert_slot(src.single_slot()))
        src.erase_meta_only(src.single_iterator());
      return;
    }
    for (auto it = src.begin(), e = src.end(); it != e;) {
      auto next = std::next(it);
      if (insert_slot(it.slot())) src.erase_meta_only(it);
      it = next;
    }
  }

  template <typename H, typename E>
  void merge(raw_hash_set<Policy, H, E, Alloc>&& src) {
    merge(src);
  }

  node_type extract(const_iterator position) {
    AssertNotDebugCapacity();
    AssertIsFull(position.control(), position.inner_.generation(),
                 position.inner_.generation_ptr(), "extract()");
    allocator_type alloc(char_alloc_ref());
    auto node = CommonAccess::Transfer<node_type>(alloc, position.slot());
    erase_meta_only(position);
    return node;
  }

  template <class K = key_type,
            std::enable_if_t<!std::is_same<K, iterator>::value, int> = 0>
  node_type extract(const key_arg<K>& key) {
    auto it = find(key);
    return it == end() ? node_type() : extract(const_iterator{it});
  }

  void swap(raw_hash_set& that) noexcept(
      IsNoThrowSwappable<hasher>() && IsNoThrowSwappable<key_equal>() &&
      IsNoThrowSwappable<allocator_type>(
          typename AllocTraits::propagate_on_container_swap{})) {
    AssertNotDebugCapacity();
    that.AssertNotDebugCapacity();
    using std::swap;
    swap_common(that);
    swap(hash_ref(), that.hash_ref());
    swap(eq_ref(), that.eq_ref());
    SwapAlloc(char_alloc_ref(), that.char_alloc_ref(),
              typename AllocTraits::propagate_on_container_swap{});
  }

  void rehash(size_t n) { Rehash(common(), GetPolicyFunctions(), n); }

  void reserve(size_t n) {
    if (ABSL_PREDICT_TRUE(n > DefaultCapacity())) {
      ReserveTableToFitNewSize(common(), GetPolicyFunctions(), n);
    }
  }

  // Extension API: support for heterogeneous keys.
  //
  //   std::unordered_set<std::string> s;
  //   // Turns "abc" into std::string.
  //   s.count("abc");
  //
  //   ch_set<std::string> s;
  //   // Uses "abc" directly without copying it into std::string.
  //   s.count("abc");
  template <class K = key_type>
  size_t count(const key_arg<K>& key) const {
    return find(key) == end() ? 0 : 1;
  }

  // Issues CPU prefetch instructions for the memory needed to find or insert
  // a key.  Like all lookup functions, this support heterogeneous keys.
  //
  // NOTE: This is a very low level operation and should not be used without
  // specific benchmarks indicating its importance.
  template <class K = key_type>
  void prefetch(const key_arg<K>& key) const {
    if (capacity() == DefaultCapacity()) return;
    (void)key;
    // Avoid probing if we won't be able to prefetch the addresses received.
#ifdef ABSL_HAVE_PREFETCH
    prefetch_heap_block();
    auto seq = probe(common(), hash_of(key));
    PrefetchToLocalCache(control() + seq.offset());
    PrefetchToLocalCache(slot_array() + seq.offset());
#endif  // ABSL_HAVE_PREFETCH
  }

  template <class K = key_type>
  ABSL_DEPRECATE_AND_INLINE()
  iterator find(const key_arg<K>& key,
                size_t) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return find(key);
  }
  // The API of find() has one extension: the type of the key argument doesn't
  // have to be key_type. This is so called heterogeneous key support.
  template <class K = key_type>
  iterator find(const key_arg<K>& key) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    AssertOnFind(key);
    if (is_small()) return find_small(key);
    prefetch_heap_block();
    return find_large(key, hash_of(key));
  }

  template <class K = key_type>
  ABSL_DEPRECATE_AND_INLINE()
  const_iterator find(const key_arg<K>& key,
                      size_t) const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return find(key);
  }
  template <class K = key_type>
  const_iterator find(const key_arg<K>& key) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return const_cast<raw_hash_set*>(this)->find(key);
  }

  template <class K = key_type>
  bool contains(const key_arg<K>& key) const {
    // Here neither the iterator returned by `find()` nor `end()` can be invalid
    // outside of potential thread-safety issues.
    // `find()`'s return value is constructed, used, and then destructed
    // all in this context.
    return !find(key).unchecked_equals(end());
  }

  template <class K = key_type>
  std::pair<iterator, iterator> equal_range(const key_arg<K>& key)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto it = find(key);
    if (it != end()) return {it, std::next(it)};
    return {it, it};
  }
  template <class K = key_type>
  std::pair<const_iterator, const_iterator> equal_range(
      const key_arg<K>& key) const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto it = find(key);
    if (it != end()) return {it, std::next(it)};
    return {it, it};
  }

  size_t bucket_count() const { return capacity(); }
  float load_factor() const {
    return capacity() ? static_cast<double>(size()) / capacity() : 0.0;
  }
  float max_load_factor() const { return 1.0f; }
  void max_load_factor(float) {
    // Does nothing.
  }

  hasher hash_function() const { return hash_ref(); }
  key_equal key_eq() const { return eq_ref(); }
  allocator_type get_allocator() const {
    return allocator_type(char_alloc_ref());
  }

  friend bool operator==(const raw_hash_set& a, const raw_hash_set& b) {
    if (a.size() != b.size()) return false;
    const raw_hash_set* outer = &a;
    const raw_hash_set* inner = &b;
    if (outer->capacity() > inner->capacity()) std::swap(outer, inner);
    for (const value_type& elem : *outer) {
      auto it = PolicyTraits::apply(FindElement{*inner}, elem);
      if (it == inner->end()) return false;
      // Note: we used key_equal to check for key equality in FindElement, but
      // we may need to do an additional comparison using
      // value_type::operator==. E.g. the keys could be equal and the
      // mapped_types could be unequal in a map or even in a set, key_equal
      // could ignore some fields that aren't ignored by operator==.
      static constexpr bool kKeyEqIsValueEq =
          std::is_same<key_type, value_type>::value &&
          std::is_same<key_equal, hash_default_eq<key_type>>::value;
      if (!kKeyEqIsValueEq && !(*it == elem)) return false;
    }
    return true;
  }

  friend bool operator!=(const raw_hash_set& a, const raw_hash_set& b) {
    return !(a == b);
  }

  template <typename H>
  friend typename std::enable_if<H::template is_hashable<value_type>::value,
                                 H>::type
  AbslHashValue(H h, const raw_hash_set& s) {
    return H::combine(H::combine_unordered(std::move(h), s.begin(), s.end()),
                      hash_internal::WeaklyMixedInteger{s.size()});
  }

  friend void swap(raw_hash_set& a,
                   raw_hash_set& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
  }

 private:
  template <class Container, typename Enabler>
  friend struct absl::container_internal::hashtable_debug_internal::
      HashtableDebugAccess;

  friend struct absl::container_internal::HashtableFreeFunctionsAccess;

  struct FindElement {
    template <class K, class... Args>
    const_iterator operator()(const K& key, Args&&...) const {
      return s.find(key);
    }
    const raw_hash_set& s;
  };

  struct EmplaceDecomposable {
    template <class K, class... Args>
    std::pair<iterator, bool> operator()(const K& key, Args&&... args) const {
      auto res = s.find_or_prepare_insert(key);
      if (res.second) {
        s.emplace_at(res.first, std::forward<Args>(args)...);
      }
      return res;
    }
    raw_hash_set& s;
  };

  template <bool do_destroy>
  struct InsertSlot {
    template <class K, class... Args>
    std::pair<iterator, bool> operator()(const K& key, Args&&...) && {
      auto res = s.find_or_prepare_insert(key);
      if (res.second) {
        s.transfer(res.first.slot(), &slot);
      } else if (do_destroy) {
        s.destroy(&slot);
      }
      return res;
    }
    raw_hash_set& s;
    // Constructed slot. Either moved into place or destroyed.
    slot_type&& slot;
  };

  template <typename... Args>
  inline void construct(slot_type* slot, Args&&... args) {
    common().RunWithReentrancyGuard([&] {
      allocator_type alloc(char_alloc_ref());
      PolicyTraits::construct(&alloc, slot, std::forward<Args>(args)...);
    });
  }
  inline void destroy(slot_type* slot) {
    common().RunWithReentrancyGuard([&] {
      allocator_type alloc(char_alloc_ref());
      PolicyTraits::destroy(&alloc, slot);
    });
  }
  inline void transfer(slot_type* to, slot_type* from) {
    common().RunWithReentrancyGuard([&] {
      allocator_type alloc(char_alloc_ref());
      PolicyTraits::transfer(&alloc, to, from);
    });
  }

  // TODO(b/289225379): consider having a helper class that has the impls for
  // SOO functionality.
  template <class K = key_type>
  iterator find_small(const key_arg<K>& key) {
    ABSL_SWISSTABLE_ASSERT(is_small());
    return empty() || !equal_to(key, single_slot()) ? end() : single_iterator();
  }

  template <class K = key_type>
  iterator find_large(const key_arg<K>& key, size_t hash) {
    ABSL_SWISSTABLE_ASSERT(!is_small());
    auto seq = probe(common(), hash);
    const h2_t h2 = H2(hash);
    const ctrl_t* ctrl = control();
    while (true) {
#ifndef ABSL_HAVE_MEMORY_SANITIZER
      absl::PrefetchToLocalCache(slot_array() + seq.offset());
#endif
      Group g{ctrl + seq.offset()};
      for (uint32_t i : g.Match(h2)) {
        if (ABSL_PREDICT_TRUE(equal_to(key, slot_array() + seq.offset(i))))
          return iterator_at(seq.offset(i));
      }
      if (ABSL_PREDICT_TRUE(g.MaskEmpty())) return end();
      seq.next();
      ABSL_SWISSTABLE_ASSERT(seq.index() <= capacity() && "full table!");
    }
  }

  // Returns true if the table needs to be sampled.
  // This should be called on insertion into an empty SOO table and in copy
  // construction when the size can fit in SOO capacity.
  bool should_sample_soo() const {
    ABSL_SWISSTABLE_ASSERT(is_soo());
    if (!ShouldSampleHashtablezInfoForAlloc<CharAlloc>()) return false;
    return ABSL_PREDICT_FALSE(ShouldSampleNextTable());
  }

  void clear_backing_array(bool reuse) {
    ABSL_SWISSTABLE_ASSERT(capacity() > DefaultCapacity());
    ClearBackingArray(common(), GetPolicyFunctions(), &char_alloc_ref(), reuse,
                      SooEnabled());
  }

  void destroy_slots() {
    ABSL_SWISSTABLE_ASSERT(!is_small());
    if (PolicyTraits::template destroy_is_trivial<Alloc>()) return;
    auto destroy_slot = [&](const ctrl_t*, void* slot) {
      this->destroy(static_cast<slot_type*>(slot));
    };
    if constexpr (SwisstableAssertAccessToDestroyedTable()) {
      CommonFields common_copy(non_soo_tag_t{}, this->common());
      common().set_capacity(InvalidCapacity::kDestroyed);
      IterateOverFullSlots(common_copy, sizeof(slot_type), destroy_slot);
      common().set_capacity(common_copy.capacity());
    } else {
      IterateOverFullSlots(common(), sizeof(slot_type), destroy_slot);
    }
  }

  void dealloc() {
    ABSL_SWISSTABLE_ASSERT(capacity() > DefaultCapacity());
    // Unpoison before returning the memory to the allocator.
    SanitizerUnpoisonMemoryRegion(slot_array(), sizeof(slot_type) * capacity());
    infoz().Unregister();
    DeallocateBackingArray<BackingArrayAlignment(alignof(slot_type)),
                           CharAlloc>(&char_alloc_ref(), capacity(), control(),
                                      sizeof(slot_type), alignof(slot_type),
                                      common().has_infoz());
  }

  void destructor_impl() {
    if (SwisstableGenerationsEnabled() &&
        capacity() >= InvalidCapacity::kMovedFrom) {
      return;
    }
    if (capacity() == 0) return;
    if (is_small()) {
      if (!empty()) {
        ABSL_SWISSTABLE_IGNORE_UNINITIALIZED(destroy(single_slot()));
      }
      if constexpr (SooEnabled()) return;
    } else {
      destroy_slots();
    }
    dealloc();
  }

  // Erases, but does not destroy, the value pointed to by `it`.
  //
  // This merely updates the pertinent control byte. This can be used in
  // conjunction with Policy::transfer to move the object to another place.
  void erase_meta_only(const_iterator it) {
    if (is_soo()) {
      common().set_empty_soo();
      return;
    }
    EraseMetaOnly(common(), it.control(), sizeof(slot_type));
  }

  template <class K>
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool equal_to(const K& key,
                                             slot_type* slot) const {
    return PolicyTraits::apply(EqualElement<K, key_equal>{key, eq_ref()},
                               PolicyTraits::element(slot));
  }
  template <class K>
  ABSL_ATTRIBUTE_ALWAYS_INLINE size_t hash_of(const K& key) const {
    return HashElement<hasher>{hash_ref()}(key);
  }
  ABSL_ATTRIBUTE_ALWAYS_INLINE size_t hash_of(slot_type* slot) const {
    return PolicyTraits::apply(HashElement<hasher>{hash_ref()},
                               PolicyTraits::element(slot));
  }

  // Casting directly from e.g. char* to slot_type* can cause compilation errors
  // on objective-C. This function converts to void* first, avoiding the issue.
  static ABSL_ATTRIBUTE_ALWAYS_INLINE slot_type* to_slot(void* buf) {
    return static_cast<slot_type*>(buf);
  }

  // Requires that lhs does not have a full SOO slot.
  static void move_common(bool rhs_is_full_soo, CharAlloc& rhs_alloc,
                          CommonFields& lhs, CommonFields&& rhs) {
    if (PolicyTraits::transfer_uses_memcpy() || !rhs_is_full_soo) {
      lhs = std::move(rhs);
    } else {
      lhs.move_non_heap_or_soo_fields(rhs);
      rhs.RunWithReentrancyGuard([&] {
        lhs.RunWithReentrancyGuard([&] {
          PolicyTraits::transfer(&rhs_alloc, to_slot(lhs.soo_data()),
                                 to_slot(rhs.soo_data()));
        });
      });
    }
  }

  // Swaps common fields making sure to avoid memcpy'ing a full SOO slot if we
  // aren't allowed to do so.
  void swap_common(raw_hash_set& that) {
    using std::swap;
    if (PolicyTraits::transfer_uses_memcpy()) {
      swap(common(), that.common());
      return;
    }
    CommonFields tmp = CommonFields(uninitialized_tag_t{});
    const bool that_is_full_soo = that.is_full_soo();
    move_common(that_is_full_soo, that.char_alloc_ref(), tmp,
                std::move(that.common()));
    move_common(is_full_soo(), char_alloc_ref(), that.common(),
                std::move(common()));
    move_common(that_is_full_soo, that.char_alloc_ref(), common(),
                std::move(tmp));
  }

  void annotate_for_bug_detection_on_move(
      ABSL_ATTRIBUTE_UNUSED raw_hash_set& that) {
    // We only enable moved-from validation when generations are enabled (rather
    // than using NDEBUG) to avoid issues in which NDEBUG is enabled in some
    // translation units but not in others.
    if (SwisstableGenerationsEnabled()) {
      that.common().set_capacity(this == &that ? InvalidCapacity::kSelfMovedFrom
                                               : InvalidCapacity::kMovedFrom);
    }
    if (!SwisstableGenerationsEnabled() || capacity() == DefaultCapacity() ||
        capacity() > kAboveMaxValidCapacity) {
      return;
    }
    common().increment_generation();
    if (!empty() && common().should_rehash_for_bug_detection_on_move()) {
      ResizeAllocatedTableWithSeedChange(common(), GetPolicyFunctions(),
                                         capacity());
    }
  }

  template <bool propagate_alloc>
  raw_hash_set& assign_impl(raw_hash_set&& that) {
    // We don't bother checking for this/that aliasing. We just need to avoid
    // breaking the invariants in that case.
    destructor_impl();
    move_common(that.is_full_soo(), that.char_alloc_ref(), common(),
                std::move(that.common()));
    hash_ref() = that.hash_ref();
    eq_ref() = that.eq_ref();
    CopyAlloc(char_alloc_ref(), that.char_alloc_ref(),
              std::integral_constant<bool, propagate_alloc>());
    that.common() = CommonFields::CreateDefault<SooEnabled()>();
    annotate_for_bug_detection_on_move(that);
    return *this;
  }

  raw_hash_set& move_elements_allocs_unequal(raw_hash_set&& that) {
    const size_t size = that.size();
    if (size == 0) return *this;
    reserve(size);
    for (iterator it = that.begin(); it != that.end(); ++it) {
      insert(std::move(PolicyTraits::element(it.slot())));
      that.destroy(it.slot());
    }
    if (!that.is_soo()) that.dealloc();
    that.common() = CommonFields::CreateDefault<SooEnabled()>();
    annotate_for_bug_detection_on_move(that);
    return *this;
  }

  raw_hash_set& move_assign(raw_hash_set&& that,
                            std::true_type /*propagate_alloc*/) {
    return assign_impl<true>(std::move(that));
  }
  raw_hash_set& move_assign(raw_hash_set&& that,
                            std::false_type /*propagate_alloc*/) {
    if (char_alloc_ref() == that.char_alloc_ref()) {
      return assign_impl<false>(std::move(that));
    }
    // Aliasing can't happen here because allocs would compare equal above.
    assert(this != &that);
    destructor_impl();
    // We can't take over that's memory so we need to move each element.
    // While moving elements, this should have that's hash/eq so copy hash/eq
    // before moving elements.
    hash_ref() = that.hash_ref();
    eq_ref() = that.eq_ref();
    return move_elements_allocs_unequal(std::move(that));
  }

  template <class K>
  std::pair<iterator, bool> find_or_prepare_insert_soo(const K& key) {
    ABSL_SWISSTABLE_ASSERT(is_soo());
    ctrl_t soo_slot_ctrl;
    if (empty()) {
      if (!should_sample_soo()) {
        common().set_full_soo();
        return {single_iterator(), true};
      }
      soo_slot_ctrl = ctrl_t::kEmpty;
    } else if (equal_to(key, single_slot())) {
      return {single_iterator(), false};
    } else {
      soo_slot_ctrl = static_cast<ctrl_t>(H2(hash_of(single_slot())));
    }
    ABSL_SWISSTABLE_ASSERT(capacity() == 1);
    const size_t hash = hash_of(key);
    constexpr bool kUseMemcpy =
        PolicyTraits::transfer_uses_memcpy() && SooEnabled();
    size_t index = GrowSooTableToNextCapacityAndPrepareInsert<
        kUseMemcpy ? OptimalMemcpySizeForSooSlotTransfer(sizeof(slot_type)) : 0,
        kUseMemcpy>(common(), GetPolicyFunctions(), hash, soo_slot_ctrl);
    return {iterator_at(index), true};
  }

  template <class K>
  std::pair<iterator, bool> find_or_prepare_insert_small(const K& key) {
    ABSL_SWISSTABLE_ASSERT(is_small());
    if constexpr (SooEnabled()) {
      return find_or_prepare_insert_soo(key);
    }
    if (!empty()) {
      if (equal_to(key, single_slot())) {
        return {single_iterator(), false};
      }
    }
    return {iterator_at_ptr(
                SmallNonSooPrepareInsert(common(), GetPolicyFunctions(),
                                         HashKey<hasher, K>{hash_ref(), key})),
            true};
  }

  template <class K>
  std::pair<iterator, bool> find_or_prepare_insert_large(const K& key) {
    ABSL_SWISSTABLE_ASSERT(!is_soo());
    prefetch_heap_block();
    const size_t hash = hash_of(key);
    auto seq = probe(common(), hash);
    const h2_t h2 = H2(hash);
    const ctrl_t* ctrl = control();
    while (true) {
#ifndef ABSL_HAVE_MEMORY_SANITIZER
      absl::PrefetchToLocalCache(slot_array() + seq.offset());
#endif
      Group g{ctrl + seq.offset()};
      for (uint32_t i : g.Match(h2)) {
        if (ABSL_PREDICT_TRUE(equal_to(key, slot_array() + seq.offset(i))))
          return {iterator_at(seq.offset(i)), false};
      }
      auto mask_empty = g.MaskEmpty();
      if (ABSL_PREDICT_TRUE(mask_empty)) {
        size_t target = seq.offset(mask_empty.LowestBitSet());
        return {iterator_at(PrepareInsertNonSoo(common(), GetPolicyFunctions(),
                                                hash,
                                                FindInfo{target, seq.index()})),
                true};
      }
      seq.next();
      ABSL_SWISSTABLE_ASSERT(seq.index() <= capacity() && "full table!");
    }
  }

 protected:
  // Asserts for correctness that we run on find/find_or_prepare_insert.
  template <class K>
  void AssertOnFind(ABSL_ATTRIBUTE_UNUSED const K& key) {
    AssertHashEqConsistent(key);
    AssertNotDebugCapacity();
  }

  // Asserts that the capacity is not a sentinel invalid value.
  void AssertNotDebugCapacity() const {
#ifdef NDEBUG
    if (!SwisstableGenerationsEnabled()) {
      return;
    }
#endif
    if (ABSL_PREDICT_TRUE(capacity() <
                          InvalidCapacity::kAboveMaxValidCapacity)) {
      return;
    }
    assert(capacity() != InvalidCapacity::kReentrance &&
           "Reentrant container access during element construction/destruction "
           "is not allowed.");
    if constexpr (SwisstableAssertAccessToDestroyedTable()) {
      if (capacity() == InvalidCapacity::kDestroyed) {
        ABSL_RAW_LOG(FATAL, "Use of destroyed hash table.");
      }
    }
    if (SwisstableGenerationsEnabled() &&
        ABSL_PREDICT_FALSE(capacity() >= InvalidCapacity::kMovedFrom)) {
      if (capacity() == InvalidCapacity::kSelfMovedFrom) {
        // If this log triggers, then a hash table was move-assigned to itself
        // and then used again later without being reinitialized.
        ABSL_RAW_LOG(FATAL, "Use of self-move-assigned hash table.");
      }
      ABSL_RAW_LOG(FATAL, "Use of moved-from hash table.");
    }
  }

  // Asserts that hash and equal functors provided by the user are consistent,
  // meaning that `eq(k1, k2)` implies `hash(k1)==hash(k2)`.
  template <class K>
  void AssertHashEqConsistent(const K& key) {
#ifdef NDEBUG
    return;
#endif
    // If the hash/eq functors are known to be consistent, then skip validation.
    if (std::is_same<hasher, absl::container_internal::StringHash>::value &&
        std::is_same<key_equal, absl::container_internal::StringEq>::value) {
      return;
    }
    if (std::is_scalar<key_type>::value &&
        std::is_same<hasher, absl::Hash<key_type>>::value &&
        std::is_same<key_equal, std::equal_to<key_type>>::value) {
      return;
    }
    if (empty()) return;

    const size_t hash_of_arg = hash_of(key);
    const auto assert_consistent = [&](const ctrl_t*, void* slot) {
      const bool is_key_equal = equal_to(key, to_slot(slot));
      if (!is_key_equal) return;

      ABSL_ATTRIBUTE_UNUSED const bool is_hash_equal =
          hash_of_arg == hash_of(to_slot(slot));
      assert((!is_key_equal || is_hash_equal) &&
             "eq(k1, k2) must imply that hash(k1) == hash(k2). "
             "hash/eq functors are inconsistent.");
    };

    if (is_small()) {
      assert_consistent(/*unused*/ nullptr, single_slot());
      return;
    }
    // We only do validation for small tables so that it's constant time.
    if (capacity() > 16) return;
    IterateOverFullSlots(common(), sizeof(slot_type), assert_consistent);
  }

  // Attempts to find `key` in the table; if it isn't found, returns an iterator
  // where the value can be inserted into, with the control byte already set to
  // `key`'s H2. Returns a bool indicating whether an insertion can take place.
  template <class K>
  std::pair<iterator, bool> find_or_prepare_insert(const K& key) {
    AssertOnFind(key);
    if (is_small()) return find_or_prepare_insert_small(key);
    return find_or_prepare_insert_large(key);
  }

  // Constructs the value in the space pointed by the iterator. This only works
  // after an unsuccessful find_or_prepare_insert() and before any other
  // modifications happen in the raw_hash_set.
  //
  // PRECONDITION: iter was returned from find_or_prepare_insert(k), where k is
  // the key decomposed from `forward<Args>(args)...`, and the bool returned by
  // find_or_prepare_insert(k) was true.
  // POSTCONDITION: *m.iterator_at(i) == value_type(forward<Args>(args)...).
  template <class... Args>
  void emplace_at(iterator iter, Args&&... args) {
    construct(iter.slot(), std::forward<Args>(args)...);

    // When is_small, find calls find_small and if size is 0, then it will
    // return an end iterator. This can happen in the raw_hash_set copy ctor.
    assert((is_small() ||
            PolicyTraits::apply(FindElement{*this}, *iter) == iter) &&
           "constructed value does not match the lookup key");
  }

  iterator iterator_at(size_t i) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return {control() + i, slot_array() + i, common().generation_ptr()};
  }
  const_iterator iterator_at(size_t i) const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return const_cast<raw_hash_set*>(this)->iterator_at(i);
  }
  iterator iterator_at_ptr(std::pair<ctrl_t*, void*> ptrs)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return {ptrs.first, to_slot(ptrs.second), common().generation_ptr()};
  }

  reference unchecked_deref(iterator it) { return it.unchecked_deref(); }

 private:
  friend struct RawHashSetTestOnlyAccess;

  // The number of slots we can still fill without needing to rehash.
  //
  // This is stored separately due to tombstones: we do not include tombstones
  // in the growth capacity, because we'd like to rehash when the table is
  // otherwise filled with tombstones: otherwise, probe sequences might get
  // unacceptably long without triggering a rehash. Callers can also force a
  // rehash via the standard `rehash(0)`, which will recompute this value as a
  // side-effect.
  //
  // See `CapacityToGrowth()`.
  size_t growth_left() const {
    ABSL_SWISSTABLE_ASSERT(!is_soo());
    return common().growth_left();
  }

  GrowthInfo& growth_info() {
    ABSL_SWISSTABLE_ASSERT(!is_soo());
    return common().growth_info();
  }
  GrowthInfo growth_info() const {
    ABSL_SWISSTABLE_ASSERT(!is_soo());
    return common().growth_info();
  }

  // Prefetch the heap-allocated memory region to resolve potential TLB and
  // cache misses. This is intended to overlap with execution of calculating the
  // hash for a key.
  void prefetch_heap_block() const {
    ABSL_SWISSTABLE_ASSERT(!is_soo());
#if ABSL_HAVE_BUILTIN(__builtin_prefetch) || defined(__GNUC__)
    __builtin_prefetch(control(), 0, 1);
#endif
  }

  CommonFields& common() { return settings_.template get<0>(); }
  const CommonFields& common() const { return settings_.template get<0>(); }

  ctrl_t* control() const {
    ABSL_SWISSTABLE_ASSERT(!is_soo());
    return common().control();
  }
  slot_type* slot_array() const {
    ABSL_SWISSTABLE_ASSERT(!is_soo());
    return static_cast<slot_type*>(common().slot_array());
  }
  slot_type* soo_slot() {
    ABSL_SWISSTABLE_ASSERT(is_soo());
    ABSL_SWISSTABLE_IGNORE_UNINITIALIZED_RETURN(
        static_cast<slot_type*>(common().soo_data()));
  }
  const slot_type* soo_slot() const {
    ABSL_SWISSTABLE_IGNORE_UNINITIALIZED_RETURN(
        const_cast<raw_hash_set*>(this)->soo_slot());
  }
  slot_type* single_slot() {
    ABSL_SWISSTABLE_ASSERT(is_small());
    return SooEnabled() ? soo_slot() : slot_array();
  }
  const slot_type* single_slot() const {
    return const_cast<raw_hash_set*>(this)->single_slot();
  }
  void decrement_small_size() {
    ABSL_SWISSTABLE_ASSERT(is_small());
    SooEnabled() ? common().set_empty_soo() : common().decrement_size();
    if (!SooEnabled()) {
      SanitizerPoisonObject(single_slot());
      growth_info().OverwriteFullAsEmpty();
    }
  }
  iterator single_iterator() {
    return {SooControl(), single_slot(), common().generation_ptr()};
  }
  const_iterator single_iterator() const {
    return const_cast<raw_hash_set*>(this)->single_iterator();
  }
  HashtablezInfoHandle infoz() {
    ABSL_SWISSTABLE_ASSERT(!is_soo());
    return common().infoz();
  }

  hasher& hash_ref() { return settings_.template get<1>(); }
  const hasher& hash_ref() const { return settings_.template get<1>(); }
  key_equal& eq_ref() { return settings_.template get<2>(); }
  const key_equal& eq_ref() const { return settings_.template get<2>(); }
  CharAlloc& char_alloc_ref() { return settings_.template get<3>(); }
  const CharAlloc& char_alloc_ref() const {
    return settings_.template get<3>();
  }

  static void* get_char_alloc_ref_fn(CommonFields& common) {
    auto* h = reinterpret_cast<raw_hash_set*>(&common);
    return &h->char_alloc_ref();
  }
  static void* get_hash_ref_fn(CommonFields& common) {
    auto* h = reinterpret_cast<raw_hash_set*>(&common);
    // TODO(b/397453582): Remove support for const hasher.
    return const_cast<std::remove_const_t<hasher>*>(&h->hash_ref());
  }
  static void transfer_n_slots_fn(void* set, void* dst, void* src,
                                  size_t count) {
    auto* src_slot = to_slot(src);
    auto* dst_slot = to_slot(dst);

    auto* h = static_cast<raw_hash_set*>(set);
    for (; count > 0; --count, ++src_slot, ++dst_slot) {
      h->transfer(dst_slot, src_slot);
    }
  }

  // TODO(b/382423690): Try to type erase entire function or at least type erase
  // by GetKey + Hash for memcpyable types.
  // TODO(b/382423690): Try to type erase for big slots: sizeof(slot_type) > 16.
  static void transfer_unprobed_elements_to_next_capacity_fn(
      CommonFields& common, const ctrl_t* old_ctrl, void* old_slots,
      void* probed_storage,
      void (*encode_probed_element)(void* probed_storage, h2_t h2,
                                    size_t source_offset, size_t h1)) {
    const size_t new_capacity = common.capacity();
    const size_t old_capacity = PreviousCapacity(new_capacity);
    ABSL_ASSUME(old_capacity + 1 >= Group::kWidth);
    ABSL_ASSUME((old_capacity + 1) % Group::kWidth == 0);

    auto* set = reinterpret_cast<raw_hash_set*>(&common);
    slot_type* old_slots_ptr = to_slot(old_slots);
    ctrl_t* new_ctrl = common.control();
    slot_type* new_slots = set->slot_array();

    const PerTableSeed seed = common.seed();

    for (size_t group_index = 0; group_index < old_capacity;
         group_index += Group::kWidth) {
      GroupFullEmptyOrDeleted old_g(old_ctrl + group_index);
      std::memset(new_ctrl + group_index, static_cast<int8_t>(ctrl_t::kEmpty),
                  Group::kWidth);
      std::memset(new_ctrl + group_index + old_capacity + 1,
                  static_cast<int8_t>(ctrl_t::kEmpty), Group::kWidth);
      // TODO(b/382423690): try to type erase everything outside of the loop.
      // We will share a lot of code in expense of one function call per group.
      for (auto in_fixed_group_index : old_g.MaskFull()) {
        size_t old_index = group_index + in_fixed_group_index;
        slot_type* old_slot = old_slots_ptr + old_index;
        // TODO(b/382423690): try to avoid entire hash calculation since we need
        // only one new bit of h1.
        size_t hash = set->hash_of(old_slot);
        size_t h1 = H1(hash, seed);
        h2_t h2 = H2(hash);
        size_t new_index = TryFindNewIndexWithoutProbing(
            h1, old_index, old_capacity, new_ctrl, new_capacity);
        // Note that encode_probed_element is allowed to use old_ctrl buffer
        // till and included the old_index.
        if (ABSL_PREDICT_FALSE(new_index == kProbedElementIndexSentinel)) {
          encode_probed_element(probed_storage, h2, old_index, h1);
          continue;
        }
        ABSL_SWISSTABLE_ASSERT((new_index & old_capacity) <= old_index);
        ABSL_SWISSTABLE_ASSERT(IsEmpty(new_ctrl[new_index]));
        new_ctrl[new_index] = static_cast<ctrl_t>(h2);
        auto* new_slot = new_slots + new_index;
        SanitizerUnpoisonMemoryRegion(new_slot, sizeof(slot_type));
        set->transfer(new_slot, old_slot);
        SanitizerPoisonMemoryRegion(old_slot, sizeof(slot_type));
      }
    }
  }

  static const PolicyFunctions& GetPolicyFunctions() {
    static_assert(sizeof(slot_type) <= (std::numeric_limits<uint32_t>::max)(),
                  "Slot size is too large. Use std::unique_ptr for value type "
                  "or use absl::node_hash_{map,set}.");
    static_assert(alignof(slot_type) <=
                  size_t{(std::numeric_limits<uint16_t>::max)()});
    static_assert(sizeof(key_type) <=
                  size_t{(std::numeric_limits<uint32_t>::max)()});
    static_assert(sizeof(value_type) <=
                  size_t{(std::numeric_limits<uint32_t>::max)()});
    static constexpr size_t kBackingArrayAlignment =
        BackingArrayAlignment(alignof(slot_type));
    static constexpr PolicyFunctions value = {
        static_cast<uint32_t>(sizeof(key_type)),
        static_cast<uint32_t>(sizeof(value_type)),
        static_cast<uint32_t>(sizeof(slot_type)),
        static_cast<uint16_t>(alignof(slot_type)), SooEnabled(),
        ShouldSampleHashtablezInfoForAlloc<CharAlloc>(),
        // TODO(b/328722020): try to type erase
        // for standard layout and alignof(Hash) <= alignof(CommonFields).
        std::is_empty_v<hasher> ? &GetRefForEmptyClass
                                : &raw_hash_set::get_hash_ref_fn,
        PolicyTraits::template get_hash_slot_fn<hasher>(),
        PolicyTraits::transfer_uses_memcpy()
            ? TransferNRelocatable<sizeof(slot_type)>
            : &raw_hash_set::transfer_n_slots_fn,
        std::is_empty_v<Alloc> ? &GetRefForEmptyClass
                               : &raw_hash_set::get_char_alloc_ref_fn,
        &AllocateBackingArray<kBackingArrayAlignment, CharAlloc>,
        &DeallocateBackingArray<kBackingArrayAlignment, CharAlloc>,
        &raw_hash_set::transfer_unprobed_elements_to_next_capacity_fn};
    return value;
  }

  // Bundle together CommonFields plus other objects which might be empty.
  // CompressedTuple will ensure that sizeof is not affected by any of the empty
  // fields that occur after CommonFields.
  absl::container_internal::CompressedTuple<CommonFields, hasher, key_equal,
                                            CharAlloc>
      settings_{CommonFields::CreateDefault<SooEnabled()>(), hasher{},
                key_equal{}, CharAlloc{}};
};

// Friend access for free functions in raw_hash_set.h.
struct HashtableFreeFunctionsAccess {
  template <class Predicate, typename Set>
  static typename Set::size_type EraseIf(Predicate& pred, Set* c) {
    if (c->empty()) {
      return 0;
    }
    if (c->is_small()) {
      auto it = c->single_iterator();
      if (!pred(*it)) {
        ABSL_SWISSTABLE_ASSERT(c->size() == 1 &&
                               "hash table was modified unexpectedly");
        return 0;
      }
      c->destroy(it.slot());
      c->erase_meta_only(it);
      return 1;
    }
    ABSL_ATTRIBUTE_UNUSED const size_t original_size_for_assert = c->size();
    size_t num_deleted = 0;
    using SlotType = typename Set::slot_type;
    IterateOverFullSlots(
        c->common(), sizeof(SlotType),
        [&](const ctrl_t* ctrl, void* slot_void) {
          auto* slot = static_cast<SlotType*>(slot_void);
          if (pred(Set::PolicyTraits::element(slot))) {
            c->destroy(slot);
            EraseMetaOnly(c->common(), ctrl, sizeof(*slot));
            ++num_deleted;
          }
        });
    // NOTE: IterateOverFullSlots allow removal of the current element, so we
    // verify the size additionally here.
    ABSL_SWISSTABLE_ASSERT(original_size_for_assert - num_deleted ==
                               c->size() &&
                           "hash table was modified unexpectedly");
    return num_deleted;
  }

  template <class Callback, typename Set>
  static void ForEach(Callback& cb, Set* c) {
    if (c->empty()) {
      return;
    }
    if (c->is_small()) {
      cb(*c->single_iterator());
      return;
    }
    using SlotType = typename Set::slot_type;
    using ElementTypeWithConstness = decltype(*c->begin());
    IterateOverFullSlots(
        c->common(), sizeof(SlotType), [&cb](const ctrl_t*, void* slot) {
          ElementTypeWithConstness& element =
              Set::PolicyTraits::element(static_cast<SlotType*>(slot));
          cb(element);
        });
  }
};

// Erases all elements that satisfy the predicate `pred` from the container `c`.
template <typename P, typename H, typename E, typename A, typename Predicate>
typename raw_hash_set<P, H, E, A>::size_type EraseIf(
    Predicate& pred, raw_hash_set<P, H, E, A>* c) {
  return HashtableFreeFunctionsAccess::EraseIf(pred, c);
}

// Calls `cb` for all elements in the container `c`.
template <typename P, typename H, typename E, typename A, typename Callback>
void ForEach(Callback& cb, raw_hash_set<P, H, E, A>* c) {
  return HashtableFreeFunctionsAccess::ForEach(cb, c);
}
template <typename P, typename H, typename E, typename A, typename Callback>
void ForEach(Callback& cb, const raw_hash_set<P, H, E, A>* c) {
  return HashtableFreeFunctionsAccess::ForEach(cb, c);
}

namespace hashtable_debug_internal {
template <typename Set>
struct HashtableDebugAccess<Set, absl::void_t<typename Set::raw_hash_set>> {
  using Traits = typename Set::PolicyTraits;
  using Slot = typename Traits::slot_type;

  static size_t GetNumProbes(const Set& set,
                             const typename Set::key_type& key) {
    if (set.is_soo()) return 0;
    size_t num_probes = 0;
    const size_t hash = set.hash_of(key);
    auto seq = probe(set.common(), hash);
    const h2_t h2 = H2(hash);
    const ctrl_t* ctrl = set.control();
    while (true) {
      container_internal::Group g{ctrl + seq.offset()};
      for (uint32_t i : g.Match(h2)) {
        if (set.equal_to(key, set.slot_array() + seq.offset(i)))
          return num_probes;
        ++num_probes;
      }
      if (g.MaskEmpty()) return num_probes;
      seq.next();
      ++num_probes;
    }
  }

  static size_t AllocatedByteSize(const Set& c) {
    size_t capacity = c.capacity();
    if (capacity == 0) return 0;
    size_t m =
        c.is_soo() ? 0 : c.common().alloc_size(sizeof(Slot), alignof(Slot));

    size_t per_slot = Traits::space_used(static_cast<const Slot*>(nullptr));
    if (per_slot != ~size_t{}) {
      m += per_slot * c.size();
    } else {
      for (auto it = c.begin(); it != c.end(); ++it) {
        m += Traits::space_used(it.slot());
      }
    }
    return m;
  }
};

}  // namespace hashtable_debug_internal

// Extern template instantiations reduce binary size and linker input size.
// Function definition is in raw_hash_set.cc.
extern template size_t GrowSooTableToNextCapacityAndPrepareInsert<0, false>(
    CommonFields&, const PolicyFunctions&, size_t, ctrl_t);
extern template size_t GrowSooTableToNextCapacityAndPrepareInsert<1, true>(
    CommonFields&, const PolicyFunctions&, size_t, ctrl_t);
extern template size_t GrowSooTableToNextCapacityAndPrepareInsert<4, true>(
    CommonFields&, const PolicyFunctions&, size_t, ctrl_t);
extern template size_t GrowSooTableToNextCapacityAndPrepareInsert<8, true>(
    CommonFields&, const PolicyFunctions&, size_t, ctrl_t);
#if UINTPTR_MAX == UINT64_MAX
extern template size_t GrowSooTableToNextCapacityAndPrepareInsert<16, true>(
    CommonFields&, const PolicyFunctions&, size_t, ctrl_t);
#endif

}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl

#undef ABSL_SWISSTABLE_ENABLE_GENERATIONS
#undef ABSL_SWISSTABLE_IGNORE_UNINITIALIZED
#undef ABSL_SWISSTABLE_IGNORE_UNINITIALIZED_RETURN
#undef ABSL_SWISSTABLE_ASSERT

#endif  // ABSL_CONTAINER_INTERNAL_RAW_HASH_SET_H_
