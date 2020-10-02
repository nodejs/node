#ifndef SRC_POLICY_POLICY_INL_H_
#define SRC_POLICY_POLICY_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "policy/policy.h"
#include "env-inl.h"
#include "util-inl.h"
#include "node_errors.h"

#include <memory>

namespace node {
namespace policy {

Policy::Policy(EnvironmentOptions* options) {
  Apply(options->policy_deny, options->policy_grant);
}

Policy::Policy(
    Policy* basis,
    const std::string& deny,
    const std::string& grant) {
  permissions_ = basis->permissions_;
  Apply(deny, grant, ApplyFlags::kIgnoreSpecials);
}

Policy::Policy(
    Policy* basis,
    const std::vector<Permission>& deny,
    const std::vector<Permission>& grant) {
  permissions_ = basis->permissions_;
  Apply(deny, grant, ApplyFlags::kIgnoreSpecials);
}

bool Policy::test(Permission permission) const {
  return UNLIKELY(permissions_.test(static_cast<size_t>(permission)));
}

#define V(name, _, parent)                                                     \
  if (permission == Permission::k##parent)                                     \
    SetRecursively(Permission::k##name, value);
void Policy::SetRecursively(Permission permission, bool value) {
  if (permission != Permission::kPermissionsRoot)
    permissions_[static_cast<size_t>(permission)] = value;
  PERMISSIONS(V)
}
#undef V

void Policy::Grant(Permission permission) {
  if (LIKELY(locked_))
    return;
  if (UNLIKELY(permission == Permission::kPermissionsCount))
    return;
  SetRecursively(permission, false);
}

void Policy::Grant(std::string permission) {
  Grant(PermissionFromName(permission));
}

void Policy::Deny(Permission permission) {
  if (UNLIKELY(permission == Permission::kPermissionsCount))
    return;
  SetRecursively(permission, true);
}

void Policy::Deny(std::string permission) {
  Deny(PermissionFromName(permission));
}

bool Policy::is_granted(Permission permission) const {
  return LIKELY(permission != Permission::kPermissionsCount) &&
      LIKELY(permission != Permission::kPermissionsRoot) &&
      !test(permission);
}

bool Policy::is_granted(std::string permission) const {
  return is_granted(PermissionFromName(permission));
}

void Policy::Apply(
    const std::string& deny,
    const std::string& grant,
    ApplyFlags flags) {
  std::vector<Permission> denied = Policy::Parse(deny);
  std::vector<Permission> granted = Policy::Parse(grant);
  Apply(denied, granted);
}

void Policy::Apply(
    const std::vector<Permission>& deny,
    const std::vector<Permission>& grant,
    ApplyFlags flags) {

  // The entire special category is denied by default if there
  // are any other denials (and the flag is set);
  if (deny.size() > 0 && flags == ApplyFlags::kDenySpecials)
    Deny(Permission::kSpecial);

  for (Permission permission : deny)
    Deny(permission);

  for (Permission permission : grant)
    Grant(permission);
  Lock();
}

#define V(Name, label, _)                                                      \
  if (strcmp(name.c_str(), label) == 0) return Permission::k##Name;
Permission Policy::PermissionFromName(const std::string& name) {
  if (strcmp(name.c_str(), "*") == 0) return Permission::kPermissionsRoot;
  PERMISSIONS(V)
  return Permission::kPermissionsCount;
}
#undef V

std::vector<Permission> Policy::Parse(
    const std::string& list,
    ParseStatus* status) {
  std::vector<Permission> permissions;
  for (auto name : SplitString(list, ',')) {
    Permission permission = PermissionFromName(name);
    if (permission != Permission::kPermissionsCount)
      permissions.push_back(permission);
    else if (status != nullptr)
      *status = ParseStatus::UNKNOWN;
  }
  return permissions;
}

PrivilegedAccessContext::PrivilegedAccessContext(
    EnvironmentOptions* options) {
  std::unique_ptr<Policy> policy = std::make_unique<Policy>(options);
  Push(std::move(policy));
}

// Push a new Policy onto the stack. The new policy starts as a
// clone of the current Policy with the additionally given
// denials/grants applied.
void PrivilegedAccessContext::Push(
    const std::string& deny,
    const std::string& grant) {
  std::unique_ptr<Policy> policy =
      std::make_unique<Policy>(
          policy_stack_.front().get(),
          deny,
          grant);
  Push(std::move(policy));
}

// Push a new Policy onto the stack. The new policy starts as a
// clone of the current Policy with the additionally given
// denials/grants applied.
void PrivilegedAccessContext::Push(
    const std::vector<Permission>& deny,
    const std::vector<Permission>& grant) {
  std::unique_ptr<Policy> policy =
      std::make_unique<Policy>(
          policy_stack_.front().get(),
          deny,
          grant);
  Push(std::move(policy));
}

void PrivilegedAccessContext::Push(std::unique_ptr<Policy> policy) {
  policy_stack_.push_front(std::move(policy));
}

bool PrivilegedAccessContext::Pop() {
  // Can't remove the root Policy
  if (policy_stack_.size() == 1)
    return false;
  policy_stack_.pop_front();
  return true;
}

bool PrivilegedAccessContext::is_granted(Permission permission) {
  return policy_stack_.front()->is_granted(permission);
}

bool PrivilegedAccessContext::is_granted(const std::string& permission) {
  return policy_stack_.front()->is_granted(permission);
}

void PrivilegedAccessContext::Deny(Permission permission) {
  policy_stack_.front()->Deny(permission);
}

void PrivilegedAccessContext::Deny(const std::string& permission) {
  policy_stack_.front()->Deny(permission);
}

PrivilegedAccessContext::Scope::Scope(
    Environment* env,
    const std::string& deny,
    const std::string& grant)
    : env_(env) {
  env_->privileged_access_context()->Push(deny, grant);
}

PrivilegedAccessContext::Scope::Scope(
    Environment* env,
    const std::vector<Permission>& deny,
    const std::vector<Permission>& grant)
    : env_(env) {
  env_->privileged_access_context()->Push(deny, grant);
}

PrivilegedAccessContext::Scope::~Scope() {
  env_->privileged_access_context()->Pop();
}

}  // namespace policy
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_POLICY_POLICY_INL_H_
