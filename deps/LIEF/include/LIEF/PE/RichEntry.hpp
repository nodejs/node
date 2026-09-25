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
#ifndef LIEF_PE_RICH_ENTRY_H
#define LIEF_PE_RICH_ENTRY_H
#include <cstdint>
#include <ostream>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace PE {

/// Class which represents an entry associated to the RichHeader
class LIEF_API RichEntry : public Object {
  public:

  RichEntry() = default;
  RichEntry(uint16_t id, uint16_t build_id, uint32_t count) :
    id_(id),
    build_id_(build_id),
    count_(count)
  {}

  RichEntry(const RichEntry&) = default;
  RichEntry& operator=(const RichEntry&) = default;
  ~RichEntry() override = default;

  /// Entry type
  uint16_t id() const {
    return id_;
  }

  /// Build number of the tool (if any)
  uint16_t build_id() const {
    return build_id_;
  }

  /// *Occurrence* count.
  uint32_t count() const {
    return count_;
  }

  void id(uint16_t id) {
    id_ = id;
  }
  void build_id(uint16_t build_id) {
    build_id_ = build_id;
  }
  void count(uint32_t count) {
    count_ = count;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const RichEntry& rich_entry);

  private:
  uint16_t id_ = 0;
  uint16_t build_id_ = 0;
  uint32_t count_ = 0;

};
}
}

#endif

