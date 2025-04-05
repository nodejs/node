#include "ffi_permission.h"

namespace node::permission {

// Currently, FFIPermission manage a single state
// Once denied, it's always denied
void FFIPermission::Apply(Environment* env,
                          const std::vector<std::string>& allow,
                          PermissionScope scope) {
  deny_all_ = true;
}

bool FFIPermission::is_granted(Environment* env,
                               PermissionScope perm,
                               const std::string_view& param) const {
  return deny_all_ == false;
}

}  // namespace node::permission
