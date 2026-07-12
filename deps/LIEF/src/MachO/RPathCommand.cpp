/* Copyright 2017 - 2021 J.Rieck (based on R. Thomas's work)
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
#include "LIEF/Visitor.hpp"
#include "LIEF/utils.hpp"

#include "LIEF/MachO/RPathCommand.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {

RPathCommand::RPathCommand(std::string path) :
  LoadCommand::LoadCommand(LoadCommand::TYPE::RPATH, 0),
  path_(std::move(path))
{
  size_ = align(sizeof(details::rpath_command) + path_.size() + 1, sizeof(uint64_t));
  original_data_.resize(size_);
}

RPathCommand::RPathCommand(const details::rpath_command& rpath) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(rpath.cmd), rpath.cmdsize},
  path_offset_(rpath.path)
{}

void RPathCommand::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& RPathCommand::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << path();
  return os;
}


}
}
