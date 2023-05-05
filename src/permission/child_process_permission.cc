#include "child_process_permission.h"

#include <string>
#include <vector>

namespace node {

namespace permission {

// Currently, ChildProcess manage a single state
// Once denied, it's always denied
void ChildProcessPermission::Apply(const std::string& allow,
                                   PermissionScope scope) {
  is_all_allowed_ = true;
}

bool ChildProcessPermission::is_granted(PermissionScope perm,
                                        const std::string_view& param) {
  return is_all_allowed_;
}

}  // namespace permission
}  // namespace node
