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
#ifndef LIEF_MACHO_DYLD_INFO_BINDING_INFO_H
#define LIEF_MACHO_DYLD_INFO_BINDING_INFO_H
#include <ostream>
#include <cstdint>

#include "LIEF/visibility.h"
#include "LIEF/MachO/BindingInfo.hpp"

namespace LIEF {
namespace MachO {

/// This class represents a symbol binding operation associated with
/// the LC_DYLD_INFO bytecode.
///
/// It does not represent a structure that exists in the Mach-O format
/// specifications but it provides a *view* on an entry of the Dyld binding opcodes.
///
/// @see: BindingInfo
class LIEF_API DyldBindingInfo : public BindingInfo {
  friend class BinaryParser;

  public:
  enum class CLASS: uint64_t  {
    WEAK     = 1u,
    LAZY     = 2u,
    STANDARD = 3u,
    THREADED = 100u
  };

  enum class TYPE: uint64_t  {
    POINTER         = 1u,
    TEXT_ABSOLUTE32 = 2u,
    TEXT_PCREL32    = 3u
  };

  public:
  DyldBindingInfo() = default;
  DyldBindingInfo(CLASS cls, TYPE type,
                  uint64_t address, int64_t addend = 0,
                  int32_t oridnal = 0, bool is_weak = false,
                  bool is_non_weak_definition = false, uint64_t offset = 0);

  DyldBindingInfo& operator=(const DyldBindingInfo& other) = default;
  DyldBindingInfo(const DyldBindingInfo& other) = default;

  DyldBindingInfo& operator=(DyldBindingInfo&&) noexcept = default;
  DyldBindingInfo(DyldBindingInfo&&) noexcept = default;

  void swap(DyldBindingInfo& other) noexcept;

  /// Class of the binding (weak, lazy, ...)
  CLASS binding_class() const {
    return class_;
  }
  void binding_class(CLASS bind_class) {
    class_ = bind_class;
  }

  /// Type of the binding. Most of the times it's TYPE::POINTER
  TYPE binding_type() const {
    return binding_type_;
  }

  void binding_type(TYPE type) {
    binding_type_ = type;
  }

  bool is_non_weak_definition() const {
    return this->is_non_weak_definition_;
  }

  void set_non_weak_definition(bool val) {
    this->is_non_weak_definition_ = val;
  }

  /// Original relative offset of the binding opcodes
  uint64_t original_offset() const {
    return offset_;
  }

  BindingInfo::TYPES type() const override {
    return BindingInfo::TYPES::DYLD_INFO;
  }

  static bool classof(const BindingInfo* info) {
    return info->type() == BindingInfo::TYPES::DYLD_INFO;
  }

  ~DyldBindingInfo() override = default;

  void accept(Visitor& visitor) const override;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const DyldBindingInfo& info) {
    os << static_cast<const BindingInfo&>(info);
    return os;
  }

  private:
  CLASS class_ = CLASS::STANDARD;
  TYPE binding_type_ = TYPE::POINTER;
  bool is_non_weak_definition_ = false;
  uint64_t offset_ = 0;
};

LIEF_API const char* to_string(DyldBindingInfo::CLASS e);
LIEF_API const char* to_string(DyldBindingInfo::TYPE e);

}
}
#endif
