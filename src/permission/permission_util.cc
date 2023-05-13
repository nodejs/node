#include "permission_util.h"

namespace node {

namespace permission {

bool SearchTree::Insert(const std::string& s) {
  tree_.Insert(s);
  return true;
}

bool SearchTree::Lookup(const std::string_view& s, bool when_empty_return) {
  FSPermission::RadixTree::Node* current_node = tree_.GetRoot();

  if (current_node->children.size() == 0) {
    return when_empty_return;
  }

  // Check if the root node has a wildcard.
  auto it = current_node->children.find('*');
  if (it != current_node->children.end()) {
    return true;
  }

  unsigned int parent_node_prefix_len = current_node->prefix.length();
  const std::string path(s);
  auto path_len = path.length();

  while (true) {
    if (parent_node_prefix_len == path_len && current_node->IsEndNode()) {
      return true;
    }

    auto node = NextNode(current_node, path, parent_node_prefix_len);
    if (node == nullptr) {
      return false;
    }

    current_node = node;
    parent_node_prefix_len += current_node->prefix.length();
    if (current_node->wildcard_child != nullptr &&
        path_len >= parent_node_prefix_len - 1 /* -1: * */) {
      return true;
    }
  }
  return false;
}

FSPermission::RadixTree::Node* SearchTree::NextNode(
    const FSPermission::RadixTree::Node* node,
    const std::string& path,
    unsigned int idx) {
  if (idx >= path.length()) {
    return nullptr;
  }

  auto it = node->children.find(path[idx]);
  if (it == node->children.end()) {
    return nullptr;
  }
  auto child = it->second;
  // match prefix
  unsigned int prefix_len = child->prefix.length();
  for (unsigned int i = 0; i < path.length(); ++i) {
    if (i >= prefix_len || child->prefix[i] == '*') {
      return child;
    }

    if (path[idx++] != child->prefix[i]) {
      return nullptr;
    }
  }
  return child;
}

}  // namespace permission

}  // namespace node
