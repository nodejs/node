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
#ifndef LIEF_OSTREAM_H
#define LIEF_OSTREAM_H
#include <limits>
#include <cassert>
#include <ios>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>

#include "LIEF/span.hpp"
#include "LIEF/optional.hpp"
#include "LIEF/endianness_support.hpp"

namespace LIEF {
class vector_iostream {
  public:
  static size_t uleb128_size(uint64_t value);
  static size_t sleb128_size(int64_t value);

  using pos_type = std::streampos;
  using off_type = std::streamoff;
  enum class RELOC_OP {
    ADD, SUB
  };

  vector_iostream() = default;
  vector_iostream(std::vector<uint8_t>& ref) :
    raw_(&ref)
  {}
  vector_iostream(bool endian_swap) :
    endian_swap_(endian_swap)
  {}

  vector_iostream& reserve(size_t size) {
    raw_->reserve(size);
    return *this;
  }

  vector_iostream& increase_capacity(size_t size) {
    raw_->reserve(raw_->size() + size);
    return *this;
  }

  vector_iostream& put(uint8_t c);
  vector_iostream& write(const uint8_t* s, std::streamsize n);
  vector_iostream& write(span<const uint8_t> sp) {
    return write(sp.data(), sp.size());
  }

  vector_iostream& write(std::vector<uint8_t> s) {
    if (s.empty()) {
      return *this;
    }
    return write(s.data(), s.size());
  }

  vector_iostream& write(const std::string& s) {
    return write(reinterpret_cast<const uint8_t*>(s.c_str()), s.size() + 1);
  }

  bool empty() const {
    return raw_->empty();
  }

  vector_iostream& write(const std::u16string& s, bool with_null_char);

  vector_iostream& write(size_t count, uint8_t value) {
    raw_->insert(raw_->end(), count, value);
    current_pos_ += count;
    return *this;
  }
  vector_iostream& write_sized_int(uint64_t value, size_t size) {
    const uint64_t stack_val = value;
    return write(reinterpret_cast<const uint8_t*>(&stack_val), size);
  }

  vector_iostream& write(const vector_iostream& other) {
    return write(other.data());
  }

  template<class T, typename = typename std::enable_if<std::is_standard_layout<T>::value && std::is_trivial<T>::value>::type>
  vector_iostream& write(const T& t) {
    const auto pos = static_cast<size_t>(tellp());
    if (raw_->size() < (pos + sizeof(T))) {
      raw_->resize(pos + sizeof(T));
    }
    if (endian_swap_) {
      T tmp = t;
      swap_endian(&tmp);
      memcpy(raw_->data() + pos, &tmp, sizeof(T));
    } else {
      memcpy(raw_->data() + pos, &t, sizeof(T));
    }
    current_pos_ += sizeof(T);
    return *this;
  }

  vector_iostream& align(size_t alignment, uint8_t fill = 0);

  template<typename T>
  vector_iostream& write(const std::pair<T, T>& p) {
    write(p.first);
    write(p.second);
    return *this;
  }

  template<typename T, size_t size>
  vector_iostream& write(const std::array<T, size>& t) {
    static_assert(std::numeric_limits<T>::is_integer, "Requires integer type");
    for (T val : t) {
      write<T>(val);
    }
    return *this;
  }

  template<typename T>
  vector_iostream& write(const std::vector<T>& elements) {
    for (const T& e : elements) {
      write(e);
    }
    return *this;
  }

  template<typename T, class U>
  vector_iostream& write(const optional<U>& opt) {
    return opt ? write<T>(*opt) : *this;
  }

  vector_iostream& write_uleb128(uint64_t value);
  vector_iostream& write_sleb128(int64_t value);

  vector_iostream& get(std::vector<uint8_t>& c) {
    c = *raw_;
    return *this;
  }
  vector_iostream& move(std::vector<uint8_t>& c) {
    c = std::move(owned_);
    return *this;
  }

  vector_iostream& flush() {
    return *this;
  }

  size_t size() const {
    return raw_->size();
  }

  // seeks:
  pos_type tellp() const {
    return current_pos_;
  }

  vector_iostream& seekp(pos_type p) {
    current_pos_ = p;
    return *this;
  }

  vector_iostream& seek_end() {
    return seekp(raw_->size());
  }

  vector_iostream& pad(size_t size, uint8_t value = 0) {
    raw_->resize(raw_->size() + size, value);
    return *this;
  }

  vector_iostream& seekp(vector_iostream::off_type p, std::ios_base::seekdir dir);

  const std::vector<uint8_t>& raw() const {
    return *raw_;
  }

  std::vector<uint8_t>& raw() {
    return *raw_;
  }

  void set_endian_swap(bool swap) {
    endian_swap_ = swap;
  }

  bool endian_swap() const {
    return endian_swap_;
  }

  template<class T>
  vector_iostream& reloc(uint64_t offset, T shift, RELOC_OP op = RELOC_OP::ADD) {
    static_assert(std::numeric_limits<T>::is_integer, "Requires integer type");
    if (offset > raw_->size() || (offset + sizeof(T) > raw_->size())) {
      return *this;
    }
    T& value = *reinterpret_cast<T*>(raw_->data() + offset);

    switch (op) {
      case RELOC_OP::ADD: value += shift; break;
      case RELOC_OP::SUB: value -= shift; break;
    }

    return *this;
  }

  vector_iostream& init_fixups(size_t count) {
    fixups_.resize(count);
    return *this;
  }


  vector_iostream& record_fixup(size_t id, bool cond) {
    if (cond) {
      fixups_[id].push_back(tellp());
    }
    return *this;
  }


  vector_iostream& record_fixup(size_t id) {
    return record_fixup(id, /*cond=*/true);
  }

  template<class T>
  vector_iostream& apply_fixup(size_t id, T value) {
    static_assert(std::numeric_limits<T>::is_integer, "Requires integer type");
    for (uint64_t offset : get_fixups(id)) {
      reloc<T>(offset, value);
    }
    return *this;
  }

  const std::vector<uint64_t>& get_fixups(size_t id) const {
    return fixups_[id];
  }

  template<class T>
  T* edit_as() {
    assert(((size_t)current_pos_ + sizeof(T)) <= raw_->size());
    return reinterpret_cast<T*>(raw_->data() + current_pos_);
  }

  template<class T>
  T* edit_as(size_t pos) {
    seekp(pos);
    assert(((size_t)current_pos_ + sizeof(T)) <= raw_->size());
    return reinterpret_cast<T*>(raw_->data() + current_pos_);
  }

  const vector_iostream& copy_into(const span<uint8_t>& sp, size_t sz) const {
    assert(sz <= raw_->size());
    std::copy(raw_->data(), raw_->data() + sz, sp.data());
    return *this;
  }

  const vector_iostream& copy_into(const span<uint8_t>& sp) const {
    if (sp.size() < this->size()) {
      return *this;
    }

    std::copy(raw_->begin(), raw_->end(), sp.begin());
    return *this;
  }

  span<const uint8_t> data() const {
    return *raw_;
  }

  vector_iostream& clear() {
    raw_->clear();
    return *this;
  }

  std::vector<uint8_t>::iterator begin() {
    return raw_->begin();
  }

  std::vector<uint8_t>::iterator end() {
    return raw_->end();
  }

  std::vector<uint8_t>::const_iterator begin() const {
    return raw_->begin();
  }

  std::vector<uint8_t>::const_iterator end() const {
    return raw_->end();
  }

  private:
  std::vector<std::vector<uint64_t>> fixups_;
  pos_type current_pos_ = 0;
  std::vector<uint8_t> owned_;
  std::vector<uint8_t>* raw_ = &owned_;
  bool endian_swap_ = false;
};

class ScopeOStream {
  public:
  ScopeOStream(const ScopeOStream&) = delete;
  ScopeOStream& operator=(const ScopeOStream&) = delete;

  ScopeOStream(ScopeOStream&&) = delete;
  ScopeOStream& operator=(ScopeOStream&&) = delete;

  explicit ScopeOStream(vector_iostream& stream, uint64_t pos) :
    pos_{stream.tellp()},
    stream_{stream}
  {
    stream_.seekp(pos);
  }

  explicit ScopeOStream(vector_iostream& stream) :
    pos_{stream.tellp()},
    stream_{stream}
  {}

  ~ScopeOStream() {
    stream_.seekp(pos_);
  }

  vector_iostream* operator->() {
    return &stream_;
  }

  vector_iostream& operator*() {
    return stream_;
  }

  const vector_iostream& operator*() const {
    return stream_;
  }

  private:
  std::streampos pos_ = 0;
  vector_iostream& stream_;
};



}
#endif
