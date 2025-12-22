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
#ifndef LIEF_MACHO_BINDING_INFO_H
#define LIEF_MACHO_BINDING_INFO_H
#include <ostream>
#include <cstdint>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"

namespace LIEF {
namespace MachO {
class DylibCommand;
class SegmentCommand;
class Symbol;
class BinaryParser;
class DyldChainedFixupsCreator;

/// Class that provides an interface over a *binding* operation.
///
/// This class does not represent a structure that exists in the Mach-O format
/// specifications but it provides a *view* of a binding operation that is performed
/// by the Dyld binding bytecode (`LC_DYLD_INFO`) or the Dyld chained fixups (`DYLD_CHAINED_FIXUPS`)
///
/// See: LIEF::MachO::ChainedBindingInfo, LIEF::MachO::DyldBindingInfo
class LIEF_API BindingInfo : public Object {

  friend class BinaryParser;
  friend class DyldChainedFixupsCreator;

  public:
  enum class TYPES {
    UNKNOWN = 0,
    DYLD_INFO,       /// Binding associated with the Dyld info opcodes
    CHAINED,         /// Binding associated with the chained fixups
    CHAINED_LIST,    /// Internal use
    INDIRECT_SYMBOL, /// Infered from the indirect symbols table
  };

  BindingInfo() = default;
  BindingInfo(const BindingInfo& other);
  BindingInfo& operator=(const BindingInfo& other);

  BindingInfo(BindingInfo&&) noexcept = default;
  BindingInfo& operator=(BindingInfo&&) noexcept = default;

  void swap(BindingInfo& other) noexcept;

  /// Check if a MachO::SegmentCommand is associated with this binding
  bool has_segment() const {
    return segment_ != nullptr;
  }

  /// The MachO::SegmentCommand associated with the BindingInfo or
  /// a nullptr of it is not bind to a SegmentCommand
  const SegmentCommand* segment() const {
    return segment_;
  }
  SegmentCommand* segment() {
    return segment_;
  }

  /// Check if a MachO::DylibCommand is tied with the BindingInfo
  bool has_library() const {
    return library_ != nullptr;
  }

  /// MachO::DylibCommand associated with the BindingInfo or a nullptr
  /// if not present
  const DylibCommand* library() const {
    return library_;
  }
  DylibCommand* library() {
    return library_;
  }

  /// Check if a MachO::Symbol is associated with the BindingInfo
  bool has_symbol() const {
    return symbol_ != nullptr;
  }

  /// MachO::Symbol associated with the BindingInfo or
  /// a nullptr if not present
  const Symbol* symbol() const {
    return symbol_;
  }
  Symbol* symbol() {
    return symbol_;
  }

  /// Address of the binding
  virtual uint64_t address() const {
    return address_;
  }

  virtual void address(uint64_t addr) {
    address_ = addr;
  }

  int32_t library_ordinal() const {
    return library_ordinal_;
  }

  void library_ordinal(int32_t ordinal) {
    library_ordinal_ = ordinal;
  }

  /// Value added to the segment's virtual address when bound
  int64_t addend() const {
    return addend_;
  }

  void addend(int64_t addend) {
    addend_ = addend;
  }

  bool is_weak_import() const {
    return is_weak_import_;
  }

  void set_weak_import(bool val = true) {
    is_weak_import_ = val;
  }

  /// The type of the binding. This type provides the origin
  /// of the binding (LC_DYLD_INFO or LC_DYLD_CHAINED_FIXUPS)
  virtual TYPES type() const = 0;

  ~BindingInfo() override = default;

  void accept(Visitor& visitor) const override;

  template<class T>
  const T* cast() const {
    static_assert(std::is_base_of<BindingInfo, T>::value, "Require BindingInfo inheritance");
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  template<class T>
  T* cast() {
    return const_cast<T*>(static_cast<const BindingInfo*>(this)->cast<T>());
  }

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const BindingInfo& binding_info);

  protected:
  SegmentCommand* segment_ = nullptr;
  Symbol*         symbol_ = nullptr;
  int32_t         library_ordinal_ = 0;
  int64_t         addend_ = 0;
  bool            is_weak_import_ = false;
  DylibCommand*   library_ = nullptr;
  uint64_t        address_ = 0;
};

}
}
#endif
