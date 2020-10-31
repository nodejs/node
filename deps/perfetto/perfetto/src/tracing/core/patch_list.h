/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SRC_TRACING_CORE_PATCH_LIST_H_
#define SRC_TRACING_CORE_PATCH_LIST_H_

#include <array>
#include <forward_list>

#include "perfetto/base/logging.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/shared_memory_abi.h"

namespace perfetto {

// Used to handle the backfilling of the headers (the |size_field|) of nested
// messages when a proto is fragmented over several chunks. These patches are
// sent out-of-band to the tracing service, after having returned the initial
// chunks of the fragment.
// TODO(crbug.com/904477): Re-disable the move constructors when all usses of
// this class have been fixed.
class Patch {
 public:
  using PatchContent = std::array<uint8_t, SharedMemoryABI::kPacketHeaderSize>;
  Patch(ChunkID c, uint16_t o) : chunk_id(c), offset(o) {}
  Patch(const Patch&) = default;  // For tests.

  const ChunkID chunk_id;
  const uint16_t offset;
  PatchContent size_field{};

  // |size_field| contains a varint. Any varint must start with != 0. Even in
  // the case we want to encode a size == 0, protozero will write a redundant
  // varint for that, that is [0x80, 0x80, 0x80, 0x00]. So the first byte is 0
  // iff we never wrote any varint into that.
  bool is_patched() const { return size_field[0] != 0; }

  // For tests.
  bool operator==(const Patch& o) const {
    return chunk_id == o.chunk_id && offset == o.offset &&
           size_field == o.size_field;
  }

 private:
  Patch& operator=(const Patch&) = delete;
};

// Note: the protozero::Message(s) will take pointers to the |size_field| of
// these entries. This container must guarantee that the Patch objects are never
// moved around (i.e. cannot be a vector because of reallocations can change
// addresses of pre-existing entries).
class PatchList {
 public:
  using ListType = std::forward_list<Patch>;
  using value_type = ListType::value_type;          // For gtest.
  using const_iterator = ListType::const_iterator;  // For gtest.

  PatchList() : last_(list_.before_begin()) {}

  Patch* emplace_back(ChunkID chunk_id, uint16_t offset) {
    PERFETTO_DCHECK(empty() || last_->chunk_id != chunk_id ||
                    offset >= last_->offset + sizeof(Patch::PatchContent));
    last_ = list_.emplace_after(last_, chunk_id, offset);
    return &*last_;
  }

  void pop_front() {
    PERFETTO_DCHECK(!list_.empty());
    list_.pop_front();
    if (empty())
      last_ = list_.before_begin();
  }

  const Patch& front() const {
    PERFETTO_DCHECK(!list_.empty());
    return list_.front();
  }

  const Patch& back() const {
    PERFETTO_DCHECK(!list_.empty());
    return *last_;
  }

  ListType::const_iterator begin() const { return list_.begin(); }
  ListType::const_iterator end() const { return list_.end(); }
  bool empty() const { return list_.empty(); }

 private:
  ListType list_;
  ListType::iterator last_;
};

}  // namespace perfetto

#endif  // SRC_TRACING_CORE_PATCH_LIST_H_
