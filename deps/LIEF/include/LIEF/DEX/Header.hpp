
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
#ifndef LIEF_DEX_HEADER_H
#define LIEF_DEX_HEADER_H

#include <array>
#include <utility>
#include <cstdint>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"

namespace LIEF {
class Visitor;

namespace DEX {
class Parser;

/// Class which represents the DEX header.
/// This is the first structure that begins the DEX format.
///
/// The official documentation is provided here:
/// https://source.android.com/devices/tech/dalvik/dex-format#header-item
class LIEF_API Header : public Object {
  friend class Parser;

  public:
  using location_t = std::pair<uint32_t, uint32_t>;

  using magic_t     = std::array<uint8_t, 8>;
  using signature_t = std::array<uint8_t, 20>;

  Header();
  Header(const Header&);
  Header& operator=(const Header&);

  template<class T>
  LIEF_LOCAL Header(const T& header);

  /// The DEX magic bytes (``DEX\n`` followed by the DEX version)
  magic_t magic() const;

  /// The file checksum
  uint32_t checksum() const;

  /// SHA-1 DEX signature (which is not really used as a signature)
  signature_t signature() const;

  /// Size of the entire file (including the current the header)
  uint32_t file_size() const;

  /// Size of this header. It should be 0x70
  uint32_t header_size() const;

  /// File endianess of the file
  uint32_t endian_tag() const;

  /// Offset from the start of the file to the map list (see: DEX::MapList)
  uint32_t map() const;

  /// Offset and size of the string pool
  location_t strings() const;
  location_t link() const;
  location_t types() const;
  location_t prototypes() const;
  location_t fields() const;
  location_t methods() const;
  location_t classes() const;
  location_t data() const;

  uint32_t nb_classes() const;

  uint32_t nb_methods() const;

  void accept(Visitor& visitor) const override;


  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Header& hdr);

  ~Header() override;

  private:
  magic_t magic_;
  uint32_t checksum_;
  signature_t signature_;
  uint32_t file_size_;

  uint32_t header_size_;
  uint32_t endian_tag_;

  uint32_t link_size_;
  uint32_t link_off_;

  uint32_t map_off_;

  uint32_t string_ids_size_;
  uint32_t string_ids_off_;

  uint32_t type_ids_size_;
  uint32_t type_ids_off_;

  uint32_t proto_ids_size_;
  uint32_t proto_ids_off_;

  uint32_t field_ids_size_;
  uint32_t field_ids_off_;

  uint32_t method_ids_size_;
  uint32_t method_ids_off_;

  uint32_t class_defs_size_;
  uint32_t class_defs_off_;

  uint32_t data_size_;
  uint32_t data_off_;
};

} // Namespace DEX
} // Namespace LIEF

#endif
