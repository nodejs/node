#include "node_shadow_realm.h"
#include "env-inl.h"
#include "node_errors.h"

namespace node {
namespace shadow_realm {
using v8::Context;
using v8::HandleScope;
using v8::Local;
using v8::MaybeLocal;
using v8::Value;

using TryCatchScope = node::errors::TryCatchScope;

// static
ShadowRealm* ShadowRealm::New(Environment* env) {
  ShadowRealm* realm = new ShadowRealm(env);
  env->AssignToContext(realm->context(), realm, ContextInfo(""));

  // We do not expect the realm bootstrapping to throw any
  // exceptions. If it does, exit the current Node.js instance.
  TryCatchScope try_catch(env, TryCatchScope::CatchMode::kFatal);
  if (realm->RunBootstrapping().IsEmpty()) {
    delete realm;
    return nullptr;
  }
  return realm;
}

// static
MaybeLocal<Context> HostCreateShadowRealmContextCallback(
    Local<Context> initiator_context) {
  Environment* env = Environment::GetCurrent(initiator_context);
  ShadowRealm* realm = ShadowRealm::New(env);
  if (realm != nullptr) {
    return realm->context();
  }
  return MaybeLocal<Context>();
}

// static
void ShadowRealm::WeakCallback(const v8::WeakCallbackInfo<ShadowRealm>& data) {
  ShadowRealm* realm = data.GetParameter();
  delete realm;
}

ShadowRealm::ShadowRealm(Environment* env)
    : Realm(env, NewContext(env->isolate()), kShadowRealm) {
  env->TrackShadowRealm(this);
  context_.SetWeak(this, WeakCallback, v8::WeakCallbackType::kParameter);
  CreateProperties();
}

ShadowRealm::~ShadowRealm() {
  while (HasCleanupHooks()) {
    RunCleanup();
  }
  if (env_ != nullptr) {
    env_->UntrackShadowRealm(this);
  }
}

void ShadowRealm::OnEnvironmentDestruct() {
  CHECK_NOT_NULL(env_);
  env_ = nullptr;  // This means that the shadow realm has out-lived the
                   // environment.
}

v8::Local<v8::Context> ShadowRealm::context() const {
  Local<Context> ctx = PersistentToLocal::Default(isolate_, context_);
  DCHECK(!ctx.IsEmpty());
  return ctx;
}

// V8 can not infer reference cycles between global persistent handles, e.g.
// the Realm's Context handle and the per-realm function handles.
// Attach the per-realm strong persistent values' lifetime to the context's
// global object to avoid the strong global references to the per-realm objects
// keep the context alive indefinitely.
#define V(PropertyName, TypeName)                                              \
  v8::Local<TypeName> ShadowRealm::PropertyName() const {                      \
    return PersistentToLocal::Default(isolate_, PropertyName##_);              \
  }                                                                            \
  void ShadowRealm::set_##PropertyName(v8::Local<TypeName> value) {            \
    HandleScope scope(isolate_);                                               \
    PropertyName##_.Reset(isolate_, value);                                    \
    v8::Local<v8::Context> ctx = context();                                    \
    if (value.IsEmpty()) {                                                     \
      ctx->Global()                                                            \
          ->SetPrivate(ctx,                                                    \
                       isolate_data()->per_realm_##PropertyName(),             \
                       v8::Undefined(isolate_))                                \
          .ToChecked();                                                        \
    } else {                                                                   \
      PropertyName##_.SetWeak();                                               \
      ctx->Global()                                                            \
          ->SetPrivate(ctx, isolate_data()->per_realm_##PropertyName(), value) \
          .ToChecked();                                                        \
    }                                                                          \
  }
PER_REALM_STRONG_PERSISTENT_VALUES(V)
#undef V

v8::MaybeLocal<v8::Value> ShadowRealm::BootstrapRealm() {
  HandleScope scope(isolate_);

  // Skip "internal/bootstrap/node" as it installs node globals and per-isolate
  // callbacks.

  if (!env_->no_browser_globals()) {
    if (ExecuteBootstrapper("internal/bootstrap/web/exposed-wildcard")
            .IsEmpty()) {
      return MaybeLocal<Value>();
    }
  }

  return v8::True(isolate_);
}

}  // namespace shadow_realm
}  // namespace node
