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
#ifndef LIEF_ART_HEADER_H
#define LIEF_ART_HEADER_H

#include <array>
#include <cstdint>

#include "LIEF/ART/types.hpp"
#include "LIEF/ART/enums.hpp"

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"

namespace LIEF {
namespace ART {
class Parser;

class LIEF_API Header : public Object {
  friend class Parser;

  public:
  using magic_t = std::array<uint8_t, 4>;

  Header();

  template<class T>
  LIEF_LOCAL Header(const T* header);

  Header(const Header&);
  Header& operator=(const Header&);

  magic_t magic() const;
  art_version_t version() const;

  uint32_t image_begin() const;
  uint32_t image_size() const;

  uint32_t oat_checksum() const;

  uint32_t oat_file_begin() const;
  uint32_t oat_file_end() const;

  uint32_t oat_data_begin() const;
  uint32_t oat_data_end() const;

  int32_t patch_delta() const;

  uint32_t image_roots() const;

  uint32_t pointer_size() const;
  bool compile_pic() const;

  uint32_t nb_sections() const;
  uint32_t nb_methods() const;

  uint32_t boot_image_begin() const;
  uint32_t boot_image_size() const;

  uint32_t boot_oat_begin() const;
  uint32_t boot_oat_size() const;

  STORAGE_MODES storage_mode() const;

  uint32_t data_size() const;

  void accept(Visitor& visitor) const override;


  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Header& hdr);

  ~Header() override;

  private:
  magic_t       magic_;
  art_version_t version_;

  uint32_t image_begin_;
  uint32_t image_size_;

  uint32_t oat_checksum_;

  uint32_t oat_file_begin_;
  uint32_t oat_file_end_;

  uint32_t oat_data_begin_;
  uint32_t oat_data_end_;

  int32_t patch_delta_;
  uint32_t image_roots_;

  uint32_t pointer_size_;

  bool compile_pic_;

  uint32_t nb_sections_;
  uint32_t nb_methods_;

  bool is_pic_;

  // From ART 29
  // ===========
  uint32_t boot_image_begin_;
  uint32_t boot_image_size_;

  uint32_t boot_oat_begin_;
  uint32_t boot_oat_size_;

  STORAGE_MODES storage_mode_;

  uint32_t data_size_;
};

} // Namespace ART
} // Namespace LIEF

#endif
