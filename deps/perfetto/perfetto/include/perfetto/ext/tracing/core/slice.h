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

#ifndef INCLUDE_PERFETTO_EXT_TRACING_CORE_SLICE_H_
#define INCLUDE_PERFETTO_EXT_TRACING_CORE_SLICE_H_

#include <stddef.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include "perfetto/base/logging.h"

namespace perfetto {

// A simple wrapper around a virtually contiguous memory range that contains a
// TracePacket, or just a portion of it.
struct Slice {
  Slice() : start(nullptr), size(0) {}
  Slice(const void* st, size_t sz) : start(st), size(sz) {}
  Slice(Slice&& other) noexcept = default;

  // Create a Slice which owns |size| bytes of memory.
  static Slice Allocate(size_t size) {
    Slice slice;
    slice.own_data_.reset(new uint8_t[size]);
    slice.start = &slice.own_data_[0];
    slice.size = size;
    return slice;
  }

  uint8_t* own_data() {
    PERFETTO_DCHECK(own_data_);
    return own_data_.get();
  }

  const void* start;
  size_t size;

 private:
  Slice(const Slice&) = delete;
  void operator=(const Slice&) = delete;

  std::unique_ptr<uint8_t[]> own_data_;
};

// TODO(primiano): most TracePacket(s) fit in a slice or two. We need something
// a bit more clever here that has inline capacity for 2 slices and then uses a
// std::forward_list or a std::vector for the less likely cases.
using Slices = std::vector<Slice>;

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_TRACING_CORE_SLICE_H_
