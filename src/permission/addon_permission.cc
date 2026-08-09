#include "addon_permission.h"

#include <string>

namespace node {

namespace permission {

// Currently, Addon manage a single state
// Once denied, it's always denied
void AddonPermission::Apply(Environment* env,
                            std::span<const std::string> allow,
                            PermissionScope scope) {
  deny_all_ = true;
}

void AddonPermission::Drop(Environment* env,
                           PermissionScope scope,
                           std::string_view param) {
  deny_all_ = true;
}

bool AddonPermission::is_granted(Environment* env,
                                 PermissionScope perm,
                                 std::string_view param) const {
  return deny_all_ == false;
}

}  // namespace permission
}  // namespace node
