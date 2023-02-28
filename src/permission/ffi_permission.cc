#include "permission/ffi_permission.h"

#include <string>
#include <vector>

namespace node {

namespace permission {

// Currently, Ffi manage a single state
// Once denied, it's always denied
void FfiPermission::Apply(const std::vector<std::string>& allow,
                          PermissionScope scope) {
  deny_all_ = true;
}

bool FfiPermission::is_granted(PermissionScope perm,
                               const std::string_view& param) {
  return deny_all_ == false;
}

}  // namespace permission
}  // namespace node
