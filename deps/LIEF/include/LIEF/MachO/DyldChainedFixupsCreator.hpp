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
#ifndef LIEF_MACHO_DYLD_CHAINED_FIXUPS_CREATOR_H
#define LIEF_MACHO_DYLD_CHAINED_FIXUPS_CREATOR_H
#include "LIEF/visibility.h"
#include "LIEF/MachO/DyldChainedFormat.hpp"
#include "LIEF/MachO/DyldChainedFixups.hpp"
#include "LIEF/errors.hpp"

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace LIEF {
namespace MachO {
class RelocationFixup;
class Symbol;
class Binary;
class ChainedBindingInfo;
class SegmentCommand;
class ChainedBindingInfoList;

class strong_map_t;

// This class aims to provide facilities to (re)create the LC_DYLD_CHAINED_FIXUPS
// command.
class LIEF_LOCAL DyldChainedFixupsCreator {
  public:
  static constexpr uint32_t MAX_IMPORTS = (uint32_t(1) << 24) - 1;
  static constexpr uint32_t BIND24_THRESHOLD = (uint32_t(1) << 16) - 1;
  struct binding_info_t {
    uint64_t address = 0;
    std::string library;
    std::string symbol;
    bool weak;
    uint64_t addend;
  };

  struct reloc_info_t {
    uint64_t addr = 0;
    uint64_t target = 0;
  };
  DyldChainedFixupsCreator() = default;

  DyldChainedFixupsCreator& fixups_version(uint32_t value) {
    fixups_version_ = value;
    return *this;
  }

  DyldChainedFixupsCreator& imports_format(DYLD_CHAINED_FORMAT fmt) {
    imports_format_ = fmt;
    return *this;
  }

  DyldChainedFixupsCreator& add_relocation(uint64_t address, uint64_t target);

  DyldChainedFixupsCreator&
    add_relocations(const std::vector<reloc_info_t>& relocations)
  {
    std::copy(relocations.begin(), relocations.end(),
              std::back_inserter(relocations_));
    return *this;
  }

  DyldChainedFixupsCreator& add_binding(uint64_t address,
                                        std::string symbol, std::string library,
                                        uint64_t addend = 0, bool weak = false);

  DyldChainedFixupsCreator& add_binding(uint64_t address,
                                        std::string symbol,
                                        uint64_t addend = 0, bool weak = false)
  {
    return add_binding(address, std::move(symbol), "", addend, weak);
  }

  DyldChainedFixupsCreator& add_bindings(const std::vector<binding_info_t>& bindings) {
    std::copy(bindings.begin(), bindings.end(), std::back_inserter(bindings_));
    return *this;
  }

  DyldChainedFixups* create(Binary& target);

  private:
  struct LIEF_LOCAL binding_rebase_t {
    enum TYPE {
      UNKNOWN = 0,
      FIXUP, BINDING,
    };
    union {
      ChainedBindingInfo* binding = nullptr;
      RelocationFixup* fixup;
    };
    TYPE type = UNKNOWN;

    binding_rebase_t(ChainedBindingInfo& info) :
      binding(&info),
      type(BINDING)
    {}

    binding_rebase_t(RelocationFixup& fixup) :
      fixup(&fixup),
      type(FIXUP)
    {}

    uint64_t addr() const;

    bool is_binding() const {
      return type == BINDING;
    }

    bool is_fixup() const {
      return type == FIXUP;
    }

    friend bool operator<(const binding_rebase_t& lhs,
                          const binding_rebase_t& rhs) {
      return lhs.addr() < rhs.addr();
    }

    friend bool operator>(const binding_rebase_t& lhs,
                          const binding_rebase_t& rhs) {
      return lhs.addr() > rhs.addr();
    }

    friend bool operator>=(const binding_rebase_t& lhs,
                           const binding_rebase_t& rhs) {
      return !(lhs < rhs);
    }

    friend bool operator<=(const binding_rebase_t& lhs,
                           const binding_rebase_t& rhs) {
      return !(lhs > rhs);
    }

  };
  LIEF_LOCAL result<size_t> lib2ord(const Binary& bin, const Symbol& sym,
                                    const std::string& lib);
  LIEF_LOCAL const Symbol* find_symbol(const Binary& bin, const std::string& name);

  LIEF_LOCAL static DYLD_CHAINED_PTR_FORMAT pointer_format(const Binary& bin, size_t imp_count);
  LIEF_LOCAL ok_error_t process_relocations(Binary& target, DYLD_CHAINED_PTR_FORMAT ptr_fmt);
  LIEF_LOCAL ok_error_t process_bindings(
    Binary& target, strong_map_t& strong_map,
    std::unordered_map<std::string, size_t>& symbols_idx, DyldChainedFixups* cmd,
    DyldChainedFixups::binding_info_t& all_bindings);

  uint32_t fixups_version_ = 0;
  DYLD_CHAINED_FORMAT imports_format_;
  std::vector<binding_info_t> bindings_;
  std::vector<reloc_info_t> relocations_;
  std::unordered_map<SegmentCommand*, std::vector<binding_rebase_t>> segment_chains_;
  std::unordered_map<std::string, size_t> lib2ord_;
};
}
}
#endif
