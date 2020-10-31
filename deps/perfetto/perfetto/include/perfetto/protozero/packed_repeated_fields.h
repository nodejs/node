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

#ifndef INCLUDE_PERFETTO_PROTOZERO_PACKED_REPEATED_FIELDS_H_
#define INCLUDE_PERFETTO_PROTOZERO_PACKED_REPEATED_FIELDS_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <type_traits>

#include "perfetto/base/logging.h"
#include "perfetto/protozero/proto_utils.h"

namespace protozero {

// This file contains classes used when encoding packed repeated fields.
// To encode such a field, the caller is first expected to accumulate all of the
// values in one of the following types (depending on the wire type of the
// individual elements), defined below:
// * protozero::PackedVarInt
// * protozero::PackedFixedSizeInt</*element_type=*/ uint32_t>
// Then that buffer is passed to the protozero-generated setters as an argument.
// After calling the setter, the buffer can be destroyed.
//
// An example of encoding a packed field:
//   protozero::HeapBuffered<protozero::Message> msg;
//   protozero::PackedVarInt buf;
//   buf.Append(42);
//   buf.Append(-1);
//   msg->set_fieldname(buf);
//   msg.SerializeAsString();

class PackedBufferBase {
 public:
  PackedBufferBase() { Reset(); }

  // Copy or move is disabled due to pointers to stack addresses.
  PackedBufferBase(const PackedBufferBase&) = delete;
  PackedBufferBase(PackedBufferBase&&) = delete;
  PackedBufferBase& operator=(const PackedBufferBase&) = delete;
  PackedBufferBase& operator=(PackedBufferBase&&) = delete;

  void Reset();

  const uint8_t* data() const { return storage_begin_; }

  size_t size() const {
    return static_cast<size_t>(write_ptr_ - storage_begin_);
  }

 protected:
  void GrowIfNeeded() {
    PERFETTO_DCHECK(write_ptr_ >= storage_begin_ && write_ptr_ <= storage_end_);
    if (PERFETTO_UNLIKELY(write_ptr_ + kMaxElementSize > storage_end_)) {
      GrowSlowpath();
    }
  }

  void GrowSlowpath();

  // max(uint64_t varint encoding, biggest fixed type (uint64)).
  static constexpr size_t kMaxElementSize = 10;

  // So sizeof(this) == 8k.
  static constexpr size_t kOnStackStorageSize = 8192 - 32;

  uint8_t* storage_begin_;
  uint8_t* storage_end_;
  uint8_t* write_ptr_;
  std::unique_ptr<uint8_t[]> heap_buf_;
  alignas(uint64_t) uint8_t stack_buf_[kOnStackStorageSize];
};

class PackedVarInt : public PackedBufferBase {
 public:
  template <typename T>
  void Append(T value) {
    GrowIfNeeded();
    write_ptr_ = proto_utils::WriteVarInt(value, write_ptr_);
  }
};

template <typename T /* e.g. uint32_t for Fixed32 */>
class PackedFixedSizeInt : public PackedBufferBase {
 public:
  void Append(T value) {
    static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                  "PackedFixedSizeInt should be used only with 32/64-bit ints");
    static_assert(sizeof(T) <= kMaxElementSize,
                  "kMaxElementSize needs to be updated");
    GrowIfNeeded();
    PERFETTO_DCHECK(reinterpret_cast<size_t>(write_ptr_) % alignof(T) == 0);
    memcpy(reinterpret_cast<T*>(write_ptr_), &value, sizeof(T));
    write_ptr_ += sizeof(T);
  }
};

}  // namespace protozero

#endif  // INCLUDE_PERFETTO_PROTOZERO_PACKED_REPEATED_FIELDS_H_
