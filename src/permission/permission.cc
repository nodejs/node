#include "permission.h"
#include "env-inl.h"
#include "node.h"
#include "node_debug.h"
#include "node_diagnostics_channel.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_file.h"

#include "permission/boolean_permission.h"
#include "permission/fs_permission.h"
#include "permission/permission_base.h"
#include "v8-fast-api-calls.h"
#include "v8-template.h"
#include "v8.h"

#include <memory>
#include <string>

namespace node {

using v8::CFunction;
using v8::Context;
using v8::DictionaryTemplate;
using v8::FastApiCallbackOptions;
using v8::FunctionCallbackInfo;
using v8::IntegrityLevel;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Undefined;
using v8::Value;

namespace permission {

namespace {

constexpr std::string_view GetDiagnosticsChannelName(PermissionScope scope) {
  switch (scope) {
    case PermissionScope::kFileSystem:
    case PermissionScope::kFileSystemRead:
    case PermissionScope::kFileSystemWrite:
      return "node:permission-model:fs";
    case PermissionScope::kChildProcess:
      return "node:permission-model:child";
    case PermissionScope::kWorkerThreads:
      return "node:permission-model:worker";
    case PermissionScope::kNet:
      return "node:permission-model:net";
    case PermissionScope::kInspector:
      return "node:permission-model:inspector";
    case PermissionScope::kWASI:
      return "node:permission-model:wasi";
    case PermissionScope::kAddon:
      return "node:permission-model:addon";
    case PermissionScope::kFFI:
      return "node:permission-model:ffi";
    case PermissionScope::kOpenSSLStore:
      return "node:permission-model:openssl-store";
    default:
      return {};
  }
}

Local<DictionaryTemplate> GetPermissionDiagnosticsTemplate(Environment* env) {
  auto tmpl = env->permission_diagnostic_channel_message();
  if (tmpl.IsEmpty()) {
    static constexpr std::string_view names[] = {
        "permission",
        "resource",
        "drop",
    };
    tmpl = DictionaryTemplate::New(env->isolate(), names);
    env->set_permission_diagnostic_channel_message(tmpl);
  }
  return tmpl;
}

// permission.drop('fs.read', '/tmp/')
// permission.drop('child')
static void Drop(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());

  const std::string deny_scope = Utf8Value(env->isolate(), args[0]).ToString();
  PermissionScope scope = Permission::StringToPermission(deny_scope);
  if (scope == PermissionScope::kPermissionsRoot) {
    return;
  }

  if (args.Length() > 1 && !args[1]->IsUndefined()) {
    Utf8Value utf8_arg(env->isolate(), args[1]);
    if (utf8_arg.length() > 0) {
      env->permission()->Drop(env, scope, utf8_arg.ToStringView());
      return;
    }
  }

  env->permission()->Drop(env, scope, "");
}

// permission.has('fs.in', '/tmp/')
// permission.has('fs.in')
static void Has(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());

  const std::string deny_scope = Utf8Value(env->isolate(), args[0]).ToString();
  PermissionScope scope = Permission::StringToPermission(deny_scope);
  if (scope == PermissionScope::kPermissionsRoot) {
    return args.GetReturnValue().Set(false);
  }

  if (args.Length() > 1 && !args[1]->IsUndefined()) {
    Utf8Value utf8_arg(env->isolate(), args[1]);
    if (utf8_arg.length() == 0) {
      args.GetReturnValue().Set(false);
      return;
    }
    return args.GetReturnValue().Set(
        env->permission()->is_granted(env, scope, utf8_arg.ToStringView()));
  }

  return args.GetReturnValue().Set(env->permission()->is_granted(env, scope));
}

static bool FastHas(Local<Value> receiver,
                    Local<Value> scope_arg,
                    // NOLINTNEXTLINE(runtime/references) This is V8 api.
                    FastApiCallbackOptions& options) {
  TRACK_V8_FAST_API_CALL("permission.has");
  auto isolate = options.isolate;
  v8::HandleScope handle_scope(isolate);
  auto context = isolate->GetCurrentContext();

  Environment* env = Environment::GetCurrent(context);

  Local<String> str;
  if (!scope_arg->ToString(context).ToLocal(&str)) {
    return false;
  }
  Utf8Value utf8_scope(isolate, str);
  PermissionScope scope =
      Permission::StringToPermission(utf8_scope.ToStringView());
  if (scope == PermissionScope::kPermissionsRoot) {
    return false;
  }

  return env->permission()->is_granted(env, scope);
}

static bool FastHasResource(
    Local<Value> receiver,
    Local<Value> scope_arg,
    Local<Value> resource_arg,
    // NOLINTNEXTLINE(runtime/references) This is V8 api.
    FastApiCallbackOptions& options) {
  TRACK_V8_FAST_API_CALL("permission.has");
  auto isolate = options.isolate;
  v8::HandleScope handle_scope(isolate);
  auto context = isolate->GetCurrentContext();

  Environment* env = Environment::GetCurrent(context);

  Local<String> str;
  if (!scope_arg->ToString(context).ToLocal(&str)) {
    return false;
  }
  Utf8Value utf8_scope(isolate, str);
  PermissionScope scope =
      Permission::StringToPermission(utf8_scope.ToStringView());
  if (scope == PermissionScope::kPermissionsRoot) {
    return false;
  }

  Local<String> res_str;
  if (!resource_arg->ToString(context).ToLocal(&res_str)) {
    return false;
  }
  Utf8Value utf8_res(isolate, res_str);
  if (utf8_res.length() == 0) {
    return false;
  }

  return env->permission()->is_granted(env, scope, utf8_res.ToStringView());
}

static CFunction fast_has_methods_[] = {CFunction::Make(FastHas),
                                        CFunction::Make(FastHasResource)};

}  // namespace

#define V(Name, label, _, __)                                                  \
  if (perm == PermissionScope::k##Name) return env->Name##_permission_string();
v8::Local<v8::String> Permission::PermissionToString(Environment* env,
                                                     PermissionScope perm) {
  PERMISSIONS(V)
  UNREACHABLE();
}
#undef V

#define V(Name, label, _, __)                                                  \
  if (perm == label) return PermissionScope::k##Name;
PermissionScope Permission::StringToPermission(std::string_view perm) {
  PERMISSIONS(V)
  return PermissionScope::kPermissionsRoot;
}
#undef V

Permission::Permission() : enabled_(false), warning_only_(false) {
  auto fs = std::make_shared<FSPermission>();
#define V(Name, _, __, ___)                                                    \
  nodes_[static_cast<size_t>(PermissionScope::k##Name)] = fs;
  FILESYSTEM_PERMISSIONS(V)
#undef V
#define V(Name, _, __, ___)                                                    \
  nodes_[static_cast<size_t>(PermissionScope::k##Name)] =                      \
      std::make_shared<DenyOnlyPermission>();
  CHILD_PROCESS_PERMISSIONS(V)
  WORKER_THREADS_PERMISSIONS(V)
  INSPECTOR_PERMISSIONS(V)
  WASI_PERMISSIONS(V)
  ADDON_PERMISSIONS(V)
  FFI_PERMISSIONS(V)
  OPENSSL_STORE_PERMISSIONS(V)
#undef V
#define V(Name, _, __, ___)                                                    \
  nodes_[static_cast<size_t>(PermissionScope::k##Name)] =                      \
      std::make_shared<AllowRevokePermission>();
  NET_PERMISSIONS(V)
#undef V
}

const char* GetErrorFlagSuggestion(node::permission::PermissionScope perm) {
  switch (perm) {
#define V(Name, _, __, Flag)                                                   \
  case node::permission::PermissionScope::k##Name:                             \
    return Flag[0] != '\0' ? "Use " Flag " to manage permissions." : "";
    PERMISSIONS(V)
#undef V
    default:
      return "";
  }
}

MaybeLocal<Value> CreateAccessDeniedError(Environment* env,
                                          PermissionScope perm,
                                          std::string_view res) {
  const char* suggestion = GetErrorFlagSuggestion(perm);
  Local<Object> err = ERR_ACCESS_DENIED(
      env->isolate(), "Access to this API has been restricted. %s", suggestion);

  Local<Value> resource_string;
  Local<Value> perm_string = Permission::PermissionToString(env, perm);
  if (!ToV8Value(env->context(), res, env->isolate())
           .ToLocal(&resource_string) ||
      err->Set(env->context(), env->permission_string(), perm_string)
          .IsNothing() ||
      err->Set(env->context(), env->resource_string(), resource_string)
          .IsNothing()) {
    return MaybeLocal<Value>();
  }
  return err;
}

void Permission::ThrowAccessDenied(Environment* env,
                                   PermissionScope perm,
                                   std::string_view res) {
  Local<Value> err;
  if (CreateAccessDeniedError(env, perm, res).ToLocal(&err)) {
    env->isolate()->ThrowException(err);
  }
  // If ToLocal returned false, then v8 will have scheduled a
  // superseding error to be thrown.
}

void Permission::AsyncThrowAccessDenied(Environment* env,
                                        fs::FSReqBase* req_wrap,
                                        PermissionScope perm,
                                        std::string_view res) {
  Local<Value> err;
  if (CreateAccessDeniedError(env, perm, res).ToLocal(&err)) {
    return req_wrap->Reject(err);
  }
  // If ToLocal returned false, then v8 will have scheduled a
  // superseding error to be thrown.
}

void Permission::EnablePermissions() {
  if (!enabled_) {
    enabled_ = true;
  }
}

void Permission::EnableWarningOnly() {
  if (!warning_only_) {
    warning_only_ = true;
  }
}

bool Permission::is_scope_granted(Environment* env,
                                  PermissionScope permission,
                                  std::string_view res) const {
  CHECK(permission != PermissionScope::kPermissionsRoot &&
        permission != PermissionScope::kPermissionsCount);
  auto& perm_node = nodes_[static_cast<size_t>(permission)];
  bool result = false;
  if (perm_node) {
    result = perm_node->is_granted(env, permission, res);
  }

  if (!result && !publishing_) {
    auto ch = GetOrCreateChannel(env, permission);
    if (ch && ch->HasSubscribers()) {
      publishing_ = true;
      v8::Isolate* isolate = env->isolate();
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = env->context();
      v8::MaybeLocal<v8::Value> values[] = {
          PermissionToString(env, permission),
          ToV8Value(context, res),
          Undefined(isolate),
      };
      ch->Publish(
          env,
          GetPermissionDiagnosticsTemplate(env)->NewInstance(context, values));
      publishing_ = false;
    }
  }

  return result;
}

BaseObjectPtr<diagnostics_channel::Channel> Permission::GetOrCreateChannel(
    Environment* env, PermissionScope scope) const {
  CHECK(scope != PermissionScope::kPermissionsRoot &&
        scope != PermissionScope::kPermissionsCount);
  auto& weak_ch = channels_[static_cast<size_t>(scope)];
  // Promote weak ref to strong for the duration of this call.
  BaseObjectPtr<diagnostics_channel::Channel> ptr(weak_ch.get());
  if (ptr) return ptr;
  auto channel_name = GetDiagnosticsChannelName(scope);
  if (auto ch = diagnostics_channel::Channel::Get(env, channel_name)) {
    weak_ch = BaseObjectWeakPtr<diagnostics_channel::Channel>(ch.get());
    return ch;
  }
  return {};
}

void Permission::Apply(Environment* env,
                       std::span<const std::string> allow,
                       PermissionScope scope) {
  auto& perm_node = nodes_[static_cast<size_t>(scope)];
  if (perm_node) {
    perm_node->Apply(env, allow, scope);
  }
}

void Permission::Drop(Environment* env,
                      PermissionScope scope,
                      std::string_view param) {
  CHECK(scope != PermissionScope::kPermissionsRoot &&
        scope != PermissionScope::kPermissionsCount);
  auto& perm_node = nodes_[static_cast<size_t>(scope)];
  if (perm_node) {
    perm_node->Drop(env, scope, param);
  }

  // Publish to diagnostics channel so observers can track drops
  if (!publishing_) {
    auto ch = GetOrCreateChannel(env, scope);
    if (ch && ch->HasSubscribers()) {
      publishing_ = true;
      v8::Isolate* isolate = env->isolate();
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = env->context();

      v8::MaybeLocal<v8::Value> values[] = {
          PermissionToString(env, scope),
          ToV8Value(context, param),
          v8::True(isolate),
      };
      ch->Publish(
          env,
          GetPermissionDiagnosticsTemplate(env)->NewInstance(context, values));
      publishing_ = false;
    }
  }
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetFastMethodNoSideEffect(
      context, target, "has", Has, {fast_has_methods_, 2});
  SetMethod(context, target, "drop", Drop);

  target->SetIntegrityLevel(context, IntegrityLevel::kFrozen).FromJust();
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(Has);
  for (const CFunction& method : fast_has_methods_) {
    registry->Register(method);
  }
  registry->Register(Drop);
}

}  // namespace permission
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(permission, node::permission::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(permission,
                                node::permission::RegisterExternalReferences)
