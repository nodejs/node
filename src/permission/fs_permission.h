#ifndef SRC_PERMISSION_FS_PERMISSION_H_
#define SRC_PERMISSION_FS_PERMISSION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <vector>
#include "permission/permission_base.h"
#include "util.h"

namespace node::permission {

class FSPermission final : public PermissionBase {
 public:
  void Apply(Environment* env,
             std::span<const std::string> allow,
             PermissionScope scope) override;
  void Drop(Environment* env,
            PermissionScope scope,
            std::string_view param) override;
  bool is_granted(Environment* env,
                  PermissionScope perm,
                  std::string_view param) const override;

  struct RadixTree {
    struct Node {
      std::string prefix;
      std::vector<std::pair<char, Node*>> children;
      Node* wildcard_child = nullptr;
      bool is_leaf = false;

      explicit Node(std::string_view pre) : prefix(pre) {}

      Node() = default;

      Node* FindChild(char label) const {
        for (const auto& [c, node] : children) {
          if (c == label) return node;
        }
        return nullptr;
      }

      void SetChild(char label, Node* node) {
        for (auto& [c, n] : children) {
          if (c == label) {
            n = node;
            return;
          }
        }
        children.emplace_back(label, node);
      }

      Node* CreateChild(std::string_view path_prefix) {
        if (path_prefix.empty() && !is_leaf) {
          is_leaf = true;
          return this;
        }

        CHECK(!path_prefix.empty());
        char label = path_prefix[0];

        Node* child = FindChild(label);
        if (child == nullptr) {
          child = new Node(path_prefix);
          children.emplace_back(label, child);
          return child;
        }
        bool child_was_end_node = child->IsEndNode();

        // swap prefix
        size_t i = 0;
        size_t prefix_len = path_prefix.length();
        for (; i < child->prefix.length(); ++i) {
          if (i >= prefix_len || path_prefix[i] != child->prefix[i]) {
            std::string parent_prefix(child->prefix.substr(0, i));
            std::string child_prefix(child->prefix.substr(i));

            child->prefix = child_prefix;
            Node* split_child = new Node(parent_prefix);
            split_child->children.emplace_back(child_prefix[0], child);
            SetChild(parent_prefix[0], split_child);

            return split_child->CreateChild(path_prefix.substr(i));
          }
        }
        if (child_was_end_node) {
          child->is_leaf = true;
        }
        return child->CreateChild(path_prefix.substr(i));
      }

      Node* CreateWildcardChild() {
        if (wildcard_child != nullptr) {
          return wildcard_child;
        }
        wildcard_child = new Node();
        return wildcard_child;
      }

      Node* NextNode(std::string_view path, size_t idx) const {
        if (idx >= path.length()) {
          return nullptr;
        }

        // wildcard node takes precedence
        if (children.size() > 1) {
          Node* wc = FindChild('*');
          if (wc != nullptr) {
            return wc;
          }
        }

        Node* child = FindChild(path[idx]);
        if (child == nullptr) {
          return nullptr;
        }
        // match prefix
        size_t prefix_len = child->prefix.length();
        for (size_t i = 0; i < path.length(); ++i) {
          if (i >= prefix_len || child->prefix[i] == '*') {
            return child;
          }

          // Handle optional trailing
          // path = /home/subdirectory
          // child = subdirectory/*
          if (idx >= path.length()) {
            if (child->prefix[i] == node::kPathSeparator) {
              continue;
            }
            // Path is exhausted but prefix expects more characters
            return nullptr;
          }

          if (path[idx++] != child->prefix[i]) {
            return nullptr;
          }
        }
        return child;
      }

      // A node can be a *end* node and have children
      // E.g: */slower*, */slown* are inserted:
      // /slow
      // ---> er
      // ---> n
      // If */slow* is inserted right after, it will create an
      // empty node
      // /slow
      // ---> '\000' ASCII (0) || \0
      // ---> er
      // ---> n
      bool IsEndNode() const {
        if (children.empty()) {
          return true;
        }
        return is_leaf;
      }
    };

    RadixTree();
    ~RadixTree();
    void Insert(const std::string& s);
    void Clear();
    bool Lookup(std::string_view s) const { return Lookup(s, false); }
    bool Lookup(std::string_view s, bool when_empty_return) const;

   private:
    Node* root_node_;
  };

 private:
  void GrantAccess(PermissionScope scope, const std::string& param);
  void RevokeAccess(PermissionScope scope, const std::string& param);
  void RebuildTree(PermissionScope scope);
  // fs granted on startup
  RadixTree granted_in_fs_;
  RadixTree granted_out_fs_;

  std::vector<std::string> granted_paths_in_;
  std::vector<std::string> granted_paths_out_;

  bool deny_all_in_ = true;
  bool deny_all_out_ = true;

  bool allow_all_in_ = false;
  bool allow_all_out_ = false;
};

}  // namespace node::permission

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_PERMISSION_FS_PERMISSION_H_
