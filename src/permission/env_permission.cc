#include "permission/env_permission.h"
#include "util-inl.h"

namespace node {

namespace permission {

void EnvPermission::Apply(const std::string& allow, PermissionScope scope) {
  if (scope == PermissionScope::kEnvironment) {
    if (allow.empty()) {
      allow_all_ = true;
      return;
    }
    for (const auto& token : SplitString(allow, ',')) {
#ifdef _WIN32
      allowed_.insert(ToUpper(token));
#else
      allowed_.insert(token);
#endif
    }
  }
}

bool EnvPermission::is_granted(PermissionScope scope,
                               const std::string_view& param) {
  if (scope == PermissionScope::kEnvironment) {
    const std::string token = std::string(param);
#ifdef _WIN32
    if (allow_all_ || allowed_.find(ToUpper(token)) != allowed_.end())
      return true;
#else
    if (allow_all_ || allowed_.find(token) != allowed_.end()) return true;
#endif
  }
  return false;
}

}  // namespace permission
}  // namespace node
