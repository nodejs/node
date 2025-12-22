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
#ifndef LIEF_PE_FACTORY_H
#define LIEF_PE_FACTORY_H

#include "LIEF/visibility.h"
#include "LIEF/PE/Binary.hpp"
#include "LIEF/PE/Section.hpp"
#include "LIEF/PE/Import.hpp"
#include <memory>

namespace LIEF {
namespace PE {

/// This factory is used to create PE from scratch
class LIEF_API Factory {
  public:
  Factory(const Factory&) = delete;
  Factory& operator=(const Factory&) = delete;

  Factory(Factory&&);
  Factory& operator=(Factory&&);

  /// Initiate the factory to construct a PE which the given type
  static std::unique_ptr<Factory> create(PE_TYPE type);

  Factory& add_section(const Section& section) {
    sections_.push_back(std::unique_ptr<Section>(new Section(section)));
    return *this;
  }

  Factory& set_arch(Header::MACHINE_TYPES arch) {
    pe_->header().machine(arch);
    return *this;
  }

  Factory& set_entrypoint(uint64_t ep) {
    pe_->optional_header().addressof_entrypoint(ep);
    return *this;
  }

  std::unique_ptr<Binary> get() {
    return process();
  }

  bool is_32bit() const {
    return pe_->type() == PE_TYPE::PE32;
  }

  bool is_64bit() const {
    return pe_->type() == PE_TYPE::PE32_PLUS;
  }

  uint32_t section_align() const {
    return pe_->optional_header().section_alignment();
  }

  uint32_t file_align() const {
    return pe_->optional_header().file_alignment();
  }

  ~Factory();

  protected:
  Factory() :
    pe_(std::unique_ptr<Binary>(new Binary{}))
  {}
  std::unique_ptr<Binary> process();

  ok_error_t check_overlapping() const;
  ok_error_t assign_locations();
  ok_error_t update_headers();
  ok_error_t move_sections();
  uint32_t sizeof_headers() const;

  std::vector<std::unique_ptr<Section>> sections_;
  std::vector<Import> imports_;
  std::unique_ptr<Binary> pe_;
};
}
}
#endif

