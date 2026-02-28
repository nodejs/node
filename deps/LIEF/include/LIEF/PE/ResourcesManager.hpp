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
#ifndef LIEF_PE_RESOURCES_MANAGER_H
#define LIEF_PE_RESOURCES_MANAGER_H
#include <ostream>

#include "LIEF/errors.hpp"
#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"
#include "LIEF/iterators.hpp"

#include "LIEF/PE/resources/ResourceVersion.hpp"
#include "LIEF/PE/resources/ResourceIcon.hpp"
#include "LIEF/PE/resources/ResourceDialog.hpp"
#include "LIEF/PE/resources/ResourceStringTable.hpp"
#include "LIEF/PE/resources/ResourceAccelerator.hpp"

namespace LIEF {
class VectorStream;

namespace PE {
class ResourceNode;

/// The Resource Manager provides an enhanced API to manipulate the resource tree.
class LIEF_API ResourcesManager : public Object {
  public:

  /// The different types of resources
  /// From https://learn.microsoft.com/en-us/windows/win32/menurc/resource-types
  enum class TYPE {
    CURSOR       = 1,
    BITMAP       = 2,
    ICON         = 3,
    MENU         = 4,
    DIALOG       = 5,
    STRING       = 6,
    FONTDIR      = 7,
    FONT         = 8,
    ACCELERATOR  = 9,
    RCDATA       = 10,
    MESSAGETABLE = 11,
    GROUP_CURSOR = 12,
    GROUP_ICON   = 14,
    VERSION      = 16,
    DLGINCLUDE   = 17,
    PLUGPLAY     = 19,
    VXD          = 20,
    ANICURSOR    = 21,
    ANIICON      = 22,
    HTML         = 23,
    MANIFEST     = 24
  };

  public:
  using dialogs_t = ResourceDialog::dialogs_t;
  using it_const_dialogs = const_ref_iterator<const dialogs_t&, const ResourceDialog*>;

  using icons_t = std::vector<ResourceIcon>;
  using it_const_icons = const_ref_iterator<icons_t>;

  using accelerators_t = std::vector<ResourceAccelerator>;
  using it_const_accelerators = const_ref_iterator<accelerators_t>;

  /// This structure represent an entry in the string table (`RT_STRING`)
  struct LIEF_API string_entry_t {
    std::u16string string;
    uint32_t id = 0;

    std::string string_u8() const;

    bool is_defined() const {
      return !string.empty();
    }

    operator bool() const {
      return is_defined();
    }

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const string_entry_t& str)
    {
      os << std::to_string(str.id) << ", " << str.string_u8();
      return os;
    }
  };

  using strings_table_t = std::vector<string_entry_t>;

  ResourcesManager() = delete;
  ResourcesManager(ResourceNode& rsrc) :
    resources_{&rsrc}
  {}

  ResourcesManager(const ResourcesManager& other) :
    Object(other),
    resources_(other.resources_)
    // Skip (on purpose) the `dialogs_` cache
  {}

  ResourcesManager& operator=(const ResourcesManager& other) {
    if (&other != this) {
      Object::operator=(other);
      resources_ = other.resources_;
      // Skip (on purpose) the `dialogs_` cache
    }
    return *this;
  }

  ResourcesManager(ResourcesManager&&) = default;
  ResourcesManager& operator=(ResourcesManager&&) = default;

  ~ResourcesManager() override = default;

  /// Return the ResourceNode associated with the given TYPE
  /// or a nullptr if not found;
  ResourceNode* get_node_type(TYPE type) {
    return const_cast<ResourceNode*>(
      static_cast<const ResourcesManager*>(this)->get_node_type(type));
  }
  const ResourceNode* get_node_type(TYPE type) const;

  /// List of TYPE present in the resources
  std::vector<TYPE> get_types() const;

  /// `true` if the resource has the given TYPE node
  bool has_type(TYPE type) const {
    return get_node_type(type) != nullptr;
  }

  /// `true` if resources contain the Manifest element
  bool has_manifest() const {
    return get_node_type(TYPE::MANIFEST) != nullptr;
  }

  /// Return the manifest as a std::string or an empty string if not found
  /// or corrupted.
  std::string manifest() const;

  /// Change or set the manifest. If the manifest node path does not exist,
  /// all required nodes are created.
  void manifest(const std::string& manifest);

  /// `true` if resources a LIEF::PE::ResourceVersion
  bool has_version() const {
    return get_node_type(TYPE::VERSION) != nullptr;
  }

  /// Return a list of verison info (`VS_VERSIONINFO`).
  std::vector<ResourceVersion> version() const;

  /// `true` if resources contain a LIEF::PE::ResourceIcon
  bool has_icons() const {
    return get_node_type(TYPE::ICON)       != nullptr &&
           get_node_type(TYPE::GROUP_ICON) != nullptr;
  }

  /// Return the list of the icons present in the resources
  it_const_icons icons() const;

  /// Add an icon to the resources
  void add_icon(const ResourceIcon& icon);

  void change_icon(const ResourceIcon& original, const ResourceIcon& newone);

  /// `true` if resources contain dialogs
  bool has_dialogs() const {
    return get_node_type(TYPE::DIALOG) != nullptr;
  }

  /// Return the list of the dialogs present in the resource
  it_const_dialogs dialogs() const;

  /// `true` if the resources contain a string table
  bool has_string_table() const {
    return get_node_type(TYPE::STRING) != nullptr;
  }

  /// Return the list of the strings embedded in the string table (`RT_STRING`)
  strings_table_t string_table() const;

  /// `true` if the resources contain html
  bool has_html() const {
    return get_node_type(TYPE::HTML) != nullptr;
  }

  /// Return the list of the html resources
  std::vector<std::string> html() const;

  /// `true` if the resources contain accelerator info
  bool has_accelerator() const {
    return get_node_type(TYPE::ACCELERATOR) != nullptr;
  }

  /// Return the list of the accelerator in the resource
  it_const_accelerators accelerator() const;

  /// Print the resource tree to the given depth
  std::string print(uint32_t depth = 0) const;

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const ResourcesManager& m);

  private:
  void print_tree(const ResourceNode& node, std::ostringstream& stream,
                  uint32_t current_depth, uint32_t max_depth,
                  const ResourceNode* parent = nullptr,
                  std::string header = "", bool is_last = false) const;
  ResourceNode* resources_ = nullptr;
  mutable dialogs_t dialogs_;
};

LIEF_API const char* to_string(ResourcesManager::TYPE type);

} // namespace PE
} // namespace LIEF

#endif
