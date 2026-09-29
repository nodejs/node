#ifndef SRC_PERMISSION_ENV_PERMISSION_H_
#define SRC_PERMISSION_ENV_PERMISSION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_mutex.h"
#include "permission/permission_base.h"

#include <atomic>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace node {

class Environment;

namespace permission {

// Splits the values of --allow-env (which may be given more than once, and
// may each hold a comma-separated list) into individual patterns.
std::vector<std::string> ParseEnvAllowList(std::span<const std::string> values);

// Whether `pattern` is `*`, a variable name, or a variable name prefix
// followed by `*`.
bool IsValidEnvAllowPattern(std::string_view pattern);

// Whether `name` matches `pattern`: `*`, an exact name, or a prefix followed
// by `*`. Names are compared case-insensitively on Windows, where environment
// variable names are case-insensitive.
bool EnvNameMatchesPattern(std::string_view pattern, std::string_view name);

// Returns the patterns matching exactly the names that both `a` and `b` match.
std::vector<std::string> IntersectEnvAllowLists(std::span<const std::string> a,
                                                std::span<const std::string> b);

// Environment variables that Node.js and its bundled dependencies read after
// startup. The scrub keeps them, and dropping the whole env scope keeps them.
std::span<const std::string_view> GetRuntimeEnvironmentDefaults();
bool IsRuntimeEnvironmentDefault(std::string_view name);

struct EnvScrubOptions {
  // Names or patterns to keep, see EnvNameMatchesPattern().
  std::span<const std::string> allow;
  // Whether to keep GetRuntimeEnvironmentDefaults().
  bool keep_runtime_defaults = true;
  // Whether to overwrite the removed entries in the environment block the
  // process started with, which /proc/<pid>/environ still exposes after
  // unsetenv(). Best effort, and currently only implemented on Linux.
  bool wipe_initial_block = true;
};

// Removes every environment variable that `options` does not keep from the
// process environment, and returns the names it removed.
//
// This modifies the process environment without any locking that native
// code calling getenv() participates in, so it must be called before any
// such thread is started.
std::vector<std::string> ScrubProcessEnvironment(
    const EnvScrubOptions& options);

// Returns the names of the variables in the process environment that neither
// `allow` nor GetRuntimeEnvironmentDefaults() match.
std::vector<std::string> FindDeniedEnvironmentVariables(
    std::span<const std::string> allow);

// In audit mode, nothing is removed. Records the names of the variables that
// ScrubProcessEnvironment() would remove instead.
void RecordAuditedEnvironmentVariables(std::span<const std::string> allow);

// Whether ScrubProcessEnvironment() has run in this process.
bool IsProcessEnvironmentScrubbed();

// Whether `name` was removed by ScrubProcessEnvironment().
bool WasRemovedByEnvironmentScrub(std::string_view name);

// Whether `name` was removed by ScrubProcessEnvironment(), or recorded by
// RecordAuditedEnvironmentVariables().
bool WasDeniedAtStartup(std::string_view name);

// Returns true only the first time it is called for a given `name`.
bool ShouldWarnAboutRemovedEnvVar(std::string_view name);

class EnvPermission final : public PermissionBase {
 public:
  void Apply(Environment* env,
             std::span<const std::string> allow,
             PermissionScope scope) override;
  void Drop(Environment* env,
            PermissionScope scope,
            std::string_view param) override;
  bool is_granted(Environment* env,
                  PermissionScope perm,
                  std::string_view param) const override;

  // Whether every environment variable is accessible. Lock-free, so that the
  // file system scope can consult it on every check, from any thread. A check
  // that races with Drop() on another thread may observe the value from
  // before the drop.
  bool granted_all() const {
    return granted_all_.load(std::memory_order_acquire);
  }

 private:
  // Removes the variables `drop_all` or `name` refer to from the
  // environment `env` exposes as process.env.
  void RemoveFromEnvironment(Environment* env,
                             bool drop_all,
                             std::string_view name);

  std::atomic<bool> granted_all_{false};
  // Guards the members below. is_granted() may be called from threads other
  // than the one that owns the Environment, such as the thread pool.
  mutable RwLock lock_;
  bool dropped_all_ = false;
  std::vector<std::string> patterns_;
  std::unordered_set<std::string> dropped_;
};

#if defined(__linux__)
// Whether reading the file at `path` must be denied because it is, or
// resolves to, the /proc/<pid>/environ file of a process, which exposes the
// environment that process started with. Every process's file is denied
// except this process's own, which is allowed only when `allow_own` is set.
// Symbolic links are resolved first, so that paths such as
// /dev/fd/../environ are recognized. `path` may be relative to the current
// working directory.
bool IsProcEnvironReadDenied(std::string_view path, bool allow_own);
#endif  // defined(__linux__)

}  // namespace permission

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_PERMISSION_ENV_PERMISSION_H_
