
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
#include "LIEF/DEX/Header.hpp"
#include "LIEF/DEX/hash.hpp"
#include "internal_utils.hpp"

#include <numeric>
#include <iomanip>
#include <sstream>

#define PRINT_FIELD(name,attr) \
  os << std::setw(WIDTH) << std::setfill(' ') << name << std::hex << attr << '\n'

#define PRINT_LOCATION(name,attr)                                               \
  os << std::setw(WIDTH) << std::setfill(' ') << name << std::hex << attr.first \
     << std::dec << " (#" << attr.second << ")" << '\n'

namespace LIEF {
namespace DEX {

Header::Header() = default;
Header::Header(const Header&) = default;
Header& Header::operator=(const Header&) = default;



Header::magic_t Header::magic() const {
  return magic_;
}

uint32_t Header::checksum() const {
  return checksum_;
}

Header::signature_t Header::signature() const {
  return signature_;
}

uint32_t Header::file_size() const {
  return file_size_;
}

uint32_t Header::header_size() const {
  return header_size_;
}

uint32_t Header::endian_tag() const {
  return endian_tag_;
}

uint32_t Header::nb_classes() const {
  return class_defs_size_;
}

uint32_t Header::nb_methods() const {
  return method_ids_size_;
}

uint32_t Header::map() const {
  return map_off_;
}

Header::location_t Header::strings() const {
  return {string_ids_off_, string_ids_size_};
}

Header::location_t Header::link() const {
  return {link_off_, link_size_};
}

Header::location_t Header::types() const {
  return {type_ids_off_, type_ids_size_};
}

Header::location_t Header::prototypes() const {
  return {proto_ids_off_, proto_ids_size_};
}

Header::location_t Header::fields() const {
  return {field_ids_off_, field_ids_size_};
}

Header::location_t Header::methods() const {
  return {method_ids_off_, method_ids_size_};
}

Header::location_t Header::classes() const {
  return {class_defs_off_, class_defs_size_};
}

Header::location_t Header::data() const {
  return {data_off_, data_size_};
}


void Header::accept(Visitor& visitor) const {
  visitor.visit(*this);
}



std::ostream& operator<<(std::ostream& os, const Header& hdr) {
  static constexpr size_t WIDTH = 20;

  std::string magic_str;
  for (uint8_t c : hdr.magic()) {
    if (::isprint(c) != 0) {
      magic_str.push_back(static_cast<char>(c));
    } else {
      std::stringstream ss;
      ss << std::dec << "'\\" << static_cast<uint32_t>(c) << "'";
      magic_str += ss.str();
    }
  }

  const Header::signature_t& sig = hdr.signature();
  std::string sig_str = std::accumulate(
      std::begin(sig),
      std::end(sig),
      std::string{},
      [] (const std::string& s, uint8_t c) {
        std::stringstream ss;
        return s + hex_str(c);
      });


  os << std::hex << std::left << std::showbase;
  PRINT_FIELD("Magic:",       magic_str);
  PRINT_FIELD("Checksum:",    hdr.checksum());
  PRINT_FIELD("Signature:",   sig_str);
  PRINT_FIELD("File Size:",   hdr.file_size());
  PRINT_FIELD("Header Size:", hdr.header_size());
  PRINT_FIELD("Endian Tag:",  hdr.endian_tag());
  PRINT_FIELD("Map Offset:",  hdr.map());

  PRINT_LOCATION("Strings:",     hdr.strings());
  PRINT_LOCATION("Link:",        hdr.link());
  PRINT_LOCATION("Types:",       hdr.types());
  PRINT_LOCATION("Prototypes:",  hdr.prototypes());
  PRINT_LOCATION("Fields:",      hdr.fields());
  PRINT_LOCATION("Methods:",     hdr.methods());
  PRINT_LOCATION("Classes:",     hdr.classes());
  PRINT_LOCATION("Data:",        hdr.data());

  return os;
}

Header::~Header() = default;



} // Namespace DEX
} // Namespace LIEF

