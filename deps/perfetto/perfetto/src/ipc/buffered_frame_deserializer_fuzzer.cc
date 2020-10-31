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

#include <stddef.h>
#include <stdint.h>

#include "perfetto/ext/base/utils.h"
#include "src/ipc/buffered_frame_deserializer.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  perfetto::ipc::BufferedFrameDeserializer bfd;
  size_t write_offset = 0;
  while (write_offset < size) {
    size_t available_size = size - write_offset;
    auto rbuf = bfd.BeginReceive();
    size_t chunk_size = std::min(available_size, rbuf.size);
    memcpy(rbuf.data, data, chunk_size);
    if (!bfd.EndReceive(chunk_size))
      break;
    write_offset += chunk_size;
  }
  return 0;
}
