#ifndef SRC_PERMISSION_PERMISSION_H_
#define SRC_PERMISSION_PERMISSION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "debug_utils.h"
#include "node_options.h"
#include "permission/child_process_permission.h"
#include "permission/fs_permission.h"
#include "permission/inspector_permission.h"
#include "permission/net_permission.h"
#include "permission/permission_base.h"
#include "permission/wasi_permission.h"
#include "permission/worker_permission.h"
#include "v8.h"

#include <string_view>
#include <unordered_map>

namespace node {

class Environment;

namespace fs {
class FSReqBase;
}

namespace permission {

#define THROW_IF_INSUFFICIENT_PERMISSIONS(env, perm_, resource_, ...)          \
  do {                                                                         \
    if (!env->permission()->is_granted(env, perm_, resource_)) [[unlikely]] {  \
      node::permission::Permission::ThrowAccessDenied(                         \
          (env), perm_, resource_);                                            \
      return __VA_ARGS__;                                                      \
    }                                                                          \
  } while (0)

#define ASYNC_THROW_IF_INSUFFICIENT_PERMISSIONS(                               \
    env, wrap, perm_, resource_, ...)                                          \
  do {                                                                         \
    if (!env->permission()->is_granted(env, perm_, resource_)) [[unlikely]] {  \
      node::permission::Permission::AsyncThrowAccessDenied(                    \
          (env), wrap, perm_, resource_);                                      \
      return __VA_ARGS__;                                                      \
    }                                                                          \
  } while (0)

#define ERR_ACCESS_DENIED_IF_INSUFFICIENT_PERMISSIONS(                         \
    env, perm_, resource_, args, ...)                                          \
  do {                                                                         \
    if (!env->permission()->is_granted(env, perm_, resource_)) [[unlikely]] {  \
      Local<Value> err_access;                                                 \
      if (permission::CreateAccessDeniedError(env, perm_, resource_)           \
              .ToLocal(&err_access)) {                                         \
        args.GetReturnValue().Set(err_access);                                 \
      } else {                                                                 \
        args.GetReturnValue().Set(UV_EACCES);                                  \
      }                                                                        \
      return __VA_ARGS__;                                                      \
    }                                                                          \
  } while (0)

#define SET_INSUFFICIENT_PERMISSION_ERROR_CALLBACK(scope)                      \
  void InsufficientPermissionError(const std::string resource) {               \
    v8::HandleScope handle_scope(env()->isolate());                            \
    v8::Context::Scope context_scope(env()->context());                        \
    v8::Local<v8::Value> arg;                                                  \
    if (!permission::CreateAccessDeniedError(env(), scope, resource)           \
             .ToLocal(&arg)) {                                                 \
    }                                                                          \
    MakeCallback(env()->oncomplete_string(), 1, &arg);                         \
  }

class Permission {
 public:
  Permission();

  FORCE_INLINE bool is_granted(Environment* env,
                               const PermissionScope permission,
                               const std::string_view& res = "") const {
    if (!enabled_) [[likely]] {
      return true;
    }
    return is_scope_granted(env, permission, res);
  }

  FORCE_INLINE bool enabled() const { return enabled_; }

  static PermissionScope StringToPermission(const std::string& perm);
  static const char* PermissionToString(PermissionScope perm);
  static void ThrowAccessDenied(Environment* env,
                                PermissionScope perm,
                                const std::string_view& res);
  static void AsyncThrowAccessDenied(Environment* env,
                                     fs::FSReqBase* req_wrap,
                                     PermissionScope perm,
                                     const std::string_view& res);

  // CLI Call
  void Apply(Environment* env,
             const std::vector<std::string>& allow,
             PermissionScope scope);
  void EnablePermissions();

 private:
  COLD_NOINLINE bool is_scope_granted(Environment* env,
                                      const PermissionScope permission,
                                      const std::string_view& res = "") const {
    auto perm_node = nodes_.find(permission);
    if (perm_node != nodes_.end()) {
      return perm_node->second->is_granted(env, permission, res);
    }
    return false;
  }

  std::unordered_map<PermissionScope, std::shared_ptr<PermissionBase>> nodes_;
  bool enabled_;
};

v8::MaybeLocal<v8::Value> CreateAccessDeniedError(Environment* env,
                                                  PermissionScope perm,
                                                  const std::string_view& res);

}  // namespace permission

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_PERMISSION_PERMISSION_H_
