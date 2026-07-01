#include "env_var_permission.h"

#include <string>
#include <vector>

namespace node {

namespace permission {

void EnvVarPermission::Apply(Environment* env,
                             const std::vector<std::string>& allow,
                             PermissionScope scope) {
  deny_all_ = true;
  if (!allow.empty()) {
    if (allow.size() == 1 && allow[0] == "*") {
      allow_all_ = true;
      return;
    }
    for (const std::string& arg : allow) {
      allowed_env_vars_.insert(arg);
    }
  }
}

bool EnvVarPermission::is_granted(Environment* env,
                                  PermissionScope perm,
                                  const std::string_view& param) const {
  if (deny_all_) {
    if (allow_all_) {
      return true;
    }
    if (param.empty()) {
      return false;
    }
    return allowed_env_vars_.find(std::string(param)) !=
           allowed_env_vars_.end();
  }
  return true;
}

}  // namespace permission
}  // namespace node
