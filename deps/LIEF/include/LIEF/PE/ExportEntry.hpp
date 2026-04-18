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
#ifndef LIEF_PE_EXPORT_ENTRY_H
#define LIEF_PE_EXPORT_ENTRY_H

#include <string>
#include <ostream>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/Abstract/Symbol.hpp"

namespace LIEF {
namespace PE {

class Builder;
class Parser;

/// Class which represents a PE Export entry (cf. PE::Export)
class LIEF_API ExportEntry : public LIEF::Symbol {

  friend class Builder;
  friend class Parser;

  public:
  struct LIEF_API forward_information_t {
    std::string library;
    std::string function;

    operator bool() const {
      return !library.empty() || !function.empty();
    }

    std::string key() const {
      return library + '.' + function;
    }

    LIEF_API friend std::ostream& operator<<(std::ostream& os, const forward_information_t& info) {
      os << info.key();
      return os;
    }
  };

  public:
  ExportEntry() = default;
  ExportEntry(uint32_t address, bool is_extern,
              uint16_t ordinal, uint32_t function_rva) :
    function_rva_{function_rva},
    ordinal_{ordinal},
    address_{address},
    is_extern_{is_extern}
  {}

  ExportEntry(std::string name, uint32_t rva) :
    LIEF::Symbol(std::move(name)),
    address_(rva)
  {}

  ExportEntry(const ExportEntry&) = default;
  ExportEntry& operator=(const ExportEntry&) = default;

  ExportEntry(ExportEntry&&) = default;
  ExportEntry& operator=(ExportEntry&&) = default;

  ~ExportEntry() override = default;

  /// Demangled representation of the symbol or an empty string if it can't
  /// be demangled
  std::string demangled_name() const;

  /// Ordinal value associated with this exported entry.
  ///
  /// This value is computed as the index of this entry in the address table
  /// plus the ordinal base (Export::ordinal_base)
  uint16_t ordinal() const {
    return ordinal_;
  }

  /// Address of the current exported function in the DLL.
  ///
  /// \warning If this entry is **external** to the DLL then it returns 0
  ///          and the external address is returned by function_rva()
  uint32_t address() const {
    return address_;
  }

  bool is_extern() const {
    return is_extern_;
  }

  bool is_forwarded() const {
    return forward_info_;
  }

  forward_information_t forward_information() const {
    return is_forwarded() ? forward_info_ : forward_information_t{};
  }

  uint32_t function_rva() const {
    return function_rva_;
  }

  void ordinal(uint16_t ordinal) {
    ordinal_ = ordinal;
  }

  void address(uint32_t address) {
    address_ = address;
  }

  void is_extern(bool is_extern) {
    is_extern_ = is_extern;
  }

  uint64_t value() const override {
    return address();
  }

  void value(uint64_t value) override {
    address(static_cast<uint32_t>(value));
  }

  void set_forward_info(std::string lib, std::string function)  {
    forward_info_.library =  std::move(lib);
    forward_info_.function = std::move(function);
  }

  void accept(Visitor& visitor) const override;


  LIEF_API friend std::ostream& operator<<(std::ostream& os, const ExportEntry& exportEntry);

  private:
  uint32_t function_rva_ = 0;
  uint16_t ordinal_ = 0;
  uint32_t address_ = 0;
  bool is_extern_ = false;

  forward_information_t forward_info_;

};

}
}

#endif /* PE_EXPORTENTRY_H */
