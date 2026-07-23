// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node_cpu_profiler.h"

#include <unordered_map>

#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "util-inl.h"
#include "v8-profiler.h"
#include "v8.h"

namespace node {
namespace cpu_profiler {

namespace i = v8::internal;

using v8::Context;
using v8::CpuProfile;
using v8::CpuProfiler;
using v8::CpuProfilingOptions;
using v8::CpuProfilingResult;
using v8::CpuProfilingStatus;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::Value;

namespace {

// Per-isolate active profiler, using thread_local because in Node's threading
// model each isolate is bound to one thread.
thread_local SampledCpuProfiler* t_active_profiler = nullptr;

constexpr int kHolderInternalFieldCount = 1;
constexpr int kHolderSharedPtrSlot = 0;

// Weak callback: when a ContextHolder JS object becomes unreachable, free the
// heap-allocated shared_ptr_slot.
struct HolderWeakData {
  std::shared_ptr<Global<Value>>* shared_ptr_slot;
  Global<Object> handle;
};

void HolderWeakCallback(const v8::WeakCallbackInfo<HolderWeakData>& info) {
  HolderWeakData* data = info.GetParameter();
  delete data->shared_ptr_slot;
  delete data;
}

// Extracts the address of a v8::Global<Value>'s underlying slot. The slot is
// V8's persistent-handle entry: its address is stable for the Global's
// lifetime, while V8 keeps the slot's *contents* (the tagged pointer to the
// referent) GC-coherently updated through compaction. Reading through this
// slot in signal-handler context is signal-safe (single load) and yields the
// current address of the referent.
//
// `slot()` is protected on v8::api_internal::IndirectHandleBase; we expose it
// by deriving and using-declaring it as public. The derived class adds no
// data members, so static_cast'ing a v8::Global<Value>* to this is layout-
// compatible.
class GlobalSlotAccessor : public v8::Global<v8::Value> {
 public:
  using v8::Global<v8::Value>::Global;
  using v8::Global<v8::Value>::slot;
};

i::Address* SlotOf(v8::Global<v8::Value>* g) {
  return static_cast<GlobalSlotAccessor*>(g)->slot();
}

// Builds the structured JS profile tree returned by Stop().
//
// Lifetime: the v8::CpuProfile* and Session* are borrowed and not retained
// past Run(). Callers can free both after Run() returns; the resulting
// JS objects are plain (no native back-pointers).
class ProfileBuilder {
 public:
  ProfileBuilder(v8::Isolate* isolate,
                 v8::Local<v8::Context> context,
                 const v8::CpuProfile* profile,
                 Session* session)
      : isolate_(isolate),
        context_(context),
        profile_(profile),
        session_(session),
        // Property name strings are created once and reused for every object.
        k_start_time_(NewKey("startTime")),
        k_end_time_(NewKey("endTime")),
        k_dropped_contexts_(NewKey("droppedContexts")),
        k_top_down_root_(NewKey("topDownRoot")),
        k_function_name_(NewKey("functionName")),
        k_script_name_(NewKey("scriptName")),
        k_line_number_(NewKey("lineNumber")),
        k_column_number_(NewKey("columnNumber")),
        k_hit_count_(NewKey("hitCount")),
        k_contexts_(NewKey("contexts")),
        k_children_(NewKey("children")),
        k_context_(NewKey("context")),
        k_timestamp_(NewKey("timestamp")) {
    // Pre-bucket samples by node so each node's contexts can be assembled in
    // one pass. node_to_samples_[node] -> indices into profile_->samples.
    int sample_count = profile_->GetSamplesCount();
    for (int i = 0; i < sample_count; ++i) {
      const v8::CpuProfileNode* node = profile_->GetSample(i);
      if (node == nullptr) continue;
      node_to_samples_[node].push_back(i);
    }
  }

  v8::Local<v8::Object> Run() {
    v8::Local<v8::Object> result = v8::Object::New(isolate_);
    Set(result, k_start_time_,
               v8::Number::New(isolate_,
                               static_cast<double>(profile_->GetStartTime())));
    Set(result, k_end_time_,
               v8::Number::New(isolate_,
                               static_cast<double>(profile_->GetEndTime())));
    Set(
        result, k_dropped_contexts_,
        v8::Number::New(isolate_, static_cast<double>(session_->dropped.load(
                                      std::memory_order_relaxed))));
    Set(result, k_top_down_root_, BuildNode(profile_->GetTopDownRoot()));
    return result;
  }

 private:
  v8::Local<v8::String> NewKey(const char* s) {
    return v8::String::NewFromUtf8(isolate_, s, v8::NewStringType::kInternalized)
        .ToLocalChecked();
  }

  void Set(v8::Local<v8::Object> obj,
                  v8::Local<v8::String> key,
                  v8::Local<v8::Value> value) {
    obj->Set(context_, key, value).Check();
  }

  v8::Local<v8::Object> BuildNode(const v8::CpuProfileNode* node) {
    v8::Local<v8::Object> obj = v8::Object::New(isolate_);

    Set(obj, k_function_name_, node->GetFunctionName());
    Set(obj, k_script_name_, node->GetScriptResourceName());
    Set(obj, k_line_number_,
               v8::Integer::New(isolate_, node->GetLineNumber()));
    Set(obj, k_column_number_,
               v8::Integer::New(isolate_, node->GetColumnNumber()));
    Set(obj, k_hit_count_,
               v8::Integer::NewFromUnsigned(isolate_, node->GetHitCount()));

    auto it = node_to_samples_.find(node);
    if (it != node_to_samples_.end()) {
      // Buffer the context-bearing entries in a vector first, then allocate
      // the JS array sized exactly to that count. Sizing the array up front
      // by sample_indices.size() and skipping context-less samples leaves
      // trailing `undefined` holes.
      std::vector<v8::Local<v8::Value>> ctx_objs;
      ctx_objs.reserve(it->second.size());
      for (int sample_idx : it->second) {
        v8::Local<v8::Value> ctx_value = ContextValueAt(sample_idx);
        if (ctx_value.IsEmpty()) continue;
        v8::Local<v8::Object> ctx_obj = v8::Object::New(isolate_);
        Set(ctx_obj, k_context_, ctx_value);
        Set(ctx_obj, k_timestamp_,
                   v8::Number::New(isolate_,
                                   static_cast<double>(
                                       profile_->GetSampleTimestamp(sample_idx))));
        ctx_objs.push_back(ctx_obj);
      }
      // If every sample at this node was context-less, omit the field rather
      // than setting an empty array.
      if (!ctx_objs.empty()) {
        v8::Local<v8::Array> contexts =
            v8::Array::New(isolate_, ctx_objs.data(), ctx_objs.size());
        Set(obj, k_contexts_, contexts);
      }
    }

    int child_count = node->GetChildrenCount();
    v8::Local<v8::Array> children = v8::Array::New(isolate_, child_count);
    for (int i = 0; i < child_count; ++i) {
      children->Set(context_, static_cast<uint32_t>(i),
                    BuildNode(node->GetChild(i)))
          .Check();
    }
    Set(obj, k_children_, children);

    return obj;
  }

  // Resolves a sample's context void* (slot index + 1, or nullptr) back to
  // the JS value its ContextHolder wrapped. Returns Local<>() if the sample
  // had no associated context.
  v8::Local<v8::Value> ContextValueAt(int sample_index) {
    void* sample_ctx = profile_->GetSampleContext(sample_index);
    if (sample_ctx == nullptr) return {};
    uintptr_t encoded = reinterpret_cast<uintptr_t>(sample_ctx);
    if (encoded == 0) return {};
    uint64_t slot_idx = encoded - 1;
    if (slot_idx >= session_->context_buffer.size()) return {};
    auto& shared = session_->context_buffer[slot_idx];
    if (!shared) return {};
    return shared->Get(isolate_);
  }

  v8::Isolate* const isolate_;
  v8::Local<v8::Context> context_;
  const v8::CpuProfile* const profile_;
  Session* const session_;

  std::unordered_map<const v8::CpuProfileNode*, std::vector<int>>
      node_to_samples_;

  v8::Local<v8::String> k_start_time_;
  v8::Local<v8::String> k_end_time_;
  v8::Local<v8::String> k_dropped_contexts_;
  v8::Local<v8::String> k_top_down_root_;
  v8::Local<v8::String> k_function_name_;
  v8::Local<v8::String> k_script_name_;
  v8::Local<v8::String> k_line_number_;
  v8::Local<v8::String> k_column_number_;
  v8::Local<v8::String> k_hit_count_;
  v8::Local<v8::String> k_contexts_;
  v8::Local<v8::String> k_children_;
  v8::Local<v8::String> k_context_;
  v8::Local<v8::String> k_timestamp_;
};

}  // namespace

Session::Session(size_t buffer_size) : context_buffer(buffer_size) {}

SampledCpuProfiler::SampledCpuProfiler(
    Environment* env,
    Local<Object> object,
    Local<ObjectTemplate> holder_template,
    Local<Value> als_resource_key,
    size_t context_buffer_size,
    int sampling_interval_us,
    uint32_t max_samples,
    bool with_context)
    : BaseObject(env, object),
      holder_template_(env->isolate(), holder_template),
      als_resource_key_(env->isolate(), als_resource_key),
      context_buffer_size_(context_buffer_size),
      sampling_interval_us_(sampling_interval_us),
      max_samples_(max_samples),
      with_context_(with_context) {
  als_key_slot_ = reinterpret_cast<uintptr_t*>(SlotOf(&als_resource_key_));
  MakeWeak();
}

SampledCpuProfiler::~SampledCpuProfiler() {
  // Best-effort teardown for the case where the JS handle is dropped without
  // calling stop()/stopAndCapture(). If a session is still active, drain it
  // (this also clears t_active_profiler via StopActiveSession). The profile
  // we'd otherwise return is just discarded.
  if (active_session_.load(std::memory_order_relaxed) != nullptr) {
    CpuProfile* profile = nullptr;
    Session* session = nullptr;
    StopActiveSession(/* restart = */ false, &profile, &session);
    if (profile != nullptr) profile->Delete();
    delete session;
  }
  // Defensive: should already be cleared by StopActiveSession (or never set
  // if no session was running), but stale TLS pointing at a freed instance
  // would break future Start calls on this thread.
  if (t_active_profiler == this) {
    t_active_profiler = nullptr;
  }
  // Tearing down the v8::CpuProfiler ends its sampling thread (StopProcessor)
  // and unregisters the GC pro/epilogue callbacks we installed against
  // processor_.
  if (cpu_profiler_ != nullptr) {
    cpu_profiler_->Dispose();
    cpu_profiler_ = nullptr;
  }
}

void SampledCpuProfiler::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  CHECK(args.IsConstructCall());
  CHECK_GE(args.Length(), 5);
  CHECK(args[0]->IsObject());  // ALS resource key (the ALS instance)
  CHECK(args[1]->IsUint32());  // context buffer size
  CHECK(args[2]->IsInt32());   // sampling interval (microseconds)
  CHECK(args[3]->IsUint32());  // V8 max samples
  CHECK(args[4]->IsBoolean()); // with_context

  Local<Value> als_resource_key = args[0];
  uint32_t buffer_size =
      args[1]->Uint32Value(env->context()).FromJust();
  int sampling_interval_us =
      args[2]->Int32Value(env->context()).FromJust();
  uint32_t max_samples =
      args[3]->Uint32Value(env->context()).FromJust();
  bool with_context = args[4]->BooleanValue(isolate);

  // ContextHolder template: one aligned-pointer internal field for the
  // heap-allocated shared_ptr<Global<Value>>.
  Local<ObjectTemplate> holder_template = ObjectTemplate::New(isolate);
  holder_template->SetInternalFieldCount(kHolderInternalFieldCount);

  new SampledCpuProfiler(env,
                         args.This(),
                         holder_template,
                         als_resource_key,
                         with_context ? buffer_size : 0,
                         sampling_interval_us,
                         max_samples,
                         with_context);
}



void SampledCpuProfiler::Start(const FunctionCallbackInfo<Value>& args) {
  SampledCpuProfiler* self;
  ASSIGN_OR_RETURN_UNWRAP(&self, args.This());
  Isolate* isolate = args.GetIsolate();

  if (self->active_session_.load(std::memory_order_relaxed) != nullptr) {
    THROW_ERR_INVALID_STATE(self->env(),
                            "CPU profile is already running");
    return;
  }

  // Can only have one active profiler per isolate/thread.
  if (t_active_profiler != nullptr) {
    THROW_ERR_INVALID_STATE(
        self->env(),
        "Another CPU profile is already active in this isolate");
    return;
  }
  t_active_profiler = self;

  auto* session = new Session(self->context_buffer_size_);
  // Publish the session before V8 starts sampling so any tick that fires
  // immediately after Start sees a valid session. Compiler fence ensures
  // the Session's construction is sequenced before its publication; the
  // signal handler's matching atomic_signal_fence(acquire) sees the fully
  // constructed Session.
  std::atomic_signal_fence(std::memory_order_release);
  self->active_session_.store(session, std::memory_order_relaxed);

  if (self->cpu_profiler_ == nullptr) {
    self->cpu_profiler_ = CpuProfiler::New(isolate);
  }

  // Only install the per-sample extractor when the embedder asked for
  // context tracking; otherwise samples are captured at the same cost as
  // the original v8.startCpuProfile().
  CpuProfilingOptions options(
      v8::kLeafNodeLineNumbers, self->max_samples_,
      self->sampling_interval_us_, v8::MaybeLocal<v8::Context>(),
      v8::CpuProfileSource::kUnspecified,
      self->with_context_ ? &SampledCpuProfiler::ExtractContext : nullptr);

  CpuProfilingResult result = self->cpu_profiler_->Start(
      v8::String::Empty(isolate), std::move(options));
  if (result.status != CpuProfilingStatus::kStarted &&
      result.status != CpuProfilingStatus::kAlreadyStarted) {
    t_active_profiler = nullptr;
    self->active_session_.store(nullptr, std::memory_order_relaxed);
    delete session;
    THROW_ERR_INVALID_STATE(self->env(),
                            "v8::CpuProfiler failed to start the profile");
    return;
  }

  self->profile_id_ = result.id;

  // Install the GC prologue/epilogue callbacks when using contexts.
  if (self->with_context_) {
    isolate->AddGCPrologueCallback(&OnGCPrologue, self);
    isolate->AddGCEpilogueCallback(&OnGCEpilogue, self);
  }
}

// Detaches the active Session and stops the V8 CpuProfile so the caller can
// drain them. If `restart` is true, allocates a fresh Session, atomically
// publishes it as the new active Session, and starts a new V8 profile so
// sampling continues with minimal gap.
bool SampledCpuProfiler::StopActiveSession(bool restart,
                                           CpuProfile** out_profile,
                                           Session** out_session) {
  Session* session = active_session_.load(std::memory_order_relaxed);
  if (session == nullptr) {
    return false;
  }

  v8::Isolate* isolate = env()->isolate();

  // Unregister GC callbacks first.
  if (with_context_) {
    isolate->RemoveGCPrologueCallback(&OnGCPrologue, this);
    isolate->RemoveGCEpilogueCallback(&OnGCEpilogue, this);
  }
  in_gc_.store(false, std::memory_order_relaxed);
  gc_cached_aligned_ptr_.store(nullptr, std::memory_order_relaxed);

  // Detach active profiler from thread. New signal handler invocations observe
  // nullptr and bail.
  t_active_profiler = nullptr;

  // Stop V8's profile; in-flight ticks for our profile id are drained.
  CpuProfile* profile = cpu_profiler_->Stop(profile_id_);

  // Detach the Session pointer.
  active_session_.store(nullptr, std::memory_order_relaxed);

  *out_profile = profile;
  *out_session = session;

  if (!restart) {
    return true;
  }

  // Allocate, start, and publish the new session.
  Session* new_session = new Session(context_buffer_size_);

  CpuProfilingOptions options(
      v8::kLeafNodeLineNumbers, max_samples_, sampling_interval_us_,
      v8::MaybeLocal<v8::Context>(), v8::CpuProfileSource::kUnspecified,
      with_context_ ? &SampledCpuProfiler::ExtractContext : nullptr);

  CpuProfilingResult result = cpu_profiler_->Start(
      v8::String::Empty(env()->isolate()), std::move(options));
  if (result.status != CpuProfilingStatus::kStarted &&
      result.status != CpuProfilingStatus::kAlreadyStarted) {
    delete new_session;
    // Restart failed; leave the binding in stopped state and signal failure
    // through unchanged active_session_ (still nullptr).
    return true;
  }

  profile_id_ = result.id;
  // Same construction-then-publish pattern as Start: compiler fence
  // sequences the new Session before its publication.
  std::atomic_signal_fence(std::memory_order_release);
  active_session_.store(new_session, std::memory_order_relaxed);
  // Reattach the active profiler to the thread.
  t_active_profiler = this;
  // Reinstall GC callbacks for the new session.
  if (with_context_) {
    isolate->AddGCPrologueCallback(&OnGCPrologue, this);
    isolate->AddGCEpilogueCallback(&OnGCEpilogue, this);
  }
  return true;
}

Local<Object> SampledCpuProfiler::BuildAndFree(CpuProfile* profile,
                                               Session* session) {
  Isolate* isolate = env()->isolate();
  Local<Context> context = env()->context();
  Local<Object> result;
  if (profile != nullptr) {
    ProfileBuilder builder(isolate, context, profile, session);
    result = builder.Run();
    profile->Delete();
  } else {
    result = Object::New(isolate);
  }
  delete session;
  return result;
}

void SampledCpuProfiler::StopAndCapture(
    const FunctionCallbackInfo<Value>& args) {
  SampledCpuProfiler* self;
  ASSIGN_OR_RETURN_UNWRAP(&self, args.This());

  CpuProfile* profile = nullptr;
  Session* session = nullptr;
  if (!self->StopActiveSession(/* restart = */ false, &profile, &session)) {
    THROW_ERR_INVALID_STATE(self->env(),
                            "CPU profile is not running");
    return;
  }
  args.GetReturnValue().Set(self->BuildAndFree(profile, session));
}

void SampledCpuProfiler::Snapshot(const FunctionCallbackInfo<Value>& args) {
  SampledCpuProfiler* self;
  ASSIGN_OR_RETURN_UNWRAP(&self, args.This());

  CpuProfile* profile = nullptr;
  Session* session = nullptr;
  if (!self->StopActiveSession(/* restart = */ true, &profile, &session)) {
    THROW_ERR_INVALID_STATE(self->env(),
                            "CPU profile is not running");
    return;
  }
  args.GetReturnValue().Set(self->BuildAndFree(profile, session));
}

namespace {

// OutputStream that accumulates Serialize() chunks into an std::string.
// Modeled on src/util.h's JSONOutputStream but inlined here to avoid the
// dependency on JSONOutputStream being available; we don't need its
// JSON-escaping logic since CpuProfile::Serialize emits already-encoded JSON.
class StringOutputStream : public v8::OutputStream {
 public:
  v8::OutputStream::WriteResult WriteAsciiChunk(char* data, int size) override {
    out_.append(data, size);
    return v8::OutputStream::kContinue;
  }
  void EndOfStream() override {}
  std::string out_;
};

}  // namespace

void SampledCpuProfiler::Stop(const FunctionCallbackInfo<Value>& args) {
  SampledCpuProfiler* self;
  ASSIGN_OR_RETURN_UNWRAP(&self, args.This());

  CpuProfile* profile = nullptr;
  Session* session = nullptr;
  if (!self->StopActiveSession(/* restart = */ false, &profile, &session)) {
    THROW_ERR_INVALID_STATE(self->env(),
                            "CPU profile is not running");
    return;
  }

  if (profile != nullptr) {
    StringOutputStream stream;
    profile->Serialize(&stream, CpuProfile::SerializationFormat::kJSON);
    Isolate* isolate = args.GetIsolate();
    Local<v8::String> result;
    if (v8::String::NewFromUtf8(isolate, stream.out_.c_str(),
                                v8::NewStringType::kNormal,
                                static_cast<int>(stream.out_.size()))
            .ToLocal(&result)) {
      args.GetReturnValue().Set(result);
    }
    profile->Delete();
  }
  delete session;
}

void SampledCpuProfiler::CreateContextHolder(
    const FunctionCallbackInfo<Value>& args) {
  SampledCpuProfiler* self;
  ASSIGN_OR_RETURN_UNWRAP(&self, args.This());
  Environment* env = self->env();
  Isolate* isolate = env->isolate();

  CHECK_EQ(args.Length(), 1);

  // Allocate the holder JS object from the template cached at construction.
  Local<ObjectTemplate> holder_tmpl = self->holder_template_.Get(isolate);
  Local<Object> holder;
  if (!holder_tmpl->NewInstance(env->context()).ToLocal(&holder)) {
    return;
  }

  // Heap-allocate a shared_ptr that owns a Global<Value> wrapping the user's
  // value. The shared_ptr's address is stored in the holder's internal field.
  // Lifetime:
  //   - Initial owner: this holder (count = 1).
  //   - Each sample captured during profiling adds a ref via the context
  //     buffer copy in ExtractContext (count++).
  //   - When the holder is GC'd, HolderWeakCallback deletes the heap
  //     shared_ptr (count--). If samples have copied it, the inner
  //     Global<Value> stays alive until those copies are released.
  //   - When stop()/post-processing clears the context buffer, all remaining
  //     refs drop and the Global<Value> is finally released.
  auto* slot = new std::shared_ptr<Global<Value>>(
      std::make_shared<Global<Value>>(isolate, args[0]));
  holder->SetAlignedPointerInInternalField(kHolderSharedPtrSlot, slot,
                                           v8::kEmbedderDataTypeTagDefault);

  // Register a weak callback to delete the holder's shared_ptr when the holder
  // is GC'd.
  auto* weak = new HolderWeakData{slot, Global<Object>(isolate, holder)};
  weak->handle.SetWeak(weak,
                       HolderWeakCallback,
                       v8::WeakCallbackType::kParameter);

  args.GetReturnValue().Set(holder);
}

// static
void SampledCpuProfiler::OnGCPrologue(v8::Isolate* isolate,
                                      v8::GCType,
                                      v8::GCCallbackFlags,
                                      void* data) {
  auto* self = static_cast<SampledCpuProfiler*>(data);
  // Capture the helper's result before flagging GC active. A signal that
  // arrives between these two stores will see in_gc_ == false and call the
  // helper live — which is still safe because the GC prologue runs before V8
  // starts mutating the heap. The compiler fence prevents the cache store
  // from being reordered after the flag store; pairs with the extractor's
  // matching atomic_signal_fence(acquire) on the read side.
  self->gc_cached_aligned_ptr_.store(
      v8::LookupCpedMapAlignedPointer(isolate, *self->als_key_slot_),
      std::memory_order_relaxed);
  std::atomic_signal_fence(std::memory_order_release);
  self->in_gc_.store(true, std::memory_order_relaxed);
}

// static
void SampledCpuProfiler::OnGCEpilogue(v8::Isolate*, v8::GCType,
                                      v8::GCCallbackFlags, void* data) {
  // No fence needed: clearing the flag first means a signal that interleaves
  // these two stores takes the live-helper path, which is safe because GC
  // has finished and the heap is consistent again.
  auto* self = static_cast<SampledCpuProfiler*>(data);
  self->in_gc_.store(false, std::memory_order_relaxed);
  self->gc_cached_aligned_ptr_.store(nullptr, std::memory_order_relaxed);
}

void* SampledCpuProfiler::ExtractContext(Isolate* isolate) {
  // SIGNAL-SAFETY CONTRACT: this function runs in CPU-profiler signal-handler
  // context. The OrderedHashMap walk through CPED is delegated to the V8 helper
  // v8::LookupCpedMapAlignedPointer, which is signal-safe. The remainder is
  // either atomic loads or a shared_ptr copy whose only side effect is an
  // atomic ref-count increment.

  // All atomics consulted here synchronize the signal handler with the same
  // thread's main-line execution using memory_order_relaxed loads paired with
  // std::atomic_signal_fence to prevent compiler reordering. On ARM memory
  // model this is cheaper than memory barriers.
  SampledCpuProfiler* self = t_active_profiler;
  if (self == nullptr) return nullptr;
  Session* session = self->active_session_.load(std::memory_order_relaxed);
  if (session == nullptr) return nullptr;
  // Pair with Start's signal_fence(release) before publishing the session,
  // so the Session's constructor effects are observed below.
  std::atomic_signal_fence(std::memory_order_acquire);

  // During GC the helper is unsafe to call (it walks heap state mid-
  // compaction), so we use the value cached at GC prologue.
  void* aligned_ptr;
  if (self->in_gc_.load(std::memory_order_relaxed)) {
    // Pair with the prologue's signal_fence(release) between cache store
    // and flag set: observing in_gc_ == true implies the cache is populated.
    std::atomic_signal_fence(std::memory_order_acquire);
    aligned_ptr = self->gc_cached_aligned_ptr_.load(std::memory_order_relaxed);
  } else {
    aligned_ptr =
        v8::LookupCpedMapAlignedPointer(isolate, *self->als_key_slot_);
  }
  if (aligned_ptr == nullptr) return nullptr;

  auto* shared_ptr_slot =
      static_cast<std::shared_ptr<Global<Value>>*>(aligned_ptr);

  // Reserve the next slot.
  uint64_t idx =
      session->next_index.fetch_add(1, std::memory_order_relaxed);
  if (idx >= session->context_buffer.size()) {
    session->dropped.fetch_add(1, std::memory_order_relaxed);
    return nullptr;
  }

  // shared_ptr copy: only side effect is an atomic ref-count increment. No
  // destructor runs because slots are default-constructed empty in Session's
  // ctor and we never overwrite (non-wrapping buffer).
  session->context_buffer[idx] = *shared_ptr_slot;

  // Pack the slot index as void*. Add 1 so a zero-index sample is
  // distinguishable from the "no context" nullptr return; Stop() subtracts 1.
  return reinterpret_cast<void*>(idx + 1);
}

void SampledCpuProfiler::Initialize(Local<Object> target,
                                    Local<Value> unused,
                                    Local<Context> context,
                                    void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, New);
  tmpl->InstanceTemplate()->SetInternalFieldCount(
      BaseObject::kInternalFieldCount);

  SetProtoMethod(isolate, tmpl, "start", Start);
  SetProtoMethod(isolate, tmpl, "stop", Stop);
  SetProtoMethod(isolate, tmpl, "stopAndCapture", StopAndCapture);
  SetProtoMethod(isolate, tmpl, "snapshot", Snapshot);
  SetProtoMethod(isolate, tmpl, "createContextHolder", CreateContextHolder);

  SetConstructorFunction(context, target, "SampledCpuProfiler", tmpl);
}

void SampledCpuProfiler::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(Start);
  registry->Register(Stop);
  registry->Register(StopAndCapture);
  registry->Register(Snapshot);
  registry->Register(CreateContextHolder);
}

}  // namespace cpu_profiler
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    cpu_profiler, node::cpu_profiler::SampledCpuProfiler::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(
    cpu_profiler,
    node::cpu_profiler::SampledCpuProfiler::RegisterExternalReferences)
