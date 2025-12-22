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
#ifndef LIEF_DEX_FILE_H
#define LIEF_DEX_FILE_H
#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"

#include "LIEF/DEX/Header.hpp"
#include "LIEF/DEX/MapList.hpp"
#include "LIEF/DEX/instructions.hpp"
#include "LIEF/DEX/deopt.hpp"
#include "LIEF/DEX/types.hpp"

namespace LIEF {
namespace DEX {
class Parser;
class Class;
class Method;
class Type;
class Prototype;
class Field;

/// Class that represents a DEX file
class LIEF_API File : public Object {
  friend class Parser;

  public:
  using classes_t = std::unordered_map<std::string, Class*>;
  using classes_list_t = std::vector<std::unique_ptr<Class>>;
  using it_classes = ref_iterator<classes_list_t&, Class*>;
  using it_const_classes = const_ref_iterator<const classes_list_t&, const Class*>;

  using methods_t = std::vector<std::unique_ptr<Method>>;
  using it_methods = ref_iterator<methods_t&, Method*>;
  using it_const_methods = const_ref_iterator<const methods_t&, const Method*>;

  using strings_t           = std::vector<std::unique_ptr<std::string>>;
  using it_strings          = ref_iterator<strings_t&, std::string*>;
  using it_const_strings    = const_ref_iterator<const strings_t&, const std::string*>;

  using types_t             = std::vector<std::unique_ptr<Type>>;
  using it_types            = ref_iterator<types_t&, Type*>;
  using it_const_types      = const_ref_iterator<const types_t&, const Type*>;

  using prototypes_t        = std::vector<std::unique_ptr<Prototype>>;
  using it_prototypes       = ref_iterator<prototypes_t&, Prototype*>;
  using it_const_prototypes = const_ref_iterator<const prototypes_t&, const Prototype*>;

  using fields_t            = std::vector<std::unique_ptr<Field>>;
  using it_fields           = ref_iterator<fields_t&, Field*>;
  using it_const_fields     = const_ref_iterator<const fields_t&, const Field*>;

  public:
  File& operator=(const File& copy) = delete;
  File(const File& copy)            = delete;

  /// Version of the current DEX file
  dex_version_t version() const;

  /// Name of this file
  const std::string& name() const;

  void name(const std::string& name);

  /// Location of this file
  const std::string& location() const;
  void location(const std::string& location);

  /// DEX header
  const Header& header() const;
  Header& header();

  /// **All** classes used in the DEX file
  it_const_classes classes() const;
  it_classes classes();

  /// Check if the given class name exists
  bool has_class(const std::string& class_name) const;

  /// Return the DEX::Class object associated with the given name
  const Class* get_class(const std::string& class_name) const;

  Class* get_class(const std::string& class_name);

  /// Return the DEX::Class object associated with the given index
  const Class* get_class(size_t index) const;

  Class* get_class(size_t index);

  /// De-optimize information
  dex2dex_info_t dex2dex_info() const;

  /// De-optimize information as JSON
  std::string dex2dex_json_info() const;

  /// Return an iterator over **all** the DEX::Method used in this DEX file
  it_const_methods methods() const;
  it_methods methods();

  /// Return an iterator over **all** the DEX::Field used in this DEX file
  it_const_fields fields() const;
  it_fields fields();

  /// String pool
  it_const_strings strings() const;
  it_strings strings();

  /// Type pool
  it_const_types types() const;
  it_types types();

  /// Prototype pool
  it_prototypes prototypes();
  it_const_prototypes prototypes() const;

  /// DEX Map
  const MapList& map() const;
  MapList& map();

  /// Extract the current dex file and deoptimize it
  std::string save(const std::string& path = "", bool deoptimize = true) const;

  std::vector<uint8_t> raw(bool deoptimize = true) const;

  void accept(Visitor& visitor) const override;


  ~File() override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const File& file);

  private:
  File();

  void add_class(std::unique_ptr<Class> cls);

  static void deoptimize_nop(uint8_t* inst_ptr, uint32_t value);
  static void deoptimize_return(uint8_t* inst_ptr, uint32_t value);
  static void deoptimize_invoke_virtual(uint8_t* inst_ptr, uint32_t value, OPCODES new_inst);
  static void deoptimize_instance_field_access(uint8_t* inst_ptr, uint32_t value, OPCODES new_inst);

  std::string name_;
  std::string location_;

  Header       header_;
  classes_t    classes_;
  methods_t    methods_;
  fields_t     fields_;
  strings_t    strings_;
  types_t      types_;
  prototypes_t prototypes_;
  MapList      map_;

  classes_list_t class_list_;
  std::vector<uint8_t> original_data_;
};

}
}

#endif
