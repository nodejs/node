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
#ifndef LIEF_HASH_H
#define LIEF_HASH_H

#include <vector>
#include <string>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"
#include "LIEF/Visitor.hpp"
#include "LIEF/span.hpp"
#include "LIEF/optional.hpp"

namespace LIEF {


class LIEF_API Hash : public Visitor {
  public:
  using value_type = size_t;
  template<class H = Hash>
  static value_type hash(const Object& obj);

  static value_type hash(const std::vector<uint8_t>& raw);
  static value_type hash(span<const uint8_t> raw);
  static value_type hash(const void* raw, size_t size);

  // combine two elements to produce a size_t.
  template<typename U = value_type>
  static value_type combine(value_type lhs, U rhs) {
    return (lhs ^ rhs) + 0x9e3779b9 + (lhs << 6) + (rhs >> 2);
  }

  public:
  using Visitor::visit;
  Hash();
  Hash(value_type init_value);

  virtual Hash& process(const Object& obj);
  virtual Hash& process(size_t integer);
  virtual Hash& process(const std::string& str);
  virtual Hash& process(const std::u16string& str);
  virtual Hash& process(const std::vector<uint8_t>& raw);
  virtual Hash& process(span<const uint8_t> raw);

  template<class T, typename = typename std::enable_if<std::is_enum<T>::value>::type>
  Hash& process(T v) {
    return process(static_cast<value_type>(v));
  }

  template<class It>
  Hash& process(typename It::iterator v) {
    return process(std::begin(v), std::end(v));
  }


  template<class T, size_t N>
  Hash& process(const std::array<T, N>& array) {
    process(std::begin(array), std::end(array));
    return *this;
  }

  template<class T>
  Hash& process(const std::vector<T>& vector) {
    process(std::begin(vector), std::end(vector));
    return *this;
  }

  template<class T>
  Hash& process(const std::set<T>& set) {
    process(std::begin(set), std::end(set));
    return *this;
  }

  template<class T>
  Hash& process(const optional<T>& opt) {
    if (opt) {
      return process(*opt);
    }
    return *this;
  }

  template<class U, class V>
  Hash& process(const std::pair<U, V>& p) {
    process(p.first);
    process(p.second);
    return *this;
  }

  template<class InputIt>
  Hash& process(InputIt begin, InputIt end) {
    for (auto&& it = begin; it != end; ++it) {
      process(*it);
    }
    return *this;
  }

  value_type value() const {
    return value_;
  }

  ~Hash() override;

  protected:
  value_type value_ = 0;

};

LIEF_API Hash::value_type hash(const Object& v);
LIEF_API Hash::value_type hash(const std::vector<uint8_t>& raw);
LIEF_API Hash::value_type hash(span<const uint8_t> raw);

template<class Hasher>
Hash::value_type Hash::hash(const Object& obj) {
  Hasher hasher;
  obj.accept(hasher);
  return hasher.value();
}

}


#endif
