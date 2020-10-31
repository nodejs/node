/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "perfetto/protozero/packed_repeated_fields.h"

#include "perfetto/ext/base/utils.h"

namespace protozero {

// static
constexpr size_t PackedBufferBase::kOnStackStorageSize;

void PackedBufferBase::GrowSlowpath() {
  size_t write_off = static_cast<size_t>(write_ptr_ - storage_begin_);
  size_t old_size = static_cast<size_t>(storage_end_ - storage_begin_);
  size_t new_size = old_size < 65536 ? (old_size * 2) : (old_size * 3 / 2);
  new_size = perfetto::base::AlignUp<4096>(new_size);
  std::unique_ptr<uint8_t[]> new_buf(new uint8_t[new_size]);
  memcpy(new_buf.get(), storage_begin_, old_size);
  heap_buf_ = std::move(new_buf);
  storage_begin_ = heap_buf_.get();
  storage_end_ = storage_begin_ + new_size;
  write_ptr_ = storage_begin_ + write_off;
}

void PackedBufferBase::Reset() {
  heap_buf_.reset();
  storage_begin_ = reinterpret_cast<uint8_t*>(&stack_buf_[0]);
  storage_end_ = reinterpret_cast<uint8_t*>(&stack_buf_[kOnStackStorageSize]);
  write_ptr_ = storage_begin_;
}

}  // namespace protozero
