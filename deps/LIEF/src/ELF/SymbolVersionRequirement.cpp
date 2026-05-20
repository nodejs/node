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
#include "LIEF/ELF/hash.hpp"

#include "LIEF/ELF/SymbolVersionAuxRequirement.hpp"
#include "LIEF/ELF/SymbolVersionRequirement.hpp"

#include "ELF/Structures.hpp"

namespace LIEF {
namespace ELF {

SymbolVersionRequirement::SymbolVersionRequirement(const details::Elf64_Verneed& header) :
  version_{header.vn_version}
{}

SymbolVersionRequirement::SymbolVersionRequirement(const details::Elf32_Verneed& header)  :
  version_{header.vn_version}
{}

SymbolVersionRequirement::SymbolVersionRequirement(const SymbolVersionRequirement& other) :
  Object{other},
  version_{other.version_},
  name_{other.name_}
{
  aux_requirements_.reserve(other.aux_requirements_.size());
  for (const std::unique_ptr<SymbolVersionAuxRequirement>& aux : other.aux_requirements_) {
    aux_requirements_.push_back(std::make_unique<SymbolVersionAuxRequirement>(*aux));
  }
}

SymbolVersionRequirement& SymbolVersionRequirement::operator=(SymbolVersionRequirement other) {
  swap(other);
  return *this;
}

void SymbolVersionRequirement::swap(SymbolVersionRequirement& other) {
  std::swap(aux_requirements_, other.aux_requirements_);
  std::swap(version_,          other.version_);
  std::swap(name_,             other.name_);
}


SymbolVersionAuxRequirement& SymbolVersionRequirement::add_aux_requirement(const SymbolVersionAuxRequirement& aux_requirement) {
  aux_requirements_.push_back(std::make_unique<SymbolVersionAuxRequirement>(aux_requirement));
  return *aux_requirements_.back();
}


bool SymbolVersionRequirement::remove_aux_requirement(SymbolVersionAuxRequirement& aux) {
  auto it = std::find_if(aux_requirements_.begin(), aux_requirements_.end(),
      [&aux] (const std::unique_ptr<SymbolVersionAuxRequirement>& element) {
        return &aux == element.get();
      }
  );

  if (it == aux_requirements_.end()) {
    return false;
  }

  aux_requirements_.erase(it);
  return true;
}

const SymbolVersionAuxRequirement*
  SymbolVersionRequirement::find_aux(const std::string& name) const
{
  auto it = std::find_if(aux_requirements_.begin(), aux_requirements_.end(),
      [&name] (const std::unique_ptr<SymbolVersionAuxRequirement>& aux) {
        return aux->name() == name;
      }
  );

  if (it == aux_requirements_.end()) {
    return nullptr;
  }

  return it->get();
}

void SymbolVersionRequirement::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

}
}
