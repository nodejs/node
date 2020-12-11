#ifndef SRC_POLICY_POLICY_H_
#define SRC_POLICY_POLICY_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_options.h"
#include "v8.h"

#include <bitset>
#include <deque>
#include <string>

namespace node {

class Environment;

namespace policy {

// Special Permissions are denied by default if any other permission is denied.
#define SPECIAL_PERMISSIONS(V)                                                 \
  V(SpecialInspector, "inspector", PermissionsRoot)                            \
  V(SpecialAddons, "addons", PermissionsRoot)                                  \
  V(SpecialChildProcess, "child_process", PermissionsRoot)

#define FILESYSTEM_PERMISSIONS(V)                                              \
  V(FileSystem, "fs", PermissionsRoot)                                         \
  V(FileSystemIn, "fs.in", FileSystem)                                         \
  V(FileSystemOut, "fs.out", FileSystem)

#define NETWORKING_PERMISSIONS(V)                                              \
  V(Net, "net", PermissionsRoot)                                               \
  V(NetIn, "net.in", Net)                                                      \
  V(NetOut, "net.out", Net)

#define EXPERIMENTAL_PERMISSIONS(V)                                            \
  V(Experimental, "wasi", PermissionsRoot)                                     \

#define PERMISSIONS(V)                                                         \
  EXPERIMENTAL_PERMISSIONS(V)                                                  \
  FILESYSTEM_PERMISSIONS(V)                                                    \
  NETWORKING_PERMISSIONS(V)                                                    \
  SPECIAL_PERMISSIONS(V)                                                       \
  V(Process, "process", PermissionsRoot)                                       \
  V(Signal, "signal", PermissionsRoot)                                         \
  V(Timing, "timing", PermissionsRoot)                                         \
  V(Env, "env", PermissionsRoot)                                               \
  V(Workers, "workers", PermissionsRoot)                                       \
  V(Policy, "policy", PermissionsRoot)

#define V(name, _, __) k##name,
enum class Permission {
  kPermissionsRoot = -1,
  PERMISSIONS(V)
  kPermissionsCount
};
#undef V

using PermissionSet =
    std::bitset<static_cast<size_t>(Permission::kPermissionsCount)>;

void RunInPrivilegedScope(
    const v8::FunctionCallbackInfo<v8::Value>& args);

class Policy final {
 public:
  static bool is_granted(Environment* env, Permission permission);
  static bool is_granted(Environment* env, std::string permission);
  static bool is_granted(Environment* env, const PermissionSet& permissions);

  static Permission PermissionFromName(const std::string& name);

  static v8::Maybe<PermissionSet> Parse(const std::string& list);

  Policy() = default;

  Policy(Policy&& other) = delete;
  Policy& operator=(Policy&& other) = delete;
  Policy(const Policy& other) = delete;
  Policy& operator=(const Policy& other) = delete;

  // Returns true after setting the permissions. If Nothing<bool>
  // is returned, the permissions could not be parsed successfully.
  v8::Maybe<bool> Apply(
      const std::string& deny,
      const std::string& grant = std::string());

  inline bool is_granted(Permission permission) const {
    return LIKELY(permission != Permission::kPermissionsCount) &&
        LIKELY(permission != Permission::kPermissionsRoot) &&
        !test(permission);
  }

  inline bool is_granted(std::string permission) const {
    return is_granted(PermissionFromName(permission));
  }

  inline bool is_granted(const PermissionSet& set) const {
    PermissionSet check = permissions_;
    check &= set;
    return check.none();
  }

  // Once a policy is locked, no additional grants will be permitted.
  inline bool is_locked() const { return locked_; }

 private:
  // Returns true after setting the permissions. If Nothing<bool>
  // is returned, the permissions could not be
  void Apply(const PermissionSet& deny, const PermissionSet& grant);

  inline bool test(Permission permission) const {
    return UNLIKELY(permissions_.test(static_cast<size_t>(permission)));
  }

  bool locked_ = false;
  PermissionSet permissions_;
};

// When code is running with the privileged scope, policy
// permission checking should be disabled.
struct PrivilegedScope {
  Environment* env;
  explicit PrivilegedScope(Environment* env_);
  ~PrivilegedScope();
};

}  // namespace policy

namespace per_process {
extern policy::Policy root_policy;
}  // namespace per_process

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_POLICY_POLICY_H_
