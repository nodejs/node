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
#ifndef LIEF_DEX_CLASS_H
#define LIEF_DEX_CLASS_H

#include <climits>
#include <string>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"
#include "LIEF/iterators.hpp"

#include "LIEF/DEX/enums.hpp"
#include "LIEF/DEX/deopt.hpp"

namespace LIEF {
namespace DEX {
class Parser;
class Field;
class Method;

/// Class which represents a DEX Class (i.e. a Java/Kotlin class)
class LIEF_API Class : public Object {
  friend class Parser;

  public:
  using access_flags_list_t = std::vector<ACCESS_FLAGS>;

  using methods_t = std::vector<Method*>;
  using it_methods = ref_iterator<methods_t&>;
  using it_const_methods = const_ref_iterator<const methods_t&>;

  using fields_t = std::vector<Field*>;
  using it_fields = ref_iterator<fields_t&>;
  using it_const_fields = const_ref_iterator<const fields_t&>;

  using it_named_methods = filter_iterator<methods_t&>;
  using it_const_named_methods = const_filter_iterator<const methods_t&>;

  using it_named_fields = filter_iterator<fields_t&>;
  using it_const_named_fields = const_filter_iterator<const fields_t&>;

  public:
  static std::string package_normalized(const std::string& pkg_name);
  static std::string fullname_normalized(const std::string& pkg_cls);
  static std::string fullname_normalized(const std::string& pkg, const std::string& cls_name);

  Class();
  Class(const Class&) = delete;
  Class& operator=(const Class&) = delete;

  Class(std::string fullname, uint32_t access_flags = ACCESS_FLAGS::ACC_UNKNOWN,
        Class* parent = nullptr, std::string source_filename = "");

  /// Mangled class name (e.g. ``Lcom/example/android/MyActivity;``)
  const std::string& fullname() const;

  /// Package Name
  std::string package_name() const;

  /// Class name
  std::string name() const;

  /// Demangled class name
  std::string pretty_name() const;

  /// Check if the class has the given access flag
  bool has(ACCESS_FLAGS f) const;

  /// Access flags used by this class
  access_flags_list_t access_flags() const;

  /// Filename associated with this class (if any)
  const std::string& source_filename() const;

  /// True if the current class extends another one
  bool has_parent() const;

  /// Parent class
  const Class* parent() const;
  Class* parent();

  /// Methods implemented in this class
  it_const_methods methods() const;
  it_methods methods();

  /// Return Methods having the given name
  it_named_methods methods(const std::string& name);
  it_const_named_methods methods(const std::string& name) const;

  /// Fields implemented in this class
  it_const_fields fields() const;
  it_fields fields();

  /// Return Fields having the given name
  it_named_fields fields(const std::string& name);
  it_const_named_fields fields(const std::string& name) const;

  /// De-optimize information
  dex2dex_class_info_t dex2dex_info() const;

  /// Original index in the DEX class pool
  size_t index() const;

  void accept(Visitor& visitor) const override;


  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Class& cls);

  ~Class() override;

  private:
  std::string fullname_;
  uint32_t    access_flags_ = ACCESS_FLAGS::ACC_UNKNOWN;
  Class*      parent_ = nullptr;
  methods_t   methods_;
  fields_t    fields_;
  std::string source_filename_;

  uint32_t original_index_ = UINT_MAX;
};

} // Namespace DEX
} // Namespace LIEF
#endif
