#include "net_permission.h"

#include <iostream>
#include <string>

namespace node {

namespace permission {

void NetPermission::Apply(Environment* env,
                          std::span<const std::string> allow,
                          PermissionScope scope) {
  allow_net_ = true;
}

void NetPermission::Drop(Environment* env,
                         PermissionScope scope,
                         std::string_view param) {
  allow_net_ = false;
}

bool NetPermission::is_granted(Environment* env,
                               PermissionScope perm,
                               std::string_view param) const {
  return allow_net_;
}

}  // namespace permission
}  // namespace node
