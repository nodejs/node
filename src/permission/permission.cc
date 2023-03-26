#include "permission.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_errors.h"
#include "node_external_reference.h"

#include "v8.h"

#include <memory>
#include <string>
#include <vector>

namespace node {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

namespace permission {

namespace {

// permission.deny('fs.read', ['/tmp/'])
// permission.deny('fs.read')
static void Deny(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  v8::Isolate* isolate = env->isolate();
  CHECK(args[0]->IsString());
  std::string deny_scope = *String::Utf8Value(isolate, args[0]);
  PermissionScope scope = Permission::StringToPermission(deny_scope);
  if (scope == PermissionScope::kPermissionsRoot) {
    return args.GetReturnValue().Set(false);
  }

  std::vector<std::string> params;
  if (args.Length() == 1 || args[1]->IsUndefined()) {
    return args.GetReturnValue().Set(env->permission()->Deny(scope, params));
  }

  CHECK(args[1]->IsArray());
  Local<Array> js_params = Local<Array>::Cast(args[1]);
  Local<Context> context = isolate->GetCurrentContext();

  for (uint32_t i = 0; i < js_params->Length(); ++i) {
    Local<Value> arg;
    if (!js_params->Get(context, Integer::New(isolate, i)).ToLocal(&arg)) {
      return;
    }
    String::Utf8Value utf8_arg(isolate, arg);
    if (*utf8_arg == nullptr) {
      return;
    }
    params.push_back(*utf8_arg);
  }

  return args.GetReturnValue().Set(env->permission()->Deny(scope, params));
}

// permission.has('fs.in', '/tmp/')
// permission.has('fs.in')
static void Has(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  v8::Isolate* isolate = env->isolate();
  CHECK(args[0]->IsString());

  String::Utf8Value utf8_deny_scope(isolate, args[0]);
  if (*utf8_deny_scope == nullptr) {
    return;
  }

  const std::string deny_scope = *utf8_deny_scope;
  PermissionScope scope = Permission::StringToPermission(deny_scope);
  if (scope == PermissionScope::kPermissionsRoot) {
    return args.GetReturnValue().Set(false);
  }

  if (args.Length() > 1 && !args[1]->IsUndefined()) {
    String::Utf8Value utf8_arg(isolate, args[1]);
    if (*utf8_arg == nullptr) {
      return;
    }
    return args.GetReturnValue().Set(
        env->permission()->is_granted(scope, *utf8_arg));
  }

  return args.GetReturnValue().Set(env->permission()->is_granted(scope));
}

}  // namespace

#define V(Name, label, _)                                                      \
  if (perm == PermissionScope::k##Name) return #Name;
const char* Permission::PermissionToString(const PermissionScope perm) {
  PERMISSIONS(V)
  return nullptr;
}
#undef V

#define V(Name, label, _)                                                      \
  if (perm == label) return PermissionScope::k##Name;
PermissionScope Permission::StringToPermission(const std::string& perm) {
  PERMISSIONS(V)
  return PermissionScope::kPermissionsRoot;
}
#undef V

Permission::Permission() : enabled_(false) {
  std::shared_ptr<PermissionBase> fs = std::make_shared<FSPermission>();
  std::shared_ptr<PermissionBase> child_p =
      std::make_shared<ChildProcessPermission>();
  std::shared_ptr<PermissionBase> worker_t =
      std::make_shared<WorkerPermission>();
#define V(Name, _, __)                                                         \
  nodes_.insert(std::make_pair(PermissionScope::k##Name, fs));
  FILESYSTEM_PERMISSIONS(V)
#undef V
#define V(Name, _, __)                                                         \
  nodes_.insert(std::make_pair(PermissionScope::k##Name, child_p));
  CHILD_PROCESS_PERMISSIONS(V)
#undef V
#define V(Name, _, __)                                                         \
  nodes_.insert(std::make_pair(PermissionScope::k##Name, worker_t));
  WORKER_THREADS_PERMISSIONS(V)
#undef V
}

void Permission::ThrowAccessDenied(Environment* env,
                                   PermissionScope perm,
                                   const std::string_view& res) {
  Local<Value> err = ERR_ACCESS_DENIED(env->isolate());
  CHECK(err->IsObject());
  if (err.As<Object>()
          ->Set(env->context(),
                env->permission_string(),
                v8::String::NewFromUtf8(env->isolate(),
                                        PermissionToString(perm),
                                        v8::NewStringType::kNormal)
                    .ToLocalChecked())
          .IsNothing() ||
      err.As<Object>()
          ->Set(env->context(),
                env->resource_string(),
                v8::String::NewFromUtf8(env->isolate(),
                                        std::string(res).c_str(),
                                        v8::NewStringType::kNormal)
                    .ToLocalChecked())
          .IsNothing())
    return;
  env->isolate()->ThrowException(err);
}

void Permission::EnablePermissions() {
  if (!enabled_) {
    enabled_ = true;
  }
}

void Permission::SetFSPermissionCaseSensitive(const bool sensitive) {
  auto it = nodes_.find(PermissionScope::kFileSystem);
  if (it != nodes_.end()) {
    auto fs_permission = std::static_pointer_cast<FSPermission>(it->second);
    if (fs_permission) {
      fs_permission->CaseSensitive(sensitive);
    }
  }
}

void Permission::Apply(const std::string& allow, PermissionScope scope) {
  auto permission = nodes_.find(scope);
  if (permission != nodes_.end()) {
    permission->second->Apply(allow, scope);
  }
}

bool Permission::Deny(PermissionScope scope,
                      const std::vector<std::string>& params) {
  auto permission = nodes_.find(scope);
  if (permission != nodes_.end()) {
    return permission->second->Deny(scope, params);
  }
  return false;
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetMethod(context, target, "deny", Deny);
  SetMethodNoSideEffect(context, target, "has", Has);

  target->SetIntegrityLevel(context, v8::IntegrityLevel::kFrozen).FromJust();
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(Deny);
  registry->Register(Has);
}

}  // namespace permission
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(permission, node::permission::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(permission,
                                node::permission::RegisterExternalReferences)
