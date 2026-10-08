//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for a block of memory.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BLOCK_H
#define LLVM_LIBC_SRC___SUPPORT_BLOCK_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/cstddef.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/new.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/math_extras.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {

/// Returns the value rounded down to the nearest multiple of alignment.
LIBC_INLINE constexpr size_t align_down(size_t value, size_t alignment) {
  // Note this shouldn't overflow since the result will always be <= value.
  return (value / alignment) * alignment;
}

/// Returns the value rounded up to the nearest multiple of alignment. May wrap
/// around.
LIBC_INLINE constexpr size_t align_up(size_t value, size_t alignment) {
  return align_down(value + alignment - 1, alignment);
}

using ByteSpan = cpp::span<LIBC_NAMESPACE::cpp::byte>;
using cpp::optional;

/// Reference to a memory region with links to adjacent blocks.
///
/// The blocks store their offsets to the previous and next blocks. The latter
/// is also the block's size. The metadata is stored in raw bytes and accessed
/// through aligned byte-copy loads and stores so the header can overlap user
/// storage without creating typed aliasing accesses.
///
/// All blocks have their usable space aligned to some multiple of MIN_ALIGN.
/// This also implies that block outer sizes are aligned to MIN_ALIGN.
///
/// As an example, the diagram below represents two contiguous blocks. The
/// indices indicate byte offsets:
///
/// @code{.unparsed}
/// Block 1:
/// +---------------------+--------------+
/// | Header              | Usable space |
/// +----------+----------+--------------+
/// | prev     | next     |              |
/// | 0......3 | 4......7 | 8........227 |
/// | 00000000 | 00000230 |  <app data>  |
/// +----------+----------+--------------+
/// Block 2:
/// +---------------------+--------------+
/// | Header              | Usable space |
/// +----------+----------+--------------+
/// | prev     | next     |              |
/// | 0......3 | 4......7 | 8........827 |
/// | 00000230 | 00000830 | f7f7....f7f7 |
/// +----------+----------+--------------+
/// @endcode
///
/// As a space optimization, when a block is allocated, it consumes the prev
/// field of the following block:
///
/// Block 1 (used):
/// +---------------------+--------------+
/// | Header              | Usable space |
/// +----------+----------+--------------+
/// | prev     | next     |              |
/// | 0......3 | 4......7 | 8........230 |
/// | 00000000 | 00000230 |  <app data>  |
/// +----------+----------+--------------+
/// Block 2:
/// +---------------------+--------------+
/// | B1       | Header   | Usable space |
/// +----------+----------+--------------+
/// |          | next     |              |
/// | 0......3 | 4......7 | 8........827 |
/// | xxxxxxxx | 00000830 | f7f7....f7f7 |
/// +----------+----------+--------------+
///
/// The next offset of a block matches the previous offset of its next block.
/// The first block in a list is denoted by having a previous offset of `0`.
class BlockRef {
  // Masks for the contents of the next field.
  static constexpr size_t PREV_FREE_MASK = 1 << 0;
  static constexpr size_t LAST_MASK = 1 << 1;
  static constexpr size_t SIZE_MASK = ~(PREV_FREE_MASK | LAST_MASK);

  // Header field offsets. The value at PREV_OFFSET is only meaningful when the
  // PREV_FREE_MASK bit is set in the next field.
  static constexpr size_t PREV_OFFSET = 0;
  static constexpr size_t NEXT_OFFSET = PREV_OFFSET + sizeof(size_t);

public:
  static constexpr size_t HEADER_SIZE = NEXT_OFFSET + sizeof(size_t);

  // To ensure block sizes have two lower unused bits, ensure usable space is
  // always aligned to at least 4 bytes. (The distances between usable spaces,
  // the outer size, is then always also 4-aligned.)
  static constexpr size_t MIN_ALIGN = cpp::max(size_t{4}, alignof(max_align_t));

  LIBC_INLINE constexpr BlockRef() = default;
  LIBC_INLINE explicit constexpr BlockRef(cpp::byte *header_ptr)
      : header_ptr(header_ptr) {}
  LIBC_INLINE explicit constexpr operator bool() const {
    return header_ptr != nullptr;
  }
  LIBC_INLINE constexpr bool operator==(BlockRef other) const {
    return header_ptr == other.header_ptr;
  }
  LIBC_INLINE constexpr bool operator!=(BlockRef other) const {
    return !(*this == other);
  }

  LIBC_INLINE uintptr_t addr() const {
    return reinterpret_cast<uintptr_t>(header_ptr);
  }

  /// Initializes a given memory region into a first block and a sentinel last
  /// block. Returns the first block, which has its usable space aligned to
  /// MIN_ALIGN.
  static optional<BlockRef> init(ByteSpan region);

  /// @returns  A pointer to a block, given a pointer to the start of the usable
  ///           space inside the block.
  ///
  /// This is the inverse of `usable_space()`.
  ///
  /// @warning  This method does not do any checking; passing a random pointer
  ///           will return a non-null pointer.
  LIBC_INLINE static BlockRef from_usable_space(void *usable_space) {
    LIBC_ASSERT(usable_space != nullptr && "usable space cannot be null");
    auto *bytes = reinterpret_cast<cpp::byte *>(usable_space);
    return BlockRef(bytes - HEADER_SIZE);
  }
  LIBC_INLINE static BlockRef from_usable_space(const void *usable_space) {
    LIBC_ASSERT(usable_space != nullptr && "usable space cannot be null");
    const auto *bytes = reinterpret_cast<const cpp::byte *>(usable_space);
    return BlockRef(const_cast<cpp::byte *>(bytes - HEADER_SIZE));
  }

  /// @returns The total size of the block in bytes, including the header.
  LIBC_INLINE size_t outer_size() const { return load_next() & SIZE_MASK; }

  LIBC_INLINE static constexpr size_t outer_size(size_t inner_size) {
    // The usable region includes the prev field of the next block.
    return inner_size - PREV_FIELD_SIZE + HEADER_SIZE;
  }

  /// @returns The number of usable bytes inside the block were it to be
  /// allocated.
  LIBC_INLINE size_t inner_size() const {
    if (!next())
      return 0;
    return inner_size(outer_size());
  }

  /// @returns The number of usable bytes inside a block with the given outer
  /// size were it to be allocated.
  LIBC_INLINE static constexpr size_t inner_size(size_t outer_size) {
    // The usable region includes the prev field of the next block.
    return inner_size_free(outer_size) + PREV_FIELD_SIZE;
  }

  /// @returns The number of usable bytes inside the block if it remains free.
  LIBC_INLINE size_t inner_size_free() const {
    if (!next())
      return 0;
    return inner_size_free(outer_size());
  }

  /// @returns The number of usable bytes inside a block with the given outer
  /// size if it remains free.
  LIBC_INLINE static constexpr size_t inner_size_free(size_t outer_size) {
    return outer_size - HEADER_SIZE;
  }

  /// @returns A pointer to the usable space inside this block.
  ///
  /// Aligned to some multiple of MIN_ALIGN.
  LIBC_INLINE cpp::byte *usable_space() const {
    auto *s = nonnull_header_ptr() + HEADER_SIZE;
    LIBC_ASSERT(reinterpret_cast<uintptr_t>(s) % MIN_ALIGN == 0 &&
                "usable space must be aligned to MIN_ALIGN");
    return s;
  }

  // @returns The region of memory the block manages, including the header.
  LIBC_INLINE ByteSpan region() const {
    return {nonnull_header_ptr(), outer_size()};
  }

  /// Attempts to split this block.
  ///
  /// If successful, the block will have an inner size of at least
  /// `new_inner_size`. The remaining space will be returned as a new block,
  /// with usable space aligned to `usable_space_alignment`. Note that the prev
  /// field of the next block counts as part of the inner size of the block.
  /// `usable_space_alignment` must be a multiple of MIN_ALIGN.
  optional<BlockRef> split(size_t new_inner_size,
                           size_t usable_space_alignment = MIN_ALIGN) const;

  /// Merges this block with the one that comes after it.
  bool merge_next() const;

  /// @returns The block immediately after this one, or a null block if this is
  /// the last block.
  LIBC_INLINE BlockRef next() const {
    size_t next_value = load_next();
    if (next_value & LAST_MASK)
      return BlockRef();
    return BlockRef(nonnull_header_ptr() + (next_value & SIZE_MASK));
  }

  /// @returns The free block immediately before this one, otherwise null.
  LIBC_INLINE BlockRef prev_free() const {
    if (!(load_next() & PREV_FREE_MASK))
      return BlockRef();
    return BlockRef(nonnull_header_ptr() - load_prev());
  }

  /// @returns Whether the block is unavailable for allocation.
  LIBC_INLINE bool used() const { return !next() || !next().prev_free(); }

  /// Marks this block as in use.
  LIBC_INLINE void mark_used() const {
    LIBC_ASSERT(next() && "last block is always considered used");
    BlockRef next_block = next();
    next_block.store_next(next_block.load_next() & ~PREV_FREE_MASK);
  }

  /// Marks this block as free.
  LIBC_INLINE void mark_free() const {
    LIBC_ASSERT(next() && "last block is always considered used");
    BlockRef next_block = next();
    next_block.store_next(next_block.load_next() | PREV_FREE_MASK);
    next_block.store_prev(outer_size());
  }

  LIBC_INLINE bool is_usable_space_aligned(size_t alignment) const {
    return reinterpret_cast<uintptr_t>(usable_space()) % alignment == 0;
  }

  // Returns the minimum inner size necessary for a block of that size to
  // always be able to allocate at the given size and alignment.
  //
  // Returns 0 if there is no such size.
  LIBC_INLINE static size_t min_size_for_allocation(size_t alignment,
                                                    size_t size) {
    LIBC_ASSERT(alignment >= MIN_ALIGN && alignment % MIN_ALIGN == 0 &&
                "alignment must be multiple of MIN_ALIGN");

    if (alignment == MIN_ALIGN)
      return size;

    // We must create a new block inside this one (splitting). This requires a
    // block header in addition to the requested size.
    if (add_overflow(size, HEADER_SIZE, size))
      return 0;

    // Beyond that, padding space may need to remain in this block to ensure
    // that the usable space of the next block is aligned.
    //
    // Consider a position P of some lesser alignment, L, with maximal distance
    // to the next position of some greater alignment, G, where G is a multiple
    // of L. P must be one L unit past a G-aligned point. If it were one L-unit
    // earlier, its distance would be zero. If it were one L-unit later, its
    // distance would not be maximal. If it were not some integral number of L
    // units away, it would not be L-aligned.
    //
    // So the maximum distance would be G - L. As a special case, if L is 1
    // (unaligned), the max distance is G - 1.
    //
    // This block's usable space is aligned to MIN_ALIGN >= header alignment.
    // With zero padding, the next block's usable space is HEADER_SIZE past it,
    // which is aligned to header alignment. Thus the max padding needed is
    // alignment - alignof(size_t).
    if (add_overflow(size, alignment - alignof(size_t), size))
      return 0;
    return size;
  }

  struct BlockInfo;

  // Divide a block into up to 3 blocks according to `BlockInfo`. Behavior is
  // undefined if allocation is not possible for the given size and alignment.
  static BlockInfo allocate(BlockRef block, size_t alignment, size_t size);

  // These two functions may wrap around.
  LIBC_INLINE static uintptr_t
  next_possible_block_start(uintptr_t ptr,
                            size_t usable_space_alignment = MIN_ALIGN) {
    return align_up(ptr + HEADER_SIZE, usable_space_alignment) - HEADER_SIZE;
  }
  LIBC_INLINE static uintptr_t
  prev_possible_block_start(uintptr_t ptr,
                            size_t usable_space_alignment = MIN_ALIGN) {
    return align_down(ptr, usable_space_alignment) - HEADER_SIZE;
  }

  /// Only for testing.
  static constexpr size_t PREV_FIELD_SIZE = sizeof(size_t);

private:
  /// Construct a block to represent a span of bytes. Overwrites only enough
  /// memory for the block header; the rest of the span is left alone.
  LIBC_INLINE static BlockRef as_block(ByteSpan bytes) {
    LIBC_ASSERT(reinterpret_cast<uintptr_t>(bytes.data()) % alignof(size_t) ==
                    0 &&
                "block start must be suitably aligned");
    LIBC_ASSERT(bytes.size() % MIN_ALIGN == 0 &&
                "block size must be aligned to MIN_ALIGN");
    BlockRef block(bytes.data());
    block.store_next(bytes.size());
    return block;
  }

  LIBC_INLINE static void make_last_block(cpp::byte *start) {
    LIBC_ASSERT(reinterpret_cast<uintptr_t>(start) % alignof(size_t) == 0 &&
                "block start must be suitably aligned");
    BlockRef last(start);
    last.store_next(HEADER_SIZE | LAST_MASK);
  }

  LIBC_INLINE cpp::byte *field_ptr(size_t offset) const {
    cpp::byte *ptr = nonnull_header_ptr() + offset;
    LIBC_ASSERT(reinterpret_cast<uintptr_t>(ptr) % alignof(size_t) == 0 &&
                "block metadata fields must be aligned");
    return ptr;
  }

  LIBC_INLINE size_t load_field(size_t offset) const {
    return *reinterpret_cast<const size_t *>(field_ptr(offset));
  }

  LIBC_INLINE void store_field(size_t offset, size_t value) const {
    new (field_ptr(offset)) size_t(value);
  }

  /// Offset from this block to the previous block. 0 if this is the first
  /// block. This field is only alive when the previous block is free;
  /// otherwise, its memory is reused as part of the previous block's usable
  /// space.
  LIBC_INLINE size_t load_prev() const { return load_field(PREV_OFFSET); }

  /// Offset from this block to the next block. Valid even if this is the last
  /// block, since it equals the size of the block.
  ///
  /// Information about the current state of the block is stored in the two low
  /// order bits of the next_ value. These are guaranteed free by a minimum
  /// alignment (and thus, alignment of the size) of 4. The lowest bit is the
  /// `prev_free` flag, and the other bit is the `last` flag.
  ///
  /// * If the `prev_free` flag is set, the block isn't the first and the
  ///   previous block is free.
  /// * If the `last` flag is set, the block is the sentinel last block. It is
  ///   summarily considered used and has no next block.
  LIBC_INLINE size_t load_next() const { return load_field(NEXT_OFFSET); }

  LIBC_INLINE void store_prev(size_t value) const {
    store_field(PREV_OFFSET, value);
  }
  LIBC_INLINE void store_next(size_t value) const {
    store_field(NEXT_OFFSET, value);
  }

  LIBC_INLINE cpp::byte *nonnull_header_ptr() const {
    LIBC_ASSERT(header_ptr != nullptr && "operation on an invalid block");
#if __has_builtin(__builtin_assume)
    __builtin_assume(header_ptr != nullptr);
#endif
    return header_ptr;
  }

  cpp::byte *header_ptr = nullptr;
};

// This is the return type for `allocate` which can split one block into up to
// three blocks.
struct BlockRef::BlockInfo {
  // This is the newly aligned block. It will have the alignment requested by a
  // call to `allocate` and at most `size`.
  BlockRef block;

  // If the usable_space in the new block was not aligned according to the
  // `alignment` parameter, we will need to split into this block and the
  // `block` to ensure `block` is properly aligned. In this case, `prev` will be
  // this new "padding" block. `prev` will be null if no new block was created
  // or we were able to merge the block before the original block with the
  // "padding" block.
  BlockRef prev;

  // This is the remainder of the next block after splitting the `block`
  // according to `size`. This can happen if there's enough space after the
  // `block`.
  BlockRef next;
};

LIBC_INLINE
optional<BlockRef> BlockRef::init(ByteSpan region) {
  if (!region.data())
    return {};

  uintptr_t start = reinterpret_cast<uintptr_t>(region.data());
  uintptr_t end = start + region.size();
  if (end < start)
    return {};

  uintptr_t block_start = next_possible_block_start(start);
  if (block_start < start)
    return {};

  uintptr_t last_start = prev_possible_block_start(end);
  if (last_start >= end)
    return {};

  if (block_start + HEADER_SIZE > last_start)
    return {};

  auto *last_start_ptr = reinterpret_cast<cpp::byte *>(last_start);
  BlockRef block =
      as_block({reinterpret_cast<cpp::byte *>(block_start), last_start_ptr});
  make_last_block(last_start_ptr);
  block.mark_free();
  return block;
}

LIBC_INLINE
BlockRef::BlockInfo BlockRef::allocate(BlockRef block, size_t alignment,
                                       size_t size) {
  LIBC_ASSERT(block.header_ptr != nullptr &&
              "allocating from an invalid block");
  LIBC_ASSERT(alignment % MIN_ALIGN == 0 &&
              "alignment must be a multiple of MIN_ALIGN");

  BlockInfo info{block, BlockRef(), BlockRef()};

  if (!info.block.is_usable_space_aligned(alignment)) {
    BlockRef original = info.block;
    // The padding block has no minimum size requirement.
    optional<BlockRef> maybe_aligned_block = original.split(0, alignment);
    LIBC_ASSERT(maybe_aligned_block.has_value() &&
                "it should always be possible to split for alignment");

    if (BlockRef prev = original.prev_free()) {
      // If there is a free block before this, we can merge the current one with
      // the newly created one.
      prev.merge_next();
    } else {
      info.prev = original;
    }

    BlockRef aligned_block = *maybe_aligned_block;
    LIBC_ASSERT(aligned_block.is_usable_space_aligned(alignment) &&
                "The aligned block isn't aligned somehow.");
    info.block = aligned_block;
  }

  // Now get a block for the requested size.
  if (optional<BlockRef> next = info.block.split(size))
    info.next = *next;

  return info;
}

LIBC_INLINE
optional<BlockRef> BlockRef::split(size_t new_inner_size,
                                   size_t usable_space_alignment) const {
  LIBC_ASSERT(usable_space_alignment % MIN_ALIGN == 0 &&
              "alignment must be a multiple of MIN_ALIGN");

  // Compute the minimum outer size that produces a block of at least
  // `new_inner_size`.
  size_t min_outer_size = outer_size(cpp::max(new_inner_size, PREV_FIELD_SIZE));

  uintptr_t start = addr();
  uintptr_t next_block_start =
      next_possible_block_start(start + min_outer_size, usable_space_alignment);
  if (next_block_start < start)
    return {};
  size_t new_outer_size = next_block_start - start;
  LIBC_ASSERT(new_outer_size % MIN_ALIGN == 0 &&
              "new size must be aligned to MIN_ALIGN");

  if (outer_size() < new_outer_size ||
      outer_size() - new_outer_size < HEADER_SIZE)
    return {};

  bool was_free = !used();

  ByteSpan new_region = region().subspan(new_outer_size);
  store_next((load_next() & ~SIZE_MASK) | new_outer_size);

  BlockRef new_block = as_block(new_region);
  new_block.mark_free();
  if (was_free)
    mark_free();

  LIBC_ASSERT(new_block.is_usable_space_aligned(usable_space_alignment) &&
              "usable space must have requested alignment");
  return new_block;
}

LIBC_INLINE
bool BlockRef::merge_next() const {
  BlockRef next_block = next();
  if (used() || next_block.used())
    return false;
  size_t new_size = outer_size() + next_block.outer_size();
  store_next((load_next() & ~SIZE_MASK) | new_size);
  next().store_prev(new_size);
  return true;
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BLOCK_H
