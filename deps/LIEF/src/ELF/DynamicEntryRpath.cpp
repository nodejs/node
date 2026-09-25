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
#include "LIEF/ELF/DynamicEntryRpath.hpp"
#include "LIEF/Visitor.hpp"
#include "logging.hpp"

#include <algorithm>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <utility>

namespace LIEF {
namespace ELF {

std::vector<std::string> DynamicEntryRpath::paths() const {
  std::stringstream ss;
  ss.str(rpath());
  std::string path;
  std::vector<std::string> paths;
  while (std::getline(ss, path, DynamicEntryRpath::delimiter)) {
    paths.push_back(path);
  }
  return paths;
}

void DynamicEntryRpath::paths(const std::vector<std::string>& paths) {
  rpath_ = std::accumulate(std::begin(paths), std::end(paths), std::string(),
      [] (const std::string& path, const std::string& new_entry) {
        return path.empty() ? new_entry :  path + DynamicEntryRpath::delimiter + new_entry;
      });
}

DynamicEntryRpath& DynamicEntryRpath::append(std::string path) {
  std::vector<std::string> paths = this->paths();
  paths.push_back(std::move(path));
  this->paths(paths);
  return *this;
}

DynamicEntryRpath& DynamicEntryRpath::remove(const std::string& path) {
  std::vector<std::string> paths = this->paths();
  paths.erase(std::remove_if(std::begin(paths), std::end(paths),
                             [&path] (const std::string& p) {
                               return p == path;
                             }),
              std::end(paths));
  this->paths(paths);
  return *this;
}

DynamicEntryRpath& DynamicEntryRpath::insert(size_t pos, const std::string& path) {
  std::vector<std::string> paths = this->paths();

  if (pos == paths.size()) {
    return append(path);
  }

  if (pos > paths.size()) {
    LIEF_ERR("pos: {:d} is out of range", pos);
    return *this;
  }
  paths.insert(std::begin(paths) + pos, path);
  this->paths(paths);
  return *this;
}


void DynamicEntryRpath::accept(Visitor& visitor) const {
  visitor.visit(*this);
}


std::ostream& DynamicEntryRpath::print(std::ostream& os) const {
  DynamicEntry::print(os);
  os << fmt::format("{:<10}", rpath());
  return os;
}

}
}



