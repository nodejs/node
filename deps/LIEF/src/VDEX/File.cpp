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

#include "LIEF/VDEX/File.hpp"
#include "LIEF/VDEX/hash.hpp"
#include "LIEF/DEX/File.hpp"

#if defined(LIEF_JSON_SUPPORT)
#include "visitors/json.hpp"
#endif

namespace LIEF {
namespace VDEX {

File::~File() = default;
File::File() = default;


const Header& File::header() const {
  return header_;
}

Header& File::header() {
  return const_cast<Header&>(static_cast<const File*>(this)->header());
}


File::it_dex_files File::dex_files() {
  return dex_files_;
}

File::it_const_dex_files File::dex_files() const {
  return dex_files_;
}

dex2dex_info_t File::dex2dex_info() const {
  dex2dex_info_t info;
  for (const std::unique_ptr<DEX::File>& dex_file : dex_files_) {
    info.emplace(dex_file.get(), dex_file->dex2dex_info());
  }
  return info;
}

std::string File::dex2dex_json_info() {

#if defined(LIEF_JSON_SUPPORT)
  json mapping = json::object();

  for (const std::unique_ptr<DEX::File>& dex_file : dex_files_) {
    json dex2dex = json::parse(dex_file->dex2dex_json_info());
    mapping[dex_file->name()] = dex2dex;
  }

  return mapping.dump();
#else
  return "";
#endif
}

void File::accept(Visitor& visitor) const {
  visitor.visit(*this);
}





std::ostream& operator<<(std::ostream& os, const File& vdex_file) {
  os << "Header" << '\n';
  os << "======" << '\n';

  os << vdex_file.header() << '\n' << '\n';


  os << "DEX Files" << '\n';
  os << "=========" << '\n';

  for (const DEX::File& f : vdex_file.dex_files()) {
    os << f << '\n' << '\n';
  }


  return os;
}

} // Namespace VDEX
} // Namespace LIEF
