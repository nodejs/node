#include "child_process_permission.h"

#include <string>
#include <vector>

namespace node {

namespace permission {

// Currently, ChildProcess manage a single state
// Once denied, it's always denied
void ChildProcessPermission::Apply(Environment* env,
                                   std::span<const std::string> allow,
                                   PermissionScope scope) {
  deny_all_ = true;
}

void ChildProcessPermission::Drop(Environment* env,
                                  PermissionScope scope,
                                  std::string_view param) {
  deny_all_ = true;
}

bool ChildProcessPermission::is_granted(Environment* env,
                                        PermissionScope perm,
                                        std::string_view param) const {
  return deny_all_ == false;
}

}  // namespace permission
}  // namespace node
