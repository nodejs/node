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
#include <fstream>

#include "LIEF/iostream.hpp"
#include "LIEF/hash.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/PE/resources/ResourceIcon.hpp"
#include "PE/Structures.hpp"

#include "logging.hpp"
#include <spdlog/fmt/fmt.h>

namespace LIEF {
namespace PE {

ResourceIcon::ResourceIcon(const details::pe_resource_icon_group& header) :
  width_{header.width},
  height_{header.height},
  color_count_{header.color_count},
  reserved_{header.reserved},
  planes_{header.planes},
  bit_count_{header.bit_count},
  id_{header.ID}
{}


ResourceIcon::ResourceIcon(const details::pe_icon_header& header) :
  width_{header.width},
  height_{header.height},
  color_count_{header.color_count},
  reserved_{header.reserved},
  planes_{header.planes},
  bit_count_{header.bit_count},
  id_{static_cast<uint32_t>(-1)}
{}

result<ResourceIcon> ResourceIcon::from_serialization(const uint8_t* buffer, size_t size) {
  const uint32_t minsz = sizeof(details::pe_resource_icon_dir) +
                         sizeof(details::pe_icon_header);
  if (size < minsz) {
    return make_error_code(lief_errors::read_error);
  }

  SpanStream stream(buffer, size);
  /* reserved */stream.read<uint16_t>();
  auto type = stream.read<uint16_t>().value_or(0);
  if (type != 1) {
    return make_error_code(lief_errors::corrupted);
  }

  auto count = stream.read<uint16_t>().value_or(0);
  if (count != 1) {
    return make_error_code(lief_errors::corrupted);
  }

  ResourceIcon icon;

  icon.width(stream.read<uint8_t>().value_or(0));
  icon.height(stream.read<uint8_t>().value_or(0));
  icon.color_count(stream.read<uint8_t>().value_or(0));
  icon.reserved(stream.read<uint8_t>().value_or(0));
  icon.planes(stream.read<uint16_t>().value_or(0));
  icon.bit_count(stream.read<uint16_t>().value_or(0));

  auto pixel_size = stream.read<uint32_t>().value_or(0);
  if (pixel_size == 0) {
    return make_error_code(lief_errors::corrupted);
  }

  auto offset = stream.read<uint32_t>().value_or(0);
  if (offset == 0 || (offset + pixel_size) > size) {
    return make_error_code(lief_errors::corrupted);
  }
  std::vector<uint8_t> pixels;
  stream.read_data(pixels, pixel_size);
  icon.pixels(std::move(pixels));
  return icon;
}

std::vector<uint8_t> ResourceIcon::serialize() const {
  if (pixels_.empty()) {
    return {};
  }

  vector_iostream ios;
  ios.reserve(sizeof(details::pe_resource_icon_dir) +
              sizeof(details::pe_icon_header) + pixels_.size());

  ios
    // pe_resource_icon_dir header
    .write<uint16_t>(/*reserved*/0)
    .write<uint16_t>(/*type=icon*/1)
    .write<uint16_t>(/*count*/1)
    // pe_icon_header header
    .write<uint8_t>(width())
    .write<uint8_t>(height())
    .write<uint8_t>(color_count())
    .write<uint8_t>(reserved())
    .write<uint16_t>(planes())
    .write<uint16_t>(bit_count())
    .write<uint32_t>(size())
    .write<uint32_t>((uint32_t)ios.tellp() + sizeof(uint32_t))
    // Raw pixels
    .write(pixels())
  ;
  return ios.raw();
}

void ResourceIcon::save(const std::string& filename) const {
  std::ofstream output_file(filename,
                            std::ios::out | std::ios::binary |
                            std::ios::trunc);
  if (!output_file) {
    LIEF_ERR("Can't open {} for writing", filename);
    return;
  }
  std::vector<uint8_t> raw = serialize();
  output_file.write((const char*)raw.data(), raw.size());
}

void ResourceIcon::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const ResourceIcon& icon) {
  static constexpr auto DEFAULT_ALIGN = 13;
  os << fmt::format("Icon id=0x{:04x}\n", icon.id());
  os << fmt::format("  {:{}}: {}x{}\n", "size", DEFAULT_ALIGN, icon.width(), icon.height());
  os << fmt::format("  {:{}}: {}\n", "color count", DEFAULT_ALIGN, icon.color_count());
  os << fmt::format("  {:{}}: {}\n", "reserved", DEFAULT_ALIGN, icon.reserved());
  os << fmt::format("  {:{}}: {}\n", "planes", DEFAULT_ALIGN, icon.planes());
  os << fmt::format("  {:{}}: {}\n", "bit count", DEFAULT_ALIGN, icon.bit_count());
  os << fmt::format("  {:{}}: 0x{:08x}\n", "pixel (hash)", DEFAULT_ALIGN, Hash::hash(icon.pixels()));
  return os;
}



}
}

