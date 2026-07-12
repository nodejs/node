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
#ifndef LIEF_MAP_ITEM_H
#define LIEF_MAP_ITEM_H

#include <cstdint>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"

namespace LIEF {
namespace DEX {
class Parser;
class Class;

/// Class which represents an element of the MapList object
class LIEF_API MapItem : public Object {
  friend class Parser;

  public:
  enum class TYPES : uint16_t {
    HEADER                  = 0x0000,
    STRING_ID               = 0x0001,
    TYPE_ID                 = 0x0002,
    PROTO_ID                = 0x0003,
    FIELD_ID                = 0x0004,
    METHOD_ID               = 0x0005,
    CLASS_DEF               = 0x0006,
    CALL_SITE_ID            = 0x0007,
    METHOD_HANDLE           = 0x0008,
    MAP_LIST                = 0x1000,
    TYPE_LIST               = 0x1001,
    ANNOTATION_SET_REF_LIST = 0x1002,
    ANNOTATION_SET          = 0x1003,
    CLASS_DATA              = 0x2000,
    CODE                    = 0x2001,
    STRING_DATA             = 0x2002,
    DEBUG_INFO              = 0x2003,
    ANNOTATION              = 0x2004,
    ENCODED_ARRAY           = 0x2005,
    ANNOTATIONS_DIRECTORY   = 0x2006,

  };

  public:
  MapItem();
  MapItem(TYPES type, uint32_t offset, uint32_t size, uint16_t reserved = 0);

  MapItem(const MapItem&);
  MapItem& operator=(const MapItem&);

  /// The type of the item
  TYPES type() const;

  /// Reserved value (likely for alignment prupose)
  uint16_t reserved() const;

  /// The number of elements (the real meaning depends on the type)
  uint32_t size() const;

  /// Offset from the start of the DEX file to the items associated with
  /// the underlying TYPES
  uint32_t offset() const;

  void accept(Visitor& visitor) const override;


  LIEF_API friend std::ostream& operator<<(std::ostream& os, const MapItem& item);

  ~MapItem() override;

  private:
  TYPES    type_;
  uint16_t reserved_;
  uint32_t size_;
  uint32_t offset_;

};

} // Namespace DEX
} // Namespace LIEF
#endif
