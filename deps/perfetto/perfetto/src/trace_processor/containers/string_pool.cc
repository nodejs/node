/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/containers/string_pool.h"

#include <limits>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"

namespace perfetto {
namespace trace_processor {

// static
constexpr size_t StringPool::kNumBlockIndexBits;
// static
constexpr size_t StringPool::kNumBlockOffsetBits;
// static
constexpr size_t StringPool::kLargeStringFlagBitMask;
// static
constexpr size_t StringPool::kBlockOffsetBitMask;
// static
constexpr size_t StringPool::kBlockIndexBitMask;
// static
constexpr size_t StringPool::kBlockSizeBytes;
// static
constexpr size_t StringPool::kMinLargeStringSizeBytes;

StringPool::StringPool() {
  static_assert(
      StringPool::kMinLargeStringSizeBytes <= StringPool::kBlockSizeBytes + 1,
      "minimum size of large strings must be small enough to support any "
      "string that doesn't fit in a Block.");

  blocks_.emplace_back(kBlockSizeBytes);

  // Reserve a slot for the null string.
  PERFETTO_CHECK(blocks_.back().TryInsert(NullTermStringView()).first);
}

StringPool::~StringPool() = default;

StringPool::StringPool(StringPool&&) = default;
StringPool& StringPool::operator=(StringPool&&) = default;

StringPool::Id StringPool::InsertString(base::StringView str, uint64_t hash) {
  // Try and find enough space in the current block for the string and the
  // metadata (varint-encoded size + the string data + the null terminator).
  bool success;
  uint32_t offset;
  std::tie(success, offset) = blocks_.back().TryInsert(str);
  if (PERFETTO_UNLIKELY(!success)) {
    // The block did not have enough space for the string. If the string is
    // large, add it into the |large_strings_| vector, to avoid discarding a
    // large portion of the current block's memory. This also enables us to
    // support strings that wouldn't fit into a single block. Otherwise, add a
    // new block to store the string.
    if (str.size() + kMaxMetadataSize >= kMinLargeStringSizeBytes) {
      return InsertLargeString(str, hash);
    } else {
      blocks_.emplace_back(kBlockSizeBytes);
    }

    // Try and reserve space again - this time we should definitely succeed.
    std::tie(success, offset) = blocks_.back().TryInsert(str);
    PERFETTO_CHECK(success);
  }

  // Compute the id from the block index and offset and add a mapping from the
  // hash to the id.
  Id string_id = Id::BlockString(blocks_.size() - 1, offset);
  string_index_.emplace(hash, string_id);
  return string_id;
}

StringPool::Id StringPool::InsertLargeString(base::StringView str,
                                             uint64_t hash) {
  large_strings_.emplace_back(new std::string(str.begin(), str.size()));
  // Compute id from the index and add a mapping from the hash to the id.
  Id string_id = Id::LargeString(large_strings_.size() - 1);
  string_index_.emplace(hash, string_id);
  return string_id;
}

std::pair<bool /*success*/, uint32_t /*offset*/> StringPool::Block::TryInsert(
    base::StringView str) {
  auto str_size = str.size();
  size_t max_pos = static_cast<size_t>(pos_) + str_size + kMaxMetadataSize;
  if (max_pos > size_)
    return std::make_pair(false, 0u);

  // Ensure that we commit up until the end of the string to memory.
  mem_.EnsureCommitted(max_pos);

  // Get where we should start writing this string.
  uint32_t offset = pos_;
  uint8_t* begin = Get(offset);

  // First write the size of the string using varint encoding.
  uint8_t* end = protozero::proto_utils::WriteVarInt(str_size, begin);

  // Next the string itself.
  if (PERFETTO_LIKELY(str_size > 0)) {
    memcpy(end, str.data(), str_size);
    end += str_size;
  }

  // Finally add a null terminator.
  *(end++) = '\0';

  // Update the end of the block and return the pointer to the string.
  pos_ = OffsetOf(end);

  return std::make_pair(true, offset);
}

StringPool::Iterator::Iterator(const StringPool* pool) : pool_(pool) {}

StringPool::Iterator& StringPool::Iterator::operator++() {
  if (block_index_ < pool_->blocks_.size()) {
    // Try and go to the next string in the current block.
    const auto& block = pool_->blocks_[block_index_];

    // Find the size of the string at the current offset in the block
    // and increment the offset by that size.
    uint32_t str_size = 0;
    const uint8_t* ptr = block.Get(block_offset_);
    ptr = ReadSize(ptr, &str_size);
    ptr += str_size + 1;
    block_offset_ = block.OffsetOf(ptr);

    // If we're out of bounds for this block, go to the start of the next block.
    if (block.pos() <= block_offset_) {
      block_index_++;
      block_offset_ = 0;
    }

    return *this;
  }

  // Advance to the next string from |large_strings_|.
  PERFETTO_DCHECK(large_strings_index_ < pool_->large_strings_.size());
  large_strings_index_++;
  return *this;
}

StringPool::Iterator::operator bool() const {
  return block_index_ < pool_->blocks_.size() ||
         large_strings_index_ < pool_->large_strings_.size();
}

NullTermStringView StringPool::Iterator::StringView() {
  return pool_->Get(StringId());
}

StringPool::Id StringPool::Iterator::StringId() {
  if (block_index_ < pool_->blocks_.size()) {
    PERFETTO_DCHECK(block_offset_ < pool_->blocks_[block_index_].pos());

    // If we're at (0, 0), we have the null string which has id 0.
    if (block_index_ == 0 && block_offset_ == 0)
      return Id::Null();
    return Id::BlockString(block_index_, block_offset_);
  }
  PERFETTO_DCHECK(large_strings_index_ < pool_->large_strings_.size());
  return Id::LargeString(large_strings_index_);
}

}  // namespace trace_processor
}  // namespace perfetto
