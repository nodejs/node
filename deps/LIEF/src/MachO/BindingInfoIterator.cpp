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
#include "LIEF/MachO/Binary.hpp"
#include "LIEF/MachO/BindingInfoIterator.hpp"
#include "LIEF/MachO/DyldInfo.hpp"
#include "LIEF/MachO/DyldBindingInfo.hpp"
#include "LIEF/MachO/ChainedBindingInfo.hpp"
#include "LIEF/MachO/IndirectBindingInfo.hpp"
#include "LIEF/MachO/DyldChainedFixups.hpp"

#include "logging.hpp"

namespace LIEF::MachO {

const BindingInfo& BindingInfoIterator::operator*() const {
  switch (origin_) {
    case ORIGIN::DYLD:
      return *dyld_info_->binding_info_.at(pos_);
    case ORIGIN::CHAINED_FIXUPS:
      return *chained_fixups_->all_bindings_.at(pos_);
    case ORIGIN::INDIRECT:
      return *binary_->indirect_bindings_.at(pos_);
    case ORIGIN::NONE:
      logging::fatal_error("Can't return a BindingInfo for a NONE iterator");
  }

  logging::fatal_error("Unsupported ORIGIN");
}
}
