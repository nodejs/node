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
#ifndef LIEF_OAT_CLASS_H
#define LIEF_OAT_CLASS_H

#include "LIEF/iterators.hpp"
#include "LIEF/OAT/enums.hpp"
#include "LIEF/DEX/deopt.hpp"

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"

#include <string>

namespace LIEF {
namespace DEX {
class Class;
}
namespace OAT {
class Parser;
class Method;

class LIEF_API Class : public Object {
  friend class Parser;

  public:
  using methods_t = std::vector<Method*>;
  using it_methods = ref_iterator<methods_t&>;
  using it_const_methods = const_ref_iterator<const methods_t&>;

  public:
  Class();

  Class(OAT_CLASS_STATUS status, OAT_CLASS_TYPES type,
        DEX::Class* dex_class, std::vector<uint32_t> bitmap = {});

  Class(const Class&);
  Class& operator=(const Class&);

  bool has_dex_class() const;
  const DEX::Class* dex_class() const;
  DEX::Class* dex_class();

  OAT_CLASS_STATUS status() const;
  OAT_CLASS_TYPES type() const;

  const std::string& fullname() const;
  size_t index() const;

  it_methods methods();
  it_const_methods methods() const;

  const std::vector<uint32_t>& bitmap() const;

  bool is_quickened(const DEX::Method& m) const;
  bool is_quickened(uint32_t relative_index) const;

  uint32_t method_offsets_index(const DEX::Method& m) const;
  uint32_t method_offsets_index(uint32_t relative_index) const;

  uint32_t relative_index(const DEX::Method& m) const;
  uint32_t relative_index(uint32_t method_absolute_index) const;

  DEX::dex2dex_class_info_t dex2dex_info() const;

  void accept(Visitor& visitor) const override;


  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Class& cls);

  ~Class() override;

  private:

  DEX::Class* dex_class_ = nullptr;

  OAT_CLASS_STATUS status_ = OAT_CLASS_STATUS::STATUS_NOTREADY;
  OAT_CLASS_TYPES  type_   = OAT_CLASS_TYPES::OAT_CLASS_NONE_COMPILED;

  std::vector<uint32_t> method_bitmap_;
  methods_t methods_;

};

} // Namespace OAT
} // Namespace LIEF
#endif
