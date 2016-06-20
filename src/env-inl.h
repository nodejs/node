#ifndef SRC_ENV_INL_H_
#define SRC_ENV_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "node.h"
#include "util.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"

#include <stddef.h>
#include <stdint.h>

namespace node {

inline Environment::IsolateData* Environment::IsolateData::Get(
    v8::Isolate* isolate) {
  return static_cast<IsolateData*>(isolate->GetData(kIsolateSlot));
}

inline Environment::IsolateData* Environment::IsolateData::GetOrCreate(
    v8::Isolate* isolate, uv_loop_t* loop) {
  IsolateData* isolate_data = Get(isolate);
  if (isolate_data == nullptr) {
    isolate_data = new IsolateData(isolate, loop);
    isolate->SetData(kIsolateSlot, isolate_data);
  }
  isolate_data->ref_count_ += 1;
  return isolate_data;
}

inline void Environment::IsolateData::Put() {
  if (--ref_count_ == 0) {
    isolate()->SetData(kIsolateSlot, nullptr);
    delete this;
  }
}

// Create string properties as internalized one byte strings.
//
// Internalized because it makes property lookups a little faster and because
// the string is created in the old space straight away.  It's going to end up
// in the old space sooner or later anyway but now it doesn't go through
// v8::Eternal's new space handling first.
//
// One byte because our strings are ASCII and we can safely skip V8's UTF-8
// decoding step.  It's a one-time cost, but why pay it when you don't have to?
inline Environment::IsolateData::IsolateData(v8::Isolate* isolate,
                                             uv_loop_t* loop)
    : event_loop_(loop),
      isolate_(isolate),
#define V(PropertyName, StringValue)                                          \
    PropertyName ## _(                                                        \
        isolate,                                                              \
        v8::Private::ForApi(                                                  \
            isolate,                                                          \
            v8::String::NewFromOneByte(                                       \
                isolate,                                                      \
                reinterpret_cast<const uint8_t*>(StringValue),                \
                v8::NewStringType::kInternalized,                             \
                sizeof(StringValue) - 1).ToLocalChecked())),
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)
#undef V
#define V(PropertyName, StringValue)                                          \
    PropertyName ## _(                                                        \
        isolate,                                                              \
        v8::String::NewFromOneByte(                                           \
            isolate,                                                          \
            reinterpret_cast<const uint8_t*>(StringValue),                    \
            v8::NewStringType::kInternalized,                                 \
            sizeof(StringValue) - 1).ToLocalChecked()),
    PER_ISOLATE_STRING_PROPERTIES(V)
#undef V
    ref_count_(0) {}

inline uv_loop_t* Environment::IsolateData::event_loop() const {
  return event_loop_;
}

inline v8::Isolate* Environment::IsolateData::isolate() const {
  return isolate_;
}

inline Environment::AsyncHooks::AsyncHooks() {
  for (int i = 0; i < kFieldsCount; i++) fields_[i] = 0;
}

inline uint32_t* Environment::AsyncHooks::fields() {
  return fields_;
}

inline int Environment::AsyncHooks::fields_count() const {
  return kFieldsCount;
}

inline bool Environment::AsyncHooks::callbacks_enabled() {
  return fields_[kEnableCallbacks] != 0;
}

inline void Environment::AsyncHooks::set_enable_callbacks(uint32_t flag) {
  fields_[kEnableCallbacks] = flag;
}

inline Environment::AsyncCallbackScope::AsyncCallbackScope(Environment* env)
    : env_(env) {
  env_->makecallback_cntr_++;
}

inline Environment::AsyncCallbackScope::~AsyncCallbackScope() {
  env_->makecallback_cntr_--;
}

inline bool Environment::AsyncCallbackScope::in_makecallback() {
  return env_->makecallback_cntr_ > 1;
}

inline Environment::DomainFlag::DomainFlag() {
  for (int i = 0; i < kFieldsCount; ++i) fields_[i] = 0;
}

inline uint32_t* Environment::DomainFlag::fields() {
  return fields_;
}

inline int Environment::DomainFlag::fields_count() const {
  return kFieldsCount;
}

inline uint32_t Environment::DomainFlag::count() const {
  return fields_[kCount];
}

inline Environment::TickInfo::TickInfo() {
  for (int i = 0; i < kFieldsCount; ++i)
    fields_[i] = 0;
}

inline uint32_t* Environment::TickInfo::fields() {
  return fields_;
}

inline int Environment::TickInfo::fields_count() const {
  return kFieldsCount;
}

inline uint32_t Environment::TickInfo::index() const {
  return fields_[kIndex];
}

inline uint32_t Environment::TickInfo::length() const {
  return fields_[kLength];
}

inline void Environment::TickInfo::set_index(uint32_t value) {
  fields_[kIndex] = value;
}

inline Environment::ArrayBufferAllocatorInfo::ArrayBufferAllocatorInfo() {
  for (int i = 0; i < kFieldsCount; ++i)
    fields_[i] = 0;
}

inline uint32_t* Environment::ArrayBufferAllocatorInfo::fields() {
  return fields_;
}

inline int Environment::ArrayBufferAllocatorInfo::fields_count() const {
  return kFieldsCount;
}

inline bool Environment::ArrayBufferAllocatorInfo::no_zero_fill() const {
  return fields_[kNoZeroFill] != 0;
}

inline void Environment::ArrayBufferAllocatorInfo::reset_fill_flag() {
  fields_[kNoZeroFill] = 0;
}

inline Environment* Environment::New(v8::Local<v8::Context> context,
                                     uv_loop_t* loop) {
  Environment* env = new Environment(context, loop);
  env->AssignToContext(context);
  return env;
}

inline void Environment::AssignToContext(v8::Local<v8::Context> context) {
  context->SetAlignedPointerInEmbedderData(kContextEmbedderDataIndex, this);
}

inline Environment* Environment::GetCurrent(v8::Isolate* isolate) {
  return GetCurrent(isolate->GetCurrentContext());
}

inline Environment* Environment::GetCurrent(v8::Local<v8::Context> context) {
  return static_cast<Environment*>(
      context->GetAlignedPointerFromEmbedderData(kContextEmbedderDataIndex));
}

inline Environment* Environment::GetCurrent(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ASSERT(info.Data()->IsExternal());
  return static_cast<Environment*>(info.Data().As<v8::External>()->Value());
}

template <typename T>
inline Environment* Environment::GetCurrent(
    const v8::PropertyCallbackInfo<T>& info) {
  ASSERT(info.Data()->IsExternal());
  // XXX(bnoordhuis) Work around a g++ 4.9.2 template type inferrer bug
  // when the expression is written as info.Data().As<v8::External>().
  v8::Local<v8::Value> data = info.Data();
  return static_cast<Environment*>(data.As<v8::External>()->Value());
}

inline Environment::Environment(v8::Local<v8::Context> context,
                                uv_loop_t* loop)
    : isolate_(context->GetIsolate()),
      isolate_data_(IsolateData::GetOrCreate(context->GetIsolate(), loop)),
      timer_base_(uv_now(loop)),
      using_domains_(false),
      printed_error_(false),
      trace_sync_io_(false),
      makecallback_cntr_(0),
      async_wrap_uid_(0),
      debugger_agent_(this),
#if HAVE_INSPECTOR
      inspector_agent_(this),
#endif
      http_parser_buffer_(nullptr),
      context_(context->GetIsolate(), context) {
  // We'll be creating new objects so make sure we've entered the context.
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(context);
  set_as_external(v8::External::New(isolate(), this));
  set_binding_cache_object(v8::Object::New(isolate()));
  set_module_load_list_array(v8::Array::New(isolate()));

  v8::Local<v8::FunctionTemplate> fn = v8::FunctionTemplate::New(isolate());
  fn->SetClassName(FIXED_ONE_BYTE_STRING(isolate(), "InternalFieldObject"));
  v8::Local<v8::ObjectTemplate> obj = fn->InstanceTemplate();
  obj->SetInternalFieldCount(1);
  set_generic_internal_field_template(obj);

  RB_INIT(&cares_task_list_);
  handle_cleanup_waiting_ = 0;
}

inline Environment::~Environment() {
  v8::HandleScope handle_scope(isolate());

  context()->SetAlignedPointerInEmbedderData(kContextEmbedderDataIndex,
                                             nullptr);
#define V(PropertyName, TypeName) PropertyName ## _.Reset();
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V
  isolate_data()->Put();

  delete[] heap_statistics_buffer_;
  delete[] heap_space_statistics_buffer_;
  delete[] http_parser_buffer_;
}

inline void Environment::CleanupHandles() {
  while (HandleCleanup* hc = handle_cleanup_queue_.PopFront()) {
    handle_cleanup_waiting_++;
    hc->cb_(this, hc->handle_, hc->arg_);
    delete hc;
  }

  while (handle_cleanup_waiting_ != 0)
    uv_run(event_loop(), UV_RUN_ONCE);
}

inline void Environment::Dispose() {
  delete this;
}

inline v8::Isolate* Environment::isolate() const {
  return isolate_;
}

inline bool Environment::async_wrap_callbacks_enabled() const {
  // The const_cast is okay, it doesn't violate conceptual const-ness.
  return const_cast<Environment*>(this)->async_hooks()->callbacks_enabled();
}

inline bool Environment::in_domain() const {
  // The const_cast is okay, it doesn't violate conceptual const-ness.
  return using_domains() &&
         const_cast<Environment*>(this)->domain_flag()->count() > 0;
}

inline Environment* Environment::from_immediate_check_handle(
    uv_check_t* handle) {
  return ContainerOf(&Environment::immediate_check_handle_, handle);
}

inline uv_check_t* Environment::immediate_check_handle() {
  return &immediate_check_handle_;
}

inline uv_idle_t* Environment::immediate_idle_handle() {
  return &immediate_idle_handle_;
}

inline Environment* Environment::from_idle_prepare_handle(
    uv_prepare_t* handle) {
  return ContainerOf(&Environment::idle_prepare_handle_, handle);
}

inline uv_prepare_t* Environment::idle_prepare_handle() {
  return &idle_prepare_handle_;
}

inline Environment* Environment::from_idle_check_handle(uv_check_t* handle) {
  return ContainerOf(&Environment::idle_check_handle_, handle);
}

inline uv_check_t* Environment::idle_check_handle() {
  return &idle_check_handle_;
}

inline void Environment::RegisterHandleCleanup(uv_handle_t* handle,
                                               HandleCleanupCb cb,
                                               void *arg) {
  handle_cleanup_queue_.PushBack(new HandleCleanup(handle, cb, arg));
}

inline void Environment::FinishHandleCleanup(uv_handle_t* handle) {
  handle_cleanup_waiting_--;
}

inline uv_loop_t* Environment::event_loop() const {
  return isolate_data()->event_loop();
}

inline Environment::AsyncHooks* Environment::async_hooks() {
  return &async_hooks_;
}

inline Environment::DomainFlag* Environment::domain_flag() {
  return &domain_flag_;
}

inline Environment::TickInfo* Environment::tick_info() {
  return &tick_info_;
}

inline Environment::ArrayBufferAllocatorInfo*
    Environment::array_buffer_allocator_info() {
  return &array_buffer_allocator_info_;
}

inline uint64_t Environment::timer_base() const {
  return timer_base_;
}

inline bool Environment::using_domains() const {
  return using_domains_;
}

inline void Environment::set_using_domains(bool value) {
  using_domains_ = value;
}

inline bool Environment::printed_error() const {
  return printed_error_;
}

inline void Environment::set_printed_error(bool value) {
  printed_error_ = value;
}

inline void Environment::set_trace_sync_io(bool value) {
  trace_sync_io_ = value;
}

inline int64_t Environment::get_async_wrap_uid() {
  return ++async_wrap_uid_;
}

inline uint32_t* Environment::heap_statistics_buffer() const {
  CHECK_NE(heap_statistics_buffer_, nullptr);
  return heap_statistics_buffer_;
}

inline void Environment::set_heap_statistics_buffer(uint32_t* pointer) {
  CHECK_EQ(heap_statistics_buffer_, nullptr);  // Should be set only once.
  heap_statistics_buffer_ = pointer;
}

inline uint32_t* Environment::heap_space_statistics_buffer() const {
  CHECK_NE(heap_space_statistics_buffer_, nullptr);
  return heap_space_statistics_buffer_;
}

inline void Environment::set_heap_space_statistics_buffer(uint32_t* pointer) {
  CHECK_EQ(heap_space_statistics_buffer_, nullptr);  // Should be set only once.
  heap_space_statistics_buffer_ = pointer;
}


inline char* Environment::http_parser_buffer() const {
  return http_parser_buffer_;
}

inline void Environment::set_http_parser_buffer(char* buffer) {
  CHECK_EQ(http_parser_buffer_, nullptr);  // Should be set only once.
  http_parser_buffer_ = buffer;
}

inline Environment* Environment::from_cares_timer_handle(uv_timer_t* handle) {
  return ContainerOf(&Environment::cares_timer_handle_, handle);
}

inline uv_timer_t* Environment::cares_timer_handle() {
  return &cares_timer_handle_;
}

inline ares_channel Environment::cares_channel() {
  return cares_channel_;
}

// Only used in the call to ares_init_options().
inline ares_channel* Environment::cares_channel_ptr() {
  return &cares_channel_;
}

inline node_ares_task_list* Environment::cares_task_list() {
  return &cares_task_list_;
}

inline Environment::IsolateData* Environment::isolate_data() const {
  return isolate_data_;
}

// this would have been a template function were it not for the fact that g++
// sometimes fails to resolve it...
#define THROW_ERROR(fun)                                                      \
  do {                                                                        \
    v8::HandleScope scope(isolate);                                           \
    isolate->ThrowException(fun(OneByteString(isolate, errmsg)));             \
  }                                                                           \
  while (0)

inline void Environment::ThrowError(v8::Isolate* isolate, const char* errmsg) {
  THROW_ERROR(v8::Exception::Error);
}

inline void Environment::ThrowTypeError(v8::Isolate* isolate,
                                        const char* errmsg) {
  THROW_ERROR(v8::Exception::TypeError);
}

inline void Environment::ThrowRangeError(v8::Isolate* isolate,
                                         const char* errmsg) {
  THROW_ERROR(v8::Exception::RangeError);
}

inline void Environment::ThrowError(const char* errmsg) {
  ThrowError(isolate(), errmsg);
}

inline void Environment::ThrowTypeError(const char* errmsg) {
  ThrowTypeError(isolate(), errmsg);
}

inline void Environment::ThrowRangeError(const char* errmsg) {
  ThrowRangeError(isolate(), errmsg);
}

inline void Environment::ThrowErrnoException(int errorno,
                                             const char* syscall,
                                             const char* message,
                                             const char* path) {
  isolate()->ThrowException(
      ErrnoException(isolate(), errorno, syscall, message, path));
}

inline void Environment::ThrowUVException(int errorno,
                                          const char* syscall,
                                          const char* message,
                                          const char* path,
                                          const char* dest) {
  isolate()->ThrowException(
      UVException(isolate(), errorno, syscall, message, path, dest));
}

inline v8::Local<v8::FunctionTemplate>
    Environment::NewFunctionTemplate(v8::FunctionCallback callback,
                                     v8::Local<v8::Signature> signature) {
  v8::Local<v8::External> external = as_external();
  return v8::FunctionTemplate::New(isolate(), callback, external, signature);
}

inline void Environment::SetMethod(v8::Local<v8::Object> that,
                                   const char* name,
                                   v8::FunctionCallback callback) {
  v8::Local<v8::Function> function =
      NewFunctionTemplate(callback)->GetFunction();
  // kInternalized strings are created in the old space.
  const v8::NewStringType type = v8::NewStringType::kInternalized;
  v8::Local<v8::String> name_string =
      v8::String::NewFromUtf8(isolate(), name, type).ToLocalChecked();
  that->Set(name_string, function);
  function->SetName(name_string);  // NODE_SET_METHOD() compatibility.
}

inline void Environment::SetProtoMethod(v8::Local<v8::FunctionTemplate> that,
                                        const char* name,
                                        v8::FunctionCallback callback) {
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate(), that);
  v8::Local<v8::FunctionTemplate> t = NewFunctionTemplate(callback, signature);
  // kInternalized strings are created in the old space.
  const v8::NewStringType type = v8::NewStringType::kInternalized;
  v8::Local<v8::String> name_string =
      v8::String::NewFromUtf8(isolate(), name, type).ToLocalChecked();
  that->PrototypeTemplate()->Set(name_string, t);
  t->SetClassName(name_string);  // NODE_SET_PROTOTYPE_METHOD() compatibility.
}

inline void Environment::SetTemplateMethod(v8::Local<v8::FunctionTemplate> that,
                                           const char* name,
                                           v8::FunctionCallback callback) {
  v8::Local<v8::FunctionTemplate> t = NewFunctionTemplate(callback);
  // kInternalized strings are created in the old space.
  const v8::NewStringType type = v8::NewStringType::kInternalized;
  v8::Local<v8::String> name_string =
      v8::String::NewFromUtf8(isolate(), name, type).ToLocalChecked();
  that->Set(name_string, t);
  t->SetClassName(name_string);  // NODE_SET_METHOD() compatibility.
}

inline v8::Local<v8::Object> Environment::NewInternalFieldObject() {
  v8::MaybeLocal<v8::Object> m_obj =
      generic_internal_field_template()->NewInstance(context());
  return m_obj.ToLocalChecked();
}

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName, StringValue)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName, StringValue)
#define V(TypeName, PropertyName, StringValue)                                \
  inline                                                                      \
  v8::Local<TypeName> Environment::IsolateData::PropertyName() const {        \
    /* Strings are immutable so casting away const-ness here is okay. */      \
    return const_cast<IsolateData*>(this)->PropertyName ## _.Get(isolate());  \
  }
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VS
#undef VP

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName, StringValue)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName, StringValue)
#define V(TypeName, PropertyName, StringValue)                                \
  inline v8::Local<TypeName> Environment::PropertyName() const {              \
    return isolate_data()->PropertyName();                                    \
  }
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VS
#undef VP

#define V(PropertyName, TypeName)                                             \
  inline v8::Local<TypeName> Environment::PropertyName() const {              \
    return StrongPersistentToLocal(PropertyName ## _);                        \
  }                                                                           \
  inline void Environment::set_ ## PropertyName(v8::Local<TypeName> value) {  \
    PropertyName ## _.Reset(isolate(), value);                                \
  }
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V

#undef ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES
#undef PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES
#undef PER_ISOLATE_STRING_PROPERTIES

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ENV_INL_H_
