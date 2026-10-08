/* Copyright 2022 - 2026 R. Thomas
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
#include "LIEF/DebugDeclOpt.hpp"

namespace LIEF {
namespace details {
class DeclOpt {};
}

DeclOpt::DeclOpt() = default;
DeclOpt::DeclOpt(const DeclOpt&) {}

// NOLINTNEXTLINE
DeclOpt& DeclOpt::operator=(const DeclOpt&) {
  return *this;
}
DeclOpt::DeclOpt(DeclOpt&&) noexcept = default;
DeclOpt& DeclOpt::operator=(DeclOpt&&) noexcept = default;
DeclOpt::~DeclOpt() = default;

uint32_t DeclOpt::indentation() const {
  return 0;
}
DeclOpt& DeclOpt::indentation(uint32_t) {
  return *this;
}

bool DeclOpt::is_cpp() const {
  return false;
}
DeclOpt& DeclOpt::is_cpp(bool) {
  return *this;
}

bool DeclOpt::show_extended_annotations() const {
  return false;
}
DeclOpt& DeclOpt::show_extended_annotations(bool) {
  return *this;
}

bool DeclOpt::include_types() const {
  return false;
}
DeclOpt& DeclOpt::include_types(bool) {
  return *this;
}

bool DeclOpt::include_locals() const {
  return false;
}
DeclOpt& DeclOpt::include_locals(bool) {
  return *this;
}

bool DeclOpt::desugar() const {
  return false;
}
DeclOpt& DeclOpt::desugar(bool) {
  return *this;
}

bool DeclOpt::show_field_offsets() const {
  return false;
}
DeclOpt& DeclOpt::show_field_offsets(bool) {
  return *this;
}

const DeclOpt::type_aliases_t& DeclOpt::type_aliases() const {
  static const type_aliases_t empty;
  return empty;
}

// NOLINTNEXTLINE
DeclOpt& DeclOpt::type_aliases(type_aliases_t) {
  return *this;
}

// NOLINTNEXTLINE
DeclOpt& DeclOpt::add_type_alias(std::string, std::string) {
  return *this;
}

}
