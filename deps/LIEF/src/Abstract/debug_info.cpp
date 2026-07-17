/* Copyright 2021 - 2025 R. Thomas
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
#include "LIEF/Abstract/Binary.hpp"
#include "LIEF/Abstract/DebugInfo.hpp"

#include "logging.hpp"
#include "messages.hpp"

namespace LIEF {
namespace details {
class DebugInfo {};
}

// ----------------------------------------------------------------------------
// Abstract/Binary.hpp
// ----------------------------------------------------------------------------
DebugInfo* Binary::debug_info() const {
  LIEF_ERR(DEBUG_FMT_NOT_SUPPORTED);
  return nullptr;
}

DebugInfo* Binary::load_debug_info(const std::string& /*path*/) {
  LIEF_ERR(DEBUG_FMT_NOT_SUPPORTED);
  return nullptr;
}


// ----------------------------------------------------------------------------
// DebugInfo/DebugInfo.hpp
// ----------------------------------------------------------------------------
DebugInfo::DebugInfo(std::unique_ptr<details::DebugInfo>) :
    impl_(nullptr)
{}

DebugInfo::~DebugInfo() = default;

}
