#ifndef SRC_POLICY_POLICY_H_
#define SRC_POLICY_POLICY_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_options.h"
#include "v8.h"

#include <bitset>
#include <deque>
#include <string>
#include <vector>

namespace node {
namespace policy {

// Special Permissions are denied by default if any other permission is denied.
#define SPECIAL_PERMISSIONS(V)                                                 \
  V(Special, "special", PermissionsRoot)                                       \
  V(SpecialInspector, "special.inspector", Special)                            \
  V(SpecialAddons, "special.addons", Special)                                  \
  V(SpecialChildProcess, "special.child_process", Special)

#define FILESYSTEM_PERMISSIONS(V)                                              \
  V(FileSystem, "fs", PermissionsRoot)                                         \
  V(FileSystemIn, "fs.in", FileSystem)                                         \
  V(FileSystemOut, "fs.out", FileSystem)

#define NETWORKING_PERMISSIONS(V)                                              \
  V(Net, "net", PermissionsRoot)                                               \
  V(NetUdp, "net.udp", Net)                                                    \
  V(NetDNS, "net.dns", Net)                                                    \
  V(NetTCP, "net.tcp", Net)                                                    \
  V(NetTCPIn, "net.tcp.in", NetTCP)                                            \
  V(NetTCPOut, "net.tcp.out", NetTCP)                                          \
  V(NetTLS, "net.tls", Net)                                                    \
  V(NetTLSLog, "net.tls.log", NetTLS)

#define EXPERIMENTAL_PERMISSIONS(V)                                            \
  V(Experimental, "experimental", PermissionsRoot)                             \
  V(ExperimentalWasi, "experimental.wasi", Experimental)

#define PERMISSIONS(V)                                                         \
  EXPERIMENTAL_PERMISSIONS(V)                                                  \
  FILESYSTEM_PERMISSIONS(V)                                                    \
  NETWORKING_PERMISSIONS(V)                                                    \
  SPECIAL_PERMISSIONS(V)                                                       \
  V(Process, "process", PermissionsRoot)                                       \
  V(Signal, "signal", PermissionsRoot)                                         \
  V(Timing, "timing", PermissionsRoot)                                         \
  V(User, "user", PermissionsRoot)                                             \
  V(Workers, "workers", PermissionsRoot)                                       \

#define V(name, _, __) k##name,
enum class Permission {
  kPermissionsRoot = -1,
  PERMISSIONS(V)
  kPermissionsCount
};
#undef V

class Policy final {
 public:
  enum class ParseStatus {
    OK,
    UNKNOWN
  };

  static inline Permission PermissionFromName(const std::string& name);

  static inline std::vector<Permission> Parse(
      const std::string& list,
      ParseStatus* status = nullptr);

  inline explicit Policy(EnvironmentOptions* options);

  inline Policy(
      Policy* basis,
      const std::string& deny,
      const std::string& grant);

  inline Policy(
      Policy* basis,
      const std::vector<Permission>& deny,
      const std::vector<Permission>& grant);

  Policy(Policy&& other) = delete;
  Policy& operator=(Policy&& other) = delete;
  Policy(const Policy& other) = delete;
  Policy& operator=(const Policy& other) = delete;

  inline void Deny(Permission permission);
  inline void Deny(std::string permission);

  inline void Grant(Permission permission);
  inline void Grant(std::string permission);

  // Once a policy is locked, no additional grants will be permitted.
  inline void Lock() { locked_ = true; }

  inline bool is_granted(Permission permission) const;
  inline bool is_granted(std::string permission) const;

  // Once a policy is locked, no additional grants will be permitted.
  inline bool is_locked() const { return locked_; }

 private:
  inline void MaybeDenySpecials(Policy* policy, Permission permission);

  enum class ApplyFlags {
    // When used, instructs Apply not to deny specials automatically
    kIgnoreSpecials,
    // When used, instructs Apply to deny specials if there are any
    // other denials
    kDenySpecials
  };

  inline void Apply(
      const std::string& deny,
      const std::string& grant,
      ApplyFlags flags = ApplyFlags::kDenySpecials);

  inline void Apply(
      const std::vector<Permission>& deny,
      const std::vector<Permission>& grant,
      ApplyFlags flags = ApplyFlags::kDenySpecials);

  inline bool test(Permission permission) const;
  inline void SetRecursively(Permission permission, bool value);

  bool locked_ = false;
  std::bitset<static_cast<size_t>(Permission::kPermissionsCount)> permissions_;
};

// The Environment always has exactly one PriviledgeAccessContext that covers
// the currently active Policy. Think of the PrivilegedAccessContext as a
// stack of Policy objects with the default or root policy as the base.
class PrivilegedAccessContext {
 public:
  class Scope {
   public:
    inline Scope(
        Environment* env,
        const std::string& deny,
        const std::string& grant);

    inline Scope(
        Environment* env,
        const std::vector<Permission>& deny,
        const std::vector<Permission>& grant);

    inline ~Scope();
   private:
    Environment* env_;
  };

  static void Run(const v8::FunctionCallbackInfo<v8::Value>& args);

  inline explicit PrivilegedAccessContext(EnvironmentOptions* options);

  PrivilegedAccessContext(
      PrivilegedAccessContext&& other) = delete;
  PrivilegedAccessContext& operator=(
      PrivilegedAccessContext&& other) = delete;
  PrivilegedAccessContext(
      const PrivilegedAccessContext& other) = delete;
  PrivilegedAccessContext& operator=(
      const PrivilegedAccessContext& other) = delete;

  inline void Push(
      const std::string& deny,
      const std::string& grant);

  inline void Push(
      const std::vector<Permission>& deny,
      const std::vector<Permission>& grant);

  inline bool Pop();

  inline bool is_granted(Permission permission);

  inline bool is_granted(const std::string& permission);

  inline void Deny(Permission permission);

  inline void Deny(const std::string& permission);

 private:
  inline void Push(std::unique_ptr<Policy> policy);
  std::deque<std::unique_ptr<Policy>> policy_stack_;
};

}  // namespace policy
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_POLICY_POLICY_H_
