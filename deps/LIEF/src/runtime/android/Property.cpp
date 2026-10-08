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
#include "LIEF/runtime/android/Property.hpp"
#include <sys/system_properties.h>
#include <format>

namespace LIEF::runtime::android {

Property Property::create_from(const prop_info& pi) {
  Property result;
  __system_property_read_callback(
      &pi,
      [](void* data, const char* name, const char* value, uint32_t serial) {
        auto& prop = *reinterpret_cast<Property*>(data);
        prop.name_ = name;
        prop.value_ = value;
        prop.serial_ = serial;
      },
      &result
  );
  return result;
}

std::string Property::to_string() const {
  return std::format("{}: {} ({})", name(), value(), serial());
}

}
