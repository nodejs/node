/* Copyright 2022 - 2025 R. Thomas
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
#include "LIEF/PDB/DebugInfo.hpp"
#include "LIEF/PDB/CompilationUnit.hpp"
#include "LIEF/PDB/PublicSymbol.hpp"
#include "LIEF/PDB/Function.hpp"
#include "LIEF/PDB/BuildMetadata.hpp"
#include "LIEF/PDB/Type.hpp"
#include "LIEF/PDB/utils.hpp"

#include "LIEF/PDB/types/Simple.hpp"
#include "LIEF/PDB/types/Array.hpp"
#include "LIEF/PDB/types/BitField.hpp"
#include "LIEF/PDB/types/ClassLike.hpp"
#include "LIEF/PDB/types/Enum.hpp"
#include "LIEF/PDB/types/Function.hpp"
#include "LIEF/PDB/types/Modifier.hpp"
#include "LIEF/PDB/types/Pointer.hpp"
#include "LIEF/PDB/types/Union.hpp"
#include "LIEF/PDB/types/Attribute.hpp"

#include "logging.hpp"
#include "messages.hpp"
#include "internal_utils.hpp"

namespace LIEF::details {
class DebugInfo {};
}

namespace LIEF::pdb {
namespace details {
class CompilationUnit {};
class CompilationUnitIt {};

class PublicSymbol {};
class PublicSymbolIt {};

class Function {};
class FunctionIt {};

class Type {};
class TypeIt {};

class BuildMetadata {};
}

namespace types::details {
class Attribute {};
class AttributeIt {};

class Method {};
class MethodIt {};
}

// ----------------------------------------------------------------------------
// PDB/DebugInfo.hpp
// ----------------------------------------------------------------------------
DebugInfo::compilation_units_it DebugInfo::compilation_units() const {
  return make_empty_iterator<CompilationUnit>();
}

DebugInfo::public_symbols_it DebugInfo::public_symbols() const {
  return make_empty_iterator<PublicSymbol>();
}

DebugInfo::types_it DebugInfo::types() const {
  return make_empty_iterator<Type>();
}

std::unique_ptr<Type> DebugInfo::find_type(const std::string&/*name*/) const {
  return nullptr;
}

uint32_t DebugInfo::age() const {
  return 0;
}

std::string DebugInfo::guid() const {
  return "";
}

std::string DebugInfo::to_string() const {
  return "";
}

std::unique_ptr<PublicSymbol>
DebugInfo::find_public_symbol(const std::string&) const {
  return nullptr;
}

std::unique_ptr<DebugInfo> DebugInfo::from_file(const std::string&) {
  LIEF_ERR(DEBUG_FMT_NOT_SUPPORTED);
  return nullptr;
}

optional<uint64_t> DebugInfo::find_function_address(const std::string& /*name*/) const {
  return nullopt();
}

// ----------------------------------------------------------------------------
// PDB/Utils.hpp
// ----------------------------------------------------------------------------
bool is_pdb(const std::string& /*path*/) {
  LIEF_ERR(DEBUG_FMT_NOT_SUPPORTED);
  return false;
}

// ----------------------------------------------------------------------------
// PDB/CompilationUnit.hpp
// ----------------------------------------------------------------------------
CompilationUnit::CompilationUnit(std::unique_ptr<details::CompilationUnit> impl) :
  impl_(std::move(impl))
{}

CompilationUnit::~CompilationUnit() = default;


std::string CompilationUnit::module_name() const {
  return "";
}

std::string CompilationUnit::object_filename() const {
  return "";
}

CompilationUnit::sources_iterator CompilationUnit::sources() const {
  static const std::vector<std::string> empty;
  return make_range(empty.begin(), empty.end());
}

CompilationUnit::function_iterator CompilationUnit::functions() const {
  return make_empty_iterator<Function>();
}

std::unique_ptr<BuildMetadata> CompilationUnit::build_metadata() const {
  return nullptr;
}

CompilationUnit::Iterator::Iterator(std::unique_ptr<details::CompilationUnitIt>) :
  impl_(nullptr)
{}

CompilationUnit::Iterator::Iterator(const Iterator&) :
  impl_(nullptr)
{}

CompilationUnit::Iterator::Iterator(Iterator&&) :
  impl_(nullptr)
{}

CompilationUnit::Iterator::~Iterator() = default;

bool operator==(const CompilationUnit::Iterator&,
                const CompilationUnit::Iterator&)
{
  return true;
}

CompilationUnit::Iterator& CompilationUnit::Iterator::operator++() {
  return *this;
}

CompilationUnit::Iterator& CompilationUnit::Iterator::operator--() {
  return *this;
}

std::unique_ptr<CompilationUnit> CompilationUnit::Iterator::operator*() const {
  return nullptr;
}

std::string CompilationUnit::to_string() const {
  return "";
}

// ----------------------------------------------------------------------------
// PDB/PublicSymbol.hpp
// ----------------------------------------------------------------------------
PublicSymbol::PublicSymbol(std::unique_ptr<details::PublicSymbol> impl) :
  impl_(std::move(impl))
{}

PublicSymbol::~PublicSymbol() = default;


PublicSymbol::Iterator::Iterator(std::unique_ptr<details::PublicSymbolIt>) :
  impl_(nullptr)
{}

PublicSymbol::Iterator::Iterator(const Iterator&) :
  impl_(nullptr)
{}

PublicSymbol::Iterator::Iterator(Iterator&&) :
  impl_(nullptr)
{}

PublicSymbol::Iterator::~Iterator() = default;

bool operator==(const PublicSymbol::Iterator&,
                const PublicSymbol::Iterator&)
{
  return true;
}

PublicSymbol::Iterator& PublicSymbol::Iterator::operator++() {
  return *this;
}

std::unique_ptr<PublicSymbol> PublicSymbol::Iterator::operator*() const {
  return nullptr;
}

std::string PublicSymbol::name() const {
  return "";
}

std::string PublicSymbol::section_name() const {
  return "";
}

uint32_t PublicSymbol::RVA() const {
  return 0;
}

std::string PublicSymbol::demangled_name() const {
  return "";
}

std::string PublicSymbol::to_string() const {
  return "";
}

// ----------------------------------------------------------------------------
// PDB/Function.hpp
// ----------------------------------------------------------------------------
Function::Function(std::unique_ptr<details::Function> impl) :
  impl_(std::move(impl))
{}

Function::~Function() = default;


Function::Iterator::Iterator(std::unique_ptr<details::FunctionIt>) :
  impl_(nullptr)
{}

Function::Iterator::Iterator(const Iterator&) :
  impl_(nullptr)
{}

Function::Iterator::Iterator(Iterator&&) :
  impl_(nullptr)
{}

Function::Iterator::~Iterator() = default;

bool operator==(const Function::Iterator&,
                const Function::Iterator&)
{
  return true;
}

Function::Iterator& Function::Iterator::operator++() {
  return *this;
}

std::unique_ptr<Function> Function::Iterator::operator*() const {
  return nullptr;
}

std::string Function::name() const {
  return "";
}

uint32_t Function::RVA() const {
  return 0;
}

uint32_t Function::code_size() const {
  return 0;
}

std::string Function::section_name() const {
  return "";
}

debug_location_t Function::debug_location() const {
  return {};
}

std::string Function::to_string() const {
  return "";
}

// ----------------------------------------------------------------------------
// PDB/Type.hpp
// ----------------------------------------------------------------------------
std::unique_ptr<Type> Type::create(std::unique_ptr<details::Type>/* impl*/) {
  return nullptr;
}

Type::KIND Type::kind() const {
  return Type::KIND::UNKNOWN;
}

Type::~Type() = default;


Type::Type(std::unique_ptr<details::Type> impl) :
  impl_(std::move(impl))
{}

Type::Iterator::Iterator(std::unique_ptr<details::TypeIt>) :
  impl_(nullptr)
{}

Type::Iterator::Iterator(const Iterator&) :
  impl_(nullptr)
{}

Type::Iterator::Iterator(Iterator&&) noexcept :
  impl_(nullptr)
{}

Type::Iterator::~Iterator() = default;

bool operator==(const Type::Iterator&,
                const Type::Iterator&)
{
  return true;
}

Type::Iterator& Type::Iterator::operator++() {
  return *this;
}

std::unique_ptr<Type> Type::Iterator::operator*() const {
  return nullptr;
}

namespace types {

// ----------------------------------------------------------------------------
// PDB/types/Simple.hpp
// ----------------------------------------------------------------------------
Simple::~Simple() = default;

// ----------------------------------------------------------------------------
// PDB/types/Array.hpp
// ----------------------------------------------------------------------------
Array::~Array() = default;

// ----------------------------------------------------------------------------
// PDB/types/BitField.hpp
// ----------------------------------------------------------------------------
BitField::~BitField() = default;

// ----------------------------------------------------------------------------
// PDB/types/ClassLike.hpp
// ----------------------------------------------------------------------------
ClassLike::attributes_iterator ClassLike::attributes() const {
  return make_empty_iterator<Attribute>();
}

ClassLike::methods_iterator ClassLike::methods() const {
  return make_empty_iterator<Method>();
}

std::string ClassLike::name() const {
  return "";
}

std::string ClassLike::unique_name() const {
  return "";
}

uint64_t ClassLike::size() const {
  return 0;
}

ClassLike::~ClassLike() = default;

Class::~Class() = default;
Structure::~Structure() = default;
Interface::~Interface() = default;

// ----------------------------------------------------------------------------
// PDB/types/Enum.hpp
// ----------------------------------------------------------------------------
Enum::~Enum() = default;

// ----------------------------------------------------------------------------
// PDB/types/Function.hpp
// ----------------------------------------------------------------------------
Function::~Function() = default;

// ----------------------------------------------------------------------------
// PDB/types/Modifier.hpp
// ----------------------------------------------------------------------------
std::unique_ptr<Type> Modifier::underlying_type() const {
  return nullptr;
}

Modifier::~Modifier() = default;

// ----------------------------------------------------------------------------
// PDB/types/Pointer.hpp
// ----------------------------------------------------------------------------
std::unique_ptr<Type> Pointer::underlying_type() const {
  return nullptr;
}

Pointer::~Pointer() = default;

// ----------------------------------------------------------------------------
// PDB/types/Union.hpp
// ----------------------------------------------------------------------------
Union::~Union() = default;

// ----------------------------------------------------------------------------
// PDB/types/Attribute.hpp
// ----------------------------------------------------------------------------
Attribute::Attribute(std::unique_ptr<details::Attribute> impl) :
  impl_(std::move(impl))
{}

Attribute::Iterator::Iterator(std::unique_ptr<details::AttributeIt>) :
  impl_(nullptr)
{}

Attribute::Iterator::Iterator(const Iterator&) :
  impl_(nullptr)
{}

Attribute::Iterator::Iterator(Iterator&&) noexcept :
  impl_(nullptr)
{}

Attribute::Iterator::~Iterator() = default;

bool operator==(const Attribute::Iterator&,
                const Attribute::Iterator&)
{
  return true;
}

Attribute::Iterator& Attribute::Iterator::operator++() {
  return *this;
}

std::unique_ptr<Attribute> Attribute::Iterator::operator*() const {
  return nullptr;
}

std::string Attribute::name() const {
  return "";
}

std::unique_ptr<Type> Attribute::type() const {
  return nullptr;
}

uint64_t Attribute::field_offset() const {
  return 0;
}

Attribute::~Attribute() = default;

// ----------------------------------------------------------------------------
// PDB/types/Method.hpp
// ----------------------------------------------------------------------------
Method::Method(std::unique_ptr<details::Method> impl) :
  impl_(std::move(impl))
{}

Method::Iterator::Iterator(std::unique_ptr<details::MethodIt>) :
  impl_(nullptr)
{}

Method::Iterator::Iterator(const Iterator&) :
  impl_(nullptr)
{}

Method::Iterator::Iterator(Iterator&&) noexcept :
  impl_(nullptr)
{}

Method::Iterator::~Iterator() = default;

bool operator==(const Method::Iterator&,
                const Method::Iterator&)
{
  return true;
}

Method::Iterator& Method::Iterator::operator++() {
  return *this;
}

std::unique_ptr<Method> Method::Iterator::operator*() const {
  return nullptr;
}

std::string Method::name() const {
  return "";
}

Method::~Method() = default;

}

// ----------------------------------------------------------------------------
// PDB/BuildMetadata.hpp
// ----------------------------------------------------------------------------
BuildMetadata::BuildMetadata(std::unique_ptr<details::BuildMetadata> /*impl*/) :
  impl_(nullptr)
{}

BuildMetadata::~BuildMetadata() = default;

BuildMetadata::version_t BuildMetadata::frontend_version() const {
  return {};
}

BuildMetadata::version_t BuildMetadata::backend_version() const {
  return {};
}

std::string BuildMetadata::version() const {
  return "";
}

std::string BuildMetadata::to_string() const {
  return "";
}

BuildMetadata::LANG BuildMetadata::language() const {
  return LANG::UNKNOWN;
}

BuildMetadata::CPU BuildMetadata::target_cpu() const {
  return CPU::UNKNOWN;
}

optional<BuildMetadata::build_info_t> BuildMetadata::build_info() const {
  return nullopt();
}

std::vector<std::string> BuildMetadata::env() const {
  return {};
}

const char* to_string(BuildMetadata::CPU) {
  return "";
}
const char* to_string(BuildMetadata::LANG) {
  return "";
}

}
