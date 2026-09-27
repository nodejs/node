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

// NOLINTBEGIN
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

class EnumEntry {};
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

std::unique_ptr<Type> DebugInfo::find_type(const std::string& /*name*/) const {
  return nullptr;
}

std::unique_ptr<Type> DebugInfo::find_type(uint32_t /*index*/) const {
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

optional<uint64_t>
    DebugInfo::find_function_address(const std::string& /*name*/) const {
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
  impl_(std::move(impl)) {}

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

CompilationUnit::Iterator::Iterator() :
  impl_(nullptr) {}

CompilationUnit::Iterator::Iterator(std::unique_ptr<details::CompilationUnitIt>) :
  impl_(nullptr) {}

CompilationUnit::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

CompilationUnit::Iterator& CompilationUnit::Iterator::operator=(const Iterator&) {
  return *this;
}

CompilationUnit::Iterator::Iterator(Iterator&&) noexcept = default;
CompilationUnit::Iterator&
    CompilationUnit::Iterator::operator=(Iterator&&) noexcept = default;

CompilationUnit::Iterator::~Iterator() = default;

bool operator==(const CompilationUnit::Iterator&,
                const CompilationUnit::Iterator&) {
  return true;
}

CompilationUnit::Iterator& CompilationUnit::Iterator::operator++() {
  return *this;
}

CompilationUnit::Iterator& CompilationUnit::Iterator::operator--() {
  return *this;
}

void CompilationUnit::Iterator::load() const {}

const CompilationUnit& CompilationUnit::Iterator::operator*() const {
  return *cached_;
}

const CompilationUnit* CompilationUnit::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<CompilationUnit> CompilationUnit::Iterator::yield() {
  return nullptr;
}

std::string CompilationUnit::to_string() const {
  return "";
}

std::string CompilationUnit::to_decl(const DeclOpt& /*opt*/) const {
  return "";
}

// ----------------------------------------------------------------------------
// PDB/PublicSymbol.hpp
// ----------------------------------------------------------------------------
PublicSymbol::PublicSymbol(std::unique_ptr<details::PublicSymbol> impl) :
  impl_(std::move(impl)) {}

PublicSymbol::~PublicSymbol() = default;


PublicSymbol::Iterator::Iterator() :
  impl_(nullptr) {}

PublicSymbol::Iterator::Iterator(std::unique_ptr<details::PublicSymbolIt>) :
  impl_(nullptr) {}

PublicSymbol::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

PublicSymbol::Iterator& PublicSymbol::Iterator::operator=(const Iterator&) {
  return *this;
}

PublicSymbol::Iterator::Iterator(Iterator&&) noexcept = default;
PublicSymbol::Iterator&
    PublicSymbol::Iterator::operator=(Iterator&&) noexcept = default;

PublicSymbol::Iterator::~Iterator() = default;

bool operator==(const PublicSymbol::Iterator&, const PublicSymbol::Iterator&) {
  return true;
}

PublicSymbol::Iterator& PublicSymbol::Iterator::operator++() {
  return *this;
}

void PublicSymbol::Iterator::load() const {}

const PublicSymbol& PublicSymbol::Iterator::operator*() const {
  return *cached_;
}

const PublicSymbol* PublicSymbol::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<PublicSymbol> PublicSymbol::Iterator::yield() {
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
  impl_(std::move(impl)) {}

Function::~Function() = default;


Function::Iterator::Iterator() :
  impl_(nullptr) {}

Function::Iterator::Iterator(std::unique_ptr<details::FunctionIt>) :
  impl_(nullptr) {}

Function::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

Function::Iterator& Function::Iterator::operator=(const Iterator&) {
  return *this;
}

Function::Iterator::Iterator(Iterator&&) noexcept = default;
Function::Iterator& Function::Iterator::operator=(Iterator&&) noexcept = default;

Function::Iterator::~Iterator() = default;

bool operator==(const Function::Iterator&, const Function::Iterator&) {
  return true;
}

Function::Iterator& Function::Iterator::operator++() {
  return *this;
}

void Function::Iterator::load() const {}

const Function& Function::Iterator::operator*() const {
  return *cached_;
}

const Function* Function::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<Function> Function::Iterator::yield() {
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

std::string Function::to_decl(const DeclOpt& /*opt*/) const {
  return "";
}

// ----------------------------------------------------------------------------
// PDB/Type.hpp
// ----------------------------------------------------------------------------
std::unique_ptr<Type> Type::create(std::unique_ptr<details::Type> /* impl*/) {
  return nullptr;
}

Type::KIND Type::kind() const {
  return Type::KIND::UNKNOWN;
}

optional<std::string> Type::name() const {
  return nullopt();
}

optional<uint64_t> Type::size() const {
  return nullopt();
}

std::string Type::to_decl(const DeclOpt& /*opt*/) const {
  return "";
}

Type::~Type() = default;


Type::Type(std::unique_ptr<details::Type> impl) :
  impl_(std::move(impl)) {}

Type::Iterator::Iterator() :
  impl_(nullptr) {}

Type::Iterator::Iterator(std::unique_ptr<details::TypeIt>) :
  impl_(nullptr) {}

Type::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

Type::Iterator& Type::Iterator::operator=(const Iterator&) {
  return *this;
}

Type::Iterator::Iterator(Iterator&&) noexcept = default;
Type::Iterator& Type::Iterator::operator=(Iterator&&) noexcept = default;

Type::Iterator::~Iterator() = default;

bool operator==(const Type::Iterator&, const Type::Iterator&) {
  return true;
}

Type::Iterator& Type::Iterator::operator++() {
  return *this;
}

void Type::Iterator::load() const {}

const Type& Type::Iterator::operator*() const {
  return *cached_;
}

const Type* Type::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<Type> Type::Iterator::yield() {
  return nullptr;
}

namespace types {

// ----------------------------------------------------------------------------
// PDB/types/Simple.hpp
// ----------------------------------------------------------------------------
Simple::TYPES Simple::type() const {
  return TYPES::UNKNOWN;
}

Simple::MODES Simple::modes() const {
  return MODES::DIRECT;
}

Simple::~Simple() = default;

// ----------------------------------------------------------------------------
// PDB/types/Array.hpp
// ----------------------------------------------------------------------------
size_t Array::numberof_elements() const {
  return 0;
}

std::unique_ptr<Type> Array::element_type() const {
  return nullptr;
}

std::unique_ptr<Type> Array::index_type() const {
  return nullptr;
}

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

std::string ClassLike::unique_name() const {
  return "";
}

ClassLike::~ClassLike() = default;

Class::~Class() = default;
Structure::~Structure() = default;
Interface::~Interface() = default;

// ----------------------------------------------------------------------------
// PDB/types/Enum.hpp
// ----------------------------------------------------------------------------
Enum::Entry::Entry(Entry&& other) noexcept = default;
Enum::Entry& Enum::Entry::operator=(Entry&& other) noexcept = default;

Enum::Entry::~Entry() = default;

Enum::Entry::Entry(std::unique_ptr<details::EnumEntry> impl) :
  impl_(std::move(impl)) {}

std::string Enum::Entry::name() const {
  return "";
}

int64_t Enum::Entry::value() const {
  return 0;
}

std::string Enum::unique_name() const {
  return "";
}

std::vector<Enum::Entry> Enum::entries() const {
  return {};
}

const Type* Enum::underlying_type() const {
  return nullptr;
}

optional<Enum::Entry> Enum::find_entry(int64_t /*value*/) const {
  return nullopt();
}

Enum::~Enum() = default;

// ----------------------------------------------------------------------------
// PDB/types/Function.hpp
// ----------------------------------------------------------------------------
std::unique_ptr<Type> Function::return_type() const {
  return nullptr;
}

Function::parameters_t Function::parameters() const {
  return {};
}

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
  impl_(std::move(impl)) {}

Attribute::Iterator::Iterator() :
  impl_(nullptr) {}

Attribute::Iterator::Iterator(std::unique_ptr<details::AttributeIt>) :
  impl_(nullptr) {}

Attribute::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

Attribute::Iterator& Attribute::Iterator::operator=(const Iterator&) {
  return *this;
}

Attribute::Iterator::Iterator(Iterator&&) noexcept = default;
Attribute::Iterator& Attribute::Iterator::operator=(Iterator&&) noexcept = default;

Attribute::Iterator::~Iterator() = default;

bool operator==(const Attribute::Iterator&, const Attribute::Iterator&) {
  return true;
}

Attribute::Iterator& Attribute::Iterator::operator++() {
  return *this;
}

void Attribute::Iterator::load() const {}

const Attribute& Attribute::Iterator::operator*() const {
  return *cached_;
}

const Attribute* Attribute::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<Attribute> Attribute::Iterator::yield() {
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
  impl_(std::move(impl)) {}

Method::Iterator::Iterator() :
  impl_(nullptr) {}

Method::Iterator::Iterator(std::unique_ptr<details::MethodIt>) :
  impl_(nullptr) {}

Method::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

Method::Iterator& Method::Iterator::operator=(const Iterator&) {
  return *this;
}

Method::Iterator::Iterator(Iterator&&) noexcept = default;
Method::Iterator& Method::Iterator::operator=(Iterator&&) noexcept = default;

Method::Iterator::~Iterator() = default;

bool operator==(const Method::Iterator&, const Method::Iterator&) {
  return true;
}

Method::Iterator& Method::Iterator::operator++() {
  return *this;
}

void Method::Iterator::load() const {}

const Method& Method::Iterator::operator*() const {
  return *cached_;
}

const Method* Method::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<Method> Method::Iterator::yield() {
  return nullptr;
}

std::string Method::name() const {
  return "";
}

Method::TYPE Method::type() const {
  return TYPE::VANILLA;
}

Method::ACCESS Method::access() const {
  return ACCESS::NONE;
}

Method::~Method() = default;

}

// ----------------------------------------------------------------------------
// PDB/BuildMetadata.hpp
// ----------------------------------------------------------------------------
BuildMetadata::BuildMetadata(std::unique_ptr<details::BuildMetadata> /*impl*/) :
  impl_(nullptr) {}

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

// NOLINTEND

}
