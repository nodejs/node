#include "node_realm.h"
#include "env-inl.h"

#include "memory_tracker-inl.h"
#include "node_builtins.h"
#include "node_process.h"
#include "util.h"

namespace node {

using builtins::BuiltinLoader;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::HandleScope;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::SnapshotCreator;
using v8::String;
using v8::Value;

Realm::Realm(Environment* env,
             v8::Local<v8::Context> context,
             const RealmSerializeInfo* realm_info)
    : env_(env), isolate_(context->GetIsolate()) {
  context_.Reset(isolate_, context);

  // Create properties if not deserializing from snapshot.
  // Or the properties are deserialized with DeserializeProperties() when the
  // env drained the deserialize requests.
  if (realm_info == nullptr) {
    CreateProperties();
  }
}

void Realm::MemoryInfo(MemoryTracker* tracker) const {
#define V(PropertyName, TypeName)                                              \
  tracker->TrackField(#PropertyName, PropertyName());
  PER_REALM_STRONG_PERSISTENT_VALUES(V)
#undef V

  tracker->TrackField("env", env_);
}

void Realm::CreateProperties() {
  HandleScope handle_scope(isolate_);
  Local<Context> ctx = context();

  // Store primordials setup by the per-context script in the environment.
  Local<Object> per_context_bindings =
      GetPerContextExports(ctx).ToLocalChecked();
  Local<Value> primordials =
      per_context_bindings->Get(ctx, env_->primordials_string())
          .ToLocalChecked();
  CHECK(primordials->IsObject());
  set_primordials(primordials.As<Object>());

  Local<String> prototype_string =
      FIXED_ONE_BYTE_STRING(isolate(), "prototype");

#define V(EnvPropertyName, PrimordialsPropertyName)                            \
  {                                                                            \
    Local<Value> ctor =                                                        \
        primordials.As<Object>()                                               \
            ->Get(ctx,                                                         \
                  FIXED_ONE_BYTE_STRING(isolate(), PrimordialsPropertyName))   \
            .ToLocalChecked();                                                 \
    CHECK(ctor->IsObject());                                                   \
    Local<Value> prototype =                                                   \
        ctor.As<Object>()->Get(ctx, prototype_string).ToLocalChecked();        \
    CHECK(prototype->IsObject());                                              \
    set_##EnvPropertyName(prototype.As<Object>());                             \
  }

  V(primordials_safe_map_prototype_object, "SafeMap");
  V(primordials_safe_set_prototype_object, "SafeSet");
  V(primordials_safe_weak_map_prototype_object, "SafeWeakMap");
  V(primordials_safe_weak_set_prototype_object, "SafeWeakSet");
#undef V

  // TODO(legendecas): some methods probably doesn't need to be created with
  // process. Distinguish them and create process object only in the principal
  // realm.
  Local<Object> process_object =
      node::CreateProcessObject(this).FromMaybe(Local<Object>());
  set_process_object(process_object);
}

RealmSerializeInfo Realm::Serialize(SnapshotCreator* creator) {
  RealmSerializeInfo info;
  Local<Context> ctx = context();

  uint32_t id = 0;
#define V(PropertyName, TypeName)                                              \
  do {                                                                         \
    Local<TypeName> field = PropertyName();                                    \
    if (!field.IsEmpty()) {                                                    \
      size_t index = creator->AddData(ctx, field);                             \
      info.persistent_values.push_back({#PropertyName, id, index});            \
    }                                                                          \
    id++;                                                                      \
  } while (0);
  PER_REALM_STRONG_PERSISTENT_VALUES(V)
#undef V

  info.context = creator->AddData(ctx, ctx);
  return info;
}

void Realm::DeserializeProperties(const RealmSerializeInfo* info) {
  Local<Context> ctx = context();

  const std::vector<PropInfo>& values = info->persistent_values;
  size_t i = 0;  // index to the array
  uint32_t id = 0;
#define V(PropertyName, TypeName)                                              \
  do {                                                                         \
    if (values.size() > i && id == values[i].id) {                             \
      const PropInfo& d = values[i];                                           \
      DCHECK_EQ(d.name, #PropertyName);                                        \
      MaybeLocal<TypeName> maybe_field =                                       \
          ctx->GetDataFromSnapshotOnce<TypeName>(d.index);                     \
      Local<TypeName> field;                                                   \
      if (!maybe_field.ToLocal(&field)) {                                      \
        fprintf(stderr,                                                        \
                "Failed to deserialize realm value " #PropertyName "\n");      \
      }                                                                        \
      set_##PropertyName(field);                                               \
      i++;                                                                     \
    }                                                                          \
    id++;                                                                      \
  } while (0);

  PER_REALM_STRONG_PERSISTENT_VALUES(V);
#undef V

  MaybeLocal<Context> maybe_ctx_from_snapshot =
      ctx->GetDataFromSnapshotOnce<Context>(info->context);
  Local<Context> ctx_from_snapshot;
  if (!maybe_ctx_from_snapshot.ToLocal(&ctx_from_snapshot)) {
    fprintf(stderr,
            "Failed to deserialize context back reference from the snapshot\n");
  }
  CHECK_EQ(ctx_from_snapshot, ctx);

  DoneBootstrapping();
}

MaybeLocal<Value> Realm::ExecuteBootstrapper(const char* id) {
  EscapableHandleScope scope(isolate());
  Local<Context> ctx = context();
  MaybeLocal<Value> result = BuiltinLoader::CompileAndCall(ctx, id, this);

  // If there was an error during bootstrap, it must be unrecoverable
  // (e.g. max call stack exceeded). Clear the stack so that the
  // AsyncCallbackScope destructor doesn't fail on the id check.
  // There are only two ways to have a stack size > 1: 1) the user manually
  // called MakeCallback or 2) user awaited during bootstrap, which triggered
  // _tickCallback().
  if (result.IsEmpty()) {
    env()->async_hooks()->clear_async_id_stack();
  }

  return scope.EscapeMaybe(result);
}

MaybeLocal<Value> Realm::BootstrapInternalLoaders() {
  EscapableHandleScope scope(isolate_);

  // Bootstrap internal loaders
  Local<Value> loader_exports;
  if (!ExecuteBootstrapper("internal/bootstrap/loaders")
           .ToLocal(&loader_exports)) {
    return MaybeLocal<Value>();
  }
  CHECK(loader_exports->IsObject());
  Local<Object> loader_exports_obj = loader_exports.As<Object>();
  Local<Value> internal_binding_loader =
      loader_exports_obj->Get(context(), env_->internal_binding_string())
          .ToLocalChecked();
  CHECK(internal_binding_loader->IsFunction());
  set_internal_binding_loader(internal_binding_loader.As<Function>());
  Local<Value> require =
      loader_exports_obj->Get(context(), env_->require_string())
          .ToLocalChecked();
  CHECK(require->IsFunction());
  set_builtin_module_require(require.As<Function>());

  return scope.Escape(loader_exports);
}

MaybeLocal<Value> Realm::BootstrapNode() {
  EscapableHandleScope scope(isolate_);

  MaybeLocal<Value> result = ExecuteBootstrapper("internal/bootstrap/node");

  if (result.IsEmpty()) {
    return MaybeLocal<Value>();
  }

  if (!env_->no_browser_globals()) {
    result = ExecuteBootstrapper("internal/bootstrap/browser");

    if (result.IsEmpty()) {
      return MaybeLocal<Value>();
    }
  }

  // TODO(joyeecheung): skip these in the snapshot building for workers.
  auto thread_switch_id =
      env_->is_main_thread() ? "internal/bootstrap/switches/is_main_thread"
                             : "internal/bootstrap/switches/is_not_main_thread";
  result = ExecuteBootstrapper(thread_switch_id);

  if (result.IsEmpty()) {
    return MaybeLocal<Value>();
  }

  auto process_state_switch_id =
      env_->owns_process_state()
          ? "internal/bootstrap/switches/does_own_process_state"
          : "internal/bootstrap/switches/does_not_own_process_state";
  result = ExecuteBootstrapper(process_state_switch_id);

  if (result.IsEmpty()) {
    return MaybeLocal<Value>();
  }

  Local<String> env_string = FIXED_ONE_BYTE_STRING(isolate_, "env");
  Local<Object> env_var_proxy;
  if (!CreateEnvVarProxy(context(), isolate_).ToLocal(&env_var_proxy) ||
      process_object()->Set(context(), env_string, env_var_proxy).IsNothing()) {
    return MaybeLocal<Value>();
  }

  return scope.EscapeMaybe(result);
}

MaybeLocal<Value> Realm::RunBootstrapping() {
  EscapableHandleScope scope(isolate_);

  CHECK(!has_run_bootstrapping_code());

  if (BootstrapInternalLoaders().IsEmpty()) {
    return MaybeLocal<Value>();
  }

  Local<Value> result;
  if (!BootstrapNode().ToLocal(&result)) {
    return MaybeLocal<Value>();
  }

  DoneBootstrapping();

  return scope.Escape(result);
}

void Realm::DoneBootstrapping() {
  has_run_bootstrapping_code_ = true;

  // Make sure that no request or handle is created during bootstrap -
  // if necessary those should be done in pre-execution.
  // Usually, doing so would trigger the checks present in the ReqWrap and
  // HandleWrap classes, so this is only a consistency check.

  // TODO(legendecas): track req_wrap and handle_wrap by realms instead of
  // environments.
  CHECK(env_->req_wrap_queue()->IsEmpty());
  CHECK(env_->handle_wrap_queue()->IsEmpty());

  // TODO(legendecas): track base object count by realms.
  env_->set_base_object_created_by_bootstrap(env_->base_object_count());
}

}  // namespace node
