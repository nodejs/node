/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
#ifndef LIEF_RUNTIME_ANDROID_PROPERTY_H
#define LIEF_RUNTIME_ANDROID_PROPERTY_H
#include <ostream>
#include <cstdint>
#include <string>

#include <LIEF/compiler_attributes.hpp>
#include <LIEF/visibility.h>

// Forward definition from include/sys/system_properties.h
typedef struct prop_info prop_info; // NOLINT

namespace LIEF {
namespace runtime {
namespace android {

/// This class represents an Android property such as `ro.boot.hardware`
class LIEF_API Property {
  public:
  Property() = default;
  Property(std::string name, std::string value, uint32_t serial) :
    name_(std::move(name)),
    value_(std::move(value)),
    serial_(serial) {}

  static Property create_from(const prop_info& pi);

  Property(const Property&) = default;
  Property& operator=(const Property&) = default;

  Property(Property&&) noexcept = default;
  Property& operator=(Property&&) noexcept = default;

  ~Property() = default;

  /// Name of the property (e.g. `ro.boot.hardware`).
  const std::string& name() const LIEF_LIFETIMEBOUND {
    return name_;
  }

  /// Value associated with the property.
  const std::string& value() const LIEF_LIFETIMEBOUND {
    return value_;
  }

  /// Serial number of the property.
  ///
  /// It is incremented each time the property is updated and can therefore be
  /// used to detect changes.
  uint32_t serial() const {
    return serial_;
  }

  /// Pretty representation of the property
  std::string to_string() const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os,
                                           const Property& prop) {
    os << prop.to_string();
    return os;
  }

  private:
  std::string name_;
  std::string value_;
  uint32_t serial_ = 0;
};
}
}
}
#endif
