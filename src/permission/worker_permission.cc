#include "permission/worker_permission.h"

#include <string>
#include <vector>

namespace node {

namespace permission {

// Currently, PolicyDenyWorker manage a single state
// Once denied, it's always denied
void WorkerPermission::Apply(const std::string& deny, PermissionScope scope) {
  deny_all_ = true;
}

bool WorkerPermission::is_granted(PermissionScope perm,
                                  const std::string_view& param) {
  return deny_all_ == false;
}

}  // namespace permission
}  // namespace node
