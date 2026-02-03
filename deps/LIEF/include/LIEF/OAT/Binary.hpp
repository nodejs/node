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
#ifndef LIEF_OAT_BINARY_H
#define LIEF_OAT_BINARY_H
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/ELF/Binary.hpp"
#include "LIEF/OAT/Header.hpp"
#include "LIEF/DEX/deopt.hpp"

namespace LIEF {
namespace DEX {
class File;
}

namespace VDEX {
class File;
}

namespace OAT {
class Parser;
class Class;
class Method;
class DexFile;

class LIEF_API Binary : public ELF::Binary {
  friend class Parser;

  public:
  using dex_files_t        = std::vector<std::unique_ptr<DEX::File>>;
  using it_dex_files       = ref_iterator<dex_files_t&, DEX::File*>;
  using it_const_dex_files = const_ref_iterator<const dex_files_t&, const DEX::File*>;

  using classes_t         = std::unordered_map<std::string, Class*>;
  using classes_list_t    = std::vector<std::unique_ptr<Class>>;
  using it_classes        = ref_iterator<classes_list_t&, Class*>;
  using it_const_classes  = const_ref_iterator<const classes_list_t&, const Class*>;

  using oat_dex_files_t        = std::vector<std::unique_ptr<DexFile>>;
  using it_oat_dex_files       = ref_iterator<oat_dex_files_t&, DexFile*>;
  using it_const_oat_dex_files = const_ref_iterator<const oat_dex_files_t&, const DexFile*>;

  using methods_t         = std::vector<std::unique_ptr<Method>>;
  using it_methods        = ref_iterator<methods_t&, Method*>;
  using it_const_methods  = const_ref_iterator<const methods_t&, const Method*>;

  using dex2dex_info_t = std::unordered_map<const DEX::File*, DEX::dex2dex_info_t>;

  public:
  Binary& operator=(const Binary& copy) = delete;
  Binary(const Binary& copy)            = delete;

  /// OAT Header
  const Header& header() const;
  Header& header();

  /// Iterator over LIEF::DEX::File
  it_dex_files dex_files();
  it_const_dex_files dex_files() const;

  /// Iterator over LIEF::OAT::DexFile
  it_oat_dex_files       oat_dex_files();
  it_const_oat_dex_files oat_dex_files() const;

  /// Iterator over LIEF::OAT::Class
  it_const_classes classes() const;
  it_classes classes();

  /// Check if the current OAT has the given class
  bool has_class(const std::string& class_name) const;


  /// Return the LIEF::OAT::Class with the given name or
  /// a nullptr if the class can't be found
  const Class* get_class(const std::string& class_name) const;

  Class* get_class(const std::string& class_name);

  /// Return the LIEF::OAT::Class at the given index or a nullptr
  /// if it does not exist
  const Class* get_class(size_t index) const;

  Class* get_class(size_t index);

  /// Iterator over LIEF::OAT::Method
  it_const_methods methods() const;
  it_methods methods();

  dex2dex_info_t dex2dex_info() const;

  std::string dex2dex_json_info();

  bool has_vdex() const {
    return vdex_ != nullptr;
  }

  static bool classof(const LIEF::Binary* bin) {
    return bin->format() == Binary::FORMATS::OAT;
  }

  void accept(Visitor& visitor) const override;

  ~Binary() override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Binary& binary);

  private:
  Binary();
  void add_class(std::unique_ptr<Class> cls);

  Header header_;
  methods_t methods_;
  dex_files_t dex_files_;
  oat_dex_files_t oat_dex_files_;

  classes_t classes_;
  classes_list_t classes_list_;

  // For OAT > 79
  std::unique_ptr<VDEX::File> vdex_;
};

}
}

#endif
