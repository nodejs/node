//===-- Perfect hash map for conversion functions ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_WCTYPE_PERFECT_HASH_MAP_H
#define LLVM_LIBC_SRC___SUPPORT_WCTYPE_PERFECT_HASH_MAP_H

#include "hdr/types/size_t.h"
#include "hdr/types/wint_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/expected.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string.h"
#include "src/__support/CPP/tuple.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/macros/config.h"
#include "src/__support/math/ceil.h"
#include "src/__support/math/log.h"
#include "src/__support/uint128.h"

#ifdef DEBUGDEBUG
#include "src/__support/OSUtil/io.h"
#endif

namespace LIBC_NAMESPACE_DECL {
namespace wctype_internal {

namespace ptrhash {

LIBC_INLINE_VAR constexpr size_t SHARDS = 1;

class FastRand {
public:
  // This seed value is very important for different inputs. Bad values are
  // known to cause compilation errors and/or incorrect computations in some
  // cases. Defaulted to 0xEF6F79ED30BA75A in the original implementation, but
  // this is not sufficient. 0x64a727ea04c46a32 is another viable seed.
  LIBC_INLINE constexpr FastRand() : seed(0xeec13c9f1362aa74) {}

  LIBC_INLINE constexpr uint8_t gen_byte() {
    return static_cast<uint8_t>(this->gen());
  }

  LIBC_INLINE constexpr uint64_t gen() {
    constexpr uint64_t WY_CONST_0 = 0x2d35'8dcc'aa6c'78a5;
    constexpr uint64_t WY_CONST_1 = 0x8bb8'4b93'962e'acc9;

    const uint64_t s = wrapping_add(seed, WY_CONST_0);
    seed = s;
    const UInt128 t =
        static_cast<UInt128>(s) * static_cast<UInt128>(s ^ WY_CONST_1);
    return static_cast<uint64_t>(t) ^ static_cast<uint64_t>(t >> 64);
  }

private:
  template <typename T> LIBC_INLINE static constexpr T wrapping_add(T a, T b) {
    while (b != 0) {
      T carry = a & b;
      a = a ^ b;
      b = carry << 1;
    }
    return a;
  }

private:
  uint64_t seed;
};

LIBC_INLINE_VAR constexpr auto BUCKET_IDX_NONE = ~static_cast<uint32_t>(0);

template <size_t MaxSize = 5> class BinaryHeap {
public:
  LIBC_INLINE constexpr BinaryHeap() = default;

  LIBC_INLINE constexpr void push(const cpp::tuple<size_t, uint32_t> &value) {
    if (current_size >= MaxSize)
      return;
    data[current_size++] = value;
  }

  LIBC_INLINE constexpr cpp::tuple<size_t, uint32_t> pop() {
    if (current_size == 0)
      return {};
    size_t max_idx = 0;
    for (size_t i = 1; i < current_size; ++i) {
      if (cmp(data[max_idx], data[i]))
        max_idx = i;
    }
    auto top = data[max_idx];
    data[max_idx] = data[current_size - 1];
    --current_size;
    return top;
  }

  LIBC_INLINE constexpr const cpp::tuple<size_t, uint32_t> &peek() const {
    size_t max_idx = 0;
    for (size_t i = 1; i < current_size; ++i) {
      if (cmp(data[max_idx], data[i]))
        max_idx = i;
    }
    return data[max_idx];
  }

  LIBC_INLINE constexpr bool empty() const { return current_size == 0; }

private:
  LIBC_INLINE static constexpr bool cmp(cpp::tuple<size_t, uint32_t> x,
                                        cpp::tuple<size_t, uint32_t> y) {
    return cpp::get<0>(x) < cpp::get<0>(y) ||
           (!(cpp::get<0>(y) < cpp::get<0>(x)) &&
            cpp::get<1>(x) < cpp::get<1>(y));
  }

private:
  cpp::array<cpp::tuple<size_t, uint32_t>, MaxSize> data{};
  size_t current_size{};
};

template <typename T> LIBC_INLINE constexpr bool is_power_of_two(T x) {
  static_assert(cpp::is_unsigned_v<T>,
                "is_power_of_two requires unsigned type");
  return x != 0 && (x & (x - 1)) == 0;
}

// Formula of Vigna, eps-cost-sharding: https://arxiv.org/abs/2503.18397
// (1-alpha)/2, so that on average we still have some room to play with.
LIBC_INLINE constexpr size_t get_parts(size_t n) {
  size_t parts = 0;
  const double eps = 0.01 / 2.0; // alpha here is 0.99 for linear configuration
  const double x = static_cast<double>(n) * eps * eps / 2.0;
  const size_t target_parts = static_cast<size_t>(x / math::log(x));
  // could be double or size_t depending on SHARDS value, so kept as auto.
  const auto parts_per_shard = target_parts / SHARDS;
  parts = ((parts_per_shard > 1) ? parts_per_shard : 1) * SHARDS;
  return parts;
}

LIBC_INLINE constexpr size_t get_slots_per_part(size_t keys_per_part) {
  size_t slots_per_part =
      static_cast<size_t>(static_cast<double>(keys_per_part) / 0.99);
  if (is_power_of_two(slots_per_part)) {
    slots_per_part += 1;
  }
  return slots_per_part;
}

template <size_t n> class PtrhashConfig {
public:
  static constexpr size_t PARTS = get_parts(n);
  static constexpr size_t KEYS_PER_PART = n / PARTS;
  static constexpr size_t PARTS_PER_SHARD = PARTS / SHARDS;
  static constexpr size_t SLOTS_PER_PART = get_slots_per_part(KEYS_PER_PART);
  static constexpr size_t SLOTS_TOTAL = PARTS * SLOTS_PER_PART;
  static constexpr size_t BUCKETS_PER_PART =
      math::ceil(KEYS_PER_PART / 3.0) + 3;
  static constexpr size_t BUCKETS_TOTAL = PARTS * BUCKETS_PER_PART;
};

// fxhash algorithm constant used in hashing numbers. Chosen for good randomness
// properties and to ensure stable, deterministic, and well-distributed outputs.
LIBC_INLINE_VAR constexpr uint64_t FXHASH_SEED = 0x517cc1b727220a95;

template <size_t n_, typename Key = wint_t,
          size_t parts_ = PtrhashConfig<n_>::PARTS,
          size_t parts_per_shard_ = PtrhashConfig<n_>::PARTS_PER_SHARD,
          size_t slots_total_ = PtrhashConfig<n_>::SLOTS_TOTAL,
          size_t buckets_total_ = PtrhashConfig<n_>::BUCKETS_TOTAL,
          size_t slots_ = PtrhashConfig<n_>::SLOTS_PER_PART,
          size_t buckets_ = PtrhashConfig<n_>::BUCKETS_PER_PART,
          typename F = cpp::array<uint32_t, slots_total_ - n_>,
          typename PilotsTypeV = cpp::array<uint8_t, buckets_total_>>
class PtrHash {
public:
  static_assert(
      cpp::is_same_v<PilotsTypeV, cpp::span<uint8_t>> ||
          cpp::is_same_v<PilotsTypeV, cpp::array<uint8_t, buckets_total_>>,
      "V must be a byte slice or byte vector");

  LIBC_INLINE constexpr PtrHash(uint64_t seed_, PilotsTypeV pilots_, F remap_)
      : seed(seed_), pilots(pilots_), remap(remap_) {}

  LIBC_INLINE constexpr PtrHash() = default;
  LIBC_INLINE constexpr PtrHash(const PtrHash &) = default;
  LIBC_INLINE constexpr PtrHash(PtrHash &&) = default;

  LIBC_INLINE constexpr PtrHash &operator=(const PtrHash &) = default;
  LIBC_INLINE constexpr PtrHash &operator=(PtrHash &&) = default;

  LIBC_INLINE constexpr size_t index(Key key) const {
    auto slot = this->index_no_remap(key);

    if (slot < n_)
      return slot;

    return this->remap[slot - n_];
  }

  LIBC_INLINE constexpr size_t index_no_remap(Key key) const {
    auto hx = this->hash_key(key);
    auto b = this->bucket(hx);
    auto pilot = this->pilots[b];
    return this->slot(hx, pilot);
  }

  LIBC_INLINE constexpr size_t slot(uint64_t hx, uint64_t pilot) const {
    return (this->part(hx) * slots_) + this->slot_in_part(hx, pilot);
  }

  LIBC_INLINE constexpr size_t slot_in_part(uint64_t hx, uint64_t pilot) const {
    return this->slot_in_part_hp(hx, this->hash_pilot(pilot));
  }

  LIBC_INLINE constexpr cpp::optional<cpp::tuple<uint64_t, PilotsTypeV, F>>
  compute_pilots(const cpp::array<Key, n_> &keys) {
    cpp::array<cpp::array<bool, slots_>, parts_> taken{};

    for (cpp::array<bool, slots_> &t : taken)
      for (size_t i = 0; i < slots_; i++)
        t[i] = 0;

    PilotsTypeV pilots{};

    size_t tries = 0;
    constexpr size_t MAX_TRIES = 10;

    // hard code random numbers for the generator to make it simpler
    constexpr uint64_t STDRNG[MAX_TRIES] = {
        0x1a275d28e2768536, 0x72737b411117ac11, 0xeb08f8fcd423148f,
        0x1d6f85975d49be9e, 0xf03250d1c097577,  0xac6e884d8db1fa90,
        0x4415d98c0c03a79f, 0xa36bfbcfddf4d5e6, 0x154aef1f436d8e98,
        0xd21f78471475f18e};

    bool contd = true;
    while (contd) {
      tries += 1;
      if (tries > MAX_TRIES)
        return {};

      this->seed = STDRNG[tries - 1];
      pilots = PilotsTypeV{0};

      for (auto &t : taken)
        for (size_t e = 0; e < pilots.size(); e++)
          t[e] = false;

      auto shard_hashes = this->shards(keys);

      contd = !this->try_build_shards(shard_hashes, pilots, taken)
                  ? true
                  : !this->remap_free_slots(taken);
    }

    this->pilots = pilots;
    return {{this->seed, this->pilots, this->remap}};
  }

  LIBC_INLINE constexpr bool
  remap_free_slots(cpp::array<cpp::array<bool, slots_>, parts_> &taken) {
    cpp::array<size_t, parts_> val{};
    for (size_t i = 0; i < taken.size(); ++i) {
      size_t counter = 0;

      for (auto element : taken[i])
        if (!element)
          counter++;

      val[i] = counter;
    }

    size_t acc = 0;

    for (const auto &item : val)
      acc += item;

    if (acc != slots_total_ - n_) {
#ifdef DEBUGDEBUG
      write_to_stderr("Not the right number of free slots left!\n");
      write_to_stderr(" total slots ");
      write_to_stderr(cpp::to_string(slots_total_));
      write_to_stderr(" - n ");
      write_to_stderr(cpp::to_string(n_));
      write_to_stderr("\n");
#endif
      return false;
    }

    if (slots_total_ == n_)
      return true;

    cpp::array<uint64_t, slots_total_ - n_> v{};
    size_t v_idx = 0;

    const auto get = [&](cpp::array<cpp::array<bool, slots_>, parts_> &t,
                         size_t idx) { return t[idx / slots_][idx % slots_]; };

    size_t p = 0;
    for (const auto &t : taken) {
      const size_t offset = p * slots_;
      for (size_t idx = 0; idx < t.size(); idx++) {
        if (!t[idx]) {
          auto result = offset + idx;
          if (result < n_) {
            while (!get(taken, n_ + v_idx)) {
              v[v_idx++] = result;
            }
            v[v_idx++] = result;
          }
        }
      }
      p++;
    }

    for (size_t i = 0; i < v.size(); i++)
      this->remap[i] = static_cast<uint32_t>(v[i]);

    return true;
  }

  LIBC_INLINE constexpr auto shards(const cpp::array<Key, n_> &keys) const {
    return this->no_sharding(keys);
  }

  LIBC_INLINE constexpr cpp::array<cpp::array<uint64_t, n_>, 1>
  no_sharding(const cpp::array<Key, n_> &keys) const {
    cpp::array<uint64_t, n_> ret{};

    for (size_t i = 0; i < keys.size(); i++)
      ret[i] = this->hash_key(keys[i]);

    return {ret};
  }

  // fxhash hasing method for number values
  LIBC_INLINE constexpr uint64_t hash_key(Key key) const {
    uint64_t value = 0;
    constexpr uint64_t BITS = sizeof(uint64_t) * 8;
    value = ((value << 5) | (value >> (BITS - 5))) ^ key;
    value *= FXHASH_SEED;
    return value ^ this->seed;
  }

  LIBC_INLINE constexpr cpp::optional<cpp::tuple<
      cpp::array<uint64_t, n_>, cpp::array<uint32_t, parts_per_shard_ + 1>>>
  sort_parts(size_t shard, cpp::array<uint64_t, n_> hashes) const {
    for (size_t i = 0; i < hashes.size(); i++) {
      for (size_t j = i + 1; j < hashes.size(); j++) {
        if (hashes[i] > hashes[j]) {
          auto temp = hashes[i];
          hashes[i] = hashes[j];
          hashes[j] = temp;
        }
      }
    }

    bool distinct = true;
    for (size_t i = 1; i < hashes.size(); ++i) {
      if (hashes[i] == hashes[i - 1]) {
        distinct = false;
        break;
      }
    }

    if (!distinct) {
#ifdef DEBUGDEBUG
      write_to_stderr("Hashes are not distinct\n");
#endif
      return cpp::nullopt;
    }

    if (!hashes.empty()) {
      LIBC_ASSERT(shard * parts_per_shard_ <= this->part(hashes[0]));
      LIBC_ASSERT(this->part(hashes[hashes.size() - 1]) <
                  (shard + 1) * parts_per_shard_);
    }

    cpp::array<uint32_t, parts_per_shard_ + 1> part_starts{};

    for (size_t part_in_shard = 1; part_in_shard < parts_per_shard_ + 1;
         ++part_in_shard) {
      auto it = cpp::lower_bound(
          hashes.begin(), hashes.end(),
          shard * parts_per_shard_ + part_in_shard,
          [&](uint64_t h, uint64_t k) { return this->part(h) < k; });

      part_starts[part_in_shard] =
          static_cast<uint32_t>(cpp::distance(hashes.begin(), it));
    }
    size_t max_part_len = 0;
    for (size_t i = 0; i + 1 < part_starts.size(); ++i) {
      auto start = part_starts[i];
      auto end = part_starts[i + 1];

      size_t len = end - start;
      max_part_len = cpp::max<size_t>(max_part_len, len);
    }

    if (max_part_len > slots_) {
      return cpp::nullopt;
    }

    return cpp::tuple<cpp::array<uint64_t, n_>,
                      cpp::array<uint32_t, parts_per_shard_ + 1>>{hashes,
                                                                  part_starts};
  }

  LIBC_INLINE constexpr bool
  try_build_shards(const cpp::array<cpp::array<uint64_t, n_>, 1> &shard_hashes,
                   PilotsTypeV &pilots,
                   cpp::array<cpp::array<bool, slots_>, parts_> &taken) const {
    const size_t pilots_chunk_size =
        cpp::max(buckets_ * parts_per_shard_, static_cast<size_t>(1));
    const size_t taken_chunk_size = parts_per_shard_;
    const size_t num_pilots_chunks =
        (pilots.size() + pilots_chunk_size - 1) / pilots_chunk_size;
    const size_t num_taken_chunks =
        (taken.size() + taken_chunk_size - 1) / taken_chunk_size;

    for (size_t shard = 0;
         shard < cpp::min(shard_hashes.size(),
                          cpp::min(num_pilots_chunks, num_taken_chunks));
         shard++) {
      cpp::array<uint64_t, n_> hashes = shard_hashes[shard];

      size_t pilots_begin = shard * pilots_chunk_size;
      size_t pilots_end =
          cpp::min(pilots_begin + pilots_chunk_size, pilots.size());

      size_t taken_begin = shard * taken_chunk_size;
      size_t taken_end = cpp::min(taken_begin + taken_chunk_size, taken.size());

      cpp::optional<cpp::tuple<cpp::array<uint64_t, n_>,
                               cpp::array<uint32_t, parts_per_shard_ + 1>>>
          sorted_parts = this->sort_parts(shard, hashes);

      if (!sorted_parts)
        return false;

      auto &[new_hashes, part_starts] = sorted_parts.value();

      if (!this->build_shard(shard, new_hashes, part_starts, pilots,
                             pilots_begin, pilots_end, taken_begin, taken_end,
                             taken))
        return false;
    }

    return true;
  }

  LIBC_INLINE constexpr bool
  build_shard(size_t shard, cpp::array<uint64_t, n_> &hashes,
              cpp::array<uint32_t, parts_per_shard_ + 1> &part_starts,
              PilotsTypeV &pilots, size_t pilots_begin, size_t pilots_end,
              size_t taken_begin, size_t taken_end,
              cpp::array<cpp::array<bool, slots_>, parts_> &taken) const {

    size_t pilots_chunk_size = pilots_end - pilots_begin;

    size_t part_in_shard = 0;
    for (size_t taken_idx = taken_begin; taken_idx < taken_end; ++taken_idx) {
      const auto num_chunks = pilots_chunk_size / buckets_;
      for (size_t i = 0; i < num_chunks; ++i) {
        size_t target_pilots_begin = pilots_begin + i * buckets_;
        size_t target_pilots_end =
            cpp::min(target_pilots_begin + buckets_, pilots_end);
        auto part = shard * parts_per_shard_ + part_in_shard;

        auto total_evictions = this->build_part(
            part,
            cpp::span<uint64_t>(hashes).subspan(part_starts[part_in_shard],
                                                part_starts[part_in_shard + 1] -
                                                    part_starts[part_in_shard]),
            cpp::span<uint8_t>(
                const_cast<uint8_t *>(pilots.data() + target_pilots_begin),
                target_pilots_end - target_pilots_begin),
            taken[taken_idx]);
        if (!total_evictions) {
          return false;
        }
      }
      part_in_shard++;
    }

    return true;
  }

  LIBC_INLINE constexpr cpp::optional<size_t>
  build_part(size_t part, cpp::span<uint64_t> hashes, cpp::span<uint8_t> pilots,
             cpp::array<bool, slots_> &taken) const {
    cpp::tuple<cpp::array<uint32_t, buckets_ + 1>,
               cpp::array<uint32_t, buckets_>>
        sorted_buckets = this->sort_buckets(part, hashes);
    cpp::array<uint32_t, buckets_ + 1> starts = cpp::get<0>(sorted_buckets);
    cpp::array<uint32_t, buckets_> bucket_order = cpp::get<1>(sorted_buckets);

    constexpr uint16_t KMAX = 256;

    cpp::array<uint32_t, slots_> slots{};

    for (size_t i = 0; i < slots_; i++)
      slots[i] = BUCKET_IDX_NONE;

    auto bucket_len = [&](uint32_t b) constexpr -> size_t {
      return starts[b + 1] - starts[b];
    };

    auto heap = BinaryHeap<>();

    auto duplicate_slots = [&](uint32_t b, uint64_t p) constexpr {
      auto hp = this->hash_pilot(p);
      auto hashes_range = hashes.subspan(starts[b], starts[b + 1] - starts[b]);

      auto i = 0;
      for (const auto &e1 : hashes_range) {
        auto hx = this->slot_in_part_hp(e1, hp);
        for (auto e2 : hashes_range.subspan(i + 1)) {
          auto hy = this->slot_in_part_hp(e2, hp);
          if (hx == hy) {
            return true;
          }
        }
        i++;
      }
      return false;
    };

    // process buckets for the current partition in batches of 16
    cpp::array<uint32_t, 16> recent{
        {BUCKET_IDX_NONE, BUCKET_IDX_NONE, BUCKET_IDX_NONE, BUCKET_IDX_NONE,
         BUCKET_IDX_NONE, BUCKET_IDX_NONE, BUCKET_IDX_NONE, BUCKET_IDX_NONE,
         BUCKET_IDX_NONE, BUCKET_IDX_NONE, BUCKET_IDX_NONE, BUCKET_IDX_NONE,
         BUCKET_IDX_NONE, BUCKET_IDX_NONE, BUCKET_IDX_NONE, BUCKET_IDX_NONE}};
    size_t total_evictions = 0;

    auto rng = FastRand();

    for (size_t iter_num = 0; iter_num < bucket_order.size(); iter_num++) {
      const auto &new_b = bucket_order[iter_num];
      const auto new_bucket =
          hashes.subspan(starts[new_b], starts[new_b + 1] - starts[new_b]);
      if (new_bucket.empty()) {
        pilots[new_b] = 0;
        continue;
      }
      const size_t new_b_len = new_bucket.size();
      size_t evictions = 0;

      heap.push({new_b_len, new_b});
      for (size_t i = 0; i < recent.size(); ++i) {
        recent[i] = BUCKET_IDX_NONE;
      }
      auto recent_idx = 0;
      recent[0] = new_b;

      while (!heap.empty()) {
        const auto b = cpp::get<1>(heap.peek());
        heap.pop();
        if (evictions > slots_ && is_power_of_two((evictions)) &&
            evictions >= 10 * slots_) {
          // evictions from previous iteration must be smaller than 10 times the
          // available slots to not explode the remap array in a single
          // partition or shard
          return cpp::nullopt;
        }
        const auto bucket =
            hashes.subspan(starts[b], starts[b + 1] - starts[b]);
        if (auto fpilot = this->find_pilot(KMAX, bucket, taken)) {
          auto &[p, hp] = fpilot.value();
          pilots[b] = static_cast<uint8_t>(p);
          for (auto &item :
               hashes.subspan(starts[b], starts[b + 1] - starts[b])) {
            auto p = this->slot_in_part_hp(item, hp);
            slots[p] = b;
          }
          continue;
        }
        uint64_t p0 = rng.gen_byte();
        cpp::tuple<size_t, uint64_t> best = {
            cpp::numeric_limits<size_t>::max(),
            cpp::numeric_limits<uint64_t>::max()};
        for (size_t delta = 0; delta < KMAX; ++delta) {
          bool build_part_loop_continue_inner_continue = false;
          const auto p = (p0 + delta) % KMAX;
          const auto hp = this->hash_pilot(p);
          size_t collision_score = 0;
          for (auto &item :
               hashes.subspan(starts[b], starts[b + 1] - starts[b])) {
            auto p = this->slot_in_part_hp(item, hp);
            const auto s = slots[p];
            size_t new_score = 0;
            if (s == BUCKET_IDX_NONE) {
              continue;
            }
            bool found = false;
            for (auto it : recent) {
              found = found || it == s;
            }
            if (found) {
              build_part_loop_continue_inner_continue = true;
              break;
            }
            const auto len = bucket_len(s);
            new_score = len * len;
            collision_score += new_score;
            if (collision_score >= cpp::get<0>(best)) {
              build_part_loop_continue_inner_continue = true;
              break;
            }
          }
          if (build_part_loop_continue_inner_continue) {
            continue;
          }
          if (!duplicate_slots(b, p)) {
            best = {collision_score, p};
            if (collision_score == new_b_len * new_b_len) {
              break;
            }
          }
        }
        if (cpp::get<0>(best) == cpp::numeric_limits<size_t>::max() &&
            cpp::get<1>(best) == cpp::numeric_limits<uint64_t>::max()) {
          return cpp::nullopt;
        }

        const auto &p = cpp::get<1>(best);
        pilots[b] = static_cast<uint8_t>(p);
        const auto hp = this->hash_pilot(p);
        for (auto &item :
             hashes.subspan(starts[b], starts[b + 1] - starts[b])) {
          auto slot = this->slot_in_part_hp(item, hp);
          const auto b2 = slots[slot];
          if (b2 != BUCKET_IDX_NONE) {
            LIBC_ASSERT(b2 != b);
            heap.push({bucket_len(b2), b2});
            evictions++;

            auto hp = this->hash_pilot(static_cast<uint64_t>(pilots[b2]));
            for (auto &item :
                 hashes.subspan(starts[b2], starts[b2 + 1] - starts[b2])) {
              auto p2 = this->slot_in_part_hp(item, hp);
              slots[p2] = BUCKET_IDX_NONE;
              taken[p2] = false;
            }
          }
          slots[slot] = b;
          taken[slot] = true;
        }

        recent_idx++;
        recent_idx %= recent.size();
        recent[recent_idx] = b;
      }

      total_evictions += evictions;
    }
    return total_evictions;
  }

  LIBC_INLINE constexpr cpp::optional<cpp::tuple<size_t, uint64_t>>
  find_pilot(uint16_t kmax, cpp::span<uint64_t> bucket,
             cpp::array<bool, slots_> &taken) const {
    const size_t r = bucket.size() >> 2 << 2; // round down to magnitude of 4
    for (size_t p = 0; p < kmax; p++) {
      const auto hp = this->hash_pilot(p);
      const auto check = [&](uint64_t hx) {
        return taken[this->slot_in_part_hp(hx, hp)];
      };
      auto bad = false;

      for (size_t i = 0; i < r; i++) {
        if (check(bucket[i])) {
          bad = true;
          break;
        }
      }

      if (bad)
        continue;

      for (auto hx : bucket.subspan(r))
        bad |= check(hx);

      if (bad)
        continue;

      if (this->try_take_pilot(bucket, hp, taken))
        return cpp::tuple<size_t, uint64_t>{p, hp};
    }
    return cpp::nullopt;
  }

  LIBC_INLINE constexpr bool
  try_take_pilot(cpp::span<uint64_t> bucket, uint64_t hp,
                 cpp::array<bool, slots_> &taken) const {
    for (size_t i = 0; i < bucket.size(); i++) {
      size_t hx = bucket[i];
      const auto slot = this->slot_in_part_hp(hx, hp);
      if (taken[slot]) {
        for (auto hx : bucket.subspan(0, i)) {
          taken[this->slot_in_part_hp(hx, hp)] = false;
        }
        return false;
      }
      taken[slot] = true;
    }
    return true;
  }

  LIBC_INLINE constexpr uint64_t hash_pilot(uint64_t p) const {
    // variant of fxhash hasing method
    uint64_t c = FXHASH_SEED;
    uint64_t b = p ^ this->seed;
    uint64_t result = 0;

    while (b != 0) {
      if (b & 1) {
        result += c;
      }
      c <<= 1;
      b >>= 1;
    }

    return result;
  }

  LIBC_INLINE constexpr size_t slot_in_part_hp(uint64_t hx, uint64_t hp) const {
    uint64_t d =
        cpp::max(PtrhashConfig<n_>::SLOTS_PER_PART, static_cast<size_t>(1));
    uint64_t m = cpp::numeric_limits<uint64_t>::max() / d + 1;
    auto lowbits = m * (hx ^ hp);
    return static_cast<size_t>(
        (static_cast<UInt128>(lowbits) * static_cast<UInt128>(d)) >> 64);
  }

  LIBC_INLINE constexpr cpp::tuple<cpp::array<uint32_t, buckets_ + 1>,
                                   cpp::array<uint32_t, buckets_>>
  sort_buckets(size_t part, cpp::span<uint64_t> hashes) const {
    cpp::array<uint32_t, buckets_ + 1> bucket_starts{};
    size_t bucket_starts_idx = 0;
    cpp::array<uint32_t, buckets_> order{};
    for (size_t i = 0; i < order.size(); ++i) {
      order[i] = BUCKET_IDX_NONE;
    }
    cpp::array<size_t, 32> bucket_len_cnt = {0};

    size_t end = 0;
    bucket_starts[bucket_starts_idx++] = static_cast<uint32_t>(end);

    for (size_t b = 0; b < buckets_; b++) {
      auto start = end;
      while (end < hashes.size() &&
             this->bucket(hashes[end]) == part * buckets_ + b) {
        end++;
      }

      auto l = end - start;
      bucket_len_cnt[l]++;
      bucket_starts[bucket_starts_idx++] = static_cast<uint32_t>(end);
    }

    LIBC_ASSERT(end == hashes.size());

    auto max_bucket_size = bucket_len_cnt.size() - 1;
    //  "Part {part}: Bucket size {max_bucket_size} is too much "
    //  "larger than the expected size of {expected_bucket_size}."
    LIBC_ASSERT(static_cast<double>(max_bucket_size) <=
                (20. * slots_ / buckets_));
    auto acc = 0;
    for (int i = static_cast<int>(max_bucket_size); i > -1; i--) {
      auto tmp = bucket_len_cnt[i];
      bucket_len_cnt[i] = acc;
      acc += tmp;
    }
    for (size_t b = 0; b < buckets_; b++) {
      size_t l = bucket_starts[b + 1] - bucket_starts[b];
      order[bucket_len_cnt[l]] = static_cast<uint32_t>(b);
      bucket_len_cnt[l] += 1;
    }

    return {bucket_starts, order};
  }

  LIBC_INLINE constexpr size_t part(uint64_t hx) const {
    return static_cast<size_t>((static_cast<UInt128>(PtrhashConfig<n_>::PARTS) *
                                static_cast<UInt128>(hx)) >>
                               64);
  }

  LIBC_INLINE constexpr size_t bucket(uint64_t hx) const {
    return static_cast<size_t>(
        (static_cast<UInt128>(PtrhashConfig<n_>::BUCKETS_TOTAL) *
         static_cast<UInt128>(hx)) >>
        64);
  }

private:
  uint64_t seed;
  PilotsTypeV pilots;
  F remap;
};

template <size_t n, typename Key = wint_t>
LIBC_INLINE constexpr auto get_params(const cpp::array<Key, n> &keys) {
  auto result = PtrHash<n>().compute_pilots(keys);

  LIBC_ASSERT(result &&
              "Unable to construct PtrHash after 10 tries. Try using a "
              "better hash or decreasing lambda.\n");

  return result.value();
}

} // namespace ptrhash

template <size_t Capacity> class PerfectHashMap {
public:
  struct Entry {
    wint_t key{};
    wint_t value{};
    LIBC_INLINE constexpr Entry() = default;
    LIBC_INLINE constexpr Entry(wint_t key, wint_t value)
        : key(key), value(value) {}
  };

  LIBC_INLINE constexpr PerfectHashMap(
      const cpp::array<cpp::array<wint_t, 2>, Capacity> &pairs,
      ptrhash::PtrHash<Capacity> &&hasher)
      : hasher(hasher) {
    for (const auto &pair : pairs) {
      const auto &key = pair[0];
      const auto &value = pair[1];
      const auto idx = hasher.index(key);
      LIBC_ASSERT(idx < Capacity && "Index out of bounds");
      this->entries[idx] = Entry{key, value};
    }
  }

  LIBC_INLINE constexpr cpp::optional<wint_t> find(const wint_t key) const {
    size_t idx = hasher.index(key);
    if (idx >= Capacity)
      return cpp::nullopt;

    const auto entry = entries[idx];
    if (entry.key != key)
      return cpp::nullopt;

    return entry.value;
  }

  LIBC_INLINE constexpr bool contains(const wint_t key) const {
    return this->find(key).has_value();
  }

  LIBC_INLINE constexpr size_t size() { return Capacity; }

private:
  Entry entries[Capacity];
  ptrhash::PtrHash<Capacity> hasher;
};

} // namespace wctype_internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_WCTYPE_PERFECT_HASH_MAP_H
