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

#include <stddef.h>
#include <stdint.h>

#include "perfetto/ext/tracing/core/slice.h"
#include "src/tracing/core/packet_stream_validator.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  perfetto::Slices slices;
  for (const uint8_t* p = data; p < data + size;) {
    size_t slice_size = *(p++);
    size_t size_left = size - static_cast<size_t>(p - data);
    slice_size = std::min(slice_size, size_left);
    slices.emplace_back(perfetto::Slice(p, slice_size));
    p += slice_size;
  }
  perfetto::PacketStreamValidator::Validate(slices);
  return 0;
}
