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
#ifndef LIEF_ELF_DATA_HANDLER_NODE_H
#define LIEF_ELF_DATA_HANDLER_NODE_H

#include <cstdint>
#include "LIEF/visibility.h"

namespace LIEF::ELF::DataHandler {

class LIEF_LOCAL Node {
  public:
  enum Type : uint8_t {
    SECTION = 0,
    SEGMENT = 1,
    UNKNOWN = 2
  };
  Node() = default;
  Node(uint64_t offset, uint64_t size, Type type) :
    size_{size}, offset_{offset}, type_{type}
  {}

  Node& operator=(const Node&) = default;
  Node(const Node&) = default;

  uint64_t size() const {
    return size_;
  }
  uint64_t offset() const {
    return offset_;
  }

  Type type() const {
    return type_;
  }

  void size(uint64_t size) {
    size_ = size;
  }

  void type(Type type) {
    type_ = type;
  }

  void offset(uint64_t offset) {
    offset_ = offset;
  }

  bool operator==(const Node& rhs) const;
  bool operator!=(const Node& rhs) const {
    return !(*this == rhs);
  }

  bool operator<(const Node& rhs) const;
  bool operator<=(const Node& rhs) const {
    return (type() == rhs.type() && !(*this > rhs));
  }

  bool operator>(const Node& rhs) const;
  bool operator>=(const Node& rhs) const {
    return (type() == rhs.type() && !(*this < rhs));
  }
  ~Node() = default;

  private:
  uint64_t size_ = 0;
  uint64_t offset_ = 0;
  Type type_ = Type::UNKNOWN;
};

} // namespace LIEF::ELF::DataHandler

#endif
