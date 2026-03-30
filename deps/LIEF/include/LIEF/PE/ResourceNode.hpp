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
#ifndef LIEF_PE_RESOURCE_NODE_H
#define LIEF_PE_RESOURCE_NODE_H
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <cstdint>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/span.hpp"

namespace LIEF {
class BinaryStream;

namespace PE {
namespace details {
struct pe_resource_directory_table;
}

class Binary;

class ResourceDirectory;
class ResourceData;

class Parser;
class Builder;

/// Class which represents a Node in the resource tree.
class LIEF_API ResourceNode : public Object {

  friend class Parser;
  friend class Builder;

  public:
  using childs_t        = std::vector<std::unique_ptr<ResourceNode>>;
  using it_childs       = ref_iterator<childs_t&, ResourceNode*>;
  using it_const_childs = const_ref_iterator<const childs_t&, ResourceNode*>;

  /// Enum that identifies the type of a node in the resource tree
  enum class TYPE {
    UNKNOWN = 0,
    DATA,
    DIRECTORY,
  };

  ResourceNode(const ResourceNode& other);
  ResourceNode& operator=(const ResourceNode& other);

  ResourceNode(ResourceNode&& other) = default;
  ResourceNode& operator=(ResourceNode&& other) = default;

  void swap(ResourceNode& other);

  ~ResourceNode() override;

  /// Parse the resource tree from the provided BinaryStream stream and
  /// with the original RVA provided in the second parameter.
  ///
  /// The RVA value should be come from the DataDirectory::RVA associated with
  /// the resource tree.
  static std::unique_ptr<ResourceNode> parse(BinaryStream& stream, uint64_t rva);

  /// Parse the resource tree from the provided buffer referenced by a
  /// pointer and the size. The second parameter is the RVA where the
  /// resource is (or was) located.
  static std::unique_ptr<ResourceNode> parse(const uint8_t* buffer, size_t size,
                                             uint64_t rva);

  /// See doc from other parse functions
  static std::unique_ptr<ResourceNode> parse(const std::vector<uint8_t>& buffer,
                                             uint64_t rva)
  {
    return parse(buffer.data(), buffer.size(), rva);
  }

  /// See doc from other parse functions
  static std::unique_ptr<ResourceNode> parse(span<const uint8_t> buffer,
                                             uint64_t rva)
  {
    return parse(buffer.data(), buffer.size(), rva);
  }

  static std::unique_ptr<ResourceNode>
    parse(BinaryStream& stream, const Binary& bin);

  virtual std::unique_ptr<ResourceNode> clone() const = 0;

  /// Integer that identifies the Type, Name, or Language ID of the entry
  /// depending on its depth in the tree.
  uint32_t id() const {
    return id_;
  }

  /// Name of the entry (if any)
  const std::u16string& name() const {
    return name_;
  }

  /// UTF-8 representation of the name()
  std::string utf8_name() const;

  /// Iterator on node's children
  it_childs childs() {
    return childs_;
  }

  it_const_childs childs() const {
    return childs_;
  }

  /// `True` if the entry uses a name as ID
  bool has_name() const {
    return (bool)(id() & 0x80000000);
  }

  /// Current depth of the Node in the resource tree
  uint32_t depth() const {
    return depth_;
  }

  /// `True` if the current entry is a ResourceDirectory.
  ///
  /// It can be safely casted with:
  ///
  /// ```cpp
  /// const auto* dir_node = node.cast<ResourceDirectory>();
  /// ```
  bool is_directory() const {
    return type_ == TYPE::DIRECTORY;
  }

  /// `True` if the current entry is a ResourceData.
  ///
  /// It can be safely casted with:
  ///
  /// ```cpp
  /// const auto* data_node = node.cast<ResourceData>();
  /// ```
  bool is_data() const {
    return type_ == TYPE::DATA;
  }

  void id(uint32_t id) {
    id_ = id;
  }

  void name(const std::string& name);

  void name(std::u16string name) {
    name_ = std::move(name);
  }

  /// Add a new child to the current node, taking the ownership
  /// of the provided `unique_ptr`
  ResourceNode& add_child(std::unique_ptr<ResourceNode> child);

  /// Add a new child to the current node
  ResourceNode& add_child(const ResourceNode& child) {
    return add_child(child.clone());
  }

  /// Delete the node with the given `id`
  void delete_child(uint32_t id);

  /// Delete the given node from the node's children
  void delete_child(const ResourceNode& node);

  void accept(Visitor& visitor) const override;

  template<class T>
  const T* cast() const {
    static_assert(std::is_base_of<ResourceNode, T>::value, "Require inheritance relationship");
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  template<class T>
  T* cast() {
    return const_cast<T*>(static_cast<const ResourceNode*>(this)->cast<T>());
  }

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const ResourceNode& node);

  std::string to_string() const {
    std::ostringstream oss;
    oss << *this;
    return oss.str();
  }

  /// Access the children at the given index in a safety way.
  ///
  /// \warning For internal use only
  ///
  /// \private
  LIEF_LOCAL const ResourceNode& safe_get_at(size_t idx) const;

  /// \private
  LIEF_LOCAL ResourceNode& safe_get_at(size_t idx) {
    return const_cast<ResourceNode&>(static_cast<const ResourceNode*>(this)->safe_get_at(idx));
  }

  /// \private
  LIEF_LOCAL void set_depth(uint32_t depth) {
    depth_ = depth;
  }

  LIEF_LOCAL void push_child(std::unique_ptr<ResourceNode> node) {
    childs_.push_back(std::move(node));
  }

  LIEF_API friend bool operator==(const ResourceNode& LHS, const ResourceNode& RHS);

  LIEF_API friend bool operator!=(const ResourceNode& LHS, const ResourceNode& RHS) {
    return !(LHS == RHS);
  }

  protected:
  ResourceNode() = default;
  ResourceNode(TYPE type) :
    type_(type)
  {}

  std::unique_ptr<ResourceNode> parse_resource_node(
      const details::pe_resource_directory_table& directory_table,
      uint32_t base_offset, uint32_t current_offset, uint32_t depth = 0);

  childs_t::iterator insert_child(std::unique_ptr<ResourceNode> child);

  TYPE type_ = TYPE::UNKNOWN;
  uint32_t id_ = 0;
  std::u16string name_;
  childs_t childs_;
  uint32_t depth_ = 0;
};
}
}
#endif /* RESOURCENODE_H */
