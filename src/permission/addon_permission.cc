#include "addon_permission.h"

#include <string>

namespace node {

namespace permission {

// Currently, Addon manage a single state
// Once denied, it's always denied
void AddonPermission::Apply(Environment* env,
                            const std::vector<std::string>& allow,
                            PermissionScope scope) {
  deny_all_ = true;
}

bool AddonPermission::is_granted(Environment* env,
                                 PermissionScope perm,
                                 const std::string_view& param) const {
  return deny_all_ == false;
}

}  // namespace permission
}  // namespace node
