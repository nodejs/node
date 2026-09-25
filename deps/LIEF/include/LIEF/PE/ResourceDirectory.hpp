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
#ifndef LIEF_PE_RESOURCE_DIRECTORY_H
#define LIEF_PE_RESOURCE_DIRECTORY_H

#include "LIEF/visibility.h"

#include "LIEF/PE/ResourceNode.hpp"

namespace LIEF {
namespace PE {

class Parser;
class Builder;

namespace details {
struct pe_resource_directory_table;
}

class LIEF_API ResourceDirectory : public ResourceNode {

  friend class Parser;
  friend class Builder;

  public:
  ResourceDirectory() :
    ResourceNode(ResourceNode::TYPE::DIRECTORY)
  {}

  ResourceDirectory(uint32_t id) :
    ResourceNode(ResourceNode::TYPE::DIRECTORY)
  {
    this->id(id);
  }

  ResourceDirectory(const details::pe_resource_directory_table& header);

  ResourceDirectory(const ResourceDirectory& other) = default;
  ResourceDirectory& operator=(const ResourceDirectory& other) = default;

  void swap(ResourceDirectory& other) noexcept;

  ~ResourceDirectory() override = default;

  std::unique_ptr<ResourceNode> clone() const override {
    return std::unique_ptr<ResourceDirectory>(new ResourceDirectory{*this});
  }

  /// Resource characteristics. This field is reserved for future use.
  /// It is currently set to zero.
  uint32_t characteristics() const {
    return characteristics_;
  }

  /// The time that the resource data was created by the
  /// resource compiler.
  uint32_t time_date_stamp() const {
    return timedatestamp_;
  }

  /// The major version number, set by the user.
  uint16_t major_version() const {
    return majorversion_;
  }

  /// The minor version number, set by the user.
  uint16_t minor_version() const {
    return minorversion_;
  }

  /// The number of directory entries immediately
  /// following the table that use strings to identify Type,
  /// Name, or Language entries (depending on the level of the table).
  uint16_t numberof_name_entries() const {
    return numberof_name_entries_;
  }

  /// The number of directory entries immediately
  /// following the Name entries that use numeric IDs for
  /// Type, Name, or Language entries.
  uint16_t numberof_id_entries() const {
    return numberof_id_entries_;
  }

  void characteristics(uint32_t characteristics) {
    characteristics_ = characteristics;
  }
  void time_date_stamp(uint32_t time_date_stamp) {
    timedatestamp_ = time_date_stamp;
  }
  void major_version(uint16_t major_version) {
    majorversion_ = major_version;
  }
  void minor_version(uint16_t minor_version) {
    minorversion_ = minor_version;
  }
  void numberof_name_entries(uint16_t numberof_name_entries) {
    numberof_name_entries_ = numberof_name_entries;
  }
  void numberof_id_entries(uint16_t numberof_id_entries) {
    numberof_id_entries_ = numberof_id_entries;
  }

  static bool classof(const ResourceNode* node) {
    return node->is_directory();
  }

  void accept(Visitor& visitor) const override;

  private:
  uint32_t characteristics_ = 0;
  uint32_t timedatestamp_ = 0;
  uint16_t majorversion_ = 0;
  uint16_t minorversion_ = 0;
  uint16_t numberof_name_entries_ = 0;
  uint16_t numberof_id_entries_ = 0;
};
}
}
#endif /* RESOURCEDIRECTORY_H */
