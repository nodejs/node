#include "net_permission.h"

#include <string>
#include <iostream>

namespace node {

namespace permission {

void NetPermission::Apply(Environment* env,
                         const std::vector<std::string>& allow,
                         PermissionScope scope) {
  allow_net_ = true;
}

// Check if network access is granted
bool NetPermission::is_granted(Environment* env,
                              PermissionScope perm,
                              const std::string_view& param) const {
  // For network permission, we simply return the allow_net_ flag
  // This is a simple implementation that either allows all network access or none
  return allow_net_;
}

}  // namespace permission
}  // namespace node
