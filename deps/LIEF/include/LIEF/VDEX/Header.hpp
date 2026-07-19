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
#ifndef LIEF_VDEX_HEADER_H
#define LIEF_VDEX_HEADER_H
#include <ostream>
#include <array>

#include "LIEF/VDEX/type_traits.hpp"
#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"

namespace LIEF {
namespace VDEX {
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

  /// Magic value used to identify VDEX
  magic_t magic() const;

  /// VDEX version number
  vdex_version_t version() const;

  /// Number of LIEF::DEX::File files registered
  uint32_t nb_dex_files() const;

  /// Size of **all** LIEF::DEX::File
  uint32_t dex_size() const;

  /// Size of verifier deps section
  uint32_t verifier_deps_size() const;

  /// Size of quickening info section
  uint32_t quickening_info_size() const;

  void accept(Visitor& visitor) const override;


  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Header& header);

  ~Header() override;

  private:
  magic_t magic_;
  vdex_version_t version_;

  uint32_t nb_dex_files_;
  uint32_t dex_size_;

  uint32_t verifier_deps_size_;
  uint32_t quickening_info_size_;
};

} // Namespace VDEX
} // Namespace LIEF

#endif
