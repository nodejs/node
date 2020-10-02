#include "policy-inl.h"  // NOLINT(build/include)
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

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Int32;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::Value;

namespace policy {

namespace {
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
}  // namespace

static void Deny(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  Utf8Value list(env->isolate(), args[0]);
  Policy::ParseStatus status = Policy::ParseStatus::OK;
  auto permissions = Policy::Parse(*list, &status);
  if (status != Policy::ParseStatus::OK)
    return args.GetReturnValue().Set(static_cast<int>(status));
  for (Permission permission : permissions)
    env->privileged_access_context()->Deny(permission);
}

static void FastCheck(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Permission permission = GetPermission(args[0]);
  CHECK_LT(permission, Permission::kPermissionsCount);
  CHECK_GT(permission, Permission::kPermissionsRoot);
  args.GetReturnValue().Set(
      env->privileged_access_context()->is_granted(GetPermission(args[0])));
}

static void Check(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  Utf8Value list(env->isolate(), args[0]);
  Policy::ParseStatus status = Policy::ParseStatus::OK;
  auto permissions = Policy::Parse(*list, &status);
  if (status != Policy::ParseStatus::OK)
    return args.GetReturnValue().Set(static_cast<int>(status));
  for (Permission permission : permissions) {
    if (!env->privileged_access_context()->is_granted(permission))
      return args.GetReturnValue().Set(false);
  }
  args.GetReturnValue().Set(true);
}

namespace {
std::vector<Permission> ToPermissions(Environment* env, Local<Array> input) {
  std::vector<Permission> permissions;
  for (size_t n = 0; n < input->Length(); n++) {
    Local<Value> val = input->Get(env->context(), n).FromMaybe(Local<Value>());
    CHECK(val->IsInt32());
    int32_t permission = val.As<Int32>()->Value();
    CHECK_GE(permission, static_cast<int32_t>(Permission::kPermissionsRoot));
    CHECK_LT(permission, static_cast<int32_t>(Permission::kPermissionsCount));
    permissions.push_back(static_cast<Permission>(permission));
  }
  return permissions;
}
}  // namespace

// The starting arguments must consist of a function along with
// two strings identifying the permissions to deny or grant.
// The remaining arguments will be passed on to the function,
void PrivilegedAccessContext::Run(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_GE(args.Length(), 3);
  CHECK(args[0]->IsString() || args[0]->IsArray());  // The denials
  CHECK_IMPLIES(args[0]->IsString(), args[1]->IsString());
  CHECK_IMPLIES(args[1]->IsArray(), args[1]->IsArray());
  CHECK(args[2]->IsFunction());  // The function to execute

  Local<Function> fn = args[2].As<Function>();

  CHECK(!args.IsConstructCall());

  SlicedArguments call_args(args, 3);
  Environment* env = Environment::GetCurrent(args);
  MaybeLocal<Value> ret;

  if (args[0]->IsArray()) {
    std::vector<Permission> deny = ToPermissions(env, args[0].As<Array>());
    std::vector<Permission> grant = ToPermissions(env, args[1].As<Array>());
    PrivilegedAccessContext::Scope privileged_scope(env, deny, grant);
    ret = fn->Call(
        env->context(),
        args.This(),
        call_args.length(),
        call_args.out());
  } else {
    Utf8Value deny(env->isolate(), args[0]);
    Utf8Value grant(env->isolate(), args[1]);
    PrivilegedAccessContext::Scope privileged_scope(env, *deny, *grant);
    ret = fn->Call(
        env->context(),
        args.This(),
        call_args.length(),
        call_args.out());
  }

  args.GetReturnValue().Set(ret.FromMaybe(Local<Value>()));
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
  registry->Register(PrivilegedAccessContext::Run);
}

}  // namespace policy
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(policy, node::policy::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(policy, node::policy::RegisterExternalReferences)
