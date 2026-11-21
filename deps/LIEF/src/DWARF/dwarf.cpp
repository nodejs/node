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
#include "LIEF/DWARF/DebugInfo.hpp"
#include "LIEF/DWARF/CompilationUnit.hpp"
#include "LIEF/DWARF/Function.hpp"
#include "LIEF/DWARF/LexicalBlock.hpp"
#include "LIEF/DWARF/Variable.hpp"
#include "LIEF/DWARF/Type.hpp"
#include "LIEF/DWARF/Scope.hpp"
#include "LIEF/DWARF/Parameter.hpp"
#include "LIEF/DWARF/Editor.hpp"
#include "LIEF/DWARF/editor/CompilationUnit.hpp"
#include "LIEF/DWARF/editor/Variable.hpp"
#include "LIEF/DWARF/editor/Function.hpp"
#include "LIEF/DWARF/editor/Type.hpp"
#include "LIEF/DWARF/editor/PointerType.hpp"
#include "LIEF/DWARF/editor/EnumType.hpp"
#include "LIEF/DWARF/editor/BaseType.hpp"
#include "LIEF/DWARF/editor/ArrayType.hpp"
#include "LIEF/DWARF/editor/FunctionType.hpp"
#include "LIEF/DWARF/editor/TypeDef.hpp"
#include "LIEF/DWARF/editor/StructType.hpp"

#include "LIEF/DWARF/types/ClassLike.hpp"
#include "LIEF/DWARF/types/Pointer.hpp"
#include "LIEF/DWARF/types/Const.hpp"
#include "LIEF/DWARF/types/Base.hpp"
#include "LIEF/DWARF/types/Array.hpp"
#include "LIEF/DWARF/types/Typedef.hpp"
#include "LIEF/DWARF/types/Atomic.hpp"
#include "LIEF/DWARF/types/Coarray.hpp"
#include "LIEF/DWARF/types/Dynamic.hpp"
#include "LIEF/DWARF/types/Enum.hpp"
#include "LIEF/DWARF/types/File.hpp"
#include "LIEF/DWARF/types/Immutable.hpp"
#include "LIEF/DWARF/types/Interface.hpp"
#include "LIEF/DWARF/types/PointerToMember.hpp"
#include "LIEF/DWARF/types/RValueRef.hpp"
#include "LIEF/DWARF/types/Reference.hpp"
#include "LIEF/DWARF/types/Restrict.hpp"
#include "LIEF/DWARF/types/SetTy.hpp"
#include "LIEF/DWARF/types/Shared.hpp"
#include "LIEF/DWARF/types/StringTy.hpp"
#include "LIEF/DWARF/types/Subroutine.hpp"
#include "LIEF/DWARF/types/TemplateAlias.hpp"
#include "LIEF/DWARF/types/Thrown.hpp"
#include "LIEF/DWARF/types/Volatile.hpp"

#include "logging.hpp"
#include "messages.hpp"
#include "internal_utils.hpp"

namespace LIEF::details {
class DebugInfo {};
}

namespace LIEF::dwarf {

namespace details {
class CompilationUnit {};
class CompilationUnitIt {};

class Function {};
class Parameter {};
class FunctionIt {};

class Variable {};
class VariableIt {};

class Type {};
class TypeIt {};

class Scope {};
class Editor {};

class LexicalBlock {};
class LexicalBlockIt {};
}

namespace types::details {
class Member {};
class EnumEntry {};
}

namespace editor::details {
class CompilationUnit {};
class Function {};
class Variable {};
class Type {};
class EnumValue {};
class FunctionTyParameter {};
class FunctionParameter {};
class FunctionLexicalBlock {};
class FunctionLabel {};
class StructMember {};
}

// ----------------------------------------------------------------------------
// DWARF/Variable.hpp
// ----------------------------------------------------------------------------
Variable::Variable(std::unique_ptr<details::Variable>) :
  impl_(nullptr) {}

std::string Variable::name() const {
  return "";
}

std::unique_ptr<Scope> Variable::scope() const {
  return nullptr;
}

std::string Variable::linkage_name() const {
  return "";
}

result<int64_t> Variable::address() const {
  return make_error_code(lief_errors::not_implemented);
}

result<uint64_t> Variable::size() const {
  return make_error_code(lief_errors::not_implemented);
}

bool Variable::is_constexpr() const {
  return true;
}

bool Variable::is_stack_based() const {
  return true;
}

std::unique_ptr<Type> Variable::type() const {
  return nullptr;
}

debug_location_t Variable::debug_location() const {
  return {};
}

std::string Variable::description() const {
  return "";
}

std::string Variable::to_decl(const DeclOpt& /*opt*/) const {
  return "";
}

Variable::~Variable() = default;

Variable::Iterator::Iterator() :
  impl_(nullptr) {}

Variable::Iterator::Iterator(std::unique_ptr<details::VariableIt>) :
  impl_(nullptr) {}

Variable::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

Variable::Iterator& Variable::Iterator::operator=(const Iterator&) {
  return *this;
}

Variable::Iterator::Iterator(Iterator&&) noexcept = default;
Variable::Iterator& Variable::Iterator::operator=(Iterator&&) noexcept = default;

Variable::Iterator::~Iterator() = default;

bool operator==(const Variable::Iterator&, const Variable::Iterator&) {
  return true;
}

Variable::Iterator& Variable::Iterator::operator++() {
  return *this;
}

Variable::Iterator& Variable::Iterator::operator--() {
  return *this;
}

void Variable::Iterator::load() const {}

const Variable& Variable::Iterator::operator*() const {
  return *cached_;
}

const Variable* Variable::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<Variable> Variable::Iterator::yield() {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/Parameter.hpp
// ----------------------------------------------------------------------------
Parameter::~Parameter() = default;


Parameter::Parameter(Parameter&& other) noexcept = default;
Parameter& Parameter::operator=(Parameter&& other) noexcept = default;

Parameter::Parameter(std::unique_ptr<details::Parameter> /*impl*/) :
  impl_(nullptr) {}

Parameter::KIND Parameter::kind() const {
  return KIND::UNKNOWN;
}

std::string Parameter::name() const {
  return "";
}

std::unique_ptr<Type> Parameter::type() const {
  return nullptr;
}

std::unique_ptr<Parameter::Location> Parameter::location() const {
  return nullptr;
}

std::unique_ptr<Parameter>
    Parameter::create(std::unique_ptr<details::Parameter> /*impl*/) {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/Function.hpp
// ----------------------------------------------------------------------------
Function::Function(std::unique_ptr<details::Function>) :
  impl_(nullptr) {}

Function::~Function() = default;

Function::instructions_it Function::instructions() const {
  return make_range<assembly::Instruction::Iterator>(
      assembly::Instruction::Iterator(), assembly::Instruction::Iterator()
  );
}

std::unique_ptr<Scope> Function::scope() const {
  return nullptr;
}

std::string Function::name() const {
  return "";
}

std::string Function::linkage_name() const {
  return "";
}

result<uint64_t> Function::address() const {
  return make_error_code(lief_errors::not_implemented);
}

Function::vars_it Function::variables() const {
  return make_empty_iterator<Variable>();
}

bool Function::is_artificial() const {
  return false;
}

bool Function::is_external() const {
  return false;
}

uint64_t Function::size() const {
  return 0;
}

std::vector<range_t> Function::ranges() const {
  return {};
}

debug_location_t Function::debug_location() const {
  return {};
}

std::unique_ptr<Type> Function::type() const {
  return nullptr;
}

Function::parameters_t Function::parameters() const {
  return {};
}

Function::lexical_blocks_it Function::lexical_blocks() const {
  return make_empty_iterator<LexicalBlock>();
}

std::string Function::description() const {
  return "";
}

std::string Function::to_decl(const DeclOpt& /*opt*/) const {
  return "";
}

Function::thrown_types_t Function::thrown_types() const {
  return {};
}

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

Function::Iterator& Function::Iterator::operator--() {
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

// ----------------------------------------------------------------------------
// DWARF/DebugInfo.hpp
// ----------------------------------------------------------------------------
std::unique_ptr<DebugInfo> DebugInfo::from_file(const std::string&) {
  LIEF_ERR(DEBUG_FMT_NOT_SUPPORTED);
  return nullptr;
}

std::unique_ptr<Function> DebugInfo::find_function(const std::string&) const {
  return nullptr;
}

std::unique_ptr<Function> DebugInfo::find_function(uint64_t) const {
  return nullptr;
}

std::unique_ptr<Variable> DebugInfo::find_variable(uint64_t /*addr*/) const {
  return nullptr;
}

std::unique_ptr<Variable>
    DebugInfo::find_variable(const std::string& /*name*/) const {
  return nullptr;
}

std::unique_ptr<Type> DebugInfo::find_type(const std::string& /*name*/) const {
  return nullptr;
}

optional<uint64_t>
    DebugInfo::find_function_address(const std::string& /*name*/) const {
  return nullopt();
}

DebugInfo::compilation_units_it DebugInfo::compilation_units() const {
  return make_empty_iterator<CompilationUnit>();
}

// ----------------------------------------------------------------------------
// DWARF/CompilationUnit.hpp
// ----------------------------------------------------------------------------
CompilationUnit::CompilationUnit(std::unique_ptr<details::CompilationUnit>) :
  impl_(nullptr) {}

CompilationUnit::~CompilationUnit() = default;


std::string CompilationUnit::name() const {
  return "";
}

std::string CompilationUnit::compilation_dir() const {
  return "";
}

std::string CompilationUnit::producer() const {
  return "";
}

uint64_t CompilationUnit::low_address() const {
  return 0;
}

uint64_t CompilationUnit::high_address() const {
  return 0;
}

uint64_t CompilationUnit::size() const {
  return 0;
}


std::vector<range_t> CompilationUnit::ranges() const {
  return {};
}

std::unique_ptr<Function>
    CompilationUnit::find_function(const std::string& /*name*/) const {
  return nullptr;
}

std::unique_ptr<Function> CompilationUnit::find_function(uint64_t /*addr*/) const {
  return nullptr;
}

std::unique_ptr<Variable> CompilationUnit::find_variable(uint64_t /*addr*/) const {
  return nullptr;
}

std::unique_ptr<Variable>
    CompilationUnit::find_variable(const std::string& /*name*/) const {
  return nullptr;
}

CompilationUnit::Language CompilationUnit::language() const {
  return {};
}

CompilationUnit::functions_it CompilationUnit::functions() const {
  return make_empty_iterator<Function>();
}

CompilationUnit::functions_it CompilationUnit::imported_functions() const {
  return make_empty_iterator<Function>();
}

CompilationUnit::types_it CompilationUnit::types() const {
  return make_empty_iterator<Type>();
}

CompilationUnit::vars_it CompilationUnit::variables() const {
  return make_empty_iterator<Variable>();
}

std::string CompilationUnit::to_decl(const DeclOpt& /*opt*/) const {
  return "";
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
  return cached_.get();
}

std::unique_ptr<CompilationUnit> CompilationUnit::Iterator::yield() {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/Type.hpp
// ----------------------------------------------------------------------------

std::unique_ptr<Type> Type::create(std::unique_ptr<details::Type> /* impl*/) {
  return nullptr;
}

std::unique_ptr<Scope> Type::scope() const {
  return nullptr;
}

Type::KIND Type::kind() const {
  return KIND::UNKNOWN;
}

result<std::string> Type::name() const {
  return make_error_code(lief_errors::not_implemented);
}

result<uint64_t> Type::size() const {
  return make_error_code(lief_errors::not_implemented);
}

debug_location_t Type::location() const {
  return {};
}

std::string Type::to_decl(const DeclOpt& /*opt*/) const {
  return "";
}

Type::Type(Type&&) noexcept = default;

Type& Type::operator=(Type&&) noexcept = default;

Type::Type(std::unique_ptr<details::Type> impl) :
  impl_(std::move(impl)) {}

Type::Type(details::Type& impl) :
  impl_(impl) {}

Type::~Type() = default;

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

Type::Iterator& Type::Iterator::operator--() {
  return *this;
}

void Type::Iterator::load() const {}

const Type& Type::Iterator::operator*() const {
  return *cached_;
}

const Type* Type::Iterator::operator->() const {
  return cached_.get();
}

std::unique_ptr<Type> Type::Iterator::yield() {
  return nullptr;
}
// ----------------------------------------------------------------------------
// DWARF/Scope.hpp
// ----------------------------------------------------------------------------
Scope::~Scope() = default;
Scope::Scope(std::unique_ptr<details::Scope> impl) :
  impl_(std::move(impl)) {}

std::string Scope::name() const {
  return "";
}

std::unique_ptr<Scope> Scope::parent() const {
  return nullptr;
}

Scope::TYPE Scope::type() const {
  return TYPE::UNKNOWN;
}

std::string Scope::chained(const std::string& /* sep */) const {
  return "";
}


// ----------------------------------------------------------------------------
// DWARF/LexicalBlock.hpp
// ----------------------------------------------------------------------------
LexicalBlock::LexicalBlock(std::unique_ptr<details::LexicalBlock>) :
  impl_(nullptr) {}

LexicalBlock::~LexicalBlock() = default;

std::string LexicalBlock::name() const {
  return "";
}

std::string LexicalBlock::description() const {
  return "";
}

LexicalBlock::sub_blocks_it LexicalBlock::sub_blocks() const {
  return make_empty_iterator<LexicalBlock>();
}

optional<uint64_t> LexicalBlock::addr() const {
  return nullopt();
}

uint64_t LexicalBlock::size() const {
  return 0;
}

optional<uint64_t> LexicalBlock::low_pc() const {
  return nullopt();
}

optional<uint64_t> LexicalBlock::high_pc() const {
  return nullopt();
}

std::vector<range_t> LexicalBlock::ranges() const {
  return {};
}

LexicalBlock::Iterator::Iterator() :
  impl_(nullptr) {}

LexicalBlock::Iterator::Iterator(std::unique_ptr<details::LexicalBlockIt>) :
  impl_(nullptr) {}

LexicalBlock::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

LexicalBlock::Iterator& LexicalBlock::Iterator::operator=(const Iterator&) {
  return *this;
}

LexicalBlock::Iterator::Iterator(Iterator&&) noexcept = default;
LexicalBlock::Iterator&
    LexicalBlock::Iterator::operator=(Iterator&&) noexcept = default;

LexicalBlock::Iterator::~Iterator() = default;

bool operator==(const LexicalBlock::Iterator&, const LexicalBlock::Iterator&) {
  return true;
}

LexicalBlock::Iterator& LexicalBlock::Iterator::operator++() {
  return *this;
}

LexicalBlock::Iterator& LexicalBlock::Iterator::operator--() {
  return *this;
}

void LexicalBlock::Iterator::load() const {}

const LexicalBlock& LexicalBlock::Iterator::operator*() const {
  return *cached_;
}

const LexicalBlock* LexicalBlock::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<LexicalBlock> LexicalBlock::Iterator::yield() {
  return nullptr;
}

namespace types {
// ----------------------------------------------------------------------------
// DWARF/types/ClassLike.hpp
// ----------------------------------------------------------------------------
ClassLike::~ClassLike() = default;

Class::~Class() = default;

Structure::~Structure() = default;

Union::~Union() = default;

Packed::~Packed() = default;

ClassLike::Member::~Member() = default;
ClassLike::Member::Member(std::unique_ptr<details::Member> impl) :
  impl_(std::move(impl)) {}

ClassLike::Member::Member(Member&& other) noexcept = default;
ClassLike::Member&
    ClassLike::Member::operator=(ClassLike::Member&& other) noexcept = default;

result<uint64_t> ClassLike::Member::offset() const {
  return make_error_code(lief_errors::not_implemented);
}

result<uint64_t> ClassLike::Member::bit_offset() const {
  return make_error_code(lief_errors::not_implemented);
}

result<uint64_t> ClassLike::Member::bit_size() const {
  return make_error_code(lief_errors::not_implemented);
}

bool ClassLike::Member::is_external() const {
  return false;
}

bool ClassLike::Member::is_declaration() const {
  return false;
}

std::unique_ptr<Type> ClassLike::Member::type() const {
  return nullptr;
}

std::string ClassLike::Member::name() const {
  return "";
}

std::vector<ClassLike::Member> ClassLike::members() const {
  return {};
}

ClassLike::functions_it ClassLike::functions() const {
  return make_empty_iterator<Function>();
}

std::unique_ptr<ClassLike::Member>
    ClassLike::find_member(uint64_t /*offset*/) const {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/types/Pointer.hpp
// ----------------------------------------------------------------------------
Pointer::~Pointer() = default;

const Type* Pointer::underlying_type() const {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/types/Const.hpp
// ----------------------------------------------------------------------------
Const::~Const() = default;

const Type* Const::underlying_type() const {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/types/Base.hpp
// ----------------------------------------------------------------------------
Base::~Base() = default;


Base::ENCODING Base::encoding() const {
  return ENCODING::NONE;
}

// ----------------------------------------------------------------------------
// DWARF/types/Array.hpp
// ----------------------------------------------------------------------------
Array::~Array() = default;

const Type* Array::underlying_type() const {
  return nullptr;
}

Array::size_info_t Array::size_info() const {
  return {};
}

// ----------------------------------------------------------------------------
// DWARF/types/Typedef.hpp
// ----------------------------------------------------------------------------
Typedef::~Typedef() = default;

const Type* Typedef::underlying_type() const {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/types/Atomic.hpp
// ----------------------------------------------------------------------------
Atomic::~Atomic() = default;

const Type* Atomic::underlying_type() const {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/types/Coarray.hpp
// ----------------------------------------------------------------------------
Coarray::~Coarray() = default;

// ----------------------------------------------------------------------------
// DWARF/types/Dynamic.hpp
// ----------------------------------------------------------------------------
Dynamic::~Dynamic() = default;

// ----------------------------------------------------------------------------
// DWARF/types/Enum.hpp
// ----------------------------------------------------------------------------
Enum::Entry::Entry(Entry&& other) noexcept = default;
Enum::Entry& Enum::Entry::operator=(Entry&& other) noexcept = default;

Enum::Entry::~Entry() = default;

Enum::Entry::Entry(std::unique_ptr<details::EnumEntry> impl) :
  impl_(std::move(impl)) {}

std::string Enum::Entry::name() const {
  return "";
}

optional<int64_t> Enum::Entry::value() const {
  return nullopt();
}

std::vector<Enum::Entry> Enum::entries() const {
  return {};
}

optional<Enum::Entry> Enum::find_entry(int64_t /*value*/) const {
  return nullopt();
}

const Type* Enum::underlying_type() const {
  return nullptr;
}

Enum::~Enum() = default;

// ----------------------------------------------------------------------------
// DWARF/types/File.hpp
// ----------------------------------------------------------------------------
File::~File() = default;

// ----------------------------------------------------------------------------
// DWARF/types/Immutable.hpp
// ----------------------------------------------------------------------------
Immutable::~Immutable() = default;

const Type* Immutable::underlying_type() const {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/types/Interface.hpp
// ----------------------------------------------------------------------------
Interface::~Interface() = default;

// ----------------------------------------------------------------------------
// DWARF/types/PointerToMember.hpp
// ----------------------------------------------------------------------------
PointerToMember::~PointerToMember() = default;

const Type* PointerToMember::underlying_type() const {
  return nullptr;
}

std::unique_ptr<Type> PointerToMember::containing_type() const {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/types/RValueReference.hpp
// ----------------------------------------------------------------------------
RValueReference::~RValueReference() = default;

const Type* RValueReference::underlying_type() const {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/types/Reference.hpp
// ----------------------------------------------------------------------------
Reference::~Reference() = default;

const Type* Reference::underlying_type() const {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/types/Restrict.hpp
// ----------------------------------------------------------------------------
Restrict::~Restrict() = default;

const Type* Restrict::underlying_type() const {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/types/SetTy.hpp
// ----------------------------------------------------------------------------
SetTy::~SetTy() = default;

const Type* SetTy::underlying_type() const {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/types/Shared.hpp
// ----------------------------------------------------------------------------
Shared::~Shared() = default;

const Type* Shared::underlying_type() const {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/types/StringTy.hpp
// ----------------------------------------------------------------------------
StringTy::~StringTy() = default;

// ----------------------------------------------------------------------------
// DWARF/types/Subroutine.hpp
// ----------------------------------------------------------------------------
Subroutine::~Subroutine() = default;

std::unique_ptr<Type> Subroutine::return_type() const {
  return {};
}

Subroutine::parameters_t Subroutine::parameters() const {
  return {};
}

// ----------------------------------------------------------------------------
// DWARF/types/TemplateAlias.hpp
// ----------------------------------------------------------------------------
TemplateAlias::~TemplateAlias() = default;

const Type* TemplateAlias::underlying_type() const {
  return nullptr;
}

TemplateAlias::parameters_t TemplateAlias::parameters() const {
  return {};
}

// ----------------------------------------------------------------------------
// DWARF/types/Thrown.hpp
// ----------------------------------------------------------------------------
Thrown::~Thrown() = default;

const Type* Thrown::underlying_type() const {
  return nullptr;
}

// ----------------------------------------------------------------------------
// DWARF/types/Volatile.hpp
// ----------------------------------------------------------------------------
Volatile::~Volatile() = default;

const Type* Volatile::underlying_type() const {
  return nullptr;
}

} // namespace types


// ----------------------------------------------------------------------------
// DWARF/Editor.hpp
// ----------------------------------------------------------------------------
Editor::Editor(std::unique_ptr<details::Editor> impl) :
  impl_(std::move(impl)) {}

Editor::~Editor() = default;

std::unique_ptr<Editor> Editor::from_binary(LIEF::Binary& /*bin*/) {
  logging::needs_lief_extended();
  return nullptr;
}

std::unique_ptr<Editor> Editor::create(FORMAT /*fmt*/, ARCH /*arch*/) {
  logging::needs_lief_extended();
  return nullptr;
}

std::unique_ptr<editor::CompilationUnit> Editor::create_compilation_unit() {
  return nullptr;
}

void Editor::write(const std::string& /*output*/) {
  return;
}

namespace editor {
// ----------------------------------------------------------------------------
// DWARF/editor/CompilationUnit.hpp
// ----------------------------------------------------------------------------
CompilationUnit::CompilationUnit(std::unique_ptr<details::CompilationUnit> impl) :
  impl_(std::move(impl)) {}


CompilationUnit& CompilationUnit::set_producer(const std::string&) {
  return *this;
}

std::unique_ptr<Function>
    CompilationUnit::create_function(const std::string& /*name*/) {
  return nullptr;
}

std::unique_ptr<Variable>
    CompilationUnit::create_variable(const std::string& /*name*/) {
  return nullptr;
}

std::unique_ptr<Type>
    CompilationUnit::create_generic_type(const std::string& /*name*/) {
  return nullptr;
}

std::unique_ptr<EnumType> CompilationUnit::create_enum(const std::string&) {
  return nullptr;
}

std::unique_ptr<TypeDef> CompilationUnit::create_typedef(const std::string&,
                                                         const Type&) {
  return nullptr;
}

std::unique_ptr<StructType> CompilationUnit::create_structure(const std::string&,
                                                              StructType::TYPE) {
  return nullptr;
}

std::unique_ptr<BaseType> CompilationUnit::create_base_type(const std::string&,
                                                            size_t,
                                                            BaseType::ENCODING) {
  return nullptr;
}

std::unique_ptr<FunctionType>
    CompilationUnit::create_function_type(const std::string&) {
  return nullptr;
}

std::unique_ptr<Type> CompilationUnit::create_void_type() {
  return nullptr;
}

std::unique_ptr<ArrayType> CompilationUnit::create_array(const std::string&,
                                                         const Type&, size_t) {
  return nullptr;
}

CompilationUnit::~CompilationUnit() = default;

// ----------------------------------------------------------------------------
// DWARF/editor/Variable.hpp
// ----------------------------------------------------------------------------
Variable::Variable(std::unique_ptr<details::Variable> impl) :
  impl_(std::move(impl)) {}

Variable& Variable::set_external() {
  return *this;
}

Variable& Variable::set_addr(uint64_t /*address*/) {
  return *this;
}

Variable& Variable::set_stack_offset(uint64_t /*address*/) {
  return *this;
}

Variable& Variable::set_type(const Type&) {
  return *this;
}

Variable& Variable::add_description(const std::string& /*description*/) {
  return *this;
}

Variable::~Variable() = default;

// ----------------------------------------------------------------------------
// DWARF/editor/Function.hpp
// ----------------------------------------------------------------------------
Function::Function(std::unique_ptr<details::Function> impl) :
  impl_(std::move(impl)) {}

Function& Function::set_address(uint64_t /*addr*/) {
  return *this;
}

Function& Function::set_low_high(uint64_t /*low*/, uint64_t /*high*/) {
  return *this;
}

Function& Function::set_ranges(const std::vector<range_t>& /*ranges*/) {
  return *this;
}

Function& Function::set_external() {
  return *this;
}

Function& Function::add_description(const std::string& /*description*/) {
  return *this;
}

Function& Function::set_return_type(const Type&) {
  return *this;
}

std::unique_ptr<Function::Parameter> Function::add_parameter(const std::string&,
                                                             const Type&) {
  return nullptr;
}

std::unique_ptr<Variable> Function::create_stack_variable(const std::string&) {
  return nullptr;
}

std::unique_ptr<Function::LexicalBlock> Function::add_lexical_block(uint64_t,
                                                                    uint64_t) {
  return nullptr;
}

std::unique_ptr<Function::Label> Function::add_label(uint64_t,
                                                     const std::string&) {
  return nullptr;
}

Function::Parameter::Parameter(std::unique_ptr<details::FunctionParameter> impl) :
  impl_(std::move(impl)) {}

Function::Parameter&
    Function::Parameter::assign_register(const std::string& /*name*/) {
  return *this;
}

Function::Parameter& Function::Parameter::assign_register(uint64_t /*reg*/) {
  return *this;
}

Function::Parameter::~Parameter() = default;


Function::LexicalBlock::LexicalBlock(
    std::unique_ptr<details::FunctionLexicalBlock> impl
) :
  impl_(std::move(impl)) {}

Function::LexicalBlock::~LexicalBlock() = default;

std::unique_ptr<Function::LexicalBlock>
    Function::LexicalBlock::add_block(uint64_t /*start*/, uint64_t /*end*/) {
  return nullptr;
}

std::unique_ptr<Function::LexicalBlock>
    Function::LexicalBlock::add_block(const std::vector<range_t>& /*ranges*/) {
  return nullptr;
}

Function::LexicalBlock&
    Function::LexicalBlock::add_description(const std::string& /*name*/) {
  return *this;
}

Function::LexicalBlock&
    Function::LexicalBlock::add_name(const std::string& /*name*/) {
  return *this;
}

Function::Label::Label(std::unique_ptr<details::FunctionLabel> impl) :
  impl_(std::move(impl)) {}

Function::Label::~Label() = default;

Function::~Function() = default;

// ----------------------------------------------------------------------------
// DWARF/editor/Type.hpp
// ----------------------------------------------------------------------------

Type::Type(std::unique_ptr<details::Type> impl) :
  impl_(std::move(impl)) {}

std::unique_ptr<Type> Type::create(std::unique_ptr<details::Type> /*impl*/) {
  return nullptr;
}


std::unique_ptr<PointerType> Type::pointer_to() const {
  return nullptr;
}

Type::~Type() = default;

// ----------------------------------------------------------------------------
// DWARF/editor/PointerType.hpp
// ----------------------------------------------------------------------------
bool PointerType::classof(const Type* /*type*/) {
  return false;
}

// ----------------------------------------------------------------------------
// DWARF/editor/EnumType.hpp
// ----------------------------------------------------------------------------
bool EnumType::classof(const Type* /*type*/) {
  return false;
}

EnumType& EnumType::set_underlying_type(const Type& /*type*/) {
  return *this;
}

EnumType& EnumType::set_size(uint64_t /*size*/) {
  return *this;
}

std::unique_ptr<EnumType::Value> EnumType::add_value(const std::string& /*name*/,
                                                     int64_t /*value*/) {
  return nullptr;
}

EnumType::Value::Value(std::unique_ptr<details::EnumValue> impl) :
  impl_(std::move(impl)) {}

EnumType::Value::~Value() = default;

// ----------------------------------------------------------------------------
// DWARF/editor/BaseType.hpp
// ----------------------------------------------------------------------------
bool BaseType::classof(const Type* /*type*/) {
  return false;
}

// ----------------------------------------------------------------------------
// DWARF/editor/ArrayType.hpp
// ----------------------------------------------------------------------------
bool ArrayType::classof(const Type* /*type*/) {
  return false;
}

// ----------------------------------------------------------------------------
// DWARF/editor/FunctionType.hpp
// ----------------------------------------------------------------------------
bool FunctionType::classof(const Type* /*type*/) {
  return false;
}

FunctionType& FunctionType::set_return_type(const Type& /*type*/) {
  return *this;
}

std::unique_ptr<FunctionType::Parameter>
    FunctionType::add_parameter(const Type& /*type*/) {
  return nullptr;
}

FunctionType::Parameter::Parameter(
    std::unique_ptr<details::FunctionTyParameter> impl
) :
  impl_(std::move(impl)) {}

FunctionType::Parameter::~Parameter() = default;

// ----------------------------------------------------------------------------
// DWARF/editor/TypeDef.hpp
// ----------------------------------------------------------------------------
bool TypeDef::classof(const Type* /*type*/) {
  return false;
}

// ----------------------------------------------------------------------------
// DWARF/editor/StructType.hpp
// ----------------------------------------------------------------------------
bool StructType::classof(const Type* /*type*/) {
  return false;
}

StructType& StructType::set_size(uint64_t /*size*/) {
  return *this;
}

std::unique_ptr<StructType::Member>
    StructType::add_member(const std::string& /*name*/, const Type& /*type*/,
                           int64_t /*offset*/) {
  return nullptr;
}

std::unique_ptr<StructType::Member>
    StructType::add_bitfield(const std::string& /*name*/, const Type& /*type*/,
                             uint64_t /*bitsize*/, int64_t /*bitoffset*/) {
  return nullptr;
}

StructType::Member::Member(std::unique_ptr<details::StructMember> impl) :
  impl_(std::move(impl)) {}

StructType::Member::~Member() = default;

}

// NOLINTEND
}
