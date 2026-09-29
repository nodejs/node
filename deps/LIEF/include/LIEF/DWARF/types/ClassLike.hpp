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
#ifndef LIEF_DWARF_TYPE_STRUCTURE_H
#define LIEF_DWARF_TYPE_STRUCTURE_H

#include "LIEF/visibility.h"
#include "LIEF/compiler_attributes.hpp"
#include "LIEF/DWARF/Type.hpp"
#include "LIEF/DWARF/Function.hpp"

namespace LIEF {
namespace dwarf {
namespace types {

namespace details {
class Member;
}

/// This class abstracts a DWARF aggregate: `DW_TAG_structure_type`,
/// `DW_TAG_class_type`, `DW_TAG_union_type`.
class LIEF_API ClassLike : public Type {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<Type, Args&&...>::value>::type>
  ClassLike(Args&&... args) :
    Type(std::forward<Args>(args)...) {}

  /// This represents a class/struct/union attribute
  class LIEF_API Member {
    public:
    Member(std::unique_ptr<details::Member> impl);
    Member(Member&& other) noexcept;
    Member& operator=(Member&& other) noexcept;

    Member(const Member&) = delete;
    Member& operator=(const Member&) = delete;

    /// Name of the member
    std::string name() const;

    /// Offset of the current member in the struct/union/class
    ///
    /// If the offset can't be resolved it returns a lief_errors
    result<uint64_t> offset() const;

    /// Offset of the current member in **bits** in the struct/union/class
    ///
    /// This function differs from offset() for aggregates using bit-field
    /// declaration:
    ///
    /// ```cpp
    /// struct S {
    ///   int flag : 4;
    ///   int opt : 1
    /// };
    /// ```
    ///
    /// Usually, `offset() * 8 == bit_offset()`
    ///
    /// If the offset can't be resolved it returns a lief_errors
    result<uint64_t> bit_offset() const;

    /// If the current member is a bit-field, this function returns its size in
    /// bits.
    result<uint64_t> bit_size() const;

    /// Type of the current member
    std::unique_ptr<Type> type() const LIEF_LIFETIMEBOUND;

    bool is_external() const;

    bool is_declaration() const;

    ~Member();

    private:
    std::unique_ptr<details::Member> impl_;
  };

  ClassLike(const ClassLike&) = delete;
  ClassLike& operator=(const ClassLike&) = delete;

  ClassLike(ClassLike&&) noexcept = default;
  ClassLike& operator=(ClassLike&&) noexcept = default;

  using functions_it = iterator_range<Function::Iterator>;

  static bool classof(const Type* type) {
    const auto kind = type->kind();
    return kind == Type::KIND::CLASS || kind == Type::KIND::STRUCT ||
           kind == Type::KIND::UNION;
  }

  /// Return the list of all the attributes defined in this class-like type
  std::vector<Member> members() const LIEF_LIFETIMEBOUND;

  /// Try to find the attribute at the given offset
  std::unique_ptr<Member> find_member(uint64_t offset) const LIEF_LIFETIMEBOUND;

  /// Iterator over the functions defined by the class-like.
  functions_it functions() const LIEF_LIFETIMEBOUND;

  ~ClassLike() override;
};

/// This class represents a DWARF `struct` type (`DW_TAG_structure_type`)
class LIEF_API Structure : public ClassLike {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<ClassLike, Args&&...>::value>::type>
  Structure(Args&&... args) :
    ClassLike(std::forward<Args>(args)...) {}

  Structure(const Structure&) = delete;
  Structure& operator=(const Structure&) = delete;

  Structure(Structure&&) noexcept = default;
  Structure& operator=(Structure&&) noexcept = default;

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::STRUCT;
  }

  ~Structure() override;
};

/// This class represents a DWARF `class` type (`DW_TAG_class_type`)
class LIEF_API Class : public ClassLike {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<ClassLike, Args&&...>::value>::type>
  Class(Args&&... args) :
    ClassLike(std::forward<Args>(args)...) {}

  Class(const Class&) = delete;
  Class& operator=(const Class&) = delete;

  Class(Class&&) noexcept = default;
  Class& operator=(Class&&) noexcept = default;

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::CLASS;
  }

  ~Class() override;
};

/// This class represents a DWARF `union` type (`DW_TAG_union_type`)
class LIEF_API Union : public ClassLike {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<ClassLike, Args&&...>::value>::type>
  Union(Args&&... args) :
    ClassLike(std::forward<Args>(args)...) {}

  Union(const Union&) = delete;
  Union& operator=(const Union&) = delete;

  Union(Union&&) noexcept = default;
  Union& operator=(Union&&) noexcept = default;

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::UNION;
  }

  ~Union() override;
};

/// This class represents a DWARF `packed` type (`DW_TAG_packed_type`)
class LIEF_API Packed : public ClassLike {
  public:
  template<typename... Args,
           typename = typename std::
               enable_if<std::is_constructible<ClassLike, Args&&...>::value>::type>
  Packed(Args&&... args) :
    ClassLike(std::forward<Args>(args)...) {}

  Packed(const Packed&) = delete;
  Packed& operator=(const Packed&) = delete;

  Packed(Packed&&) noexcept = default;
  Packed& operator=(Packed&&) noexcept = default;

  static bool classof(const Type* type) {
    return type->kind() == Type::KIND::PACKED;
  }

  ~Packed() override;
};

}
}
}
#endif
