#if HAVE_OPENSSL && HAVE_QUIC
#include "guard.h"
#ifndef OPENSSL_NO_QUIC
#include <base_object-inl.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <node.h>
#include <node_errors.h>
#include <node_external_reference.h>
#include <node_mem-inl.h>
#include <node_realm-inl.h>
#include <node_sockaddr-inl.h>
#include <v8.h>
#include "bindingdata.h"
#include "session.h"
#include "session_manager.h"

namespace node {

using mem::kReserveSizeAndAlign;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

namespace quic {

// ============================================================================
// Thread-local QUIC allocator.
//
// Both ngtcp2 and nghttp3 take an allocator struct (ngtcp2_mem /
// nghttp3_mem) whose pointer is stored inside every object they
// allocate. Some of those objects — notably nghttp3 rcbufs backing
// V8 external strings — can outlive the BindingData that created them
// (freed during V8 isolate teardown, after Environment cleanup).
//
// To handle this safely, both allocators live in a thread-local static
// struct that is never destroyed. Memory tracking goes through the
// BindingData pointer when it is alive and is silently skipped during
// teardown (after ~BindingData nulls the pointer).
//
// The allocation functions use the same prepended-size-header scheme as
// NgLibMemoryManager (node_mem-inl.h) so that frees always know the
// allocation size regardless of whether BindingData is still around.

namespace {
struct QuicAllocState {
  BindingData* binding = nullptr;
  ngtcp2_mem ngtcp2 = {};
  nghttp3_mem nghttp3 = {};
};
thread_local QuicAllocState quic_alloc_state;

// Core allocation functions shared by both ngtcp2 and nghttp3.
// user_data always points to the thread-local QuicAllocState.

void* QuicRealloc(void* ptr, size_t size, void* user_data) {
  auto* state = static_cast<QuicAllocState*>(user_data);

  size_t previous_size = 0;
  char* original_ptr = nullptr;

  if (size > 0) size += kReserveSizeAndAlign;

  if (ptr != nullptr) {
    original_ptr = static_cast<char*>(ptr) - kReserveSizeAndAlign;
    previous_size = *reinterpret_cast<size_t*>(original_ptr);
    if (previous_size == 0) {
      char* ret = UncheckedRealloc(original_ptr, size);
      if (ret != nullptr) ret += kReserveSizeAndAlign;
      return ret;
    }
  }

  if (state->binding) {
    state->binding->CheckAllocatedSize(previous_size);
  }

  char* mem = UncheckedRealloc(original_ptr, size);

  if (mem != nullptr) {
    const int64_t new_size = size - previous_size;
    if (state->binding) {
      state->binding->IncreaseAllocatedSize(new_size);
      state->binding->env()->external_memory_accounter()->Update(
          state->binding->env()->isolate(), new_size);
    }
    *reinterpret_cast<size_t*>(mem) = size;
    mem += kReserveSizeAndAlign;
  } else if (size == 0) {
    if (state->binding) {
      state->binding->DecreaseAllocatedSize(previous_size);
      state->binding->env()->external_memory_accounter()->Decrease(
          state->binding->env()->isolate(), previous_size);
    }
  }
  return mem;
}

void* QuicMalloc(size_t size, void* user_data) {
  return QuicRealloc(nullptr, size, user_data);
}

void QuicFree(void* ptr, void* user_data) {
  if (ptr == nullptr) return;
  CHECK_NULL(QuicRealloc(ptr, 0, user_data));
}

void* QuicCalloc(size_t nmemb, size_t size, void* user_data) {
  size_t real_size = MultiplyWithOverflowCheck(nmemb, size);
  void* mem = QuicMalloc(real_size, user_data);
  if (mem != nullptr) memset(mem, 0, real_size);
  return mem;
}

// Thin wrappers with the correct function-pointer types for each
// library. The signatures happen to be identical today, but keeping
// them separate avoids ABI coupling between ngtcp2 and nghttp3.

void* Ngtcp2Malloc(size_t size, void* ud) {
  return QuicMalloc(size, ud);
}
void Ngtcp2Free(void* ptr, void* ud) {
  QuicFree(ptr, ud);
}
void* Ngtcp2Calloc(size_t n, size_t s, void* ud) {
  return QuicCalloc(n, s, ud);
}
void* Ngtcp2Realloc(void* ptr, size_t size, void* ud) {
  return QuicRealloc(ptr, size, ud);
}

void* Nghttp3Malloc(size_t size, void* ud) {
  return QuicMalloc(size, ud);
}
void Nghttp3Free(void* ptr, void* ud) {
  QuicFree(ptr, ud);
}
void* Nghttp3Calloc(size_t n, size_t s, void* ud) {
  return QuicCalloc(n, s, ud);
}
void* Nghttp3Realloc(void* ptr, size_t size, void* ud) {
  return QuicRealloc(ptr, size, ud);
}
}  // namespace

BindingData& BindingData::Get(Environment* env) {
  return *(env->principal_realm()->GetBindingData<BindingData>());
}

BindingData::~BindingData() {
  quic_alloc_state.binding = nullptr;
}

ngtcp2_mem* BindingData::ngtcp2_allocator() {
  quic_alloc_state.binding = this;
  quic_alloc_state.ngtcp2 = {
      &quic_alloc_state,
      Ngtcp2Malloc,
      Ngtcp2Free,
      Ngtcp2Calloc,
      Ngtcp2Realloc,
  };
  return &quic_alloc_state.ngtcp2;
}

nghttp3_mem* BindingData::nghttp3_allocator() {
  quic_alloc_state.binding = this;
  quic_alloc_state.nghttp3 = {
      &quic_alloc_state,
      Nghttp3Malloc,
      Nghttp3Free,
      Nghttp3Calloc,
      Nghttp3Realloc,
  };
  return &quic_alloc_state.nghttp3;
}

void BindingData::CheckAllocatedSize(size_t previous_size) const {
  CHECK_GE(current_ngtcp2_memory_, previous_size);
}

void BindingData::IncreaseAllocatedSize(size_t size) {
  CHECK_GE(current_ngtcp2_memory_ + size, current_ngtcp2_memory_);
  current_ngtcp2_memory_ += size;
}

void BindingData::DecreaseAllocatedSize(size_t size) {
  CHECK_LE(current_ngtcp2_memory_ - size, current_ngtcp2_memory_);
  current_ngtcp2_memory_ -= size;
}

// Forwards detailed(verbose) debugging information from nghttp3. Enabled using
// the NODE_DEBUG_NATIVE=NGHTTP3 category.
void nghttp3_debug_log(const char* fmt, va_list args) {
  auto isolate = v8::Isolate::GetCurrent();
  if (isolate == nullptr) return;
  auto env = Environment::GetCurrent(isolate);
  if (env->enabled_debug_list()->enabled(DebugCategory::NGHTTP3)) {
    fprintf(stderr, "nghttp3 ");
    vfprintf(stderr, fmt, args);
  }
}

void BindingData::InitPerContext(Realm* realm, Local<Object> target) {
  nghttp3_set_debug_vprintf_callback(nghttp3_debug_log);
  SetMethod(realm->context(), target, "setCallbacks", SetCallbacks);
  Realm::GetCurrent(realm->context())->AddBindingData<BindingData>(target);
}

void BindingData::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(IllegalConstructor);
  registry->Register(SetCallbacks);
}

BindingData::BindingData(Realm* realm, Local<Object> object)
    : BaseObject(realm, object) {
  MakeWeak();
}

SessionManager& BindingData::session_manager() {
  if (!session_manager_) {
    session_manager_ = std::make_unique<SessionManager>(env());
  }
  return *session_manager_;
}

void BindingData::MemoryInfo(MemoryTracker* tracker) const {
#define V(name, _) tracker->TrackField(#name, name##_callback());

  QUIC_JS_CALLBACKS(V)

#undef V

#define V(name, _) tracker->TrackField(#name, name##_string());

  QUIC_STRINGS(V)

#undef V
}

#define V(name)                                                                \
  void BindingData::set_##name##_constructor_template(                         \
      Local<FunctionTemplate> tmpl) {                                          \
    name##_constructor_template_.Reset(env()->isolate(), tmpl);                \
  }                                                                            \
  Local<FunctionTemplate> BindingData::name##_constructor_template() const {   \
    return PersistentToLocal::Default(env()->isolate(),                        \
                                      name##_constructor_template_);           \
  }

QUIC_CONSTRUCTORS(V)

#undef V

#define V(name, _)                                                             \
  void BindingData::set_##name##_callback(Local<Function> fn) {                \
    name##_callback_.Reset(env()->isolate(), fn);                              \
  }                                                                            \
  Local<Function> BindingData::name##_callback() const {                       \
    return PersistentToLocal::Default(env()->isolate(), name##_callback_);     \
  }

QUIC_JS_CALLBACKS(V)

#undef V

#define V(name, value)                                                         \
  Local<String> BindingData::name##_string() const {                           \
    if (name##_string_.IsEmpty())                                              \
      name##_string_.Set(env()->isolate(),                                     \
                         OneByteString(env()->isolate(), value));              \
    return name##_string_.Get(env()->isolate());                               \
  }

QUIC_STRINGS(V)

#undef V

#define V(name, value)                                                         \
  Local<String> BindingData::on_##name##_string() const {                      \
    if (on_##name##_string_.IsEmpty())                                         \
      on_##name##_string_.Set(                                                 \
          env()->isolate(),                                                    \
          FIXED_ONE_BYTE_STRING(env()->isolate(), "on" #value));               \
    return on_##name##_string_.Get(env()->isolate());                          \
  }

QUIC_JS_CALLBACKS(V)

#undef V

JS_METHOD_IMPL(BindingData::SetCallbacks) {
  auto env = Environment::GetCurrent(args);
  auto isolate = env->isolate();
  auto& state = Get(env);
  CHECK(args[0]->IsObject());
  Local<Object> obj = args[0].As<Object>();

#define V(name, key)                                                           \
  do {                                                                         \
    Local<Value> val;                                                          \
    if (!obj->Get(env->context(), state.on_##name##_string()).ToLocal(&val) || \
        !val->IsFunction()) {                                                  \
      return THROW_ERR_MISSING_ARGS(isolate, "Missing Callback: on" #key);     \
    }                                                                          \
    state.set_##name##_callback(val.As<Function>());                           \
  } while (0);

  QUIC_JS_CALLBACKS(V)

#undef V
}

NgTcp2CallbackScope::NgTcp2CallbackScope(Session* session) : session(session) {
  CHECK(!session->in_ngtcp2_callback_scope_);
  session->in_ngtcp2_callback_scope_ = true;
}

NgTcp2CallbackScope::~NgTcp2CallbackScope() {
  session->in_ngtcp2_callback_scope_ = false;
  if (session->destroy_deferred_) {
    session->destroy_deferred_ = false;
    session->Destroy();
  }
}

NgHttp3CallbackScope::NgHttp3CallbackScope(Session* session)
    : session(session) {
  CHECK(!session->in_nghttp3_callback_scope_);
  session->in_nghttp3_callback_scope_ = true;
}

NgHttp3CallbackScope::~NgHttp3CallbackScope() {
  session->in_nghttp3_callback_scope_ = false;
  if (session->destroy_deferred_) {
    session->destroy_deferred_ = false;
    session->Destroy();
  }
}

CallbackScopeBase::CallbackScopeBase(Environment* env)
    : env(env), context_scope(env->context()), try_catch(env->isolate()) {}

CallbackScopeBase::~CallbackScopeBase() {
  if (try_catch.HasCaught()) {
    if (!try_catch.HasTerminated() && env->can_call_into_js()) {
      errors::TriggerUncaughtException(env->isolate(), try_catch);
    } else {
      try_catch.ReThrow();
    }
  }
}

JS_METHOD_IMPL(IllegalConstructor) {
  THROW_ERR_ILLEGAL_CONSTRUCTOR(Environment::GetCurrent(args));
}

}  // namespace quic
}  // namespace node

#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
