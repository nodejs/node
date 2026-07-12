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
#ifndef LIEF_BINARY_STREAM_H
#define LIEF_BINARY_STREAM_H

#include <cstdint>
#include <vector>
#include <cstring>
#include <string>
#include <algorithm>

#include "LIEF/endianness_support.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
class ASN1Reader;

/// Class that is used to a read stream of data from different sources
class LIEF_API BinaryStream {
  public:
  friend class ASN1Reader;

  enum class STREAM_TYPE {
    UNKNOWN = 0,
    VECTOR,
    MEMORY,
    SPAN,
    FILE,

    ELF_DATA_HANDLER,
  };

  BinaryStream(STREAM_TYPE type) :
    stype_(type)
  {}
  virtual ~BinaryStream() = default;
  virtual uint64_t size() const = 0;

  STREAM_TYPE type() const {
    return stype_;
  }

  result<uint64_t> read_uleb128(size_t* size = nullptr) const;
  result<uint64_t> read_sleb128(size_t* size = nullptr) const;

  result<int64_t> read_dwarf_encoded(uint8_t encoding) const;

  result<std::string> read_string(size_t maxsize = ~static_cast<size_t>(0)) const;
  result<std::string> peek_string(size_t maxsize = ~static_cast<size_t>(0)) const;
  result<std::string> peek_string_at(size_t offset, size_t maxsize = ~static_cast<size_t>(0)) const;

  result<std::u16string> read_u16string() const;
  result<std::u16string> peek_u16string() const;

  result<std::string> read_mutf8(size_t maxsize = ~static_cast<size_t>(0)) const;

  result<std::u16string> read_u16string(size_t length) const;
  result<std::u16string> peek_u16string(size_t length) const;
  result<std::u16string> peek_u16string_at(size_t offset, size_t length) const;


  virtual ok_error_t peek_data(std::vector<uint8_t>& container,
                               uint64_t offset, uint64_t size,
                               uint64_t virtual_address = 0)
  {
    if (size == 0) {
      return ok();
    }
    // Even though offset + size < ... => offset < ...
    // the addition could overflow so it's worth checking both
    const bool read_ok = offset <= this->size() && (offset + size) <= this->size()
                                                /* Check for an overflow */
                                                && (static_cast<int64_t>(offset) >= 0 && static_cast<int64_t>(size) >= 0)
                                                && (static_cast<int64_t>(offset + size) >= 0);
    if (!read_ok) {
      return make_error_code(lief_errors::read_error);
    }
    container.resize(size);
    if (peek_in(container.data(), offset, size, virtual_address)) {
      return ok();
    }
    return make_error_code(lief_errors::read_error);
  }

  virtual ok_error_t read_data(std::vector<uint8_t>& container, uint64_t size) {
    if (!peek_data(container, pos(), size)) {
      return make_error_code(lief_errors::read_error);
    }

    increment_pos(size);
    return ok();
  }

  ok_error_t read_data(std::vector<uint8_t>& container) {
    const size_t size = this->size() - this->pos();
    return read_data(container, size);
  }

  template<class T>
  ok_error_t read_objects(std::vector<T>& container, uint64_t count) {
    if (count == 0) {
      return ok();
    }
    const size_t size = count * sizeof(T);
    auto ret = peek_objects(container, count);
    if (!ret) {
      return make_error_code(lief_errors::read_error);
    }
    increment_pos(size);
    return ok();
  }

  template<class T>
  ok_error_t peek_objects(std::vector<T>& container, uint64_t count) {
    return peek_objects_at(pos(), container, count);
  }

  template<class T>
  ok_error_t peek_objects_at(uint64_t offset, std::vector<T>& container, uint64_t count) {
    if (count == 0) {
      return ok();
    }
    const auto current_p = pos();
    setpos(offset);

    const size_t size = count * sizeof(T);

    if (!can_read(offset, size)) {
      setpos(current_p);
      return make_error_code(lief_errors::read_error);
    }

    container.resize(count);

    if (!peek_in(container.data(), pos(), size)) {
      setpos(current_p);
      return make_error_code(lief_errors::read_error);
    }

    setpos(current_p);
    return ok();
  }

  void setpos(size_t pos) const {
    pos_ = pos;
  }

  const BinaryStream& increment_pos(size_t value) const {
    pos_ += value;
    return *this;
  }

  void decrement_pos(size_t value) const {
    if (pos_ > value) {
      pos_ -= value;
    } else {
      pos_ = 0;
    }
  }

  size_t pos() const {
    return pos_;
  }

  operator bool() const {
    return pos_ < size();
  }

  template<class T>
  const T* read_array(size_t size) const;

  template<class T, size_t N>
  ok_error_t peek_array(std::array<T, N>& dst) const {
    if /*constexpr*/ (N == 0) {
      return ok();
    }
    // Even though offset + size < ... => offset < ...
    // the addition could overflow so it's worth checking both
    const bool read_ok = pos_ <= size() && (pos_ + N) <= size()
      /* Check for an overflow */
      && (static_cast<int64_t>(pos_) >= 0 && static_cast<int64_t>(N) >= 0)
      && (static_cast<int64_t>(pos_ + N) >= 0);

    if (!read_ok) {
      return make_error_code(lief_errors::read_error);
    }
    if (peek_in(dst.data(), pos_, N)) {
      return ok();
    }
    return make_error_code(lief_errors::read_error);
  }

  template<class T, size_t N>
  ok_error_t read_array(std::array<T, N>& dst) const {
    if (!peek_array<T, N>(dst)) {
      return make_error_code(lief_errors::read_error);
    }

    increment_pos(N);
    return ok();
  }

  template<class T>
  result<T> peek() const;

  template<class T>
  result<T> peek(size_t offset) const;

  template<class T>
  const T* peek_array(size_t size) const;

  template<class T>
  const T* peek_array(size_t offset, size_t size) const;

  template<class T>
  result<T> read() const;

  template<typename T>
  bool can_read() const;

  template<typename T>
  bool can_read(size_t offset) const;

  bool can_read(int64_t offset, int64_t size) const {
    return offset < (int64_t)this->size() && (offset + size) < (int64_t)this->size();
  }

  size_t align(size_t align_on) const;

  void set_endian_swap(bool swap) {
    endian_swap_ = swap;
  }

  template<class T>
  static bool is_all_zero(const T& buffer) {
    const auto* ptr = reinterpret_cast<const uint8_t *const>(&buffer);
    return std::all_of(ptr, ptr + sizeof(T),
                       [] (uint8_t x) { return x == 0; });
  }

  bool should_swap() const {
    return endian_swap_;
  }

  virtual const uint8_t* p() const  {
    return nullptr;
  }

  virtual uint8_t* start() {
    return const_cast<uint8_t*>(static_cast<const BinaryStream*>(this)->start());
  }

  virtual uint8_t* p() {
    return const_cast<uint8_t*>(static_cast<const BinaryStream*>(this)->p());
  }

  virtual uint8_t* end() {
    return const_cast<uint8_t*>(static_cast<const BinaryStream*>(this)->end());
  }

  virtual const uint8_t* start() const {
    return nullptr;
  }

  virtual const uint8_t* end() const {
    return nullptr;
  }

  virtual result<const void*> read_at(uint64_t offset, uint64_t size,
                                      uint64_t virtual_address = 0) const = 0;
  virtual ok_error_t peek_in(void* dst, uint64_t offset, uint64_t size,
                             uint64_t virtual_address = 0) const {
    if (auto raw = read_at(offset, size, virtual_address)) {
      if (dst == nullptr) {
        return make_error_code(lief_errors::read_error);
      }

      const void* ptr = *raw;

      if (ptr == nullptr) {
        return make_error_code(lief_errors::read_error);
      }

      memcpy(dst, ptr, size);
      return ok();
    }
    return make_error_code(lief_errors::read_error);
  }

  protected:
  BinaryStream() = default;

  mutable size_t pos_ = 0;
  bool endian_swap_ = false;
  STREAM_TYPE stype_ = STREAM_TYPE::UNKNOWN;
};

class ScopedStream {
  public:
  ScopedStream(const ScopedStream&) = delete;
  ScopedStream& operator=(const ScopedStream&) = delete;

  ScopedStream(ScopedStream&&) = delete;
  ScopedStream& operator=(ScopedStream&&) = delete;

  explicit ScopedStream(BinaryStream& stream, uint64_t pos) :
    pos_{stream.pos()},
    stream_{stream}
  {
    stream_.setpos(pos);
  }

  explicit ScopedStream(BinaryStream& stream) :
    pos_{stream.pos()},
    stream_{stream}
  {}

  ~ScopedStream() {
    stream_.setpos(pos_);
  }

  BinaryStream* operator->() {
    return &stream_;
  }

  BinaryStream& operator*() {
    return stream_;
  }

  const BinaryStream& operator*() const {
    return stream_;
  }

  private:
  uint64_t pos_ = 0;
  BinaryStream& stream_;
};

class ToggleEndianness {
  public:
  ToggleEndianness(const ToggleEndianness&) = delete;
  ToggleEndianness& operator=(const ToggleEndianness&) = delete;

  ToggleEndianness(ToggleEndianness&&) = delete;
  ToggleEndianness& operator=(ToggleEndianness&&) = delete;

  explicit ToggleEndianness(BinaryStream& stream, bool value) :
    endian_swap_(stream.should_swap()),
    stream_{stream}
  {
    stream.set_endian_swap(value);
  }

  explicit ToggleEndianness(BinaryStream& stream) :
    endian_swap_(stream.should_swap()),
    stream_{stream}
  {
    stream.set_endian_swap(!stream_.should_swap());
  }

  ~ToggleEndianness() {
    stream_.set_endian_swap(endian_swap_);
  }

  BinaryStream* operator->() {
    return &stream_;
  }

  BinaryStream& operator*() {
    return stream_;
  }

  const BinaryStream& operator*() const {
    return stream_;
  }

  private:
  bool endian_swap_ = false;
  BinaryStream& stream_;
};


template<class T>
result<T> BinaryStream::read() const {
  result<T> tmp = this->peek<T>();
  if (!tmp) {
    return tmp;
  }
  this->increment_pos(sizeof(T));
  return tmp;
}

template<class T>
result<T> BinaryStream::peek() const {
  const auto current_p = pos();
  T ret{};
  if (auto res = peek_in(&ret, pos(), sizeof(T))) {
    setpos(current_p);
    if (endian_swap_) {
      swap_endian(&ret);
    }
    return ret;
  }

  setpos(current_p);
  return make_error_code(lief_errors::read_error);
}

template<class T>
result<T> BinaryStream::peek(size_t offset) const {
  const size_t saved_offset = this->pos();
  this->setpos(offset);
  result<T> r = this->peek<T>();
  this->setpos(saved_offset);
  return r;
}


template<class T>
const T* BinaryStream::peek_array(size_t size) const {
  result<const void*> raw = this->read_at(this->pos(), sizeof(T) * size);
  if (!raw) {
    return nullptr;
  }
  return reinterpret_cast<const T*>(raw.value());
}

template<class T>
const T* BinaryStream::peek_array(size_t offset, size_t size) const {
  const size_t saved_offset = this->pos();
  this->setpos(offset);
  const T* r = this->peek_array<T>(size);
  this->setpos(saved_offset);
  return r;
}


template<typename T>
bool BinaryStream::can_read() const {
  // Even though pos_ + sizeof(T) < ... => pos_ < ...
  // the addition could overflow so it's worth checking both
  return pos_ < size() && (pos_ + sizeof(T)) < size();
}


template<typename T>
bool BinaryStream::can_read(size_t offset) const {
  // Even though offset + sizeof(T) < ... => offset < ...
  // the addition could overflow so it's worth checking both
  return offset < size() && (offset + sizeof(T)) < size();
}


template<class T>
const T* BinaryStream::read_array(size_t size) const {
  const T* tmp = this->peek_array<T>(size);
  this->increment_pos(sizeof(T) * size);
  return tmp;
}

}
#endif
