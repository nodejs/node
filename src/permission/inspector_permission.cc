#include "inspector_permission.h"

#include <string>

namespace node {

namespace permission {

// Currently, Inspector manage a single state
// Once denied, it's always denied
void InspectorPermission::Apply(Environment* env,
                                std::span<const std::string> allow,
                                PermissionScope scope) {
  deny_all_ = true;
}

void InspectorPermission::Drop(Environment* env,
                               PermissionScope scope,
                               std::string_view param) {
  deny_all_ = true;
}

bool InspectorPermission::is_granted(Environment* env,
                                     PermissionScope perm,
                                     std::string_view param) const {
  return deny_all_ == false;
}

}  // namespace permission
}  // namespace node
