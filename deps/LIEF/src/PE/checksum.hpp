/* Copyright 2024 - 2025 R. Thomas
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
#ifndef LIEF_PE_CHECKSUM_H
#define LIEF_PE_CHECKSUM_H
#include <cstdint>
#include <type_traits>
#include <cstddef>

#include "LIEF/span.hpp"

namespace LIEF {
class BinaryStream;
class ChecksumStream {
  public:
  static constexpr auto LEFTOVER_BIT = 30;
  ChecksumStream() = default;
  ChecksumStream(uint32_t checksum) :
    checksum_(checksum)
  {}

  ChecksumStream& put(uint8_t c) {
    write(&c, 1);
    return *this;
  }

  uint8_t leftover() const {
    return static_cast<uint8_t>(leftover_ & 0xFF);
  }

  bool has_leftover() const {
    return (leftover_ >> LEFTOVER_BIT) & 1;
  }

  ChecksumStream& set_leftover(uint8_t value) {
    leftover_ = (uint32_t(1) << LEFTOVER_BIT) | value;
    return *this;
  }


  ChecksumStream& clear_leftover() {
    leftover_ = 0;
    return *this;
  }

  ChecksumStream& write(BinaryStream& stream);
  ChecksumStream& write(const uint8_t* s, size_t n);

  ChecksumStream& write_sized_int(uint64_t value, size_t size) {
    return write(reinterpret_cast<const uint8_t*>(&value), size);
  }

  template<class T, typename = typename std::enable_if<
    (std::is_standard_layout_v<T> && std::is_trivial_v<T>) || std::is_integral_v<T>
  >::type>
  ChecksumStream& write(const T& t) {
    return write(reinterpret_cast<const uint8_t*>(&t), sizeof(T));
  }

  ChecksumStream& write(span<const uint8_t> sp) {
    return write(sp.data(), sp.size());
  }

  uint32_t finalize();

  private:
  uint32_t checksum_ = 0;
  uint32_t partial_sum_ = 0;
  size_t size_ = 0;
  uint32_t leftover_ = 0;
};
}
#endif
