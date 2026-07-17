/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "PE/checksum.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

namespace LIEF {
ChecksumStream& ChecksumStream::write(const uint8_t* s, size_t n) {
  SpanStream stream(s, n);
  return write(stream);
}

ChecksumStream& ChecksumStream::write(BinaryStream& chk_stream) {
  if (has_leftover()) {
    uint16_t chunk = leftover();
    if (auto res = chk_stream.read<uint8_t>()) {
      chunk = (*res << 8) | chunk;
    }
    clear_leftover();
    partial_sum_ += chunk;
    partial_sum_ = (partial_sum_ >> 16) + (partial_sum_ & 0xffff);
  }

  const uint64_t file_length = chk_stream.size() - chk_stream.pos();
  uint64_t nb_chunk = (file_length + 1) / sizeof(uint16_t); // Number of uint16_t chunks

  while (chk_stream) {
    uint16_t chunk = 0;
    if (auto res = chk_stream.read<uint16_t>()) {
      chunk = *res;
    } else {
      break;
    }
    --nb_chunk;
    partial_sum_ += chunk;
    partial_sum_ = (partial_sum_ >> 16) + (partial_sum_ & 0xffff);
  }

  if (nb_chunk > 0) {
    if (auto res = chk_stream.read<uint8_t>()) {
      set_leftover(*res);
    }
  }

  size_ += chk_stream.size();
  return *this;
}

uint32_t ChecksumStream::finalize() {
  if (has_leftover()) {
    uint8_t chunk = leftover();
    clear_leftover();
    partial_sum_ += chunk;
    partial_sum_ = (partial_sum_ >> 16) + (partial_sum_ & 0xffff);
  }

  auto partial_sum_res = static_cast<uint16_t>(((partial_sum_ >> 16) + partial_sum_) & 0xffff);
  const uint32_t binary_checksum = checksum_;
  const uint32_t adjust_sum_lsb = binary_checksum & 0xFFFF;
  const uint32_t adjust_sum_msb = binary_checksum >> 16;

  partial_sum_res -= static_cast<int>(partial_sum_res < adjust_sum_lsb);
  partial_sum_res -= adjust_sum_lsb;

  partial_sum_res -= static_cast<int>(partial_sum_res < adjust_sum_msb);
  partial_sum_res -= adjust_sum_msb;

  return static_cast<uint32_t>(partial_sum_res) + size_;
}
}
