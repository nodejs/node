#include "child_process_permission.h"

#include <string>
#include <vector>

namespace node {

namespace permission {

// Currently, ChildProcess manage a single state
// Once denied, it's always denied
void ChildProcessPermission::Apply(const std::string& deny,
                                   PermissionScope scope) {
  deny_all_ = true;
}

bool ChildProcessPermission::is_granted(PermissionScope perm,
                                        const std::string_view& param) {
  return deny_all_ == false;
}

}  // namespace permission
}  // namespace node
