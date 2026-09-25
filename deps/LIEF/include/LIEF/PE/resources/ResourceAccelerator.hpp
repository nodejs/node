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
#ifndef LIEF_PE_RESOURCE_ACCELERATOR_H
#define LIEF_PE_RESOURCE_ACCELERATOR_H

#include <string>
#include <ostream>
#include <vector>

#include "LIEF/visibility.h"

#include "LIEF/Object.hpp"

#include "LIEF/PE/resources/AcceleratorCodes.hpp"
#include "LIEF/enums.hpp"

namespace LIEF {
namespace PE {
class ResourcesManager;

namespace details {
struct pe_resource_acceltableentry;
}

class LIEF_API ResourceAccelerator : public Object {
  friend class ResourcesManager;
  public:

  /// From: https://docs.microsoft.com/en-us/windows/win32/menurc/acceltableentry
  enum class FLAGS : uint32_t {
    /// The accelerator key is a virtual-key code. If this flag is not specified,
    /// the accelerator key is assumed to specify an ASCII character code.
    VIRTKEY = 0x01,

    /// A menu item on the menu bar is not highlighted when an accelerator is
    /// used. This attribute is obsolete and retained only for backward
    /// compatibility with resource files designed for 16-bit Windows.
    NOINVERT = 0x02,

    /// The accelerator is activated only if the user presses the SHIFT key.
    /// This flag applies only to virtual keys.
    SHIFT = 0x04,

    /// The accelerator is activated only if the user presses the CTRL key.
    /// This flag applies only to virtual keys.
    CONTROL = 0x08,

    /// The accelerator is activated only if the user presses the ALT key.
    /// This flag applies only to virtual keys.
    ALT = 0x10,

    /// The entry is last in an accelerator table.
    END = 0x80,
  };

  ResourceAccelerator() = default;
  explicit ResourceAccelerator(const details::pe_resource_acceltableentry&);

  ResourceAccelerator(const ResourceAccelerator&) = default;
  ResourceAccelerator& operator=(const ResourceAccelerator&) = default;

  ResourceAccelerator(ResourceAccelerator&&) = default;
  ResourceAccelerator& operator=(ResourceAccelerator&&) = default;

  ~ResourceAccelerator() override = default;

  std::vector<FLAGS> flags_list() const;

  const char* ansi_str() const {
    return to_string((ACCELERATOR_CODES)ansi());
  }

  /// Describe the keyboard accelerator characteristics
  int16_t flags() const {
    return flags_;
  }

  /// An ANSI character value or a virtual-key code that identifies the
  /// accelerator key
  int16_t ansi() const {
    return ansi_;
  }

  /// An identifier for the keyboard accelerator
  uint16_t id() const {
    return id_;
  }

  /// The number of bytes inserted to ensure that the structure is aligned on a
  /// DWORD boundary.
  int16_t padding() const {
    return padding_;
  }

  /// Whether the entry has the given flag
  bool has(FLAGS flag) const {
    return (flags_ & (int16_t)flag) != 0;
  }

  ResourceAccelerator& add(FLAGS flag) {
    flags_ |= (int16_t)flag;
    return *this;
  }

  ResourceAccelerator& remove(FLAGS flag) {
    flags_ &= ~(int16_t)flag;
    return *this;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const ResourceAccelerator& acc);

  private:
  int16_t flags_ = 0;
  int16_t ansi_ = 0;
  uint16_t id_ = 0;
  int16_t padding_ = 0;
};

LIEF_API const char* to_string(ResourceAccelerator::FLAGS e);

}
}

ENABLE_BITMASK_OPERATORS(LIEF::PE::ResourceAccelerator::FLAGS);
#endif
