#ifndef SRC_PERMISSION_ENV_PERMISSION_H_
#define SRC_PERMISSION_ENV_PERMISSION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <string>
#include <unordered_set>
#include "permission/permission_base.h"

namespace node {

namespace permission {

class EnvPermission final : public PermissionBase {
 public:
  void Apply(const std::string& allow, PermissionScope scope) override;
  bool is_granted(PermissionScope scope,
                  const std::string_view& param = "") override;

 private:
  bool allow_all_ = false;
  std::unordered_set<std::string> allowed_;
};

}  // namespace permission

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_PERMISSION_ENV_PERMISSION_H_
