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
#include "LIEF/ObjC/Metadata.hpp"
#include "LIEF/ObjC/Class.hpp"
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

class IVar {};
class IVarIt {};

class Property {};
class PropertyIt {};
}

// ----------------------------------------------------------------------------
// ObjC/Metadata.hpp
// ----------------------------------------------------------------------------
Metadata::Metadata(std::unique_ptr<details::Metadata>) :
  impl_(nullptr)
{}

std::string Metadata::to_decl(const DeclOpt&) const {
  return "";
}

Metadata::classes_it Metadata::classes() const {
  return make_empty_iterator<Class>();
}

Metadata::protocols_it Metadata::protocols() const {
  return make_empty_iterator<Protocol>();
}

std::unique_ptr<Class> Metadata::get_class(const std::string&/*name*/) const {
  return nullptr;
}

std::unique_ptr<Protocol> Metadata::get_protocol(const std::string&/*name*/) const {
  return nullptr;
}

Metadata::~Metadata() = default;

// ----------------------------------------------------------------------------
// ObjC/Class.hpp
// ----------------------------------------------------------------------------
Class::Iterator::Iterator(std::unique_ptr<details::ClassIt>) :
  impl_(nullptr)
{}

Class::Iterator::Iterator(const Iterator&) :
  impl_(nullptr)
{}

Class::Iterator::Iterator(Iterator&&) noexcept :
  impl_(nullptr)
{}

Class::Iterator::~Iterator() = default;

bool operator==(const Class::Iterator&,
                const Class::Iterator&)
{
  return true;
}

Class::Iterator& Class::Iterator::operator++() {
  return *this;
}

Class::Iterator& Class::Iterator::operator--() {
  return *this;
}

std::unique_ptr<Class> Class::Iterator::operator*() const {
  return nullptr;
}

Class::Class(std::unique_ptr<details::Class> impl) :
  impl_(std::move(impl))
{}

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
Protocol::Iterator::Iterator(std::unique_ptr<details::ProtocolIt>) :
  impl_(nullptr)
{}

Protocol::Iterator::Iterator(const Iterator&) :
  impl_(nullptr)
{}

Protocol::Iterator::Iterator(Iterator&&) noexcept :
  impl_(nullptr)
{}

Protocol::Iterator::~Iterator() = default;

bool operator==(const Protocol::Iterator&,
                const Protocol::Iterator&)
{
  return true;
}

Protocol::Iterator& Protocol::Iterator::operator++() {
  return *this;
}

Protocol::Iterator& Protocol::Iterator::operator--() {
  return *this;
}

std::unique_ptr<Protocol> Protocol::Iterator::operator*() const {
  return nullptr;
}

Protocol::Protocol(std::unique_ptr<details::Protocol> impl) :
  impl_(std::move(impl))
{}

Protocol::~Protocol() = default;

std::string Protocol::mangled_name() const {
  return "";
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
// ObjC/Property.hpp
// ----------------------------------------------------------------------------
Property::Iterator::Iterator(std::unique_ptr<details::PropertyIt>) :
  impl_(nullptr)
{}

Property::Iterator::Iterator(const Iterator&) :
  impl_(nullptr)
{}

Property::Iterator::Iterator(Iterator&&) noexcept :
  impl_(nullptr)
{}

Property::Iterator::~Iterator() = default;

bool operator==(const Property::Iterator&,
                const Property::Iterator&)
{
  return true;
}

Property::Iterator& Property::Iterator::operator++() {
  return *this;
}

Property::Iterator& Property::Iterator::operator--() {
  return *this;
}

std::unique_ptr<Property> Property::Iterator::operator*() const {
  return nullptr;
}

Property::Property(std::unique_ptr<details::Property> impl) :
  impl_(std::move(impl))
{}

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

Method::Iterator& Method::Iterator::operator--() {
  return *this;
}

std::unique_ptr<Method> Method::Iterator::operator*() const {
  return nullptr;
}

Method::Method(std::unique_ptr<details::Method> impl) :
  impl_(std::move(impl))
{}

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
IVar::Iterator::Iterator(std::unique_ptr<details::IVarIt>) :
  impl_(nullptr)
{}

IVar::Iterator::Iterator(const Iterator&) :
  impl_(nullptr)
{}

IVar::Iterator::Iterator(Iterator&&) noexcept :
  impl_(nullptr)
{}

IVar::Iterator::~Iterator() = default;

bool operator==(const IVar::Iterator&,
                const IVar::Iterator&)
{
  return true;
}

IVar::Iterator& IVar::Iterator::operator++() {
  return *this;
}

IVar::Iterator& IVar::Iterator::operator--() {
  return *this;
}

std::unique_ptr<IVar> IVar::Iterator::operator*() const {
  return nullptr;
}

IVar::IVar(std::unique_ptr<details::IVar> impl) :
  impl_(std::move(impl))
{}

IVar::~IVar() = default;

std::string IVar::name() const {
  return "";
}

std::string IVar::mangled_type() const {
  return "";
}

}
