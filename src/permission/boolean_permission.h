#ifndef SRC_PERMISSION_BOOLEAN_PERMISSION_H_
#define SRC_PERMISSION_BOOLEAN_PERMISSION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "permission/permission_base.h"

namespace node::permission {

// A simple boolean permission that either denies or allows access.
// Used for permission scopes that don't need per-resource granularity.
template <bool deny_only>
class BooleanPermission final : public PermissionBase {
 public:
  void Apply(Environment* env,
             std::span<const std::string> allow,
             PermissionScope scope) override {
    flag_ = true;
  }

  void Drop(Environment* env,
            PermissionScope scope,
            std::string_view param) override {
    flag_ = deny_only;
  }

  bool is_granted(Environment* env,
                  PermissionScope perm,
                  std::string_view param) const override {
    if constexpr (deny_only) {
      return !flag_;
    } else {
      return flag_;
    }
  }

 private:
  bool flag_ = false;
};

// Once denied, the permission cannot be re-granted.
using DenyOnlyPermission = BooleanPermission<true>;

// Apply grants access, Drop revokes.
using AllowRevokePermission = BooleanPermission<false>;

}  // namespace node::permission

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_PERMISSION_BOOLEAN_PERMISSION_H_
