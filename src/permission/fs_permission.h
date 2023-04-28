#ifndef SRC_PERMISSION_FS_PERMISSION_H_
#define SRC_PERMISSION_FS_PERMISSION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"

#include <unordered_map>
#include <vector>
#include "permission/permission_base.h"
#include "util.h"

namespace node {

namespace permission {

class FSPermission final : public PermissionBase {
 public:
  void Apply(const std::string& deny, PermissionScope scope) override;
  bool is_granted(PermissionScope perm, const std::string_view& param) override;

  // For debugging purposes, use the gist function to print the whole tree
  // https://gist.github.com/RafaelGSS/5b4f09c559a54f53f9b7c8c030744d19
  struct RadixTree {
    struct Node {
      std::string prefix;
      std::unordered_map<char, Node*> children;
      Node* wildcard_child;

      explicit Node(const std::string& pre)
          : prefix(pre), wildcard_child(nullptr) {}

      Node() : wildcard_child(nullptr) {}

      Node* CreateChild(std::string prefix) {
        char label = prefix[0];

        Node* child = children[label];
        if (child == nullptr) {
          children[label] = new Node(prefix);
          return children[label];
        }

        // swap prefix
        unsigned int i = 0;
        unsigned int prefix_len = prefix.length();
        for (; i < child->prefix.length(); ++i) {
          if (i > prefix_len || prefix[i] != child->prefix[i]) {
            std::string parent_prefix = child->prefix.substr(0, i);
            std::string child_prefix = child->prefix.substr(i);

            child->prefix = child_prefix;
            Node* split_child = new Node(parent_prefix);
            split_child->children[child_prefix[0]] = child;
            children[parent_prefix[0]] = split_child;

            return split_child->CreateChild(prefix.substr(i));
          }
        }
        return child->CreateChild(prefix.substr(i));
      }

      Node* CreateWildcardChild() {
        if (wildcard_child != nullptr) {
          return wildcard_child;
        }
        wildcard_child = new Node();
        return wildcard_child;
      }

      Node* NextNode(const std::string& path, unsigned int idx) {
        if (idx >= path.length()) {
          return nullptr;
        }

        auto it = children.find(path[idx]);
        if (it == children.end()) {
          return nullptr;
        }
        auto child = it->second;
        // match prefix
        unsigned int prefix_len = child->prefix.length();
        for (unsigned int i = 0; i < path.length(); ++i) {
          if (i >= prefix_len || child->prefix[i] == '*') {
            return child;
          }

          // Handle optional trailing
          // path = /home/subdirectory
          // child = subdirectory/*
          if (idx >= path.length() &&
              child->prefix[i] == node::kPathSeparator) {
            continue;
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
      bool IsEndNode() {
        if (children.size() == 0) {
          return true;
        }
        return children['\0'] != nullptr;
      }
    };

    RadixTree();
    ~RadixTree();
    void Insert(const std::string& s);
    bool Lookup(const std::string_view& s) { return Lookup(s, false); }
    bool Lookup(const std::string_view& s, bool when_empty_return);

   private:
    Node* root_node_;
  };

 private:
  void GrantAccess(PermissionScope scope, std::string param);
  void RestrictAccess(PermissionScope scope,
                      const std::vector<std::string>& params);
  // fs granted on startup
  RadixTree granted_in_fs_;
  RadixTree granted_out_fs_;

  bool deny_all_in_ = true;
  bool deny_all_out_ = true;

  bool allow_all_in_ = false;
  bool allow_all_out_ = false;
};

}  // namespace permission

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_PERMISSION_FS_PERMISSION_H_
