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
#ifndef LIEF_PE_RESOURCE_DATA_H
#define LIEF_PE_RESOURCE_DATA_H

#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/PE/ResourceNode.hpp"
#include "LIEF/span.hpp"
#include "LIEF/iostream.hpp"

namespace LIEF {
namespace PE {

class Parser;
class Builder;

/// Class which represents a Data Node in the PE resources tree
class LIEF_API ResourceData : public ResourceNode {
  friend class Parser;
  friend class Builder;

  public:
  ResourceData() :
    ResourceNode(ResourceNode::TYPE::DATA)
  {}
  ResourceData(std::vector<uint8_t> content, uint32_t code_page = 0) :
    ResourceNode(ResourceNode::TYPE::DATA),
    content_(std::move(content)),
    code_page_(code_page)
  {}

  ResourceData(const std::string& str, uint32_t code_page = 0) :
    ResourceNode(ResourceNode::TYPE::DATA),
    content_{str.begin(), str.end()},
    code_page_(code_page)
  {}

  ResourceData(const ResourceData& other) = default;
  ResourceData& operator=(const ResourceData& other) = default;
  void swap(ResourceData& other) noexcept;

  ~ResourceData() override = default;

  std::unique_ptr<ResourceNode> clone() const override {
    return std::unique_ptr<ResourceData>{new ResourceData{*this}};
  }

  /// Return the code page that is used to decode code point values within the
  /// resource data. Typically, the code page is the unicode code page.
  uint32_t code_page() const {
    return code_page_;
  }

  /// Resource content
  span<const uint8_t> content() const {
    return content_;
  }

  span<uint8_t> content() {
    return content_;
  }

  /// Reserved value. Should be `0`
  uint32_t reserved() const {
    return reserved_;
  }

  /// Offset of the content within the resource
  ///
  /// @warning This value may change when rebuilding resource table
  uint32_t offset() const {
    return offset_;
  }

  void code_page(uint32_t code_page) {
    code_page_ = code_page;
  }

  void content(std::vector<uint8_t> content) {
    content_ = std::move(content);
  }

  void content(const std::string& string) {
    content_ = {string.begin(), string.end()};
  }

  void content(const uint8_t* buffer, size_t size) {
    content_ = {buffer, buffer + size};
  }

  void reserved(uint32_t value) {
    reserved_ = value;
  }

  /// \private
  LIEF_LOCAL vector_iostream edit() {
    return vector_iostream(content_);
  }

  /// \private
  LIEF_LOCAL const std::vector<uint8_t> raw_content() const {
    return content_;
  }

  /// \private
  LIEF_LOCAL void set_offset(uint32_t offset) {
    offset_ = offset;
  }

  static bool classof(const ResourceNode* node) {
    return node->is_data();
  }

  void accept(Visitor& visitor) const override;

  private:
  std::vector<uint8_t> content_;
  uint32_t code_page_ = 0;
  uint32_t reserved_ = 0;
  uint32_t offset_ = 0;
};

} // namespace PE
} // namepsace LIEF
#endif /* RESOURCEDATA_H */
