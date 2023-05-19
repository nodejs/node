#ifndef SRC_PERMISSION_ENV_PERMISSION_H_
#define SRC_PERMISSION_ENV_PERMISSION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory>
#include <string>
#include "permission/permission_util.h"

namespace node {

namespace permission {

class EnvPermission final : public PermissionBase {
 public:
  class Patterns final : public SearchTree {
   public:
    bool Insert(const std::string& s) override;
    bool Lookup(const std::string_view& s,
                bool when_empty_return = false) override;
  };

  EnvPermission()
      : denied_patterns_(std::make_shared<Patterns>()),
        granted_patterns_(std::make_shared<Patterns>()) {}

  void Apply(const std::string& allow, PermissionScope scope) override;
  bool is_granted(PermissionScope scope,
                  const std::string_view& param = "") override;

 private:
  bool IsGranted(const std::string_view& param);
  bool Apply(const std::shared_ptr<Patterns>& patterns,
             const std::string_view& pattern);

  std::shared_ptr<Patterns> denied_patterns_;
  std::shared_ptr<Patterns> granted_patterns_;
};

}  // namespace permission

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_PERMISSION_ENV_PERMISSION_H_
