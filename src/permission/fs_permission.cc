#include "fs_permission.h"
#include "base_object-inl.h"
#include "util.h"
#include "v8.h"

#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::string WildcardIfDir(const std::string& res) noexcept {
  uv_fs_t req;
  int rc = uv_fs_stat(nullptr, &req, res.c_str(), nullptr);
  if (rc == 0) {
    const uv_stat_t* const s = static_cast<const uv_stat_t*>(req.ptr);
    if (s->st_mode & S_IFDIR) {
      // add wildcard when directory
      if (res.back() == node::kPathSeparator) {
        return res + "*";
      }
      return res + node::kPathSeparator + "*";
    }
  }
  uv_fs_req_cleanup(&req);
  return res;
}

void FreeRecursivelyNode(
    node::permission::FSPermission::RadixTree::Node* node) {
  if (node == nullptr) {
    return;
  }

  if (node->children.size()) {
    for (auto& c : node->children) {
      FreeRecursivelyNode(c.second);
    }
  }

  if (node->wildcard_child != nullptr) {
    delete node->wildcard_child;
  }
  delete node;
}

bool is_tree_granted(node::permission::FSPermission::RadixTree* granted_tree,
                     const std::string_view& param) {
#ifdef _WIN32
  // is UNC file path
  if (param.rfind("\\\\", 0) == 0) {
    // return lookup with normalized param
    int starting_pos = 4;  // "\\?\"
    if (param.rfind("\\\\?\\UNC\\") == 0) {
      starting_pos += 4;  // "UNC\"
    }
    auto normalized = param.substr(starting_pos);
    return granted_tree->Lookup(normalized, true);
  }
#endif
  return granted_tree->Lookup(param, true);
}

}  // namespace

namespace node {

namespace permission {

// allow = '*'
// allow = '/tmp/,/home/example.js'
void FSPermission::Apply(const std::string& allow, PermissionScope scope) {
  for (const auto& res : SplitString(allow, ',')) {
    if (res == "*") {
      if (scope == PermissionScope::kFileSystemRead) {
        deny_all_in_ = false;
        allow_all_in_ = true;
      } else {
        deny_all_out_ = false;
        allow_all_out_ = true;
      }
      return;
    }
    GrantAccess(scope, res);
  }
}

void FSPermission::GrantAccess(PermissionScope perm, std::string res) {
  const std::string path = WildcardIfDir(res);
  if (perm == PermissionScope::kFileSystemRead) {
    granted_in_fs_.Insert(path);
    deny_all_in_ = false;
  } else if (perm == PermissionScope::kFileSystemWrite) {
    granted_out_fs_.Insert(path);
    deny_all_out_ = false;
  }
}

bool FSPermission::is_granted(PermissionScope perm,
                              const std::string_view& param = "") {
  switch (perm) {
    case PermissionScope::kFileSystem:
      return allow_all_in_ && allow_all_out_;
    case PermissionScope::kFileSystemRead:
      return !deny_all_in_ &&
             ((param.empty() && allow_all_in_) || allow_all_in_ ||
              is_tree_granted(&granted_in_fs_, param));
    case PermissionScope::kFileSystemWrite:
      return !deny_all_out_ &&
             ((param.empty() && allow_all_out_) || allow_all_out_ ||
              is_tree_granted(&granted_out_fs_, param));
    default:
      return false;
  }
}

FSPermission::RadixTree::RadixTree() : root_node_(new Node("")) {}

FSPermission::RadixTree::~RadixTree() {
  FreeRecursivelyNode(root_node_);
}

bool FSPermission::RadixTree::Lookup(const std::string_view& s,
                                     bool when_empty_return = false) {
  FSPermission::RadixTree::Node* current_node = root_node_;
  if (current_node->children.size() == 0) {
    return when_empty_return;
  }

  unsigned int parent_node_prefix_len = current_node->prefix.length();
  const std::string path(s);
  auto path_len = path.length();

  while (true) {
    if (parent_node_prefix_len == path_len && current_node->IsEndNode()) {
      return true;
    }

    auto node = current_node->NextNode(path, parent_node_prefix_len);
    if (node == nullptr) {
      return false;
    }

    current_node = node;
    parent_node_prefix_len += current_node->prefix.length();
    if (current_node->wildcard_child != nullptr &&
        path_len >= (parent_node_prefix_len - 2 /* slash* */)) {
      return true;
    }
  }
}

void FSPermission::RadixTree::Insert(const std::string& path) {
  FSPermission::RadixTree::Node* current_node = root_node_;

  unsigned int parent_node_prefix_len = current_node->prefix.length();
  int path_len = path.length();

  for (int i = 1; i <= path_len; ++i) {
    bool is_wildcard_node = path[i - 1] == '*';
    bool is_last_char = i == path_len;

    if (is_wildcard_node || is_last_char) {
      std::string node_path = path.substr(parent_node_prefix_len, i);
      current_node = current_node->CreateChild(node_path);
    }

    if (is_wildcard_node) {
      current_node = current_node->CreateWildcardChild();
      parent_node_prefix_len = i;
    }
  }
}

}  // namespace permission
}  // namespace node
