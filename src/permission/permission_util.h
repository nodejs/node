#ifndef SRC_PERMISSION_PERMISSION_UTIL_H_
#define SRC_PERMISSION_PERMISSION_UTIL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <string>
#include "permission/fs_permission.h"

namespace node {

namespace permission {

class SearchTree {
 public:
  virtual bool Insert(const std::string& s);
  virtual bool Lookup(const std::string_view& s,
                      bool when_empty_return = false);
  bool Empty() { return tree_.Empty(); }

 private:
  // This class delegates most of its feature to FSPermission::RadixTree. The
  // modified parts are as follows, while the rest of the implementations is
  // from RadixTree:
  //  1. Removed handling the path string in Lookup and NextNode.
  //  2. Added checking if the root node has a wildcard in Lookup.
  FSPermission::RadixTree::Node* NextNode(
      const FSPermission::RadixTree::Node* node,
      const std::string& path,
      unsigned int idx);
  FSPermission::RadixTree::Node* GetRoot() { return tree_.GetRoot(); }

  // TODO(daeyeon): modify the file system specific parts in RadixTree to make
  // it usable for both FsPermission and EnvPermission.
  FSPermission::RadixTree tree_;
};

}  // namespace permission

}  // namespace node
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_PERMISSION_PERMISSION_UTIL_H_
