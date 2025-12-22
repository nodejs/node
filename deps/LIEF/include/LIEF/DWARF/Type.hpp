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
#ifndef LIEF_DWARF_TYPE_H
#define LIEF_DWARF_TYPE_H

#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"
#include "LIEF/debug_loc.hpp"
#include "LIEF/canbe_unique.hpp"

namespace LIEF {
namespace dwarf {
class Scope;

namespace details {
class Type;
class TypeIt;
}

/// This class represents a DWARF Type which includes:
///
/// - `DW_TAG_array_type`
/// - `DW_TAG_atomic_type`
/// - `DW_TAG_base_type`
/// - `DW_TAG_class_type`
/// - `DW_TAG_coarray_type`
/// - `DW_TAG_const_type`
/// - `DW_TAG_dynamic_type`
/// - `DW_TAG_enumeration_type`
/// - `DW_TAG_file_type`
/// - `DW_TAG_immutable_type`
/// - `DW_TAG_interface_type`
/// - `DW_TAG_packed_type`
/// - `DW_TAG_pointer_type`
/// - `DW_TAG_ptr_to_member_type`
/// - `DW_TAG_reference_type`
/// - `DW_TAG_restrict_type`
/// - `DW_TAG_rvalue_reference_type`
/// - `DW_TAG_set_type`
/// - `DW_TAG_shared_type`
/// - `DW_TAG_string_type`
/// - `DW_TAG_structure_type`
/// - `DW_TAG_subroutine_type`
/// - `DW_TAG_template_alias`
/// - `DW_TAG_thrown_type`
/// - `DW_TAG_typedef`
/// - `DW_TAG_union_type`
/// - `DW_TAG_unspecified_type`
/// - `DW_TAG_volatile_type`
class LIEF_API Type {
  public:
  class LIEF_API Iterator {
    public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::unique_ptr<Type>;
    using difference_type = std::ptrdiff_t;
    using pointer = Type*;
    using reference = std::unique_ptr<Type>&;
    using implementation = details::TypeIt;

    class LIEF_API PointerProxy {
      // Inspired from LLVM's iterator_facade_base
      friend class Iterator;
      public:
      pointer operator->() const { return R.get(); }

      private:
      value_type R;

      template <typename RefT>
      PointerProxy(RefT &&R) : R(std::forward<RefT>(R)) {} // NOLINT(bugprone-forwarding-reference-overload)
    };

    Iterator(const Iterator&);
    Iterator(Iterator&&) noexcept;
    Iterator(std::unique_ptr<details::TypeIt> impl);
    ~Iterator();

    friend LIEF_API bool operator==(const Iterator& LHS, const Iterator& RHS);
    friend LIEF_API bool operator!=(const Iterator& LHS, const Iterator& RHS) {
      return !(LHS == RHS);
    }

    Iterator& operator++();
    Iterator& operator--();

    Iterator operator--(int) {
      Iterator tmp = *static_cast<Iterator*>(this);
      --*static_cast<Iterator *>(this);
      return tmp;
    }

    Iterator operator++(int) {
      Iterator tmp = *static_cast<Iterator*>(this);
      ++*static_cast<Iterator *>(this);
      return tmp;
    }

    std::unique_ptr<Type> operator*() const;

    PointerProxy operator->() const {
      return static_cast<const Iterator*>(this)->operator*();
    }

    private:
    std::unique_ptr<details::TypeIt> impl_;
  };

  virtual ~Type();

  enum class KIND {
    UNKNOWN = 0,
    UNSPECIFIED,
    BASE,
    CONST_KIND,
    CLASS,
    ARRAY,
    POINTER,
    STRUCT,
    UNION,
    TYPEDEF,
    REF,
    SET_TYPE,
    STRING,
    SUBROUTINE,
    POINTER_MEMBER,
    PACKED,
    FILE,
    THROWN,
    VOLATILE,
    RESTRICT,
    INTERFACE,
    SHARED,
    RVALREF,
    TEMPLATE_ALIAS,
    COARRAY,
    DYNAMIC,
    ATOMIC,
    IMMUTABLE,
    ENUM,
  };

  KIND kind() const;

  /// Whether this type is a `DW_TAG_unspecified_type`
  bool is_unspecified() const {
    return kind() == KIND::UNSPECIFIED;
  }

  /// Return the type's name using either `DW_AT_name` or `DW_AT_picture_string`
  /// (if any).
  result<std::string> name() const;

  /// Return the size of the type or an error if it can't be computed.
  ///
  /// This size should match the equivalent of `sizeof(Type)`.
  result<uint64_t> size() const;

  /// Return the debug location where this type is defined.
  debug_location_t location() const;

  /// Return the scope in which this type is defined
  std::unique_ptr<Scope> scope() const;

  template<class T>
  const T* as() const {
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  static std::unique_ptr<Type> create(std::unique_ptr<details::Type> impl);

  protected:
  Type(std::unique_ptr<details::Type> impl);
  Type(details::Type& impl);

  LIEF::details::canbe_unique<details::Type> impl_;
};

}
}
#endif
