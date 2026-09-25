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
#ifndef LIEF_VDEX_FILE_H
#define LIEF_VDEX_FILE_H
#include <ostream>

#include "LIEF/VDEX/Header.hpp"
#include "LIEF/VDEX/type_traits.hpp"

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"
#include "LIEF/iterators.hpp"

#include <vector>
#include <memory>

namespace LIEF {
namespace DEX {
class File;
}
namespace OAT {
class Binary;
}

namespace VDEX {
class Parser;

/// Main class for the VDEX module which represents a VDEX file
class LIEF_API File : public Object {
  friend class Parser;
  friend class OAT::Binary;

  public:
  using dex_files_t = std::vector<std::unique_ptr<DEX::File>>;
  using it_dex_files = ref_iterator<dex_files_t&, DEX::File*>;
  using it_const_dex_files = const_ref_iterator<const dex_files_t&, const DEX::File*>;

  File& operator=(const File& copy) = delete;
  File(const File& copy)            = delete;

  /// VDEX Header
  const Header& header() const;
  Header& header();

  /// Iterator over LIEF::DEX::Files registered
  it_dex_files       dex_files();
  it_const_dex_files dex_files() const;

  dex2dex_info_t dex2dex_info() const;

  std::string dex2dex_json_info();

  void accept(Visitor& visitor) const override;


  ~File() override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const File& vdex_file);

  private:
  File();

  Header header_;
  dex_files_t dex_files_;
};

}
}

#endif
