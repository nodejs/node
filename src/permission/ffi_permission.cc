#include "permission/ffi_permission.h"

#include <string>
#include <vector>

namespace node {

namespace permission {

// Currently, FFIPermission manages a single global deny state for FFI.
void FFIPermission::Apply(Environment* env,
                          const std::vector<std::string>& allow,
                          PermissionScope scope) {
  deny_all_ = true;
}

bool FFIPermission::is_granted(Environment* env,
                               PermissionScope perm,
                               const std::string_view& param) const {
  return perm != PermissionScope::kFFI || !deny_all_;
}

}  // namespace permission
}  // namespace node
