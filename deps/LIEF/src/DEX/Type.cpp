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

#include "LIEF/DEX/Type.hpp"
#include "LIEF/DEX/hash.hpp"
#include "LIEF/DEX/Class.hpp"
#include "logging.hpp"

namespace LIEF {
namespace DEX {

Type::Type() = default;

Type::Type(const std::string& mangled) {
  parse(mangled);
}


Type::Type(const Type& other) :
  Object{other},
  type_{other.type_}
{
  switch (type()) {
    case TYPES::ARRAY:
      {
        array_ = new array_t{};
        std::copy(
            std::begin(other.array()),
            std::end(other.array()),
            std::back_inserter(*array_));
        break;
      }

    case TYPES::CLASS:
      {
        cls_ = other.cls_;
        break;
      }

    case TYPES::PRIMITIVE:
      {
        basic_ = new PRIMITIVES{other.primitive()};
        break;
      }

    default:
      {}
  }
}

Type::TYPES Type::type() const {
  return type_;
}

const Class& Type::cls() const {
  return *cls_;
}

const Type::array_t& Type::array() const {
  return *array_;
}

const Type::PRIMITIVES& Type::primitive() const {
  return *basic_;
}


Class& Type::cls() {
  return const_cast<Class&>(static_cast<const Type*>(this)->cls());
}

Type::array_t& Type::array() {
  return const_cast<Type::array_t&>(static_cast<const Type*>(this)->array());
}

Type::PRIMITIVES& Type::primitive() {
  return const_cast<Type::PRIMITIVES&>(static_cast<const Type*>(this)->primitive());
}

const Type& Type::underlying_array_type() const {
  const Type* underlying_type = this;
  while (underlying_type->type() == TYPES::ARRAY) {
    underlying_type = &underlying_type->array().back();
  }
  return *underlying_type;
}


Type& Type::underlying_array_type() {
  return const_cast<Type&>((static_cast<const Type*>(this)->underlying_array_type()));
}


void Type::parse(const std::string& type) {
  const char t = type[0];
  switch(t) {
    case 'V':
      {
        type_ = Type::TYPES::PRIMITIVE;
        basic_ = new Type::PRIMITIVES{Type::PRIMITIVES::VOID_T};
        break;
      }

    case 'Z':
      {
        type_ = Type::TYPES::PRIMITIVE;
        basic_ = new Type::PRIMITIVES{Type::PRIMITIVES::BOOLEAN};
        break;
      }

    case 'B':
      {
        type_ = Type::TYPES::PRIMITIVE;
        basic_ = new Type::PRIMITIVES{Type::PRIMITIVES::BYTE};
        break;
      }

    case 'S':
      {
        type_ = Type::TYPES::PRIMITIVE;
        basic_ = new Type::PRIMITIVES{Type::PRIMITIVES::SHORT};
        break;
      }

    case 'C':
      {
        type_ = Type::TYPES::PRIMITIVE;
        basic_ = new Type::PRIMITIVES{Type::PRIMITIVES::CHAR};
        break;
      }

    case 'I':
      {
        type_ = Type::TYPES::PRIMITIVE;
        basic_ = new Type::PRIMITIVES{Type::PRIMITIVES::INT};
        break;
      }

    case 'J':
      {
        type_ = Type::TYPES::PRIMITIVE;
        basic_ = new Type::PRIMITIVES{Type::PRIMITIVES::LONG};
        break;
      }

    case 'F':
      {
        type_ = Type::TYPES::PRIMITIVE;
        basic_ = new Type::PRIMITIVES{Type::PRIMITIVES::FLOAT};
        break;
      }

    case 'D':
      {
        type_ = Type::TYPES::PRIMITIVE;
        basic_ = new Type::PRIMITIVES{Type::PRIMITIVES::DOUBLE};
        break;
      }

    case 'L': //CLASS
      {
        type_ = Type::TYPES::CLASS;
        break;
      }

    case '[': //ARRAY
      {
        if (array_ == nullptr) {
          array_ = new array_t{};
        }
        type_ = Type::TYPES::ARRAY;
        array_->emplace_back(type.substr(1));
        break;
      }

    default:
      {
        LIEF_WARN("Unknown type: '{}'", t);
      }
  }
}

size_t Type::dim() const {
  if (type() != TYPES::ARRAY) {
    return 0;
  }

  const Type* t = this;
  size_t d = 0;
  while (t->type() == TYPES::ARRAY) {
    ++d;
    t = &(t->array().back());
  }
  return d;
}

void Type::accept(Visitor& visitor) const {
  visitor.visit(*this);
}



std::ostream& operator<<(std::ostream& os, const Type& type) {
  switch (type.type()) {
    case Type::TYPES::ARRAY:
      {
        os << type.underlying_array_type();
        for (size_t i = 0; i < type.dim(); ++i) {
          os << "[]";
        }
        return os;
      }

    case Type::TYPES::CLASS:
      {
        os << type.cls().fullname();
        return os;
      }

    case Type::TYPES::PRIMITIVE:
      {
        os << Type::pretty_name(type.primitive());
        return os;
      }

    default:
      {
        return os;
      }
  }
  return os;
}


std::string Type::pretty_name(PRIMITIVES p) {
  switch (p) {
    case PRIMITIVES::BOOLEAN:
      {
        return "bool";
      }

    case PRIMITIVES::BYTE:
      {
        return "byte";
      }

    case PRIMITIVES::CHAR:
      {
        return "char";
      }

    case PRIMITIVES::DOUBLE:
      {
        return "double";
      }

    case PRIMITIVES::FLOAT:
      {
        return "float";
      }

    case PRIMITIVES::INT:
      {
        return "int";
      }

    case PRIMITIVES::LONG:
      {
        return "long";
      }

    case PRIMITIVES::SHORT:
      {
        return "short";
      }

    case PRIMITIVES::VOID_T:
      {
        return "void";
      }

    default:
      {
        return "";
      }
  }
}

Type::~Type() {
  switch (type()) {
    case Type::TYPES::ARRAY:
      {
        delete array_;
        break;
      }

    case Type::TYPES::PRIMITIVE:
      {
        delete basic_;
        break;
      }

    case Type::TYPES::CLASS:
    case Type::TYPES::UNKNOWN:
    default:
      {
      }
  }
}

}
}
