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
#ifndef LIEF_HASH_STREAM_H
#define LIEF_HASH_STREAM_H
#include <vector>
#include <string>
#include <array>
#include <memory>
#include <type_traits>

#include "LIEF/span.hpp"

namespace LIEF {
class hashstream {
  public:
  enum class HASH {
    MD5,
    SHA1,
    SHA224,
    SHA256,
    SHA384,
    SHA512
  };
  hashstream(HASH type);

  hashstream& write(const uint8_t* s, size_t n);
  hashstream& put(uint8_t c) {
    return write(&c, 1);
  }
  hashstream& write(const std::vector<uint8_t>& s) {
    return write(s.data(), s.size());
  }
  hashstream& write(const std::string& s) {
    return write(reinterpret_cast<const uint8_t*>(s.c_str()), s.size() + 1);
  }
  hashstream& write(size_t count, uint8_t value) {
    return write(std::vector<uint8_t>(count, value));
  }
  hashstream& write_sized_int(uint64_t value, size_t size) {
    return write(reinterpret_cast<const uint8_t*>(&value), size);
  }

  hashstream& align(size_t size, uint8_t val = 0);

  template<typename T>
  hashstream& write(span<const T> s) {
    static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, char>, "Require an integer");
    return write(reinterpret_cast<const uint8_t*>(s.data()), s.size());
  }

  template<class Integer>
  hashstream& write(Integer integer) {
    static_assert(std::is_integral<Integer>::value, "Require an integer");
    const auto* int_p = reinterpret_cast<const uint8_t*>(&integer);
    return write(int_p, sizeof(Integer));
  }

  template<typename T, size_t size>
  hashstream& write(const std::array<T, size>& t) {
    static_assert(std::is_integral<T>::value, "Require an integer");
    for (T val : t) {
      write<T>(val);
    }
    return *this;
  }

  hashstream& get(std::vector<uint8_t>& c) {
    flush();
    c = output_;
    return *this;
  }
  hashstream& flush();

  std::vector<uint8_t>& raw() {
    flush();
    return output_;
  }
  ~hashstream();

  private:
  std::vector<uint8_t> output_;
  using md_context_t = intptr_t;
  std::unique_ptr<md_context_t> ctx_;
};


}
#endif
