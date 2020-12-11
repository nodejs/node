#include "policy.h"
#include "aliased_struct-inl.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_external_reference.h"

#include "v8.h"

#include <string>
#include <vector>

namespace node {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Int32;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Object;
using v8::Value;

namespace per_process {
// The root policy is establish at process start using
// the --policy-grant and --policy-deny command line
// arguments. Every node::Environment has it's own
// Policy that derives from the root.
policy::Policy root_policy;
}  // namespace per_process

namespace policy {

PrivilegedScope::PrivilegedScope(Environment* env_) : env(env_) {
  env->set_in_privileged_scope(true);
}

PrivilegedScope::~PrivilegedScope() {
  env->set_in_privileged_scope(false);
}

namespace {
Mutex apply_mutex_;

Permission GetPermission(Local<Value> arg) {
  if (!arg->IsInt32())
    return Permission::kPermissionsCount;
  int32_t permission = arg.As<Int32>()->Value();
  if (permission < static_cast<int>(Permission::kPermissionsRoot) ||
      permission > static_cast<int>(Permission::kPermissionsCount)) {
    return Permission::kPermissionsCount;
  }
  return static_cast<Permission>(permission);
}

static void Deny(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  Utf8Value list(env->isolate(), args[0]);
  // If Apply returns Nothing<bool>, there was an error
  // parsing the list, in which case we'll return undefined.
  if (per_process::root_policy.Apply(*list).IsJust())
    return args.GetReturnValue().Set(true);
}

static void FastCheck(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Permission permission = GetPermission(args[0]);
  CHECK_LT(permission, Permission::kPermissionsCount);
  CHECK_GT(permission, Permission::kPermissionsRoot);
  args.GetReturnValue().Set(Policy::is_granted(env, GetPermission(args[0])));
}

static void Check(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  Utf8Value list(env->isolate(), args[0]);
  Maybe<PermissionSet> permissions = Policy::Parse(*list);
  // If permissions is empty, there was an error parsing.
  // return undefined to indicate check failure.
  if (permissions.IsNothing()) return;
  args.GetReturnValue().Set(Policy::is_granted(env, permissions.FromJust()));
}

#define V(name, _, parent)                                                     \
  if (permission == Permission::k##parent)                                     \
    SetRecursively(set, Permission::k##name);
void SetRecursively(PermissionSet* set, Permission permission) {
  if (permission != Permission::kPermissionsRoot)
    set->set(static_cast<size_t>(permission));
  PERMISSIONS(V)
}
#undef V

}  // namespace

void RunInPrivilegedScope(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsFunction());  // The function to execute
  CHECK(!args.IsConstructCall());
  Local<Function> fn = args[0].As<Function>();

  Environment* env = Environment::GetCurrent(args);
  PrivilegedScope privileged_scope(env);

  SlicedArguments call_args(args, 1);

  Local<Value> ret;
  if (fn->Call(
          env->context(),
          args.This(),
          call_args.length(),
          call_args.out()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

bool Policy::is_granted(Environment* env, Permission permission) {
  return env->in_privileged_scope()
      ? true
      : per_process::root_policy.is_granted(permission);
}

bool Policy::is_granted(Environment* env, std::string permission) {
  return env->in_privileged_scope()
      ? true
      : per_process::root_policy.is_granted(permission);
}

bool Policy::is_granted(Environment* env, const PermissionSet& permissions) {
  return env->in_privileged_scope()
      ? true
      : per_process::root_policy.is_granted(permissions);
}

Maybe<PermissionSet> Policy::Parse(const std::string& list) {
  PermissionSet set;
  for (const auto& name : SplitString(list, ',')) {
    Permission permission = PermissionFromName(name);
    if (permission == Permission::kPermissionsCount)
      return Nothing<PermissionSet>();
    SetRecursively(&set, permission);
  }
  return Just(set);
}

#define V(Name, label, _)                                                      \
  if (strcmp(name.c_str(), label) == 0) return Permission::k##Name;
Permission Policy::PermissionFromName(const std::string& name) {
  if (strcmp(name.c_str(), "*") == 0) return Permission::kPermissionsRoot;
  PERMISSIONS(V)
  return Permission::kPermissionsCount;
}
#undef V

Maybe<bool> Policy::Apply(const std::string& deny, const std::string& grant) {
  Maybe<PermissionSet> deny_set = Parse(deny);
  if (deny_set.IsNothing()) return Nothing<bool>();
  Maybe<PermissionSet> grant_set = Parse(grant);
  if (grant_set.IsNothing()) return Nothing<bool>();
  Apply(deny_set.FromJust(), grant_set.FromJust());
  return Just(true);;
}

void Policy::Apply(const PermissionSet& deny, const PermissionSet& grant) {
  // permissions_ is an inverted set. If a bit is *set* in
  // permissions_, then the permission is *denied*, otherwise
  // it is granted.

  // Just in case Deny is called from multiple Worker threads.
  // TODO(@jasnell): Do we want to allow workers to call deny?
  Mutex::ScopedLock lock(apply_mutex_);

  if (deny.count() > 0) {
#define V(name, _, __)                                                         \
  permissions_.set(static_cast<size_t>(Permission::k##name));
    SPECIAL_PERMISSIONS(V)
#undef V
  }

  permissions_ |= deny;

  if (!locked_)
    permissions_ &= ~grant;

  locked_ = true;
}

void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "deny", Deny);
  env->SetMethodNoSideEffect(target, "fastCheck", FastCheck);
  env->SetMethodNoSideEffect(target, "check", Check);

  #define V(name, _, __)                                                       \
    constexpr int kPermission##name = static_cast<int>(Permission::k##name);   \
    NODE_DEFINE_CONSTANT(target, kPermission##name);
  PERMISSIONS(V)
  #undef V

  // internalBinding('policy') should be frozen
  target->SetIntegrityLevel(context, v8::IntegrityLevel::kFrozen).FromJust();
}

void RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(Deny);
  registry->Register(FastCheck);
  registry->Register(Check);
  registry->Register(RunInPrivilegedScope);
}

}  // namespace policy
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(policy, node::policy::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(policy, node::policy::RegisterExternalReferences)
