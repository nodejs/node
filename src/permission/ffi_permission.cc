#include "permission/ffi_permission.h"

#include <string>

namespace node {

namespace permission {

// Currently, FFIPermission manages a single global deny state for FFI.
void FFIPermission::Apply(Environment* env,
                          std::span<const std::string> allow,
                          PermissionScope scope) {
  deny_all_ = true;
}

void FFIPermission::Drop(Environment* env,
                         PermissionScope scope,
                         std::string_view param) {
  deny_all_ = true;
}

bool FFIPermission::is_granted(Environment* env,
                               PermissionScope perm,
                               std::string_view param) const {
  return perm != PermissionScope::kFFI || !deny_all_;
}

}  // namespace permission
}  // namespace node
