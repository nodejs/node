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
#include "LIEF/ObjC/Metadata.hpp"
#include "LIEF/ObjC/Class.hpp"
#include "LIEF/ObjC/Category.hpp"
#include "LIEF/MachO/Binary.hpp"

#include "logging.hpp"
#include "messages.hpp"
#include "internal_utils.hpp"

// ----------------------------------------------------------------------------
// Mach-O Interface
// ----------------------------------------------------------------------------
namespace LIEF::MachO {
std::unique_ptr<objc::Metadata> Binary::objc_metadata() const {
  LIEF_ERR(OBJC_NOT_SUPPORTED);
  return nullptr;
}
}

namespace LIEF::objc {
namespace details {
class Metadata {};

class Class {};
class ClassIt {};

class Method {};
class MethodIt {};

class Protocol {};
class ProtocolIt {};

class Category {};
class CategoryIt {};

class IVar {};
class IVarIt {};

class Property {};
class PropertyIt {};
}

// ----------------------------------------------------------------------------
// ObjC/Metadata.hpp
// ----------------------------------------------------------------------------
Metadata::Metadata(std::unique_ptr<details::Metadata>) :
  impl_(nullptr) {}

std::string Metadata::to_decl(const DeclOpt&) const {
  return "";
}

Metadata::classes_it Metadata::classes() const {
  return make_empty_iterator<Class>();
}

Metadata::protocols_it Metadata::protocols() const {
  return make_empty_iterator<Protocol>();
}

Metadata::categories_it Metadata::categories() const {
  return make_empty_iterator<Category>();
}

std::unique_ptr<Class> Metadata::get_class(const std::string& /*name*/) const {
  return nullptr;
}

std::unique_ptr<Protocol>
    Metadata::get_protocol(const std::string& /*name*/) const {
  return nullptr;
}

Metadata::~Metadata() = default;

// ----------------------------------------------------------------------------
// ObjC/Class.hpp
// ----------------------------------------------------------------------------
Class::Iterator::Iterator() :
  impl_(nullptr) {}

Class::Iterator::Iterator(std::unique_ptr<details::ClassIt>) :
  impl_(nullptr) {}

Class::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

Class::Iterator& Class::Iterator::operator=(const Iterator&) {
  return *this;
}

Class::Iterator::Iterator(Iterator&&) noexcept = default;
Class::Iterator& Class::Iterator::operator=(Iterator&&) noexcept = default;

Class::Iterator::~Iterator() = default;

bool operator==(const Class::Iterator&, const Class::Iterator&) {
  return true;
}

Class::Iterator& Class::Iterator::operator++() {
  cached_.reset();
  return *this;
}

Class::Iterator& Class::Iterator::operator--() {
  return *this;
}

void Class::Iterator::load() const {}

const Class& Class::Iterator::operator*() const {
  return *cached_;
}

const Class* Class::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<Class> Class::Iterator::yield() {
  return nullptr;
}

Class::Class(std::unique_ptr<details::Class> impl) :
  impl_(std::move(impl)) {}

std::string Class::name() const {
  return "";
}

std::string Class::demangled_name() const {
  return "";
}

std::string Class::to_decl(const DeclOpt&) const {
  return "";
}


std::unique_ptr<Class> Class::super_class() const {
  return nullptr;
}

std::string Class::super_name() const {
  return "";
}

std::string Class::demangled_super_name() const {
  return "";
}

bool Class::is_meta() const {
  return false;
}

Class::methods_t Class::methods() const {
  return make_empty_iterator<Method>();
}

Class::protocols_t Class::protocols() const {
  return make_empty_iterator<Protocol>();
}

Class::properties_t Class::properties() const {
  return make_empty_iterator<Property>();
}

Class::ivars_t Class::ivars() const {
  return make_empty_iterator<IVar>();
}

Class::~Class() = default;

// ----------------------------------------------------------------------------
// ObjC/Protocol.hpp
// ----------------------------------------------------------------------------
Protocol::Iterator::Iterator() :
  impl_(nullptr) {}

Protocol::Iterator::Iterator(std::unique_ptr<details::ProtocolIt>) :
  impl_(nullptr) {}

Protocol::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

Protocol::Iterator& Protocol::Iterator::operator=(const Iterator&) {
  return *this;
}

Protocol::Iterator::Iterator(Iterator&&) noexcept = default;
Protocol::Iterator& Protocol::Iterator::operator=(Iterator&&) noexcept = default;

Protocol::Iterator::~Iterator() = default;

bool operator==(const Protocol::Iterator&, const Protocol::Iterator&) {
  return true;
}

Protocol::Iterator& Protocol::Iterator::operator++() {
  return *this;
}

Protocol::Iterator& Protocol::Iterator::operator--() {
  return *this;
}

void Protocol::Iterator::load() const {}

const Protocol& Protocol::Iterator::operator*() const {
  return *cached_;
}

const Protocol* Protocol::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<Protocol> Protocol::Iterator::yield() {
  return nullptr;
}

Protocol::Protocol(std::unique_ptr<details::Protocol> impl) :
  impl_(std::move(impl)) {}

Protocol::~Protocol() = default;

std::string Protocol::mangled_name() const {
  return "";
}

Protocol::protocols_it Protocol::protocols() const {
  return make_empty_iterator<Protocol>();
}

std::string Protocol::to_decl(const DeclOpt&) const {
  return "";
}

Protocol::methods_it Protocol::optional_methods() const {
  return make_empty_iterator<Method>();
}

Protocol::methods_it Protocol::required_methods() const {
  return make_empty_iterator<Method>();
}

Protocol::properties_it Protocol::properties() const {
  return make_empty_iterator<Property>();
}

// ----------------------------------------------------------------------------
// ObjC/Category.hpp
// ----------------------------------------------------------------------------
Category::Iterator::Iterator() :
  impl_(nullptr) {}

Category::Iterator::Iterator(std::unique_ptr<details::CategoryIt>) :
  impl_(nullptr) {}

Category::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

Category::Iterator& Category::Iterator::operator=(const Iterator&) {
  return *this;
}

Category::Iterator::Iterator(Iterator&&) noexcept = default;
Category::Iterator& Category::Iterator::operator=(Iterator&&) noexcept = default;

Category::Iterator::~Iterator() = default;

bool operator==(const Category::Iterator&, const Category::Iterator&) {
  return true;
}

Category::Iterator& Category::Iterator::operator++() {
  return *this;
}

Category::Iterator& Category::Iterator::operator--() {
  return *this;
}

void Category::Iterator::load() const {}

const Category& Category::Iterator::operator*() const {
  return *cached_;
}

const Category* Category::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<Category> Category::Iterator::yield() {
  return nullptr;
}

Category::Category(std::unique_ptr<details::Category> impl) :
  impl_(std::move(impl)) {}

Category::~Category() = default;

std::string Category::name() const {
  return "";
}

std::string Category::class_name() const {
  return "";
}

std::string Category::to_decl(const DeclOpt&) const {
  return "";
}

Category::methods_t Category::methods() const {
  return make_empty_iterator<Method>();
}

Category::protocols_t Category::protocols() const {
  return make_empty_iterator<Protocol>();
}

Category::properties_t Category::properties() const {
  return make_empty_iterator<Property>();
}

// ----------------------------------------------------------------------------
// ObjC/Property.hpp
// ----------------------------------------------------------------------------
Property::Iterator::Iterator() :
  impl_(nullptr) {}

Property::Iterator::Iterator(std::unique_ptr<details::PropertyIt>) :
  impl_(nullptr) {}

Property::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

Property::Iterator& Property::Iterator::operator=(const Iterator&) {
  return *this;
}

Property::Iterator::Iterator(Iterator&&) noexcept = default;
Property::Iterator& Property::Iterator::operator=(Iterator&&) noexcept = default;

Property::Iterator::~Iterator() = default;

bool operator==(const Property::Iterator&, const Property::Iterator&) {
  return true;
}

Property::Iterator& Property::Iterator::operator++() {
  return *this;
}

Property::Iterator& Property::Iterator::operator--() {
  return *this;
}

void Property::Iterator::load() const {}

const Property& Property::Iterator::operator*() const {
  return *cached_;
}

const Property* Property::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<Property> Property::Iterator::yield() {
  return nullptr;
}

Property::Property(std::unique_ptr<details::Property> impl) :
  impl_(std::move(impl)) {}

Property::~Property() = default;

std::string Property::name() const {
  return "";
}

std::string Property::attribute() const {
  return "";
}


// ----------------------------------------------------------------------------
// ObjC/Method.hpp
// ----------------------------------------------------------------------------
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

Method::Iterator& Method::Iterator::operator--() {
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

Method::Method(std::unique_ptr<details::Method> impl) :
  impl_(std::move(impl)) {}

Method::~Method() = default;

std::string Method::name() const {
  return "";
}

std::string Method::mangled_type() const {
  return "";
}

uintptr_t Method::address() const {
  return 0;
}

bool Method::is_instance() const {
  return false;
}

// ----------------------------------------------------------------------------
// ObjC/IVar.hpp
// ----------------------------------------------------------------------------
IVar::Iterator::Iterator() :
  impl_(nullptr) {}

IVar::Iterator::Iterator(std::unique_ptr<details::IVarIt>) :
  impl_(nullptr) {}

IVar::Iterator::Iterator(const Iterator&) :
  impl_(nullptr) {}

IVar::Iterator& IVar::Iterator::operator=(const Iterator&) {
  return *this;
}

IVar::Iterator::Iterator(Iterator&&) noexcept = default;
IVar::Iterator& IVar::Iterator::operator=(Iterator&&) noexcept = default;

IVar::Iterator::~Iterator() = default;

bool operator==(const IVar::Iterator&, const IVar::Iterator&) {
  return true;
}

IVar::Iterator& IVar::Iterator::operator++() {
  return *this;
}

IVar::Iterator& IVar::Iterator::operator--() {
  return *this;
}

void IVar::Iterator::load() const {}

const IVar& IVar::Iterator::operator*() const {
  return *cached_;
}

const IVar* IVar::Iterator::operator->() const {
  return nullptr;
}

std::unique_ptr<IVar> IVar::Iterator::yield() {
  return nullptr;
}

IVar::IVar(std::unique_ptr<details::IVar> impl) :
  impl_(std::move(impl)) {}

IVar::~IVar() = default;

std::string IVar::name() const {
  return "";
}

std::string IVar::mangled_type() const {
  return "";
}

}
// NOLINTEND
