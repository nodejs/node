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
#ifndef LIEF_PE_RESOURCE_ICON_H
#define LIEF_PE_RESOURCE_ICON_H
#include <ostream>
#include <climits>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"

#include "LIEF/span.hpp"
#include "LIEF/Object.hpp"

namespace LIEF {
namespace PE {
class ResourcesManager;

namespace details {
struct pe_resource_icon_group;
struct pe_icon_header;
}

class LIEF_API ResourceIcon : public Object {
  friend class ResourcesManager;

  public:
  static result<ResourceIcon>
    from_serialization(const uint8_t* buffer, size_t size);

  static result<ResourceIcon> from_serialization(const std::vector<uint8_t>& bytes) {
    return from_serialization(bytes.data(), bytes.size());
  }

  static result<ResourceIcon> from_bytes(LIEF::span<const uint8_t> bytes) {
    return from_serialization(bytes.data(), bytes.size_bytes());
  }

  ResourceIcon() = default;
  ResourceIcon(const details::pe_resource_icon_group& header);
  ResourceIcon(const details::pe_icon_header& header);

  ResourceIcon(const ResourceIcon&) = default;
  ResourceIcon& operator=(const ResourceIcon&) = default;

  ResourceIcon(ResourceIcon&&) = default;
  ResourceIcon& operator=(ResourceIcon&&) = default;

  ~ResourceIcon() override = default;

  /// Id associated with the icon
  uint32_t id() const {
    return id_;
  }

  /// Language associated with the icon
  uint32_t lang() const {
    return lang_;
  }

  /// Sub language associated with the icon
  uint32_t sublang() const {
    return sublang_;
  }

  /// Width in pixels of the image
  uint8_t width() const {
    return width_;
  }

  /// Height in pixels of the image
  uint8_t height() const {
    return height_;
  }

  /// Number of colors in image (0 if >=8bpp)
  uint8_t color_count() const {
    return color_count_;
  }

  /// Reserved (must be 0)
  uint8_t reserved() const {
    return reserved_;
  }

  /// Color Planes
  uint16_t planes() const {
    return planes_;
  }

  /// Bits per pixel
  uint16_t bit_count() const {
    return bit_count_;
  }

  /// Size in bytes of the image
  uint32_t size() const {
    return pixels_.size();
  }

  /// Pixels of the image (as bytes)
  span<const uint8_t> pixels() const {
    return pixels_;
  }

  void id(uint32_t id) {
    id_ = id;
  }

  void lang(uint32_t lang) {
    lang_ = lang;
  }

  void sublang(uint32_t sublang) {
    sublang_ = sublang;
  }

  void width(uint8_t width) {
    width_ = width;
  }

  void height(uint8_t height) {
    height_ = height;
  }

  void color_count(uint8_t color_count) {
    color_count_ = color_count;
  }

  void reserved(uint8_t reserved) {
    reserved_ = reserved;
  }

  void planes(uint16_t planes) {
    planes_ = planes;
  }

  void bit_count(uint16_t bit_count) {
    bit_count_ = bit_count;
  }

  void pixels(std::vector<uint8_t> pixels) {
    pixels_ = std::move(pixels);
  }

  /// Save the icon to the given filename
  ///
  /// @param[in] filename Path to file in which the icon will be saved
  void save(const std::string& filename) const;

  std::vector<uint8_t> serialize() const;

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const ResourceIcon& entry);

  private:
  uint8_t width_ = 0;
  uint8_t height_ = 0;
  uint8_t color_count_ = 0;
  uint8_t reserved_ = 0;
  uint16_t planes_ = 0;
  uint16_t bit_count_ = 0;
  uint32_t id_ = uint32_t(-1);
  uint32_t lang_ = /* LANG_NEUTRAL */0;
  uint32_t sublang_ = 0 /* SUBLANG_NEUTRAL */;
  std::vector<uint8_t> pixels_;
};

}
}


#endif
