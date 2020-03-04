// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"

#include <stdlib.h>

#include <atomic>
#include <fstream>  // NOLINT(readability/streams)
#include <memory>
#include <sstream>
#include <unordered_map>

#include "src/api/api-inl.h"
#include "src/ast/ast-value-factory.h"
#include "src/ast/scopes.h"
#include "src/base/hashmap.h"
#include "src/base/platform/platform.h"
#include "src/base/sys-info.h"
#include "src/base/utils/random-number-generator.h"
#include "src/builtins/builtins-promise.h"
#include "src/builtins/constants-table-builder.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/common/ptr-compr.h"
#include "src/compiler-dispatcher/compiler-dispatcher.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/date/date.h"
#include "src/debug/debug-frames.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/diagnostics/compilation-statistics.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/messages.h"
#include "src/execution/microtask-queue.h"
#include "src/execution/protectors-inl.h"
#include "src/execution/runtime-profiler.h"
#include "src/execution/simulator.h"
#include "src/execution/v8threads.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/ic/stub-cache.h"
#include "src/init/bootstrapper.h"
#include "src/init/setup-isolate.h"
#include "src/init/v8.h"
#include "src/interpreter/interpreter.h"
#include "src/libsampler/sampler.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/backing-store.h"
#include "src/objects/elements.h"
#include "src/objects/frame-array-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/promise-inl.h"
#include "src/objects/prototype.h"
#include "src/objects/slots.h"
#include "src/objects/smi.h"
#include "src/objects/stack-frame-info-inl.h"
#include "src/objects/visitors.h"
#include "src/profiler/heap-profiler.h"
#include "src/profiler/tracing-cpu-profiler.h"
#include "src/regexp/regexp-stack.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/snapshot/embedded/embedded-file-writer.h"
#include "src/snapshot/read-only-deserializer.h"
#include "src/snapshot/startup-deserializer.h"
#include "src/strings/string-builder-inl.h"
#include "src/strings/string-stream.h"
#include "src/tasks/cancelable-task.h"
#include "src/tracing/tracing-category-observer.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/ostreams.h"
#include "src/utils/version.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects.h"
#include "src/zone/accounting-allocator.h"
#ifdef V8_INTL_SUPPORT
#include "unicode/uobject.h"
#endif  // V8_INTL_SUPPORT

#if defined(V8_OS_WIN64)
#include "src/diagnostics/unwinding-info-win64.h"
#endif  // V8_OS_WIN64

extern "C" const uint8_t* v8_Default_embedded_blob_;
extern "C" uint32_t v8_Default_embedded_blob_size_;

namespace v8 {
namespace internal {

#ifdef DEBUG
#define TRACE_ISOLATE(tag)                                                  \
  do {                                                                      \
    if (FLAG_trace_isolates) {                                              \
      PrintF("Isolate %p (id %d)" #tag "\n", reinterpret_cast<void*>(this), \
             id());                                                         \
    }                                                                       \
  } while (false)
#else
#define TRACE_ISOLATE(tag)
#endif

const uint8_t* DefaultEmbeddedBlob() { return v8_Default_embedded_blob_; }
uint32_t DefaultEmbeddedBlobSize() { return v8_Default_embedded_blob_size_; }

#ifdef V8_MULTI_SNAPSHOTS
extern "C" const uint8_t* v8_Trusted_embedded_blob_;
extern "C" uint32_t v8_Trusted_embedded_blob_size_;

const uint8_t* TrustedEmbeddedBlob() { return v8_Trusted_embedded_blob_; }
uint32_t TrustedEmbeddedBlobSize() { return v8_Trusted_embedded_blob_size_; }
#endif

namespace {
// These variables provide access to the current embedded blob without requiring
// an isolate instance. This is needed e.g. by Code::InstructionStart, which may
// not have access to an isolate but still needs to access the embedded blob.
// The variables are initialized by each isolate in Init(). Writes and reads are
// relaxed since we can guarantee that the current thread has initialized these
// variables before accessing them. Different threads may race, but this is fine
// since they all attempt to set the same values of the blob pointer and size.

std::atomic<const uint8_t*> current_embedded_blob_(nullptr);
std::atomic<uint32_t> current_embedded_blob_size_(0);

// The various workflows around embedded snapshots are fairly complex. We need
// to support plain old snapshot builds, nosnap builds, and the requirements of
// subtly different serialization tests. There's two related knobs to twiddle:
//
// - The default embedded blob may be overridden by setting the sticky embedded
// blob. This is set automatically whenever we create a new embedded blob.
//
// - Lifecycle management can be either manual or set to refcounting.
//
// A few situations to demonstrate their use:
//
// - A plain old snapshot build neither overrides the default blob nor
// refcounts.
//
// - mksnapshot sets the sticky blob and manually frees the embedded
// blob once done.
//
// - Most serializer tests do the same.
//
// - Nosnapshot builds set the sticky blob and enable refcounting.

// This mutex protects access to the following variables:
// - sticky_embedded_blob_
// - sticky_embedded_blob_size_
// - enable_embedded_blob_refcounting_
// - current_embedded_blob_refs_
base::LazyMutex current_embedded_blob_refcount_mutex_ = LAZY_MUTEX_INITIALIZER;

const uint8_t* sticky_embedded_blob_ = nullptr;
uint32_t sticky_embedded_blob_size_ = 0;

bool enable_embedded_blob_refcounting_ = true;
int current_embedded_blob_refs_ = 0;

const uint8_t* StickyEmbeddedBlob() { return sticky_embedded_blob_; }
uint32_t StickyEmbeddedBlobSize() { return sticky_embedded_blob_size_; }

void SetStickyEmbeddedBlob(const uint8_t* blob, uint32_t blob_size) {
  sticky_embedded_blob_ = blob;
  sticky_embedded_blob_size_ = blob_size;
}

}  // namespace

void DisableEmbeddedBlobRefcounting() {
  base::MutexGuard guard(current_embedded_blob_refcount_mutex_.Pointer());
  enable_embedded_blob_refcounting_ = false;
}

void FreeCurrentEmbeddedBlob() {
  CHECK(!enable_embedded_blob_refcounting_);
  base::MutexGuard guard(current_embedded_blob_refcount_mutex_.Pointer());

  if (StickyEmbeddedBlob() == nullptr) return;

  CHECK_EQ(StickyEmbeddedBlob(), Isolate::CurrentEmbeddedBlob());

  InstructionStream::FreeOffHeapInstructionStream(
      const_cast<uint8_t*>(Isolate::CurrentEmbeddedBlob()),
      Isolate::CurrentEmbeddedBlobSize());

  current_embedded_blob_.store(nullptr, std::memory_order_relaxed);
  current_embedded_blob_size_.store(0, std::memory_order_relaxed);
  sticky_embedded_blob_ = nullptr;
  sticky_embedded_blob_size_ = 0;
}

// static
bool Isolate::CurrentEmbeddedBlobIsBinaryEmbedded() {
  // In some situations, we must be able to rely on the embedded blob being
  // immortal immovable. This is the case if the blob is binary-embedded.
  // See blob lifecycle controls above for descriptions of when the current
  // embedded blob may change (e.g. in tests or mksnapshot). If the blob is
  // binary-embedded, it is immortal immovable.
  const uint8_t* blob =
      current_embedded_blob_.load(std::memory_order::memory_order_relaxed);
  if (blob == nullptr) return false;
#ifdef V8_MULTI_SNAPSHOTS
  if (blob == TrustedEmbeddedBlob()) return true;
#endif
  return blob == DefaultEmbeddedBlob();
}

void Isolate::SetEmbeddedBlob(const uint8_t* blob, uint32_t blob_size) {
  CHECK_NOT_NULL(blob);

  embedded_blob_ = blob;
  embedded_blob_size_ = blob_size;
  current_embedded_blob_.store(blob, std::memory_order_relaxed);
  current_embedded_blob_size_.store(blob_size, std::memory_order_relaxed);

#ifdef DEBUG
  // Verify that the contents of the embedded blob are unchanged from
  // serialization-time, just to ensure the compiler isn't messing with us.
  EmbeddedData d = EmbeddedData::FromBlob();
  if (d.EmbeddedBlobHash() != d.CreateEmbeddedBlobHash()) {
    FATAL(
        "Embedded blob checksum verification failed. This indicates that the "
        "embedded blob has been modified since compilation time. A common "
        "cause is a debugging breakpoint set within builtin code.");
  }
#endif  // DEBUG
}

void Isolate::ClearEmbeddedBlob() {
  CHECK(enable_embedded_blob_refcounting_);
  CHECK_EQ(embedded_blob_, CurrentEmbeddedBlob());
  CHECK_EQ(embedded_blob_, StickyEmbeddedBlob());

  embedded_blob_ = nullptr;
  embedded_blob_size_ = 0;
  current_embedded_blob_.store(nullptr, std::memory_order_relaxed);
  current_embedded_blob_size_.store(0, std::memory_order_relaxed);
  sticky_embedded_blob_ = nullptr;
  sticky_embedded_blob_size_ = 0;
}

const uint8_t* Isolate::embedded_blob() const { return embedded_blob_; }
uint32_t Isolate::embedded_blob_size() const { return embedded_blob_size_; }

// static
const uint8_t* Isolate::CurrentEmbeddedBlob() {
  return current_embedded_blob_.load(std::memory_order::memory_order_relaxed);
}

// static
uint32_t Isolate::CurrentEmbeddedBlobSize() {
  return current_embedded_blob_size_.load(
      std::memory_order::memory_order_relaxed);
}

size_t Isolate::HashIsolateForEmbeddedBlob() {
  DCHECK(builtins_.is_initialized());
  DCHECK(Builtins::AllBuiltinsAreIsolateIndependent());

  DisallowHeapAllocation no_gc;

  static constexpr size_t kSeed = 0;
  size_t hash = kSeed;

  // Hash data sections of builtin code objects.
  for (int i = 0; i < Builtins::builtin_count; i++) {
    Code code = heap_.builtin(i);

    DCHECK(Internals::HasHeapObjectTag(code.ptr()));
    uint8_t* const code_ptr =
        reinterpret_cast<uint8_t*>(code.ptr() - kHeapObjectTag);

    // These static asserts ensure we don't miss relevant fields. We don't hash
    // instruction size and flags since they change when creating the off-heap
    // trampolines. Other data fields must remain the same.
    STATIC_ASSERT(Code::kInstructionSizeOffset == Code::kDataStart);
    STATIC_ASSERT(Code::kFlagsOffset == Code::kInstructionSizeOffsetEnd + 1);
    STATIC_ASSERT(Code::kSafepointTableOffsetOffset ==
                  Code::kFlagsOffsetEnd + 1);
    static constexpr int kStartOffset = Code::kSafepointTableOffsetOffset;

    for (int j = kStartOffset; j < Code::kUnalignedHeaderSize; j++) {
      hash = base::hash_combine(hash, size_t{code_ptr[j]});
    }
  }

  // The builtins constants table is also tightly tied to embedded builtins.
  hash = base::hash_combine(
      hash, static_cast<size_t>(heap_.builtins_constants_table().length()));

  return hash;
}

base::Thread::LocalStorageKey Isolate::isolate_key_;
base::Thread::LocalStorageKey Isolate::per_isolate_thread_data_key_;
#if DEBUG
std::atomic<bool> Isolate::isolate_key_created_{false};
#endif

namespace {
// A global counter for all generated Isolates, might overflow.
std::atomic<int> isolate_counter{0};
}  // namespace

Isolate::PerIsolateThreadData*
Isolate::FindOrAllocatePerThreadDataForThisThread() {
  ThreadId thread_id = ThreadId::Current();
  PerIsolateThreadData* per_thread = nullptr;
  {
    base::MutexGuard lock_guard(&thread_data_table_mutex_);
    per_thread = thread_data_table_.Lookup(thread_id);
    if (per_thread == nullptr) {
      if (FLAG_adjust_os_scheduling_parameters) {
        base::OS::AdjustSchedulingParams();
      }
      per_thread = new PerIsolateThreadData(this, thread_id);
      thread_data_table_.Insert(per_thread);
    }
    DCHECK(thread_data_table_.Lookup(thread_id) == per_thread);
  }
  return per_thread;
}

void Isolate::DiscardPerThreadDataForThisThread() {
  ThreadId thread_id = ThreadId::TryGetCurrent();
  if (thread_id.IsValid()) {
    DCHECK_NE(thread_manager_->mutex_owner_.load(std::memory_order_relaxed),
              thread_id);
    base::MutexGuard lock_guard(&thread_data_table_mutex_);
    PerIsolateThreadData* per_thread = thread_data_table_.Lookup(thread_id);
    if (per_thread) {
      DCHECK(!per_thread->thread_state_);
      thread_data_table_.Remove(per_thread);
    }
  }
}

Isolate::PerIsolateThreadData* Isolate::FindPerThreadDataForThisThread() {
  ThreadId thread_id = ThreadId::Current();
  return FindPerThreadDataForThread(thread_id);
}

Isolate::PerIsolateThreadData* Isolate::FindPerThreadDataForThread(
    ThreadId thread_id) {
  PerIsolateThreadData* per_thread = nullptr;
  {
    base::MutexGuard lock_guard(&thread_data_table_mutex_);
    per_thread = thread_data_table_.Lookup(thread_id);
  }
  return per_thread;
}

void Isolate::InitializeOncePerProcess() {
  isolate_key_ = base::Thread::CreateThreadLocalKey();
#if DEBUG
  bool expected = false;
  DCHECK_EQ(true, isolate_key_created_.compare_exchange_strong(
                      expected, true, std::memory_order_relaxed));
#endif
  per_isolate_thread_data_key_ = base::Thread::CreateThreadLocalKey();
}

Address Isolate::get_address_from_id(IsolateAddressId id) {
  return isolate_addresses_[id];
}

char* Isolate::Iterate(RootVisitor* v, char* thread_storage) {
  ThreadLocalTop* thread = reinterpret_cast<ThreadLocalTop*>(thread_storage);
  Iterate(v, thread);
  return thread_storage + sizeof(ThreadLocalTop);
}

void Isolate::IterateThread(ThreadVisitor* v, char* t) {
  ThreadLocalTop* thread = reinterpret_cast<ThreadLocalTop*>(t);
  v->VisitThread(this, thread);
}

void Isolate::Iterate(RootVisitor* v, ThreadLocalTop* thread) {
  // Visit the roots from the top for a given thread.
  v->VisitRootPointer(Root::kTop, nullptr,
                      FullObjectSlot(&thread->pending_exception_));
  v->VisitRootPointer(Root::kTop, nullptr,
                      FullObjectSlot(&thread->pending_message_obj_));
  v->VisitRootPointer(Root::kTop, nullptr, FullObjectSlot(&thread->context_));
  v->VisitRootPointer(Root::kTop, nullptr,
                      FullObjectSlot(&thread->scheduled_exception_));

  for (v8::TryCatch* block = thread->try_catch_handler_; block != nullptr;
       block = block->next_) {
    // TODO(3770): Make TryCatch::exception_ an Address (and message_obj_ too).
    v->VisitRootPointer(
        Root::kTop, nullptr,
        FullObjectSlot(reinterpret_cast<Address>(&(block->exception_))));
    v->VisitRootPointer(
        Root::kTop, nullptr,
        FullObjectSlot(reinterpret_cast<Address>(&(block->message_obj_))));
  }

  // Iterate over pointers on native execution stack.
  wasm::WasmCodeRefScope wasm_code_ref_scope;
  for (StackFrameIterator it(this, thread); !it.done(); it.Advance()) {
    it.frame()->Iterate(v);
  }
}

void Isolate::Iterate(RootVisitor* v) {
  ThreadLocalTop* current_t = thread_local_top();
  Iterate(v, current_t);
}

void Isolate::IterateDeferredHandles(RootVisitor* visitor) {
  for (DeferredHandles* deferred = deferred_handles_head_; deferred != nullptr;
       deferred = deferred->next_) {
    deferred->Iterate(visitor);
  }
}

#ifdef DEBUG
bool Isolate::IsDeferredHandle(Address* handle) {
  // Comparing unrelated pointers (not from the same array) is undefined
  // behavior, so cast to Address before making arbitrary comparisons.
  Address handle_as_address = reinterpret_cast<Address>(handle);
  // Each DeferredHandles instance keeps the handles to one job in the
  // concurrent recompilation queue, containing a list of blocks.  Each block
  // contains kHandleBlockSize handles except for the first block, which may
  // not be fully filled.
  // We iterate through all the blocks to see whether the argument handle
  // belongs to one of the blocks.  If so, it is deferred.
  for (DeferredHandles* deferred = deferred_handles_head_; deferred != nullptr;
       deferred = deferred->next_) {
    std::vector<Address*>* blocks = &deferred->blocks_;
    for (size_t i = 0; i < blocks->size(); i++) {
      Address* block_limit = (i == 0) ? deferred->first_block_limit_
                                      : blocks->at(i) + kHandleBlockSize;
      if (reinterpret_cast<Address>(blocks->at(i)) <= handle_as_address &&
          handle_as_address < reinterpret_cast<Address>(block_limit)) {
        return true;
      }
    }
  }
  return false;
}
#endif  // DEBUG

void Isolate::RegisterTryCatchHandler(v8::TryCatch* that) {
  thread_local_top()->try_catch_handler_ = that;
}

void Isolate::UnregisterTryCatchHandler(v8::TryCatch* that) {
  DCHECK(thread_local_top()->try_catch_handler_ == that);
  thread_local_top()->try_catch_handler_ = that->next_;
}

Handle<String> Isolate::StackTraceString() {
  if (stack_trace_nesting_level_ == 0) {
    stack_trace_nesting_level_++;
    HeapStringAllocator allocator;
    StringStream::ClearMentionedObjectCache(this);
    StringStream accumulator(&allocator);
    incomplete_message_ = &accumulator;
    PrintStack(&accumulator);
    Handle<String> stack_trace = accumulator.ToString(this);
    incomplete_message_ = nullptr;
    stack_trace_nesting_level_ = 0;
    return stack_trace;
  } else if (stack_trace_nesting_level_ == 1) {
    stack_trace_nesting_level_++;
    base::OS::PrintError(
        "\n\nAttempt to print stack while printing stack (double fault)\n");
    base::OS::PrintError(
        "If you are lucky you may find a partial stack dump on stdout.\n\n");
    incomplete_message_->OutputToStdOut();
    return factory()->empty_string();
  } else {
    base::OS::Abort();
    // Unreachable
    return factory()->empty_string();
  }
}

void Isolate::PushStackTraceAndDie(void* ptr1, void* ptr2, void* ptr3,
                                   void* ptr4) {
  StackTraceFailureMessage message(this, ptr1, ptr2, ptr3, ptr4);
  message.Print();
  base::OS::Abort();
}

void StackTraceFailureMessage::Print() volatile {
  // Print the details of this failure message object, including its own address
  // to force stack allocation.
  base::OS::PrintError(
      "Stacktrace:\n   ptr1=%p\n    ptr2=%p\n    ptr3=%p\n    ptr4=%p\n    "
      "failure_message_object=%p\n%s",
      ptr1_, ptr2_, ptr3_, ptr4_, this, &js_stack_trace_[0]);
}

StackTraceFailureMessage::StackTraceFailureMessage(Isolate* isolate, void* ptr1,
                                                   void* ptr2, void* ptr3,
                                                   void* ptr4) {
  isolate_ = isolate;
  ptr1_ = ptr1;
  ptr2_ = ptr2;
  ptr3_ = ptr3;
  ptr4_ = ptr4;
  // Write a stracktrace into the {js_stack_trace_} buffer.
  const size_t buffer_length = arraysize(js_stack_trace_);
  memset(&js_stack_trace_, 0, buffer_length);
  FixedStringAllocator fixed(&js_stack_trace_[0], buffer_length - 1);
  StringStream accumulator(&fixed, StringStream::kPrintObjectConcise);
  isolate->PrintStack(&accumulator, Isolate::kPrintStackVerbose);
  // Keeping a reference to the last code objects to increase likelyhood that
  // they get included in the minidump.
  const size_t code_objects_length = arraysize(code_objects_);
  size_t i = 0;
  StackFrameIterator it(isolate);
  for (; !it.done() && i < code_objects_length; it.Advance()) {
    code_objects_[i++] =
        reinterpret_cast<void*>(it.frame()->unchecked_code().ptr());
  }
}

namespace {

class StackFrameCacheHelper : public AllStatic {
 public:
  static MaybeHandle<StackTraceFrame> LookupCachedFrame(
      Isolate* isolate, Handle<AbstractCode> code, int code_offset) {
    if (FLAG_optimize_for_size) return MaybeHandle<StackTraceFrame>();

    const auto maybe_cache = handle(code->stack_frame_cache(), isolate);
    if (!maybe_cache->IsSimpleNumberDictionary())
      return MaybeHandle<StackTraceFrame>();

    const auto cache = Handle<SimpleNumberDictionary>::cast(maybe_cache);
    const InternalIndex entry = cache->FindEntry(isolate, code_offset);
    if (entry.is_found()) {
      return handle(StackTraceFrame::cast(cache->ValueAt(entry)), isolate);
    }
    return MaybeHandle<StackTraceFrame>();
  }

  static void CacheFrameAndUpdateCache(Isolate* isolate,
                                       Handle<AbstractCode> code,
                                       int code_offset,
                                       Handle<StackTraceFrame> frame) {
    if (FLAG_optimize_for_size) return;

    const auto maybe_cache = handle(code->stack_frame_cache(), isolate);
    const auto cache = maybe_cache->IsSimpleNumberDictionary()
                           ? Handle<SimpleNumberDictionary>::cast(maybe_cache)
                           : SimpleNumberDictionary::New(isolate, 1);
    Handle<SimpleNumberDictionary> new_cache =
        SimpleNumberDictionary::Set(isolate, cache, code_offset, frame);
    if (*new_cache != *cache || !maybe_cache->IsSimpleNumberDictionary()) {
      AbstractCode::SetStackFrameCache(code, new_cache);
    }
  }
};

}  // anonymous namespace

class FrameArrayBuilder {
 public:
  enum FrameFilterMode { ALL, CURRENT_SECURITY_CONTEXT };

  FrameArrayBuilder(Isolate* isolate, FrameSkipMode mode, int limit,
                    Handle<Object> caller, FrameFilterMode filter_mode)
      : isolate_(isolate),
        mode_(mode),
        limit_(limit),
        caller_(caller),
        check_security_context_(filter_mode == CURRENT_SECURITY_CONTEXT) {
    switch (mode_) {
      case SKIP_FIRST:
        skip_next_frame_ = true;
        break;
      case SKIP_UNTIL_SEEN:
        DCHECK(caller_->IsJSFunction());
        skip_next_frame_ = true;
        break;
      case SKIP_NONE:
        skip_next_frame_ = false;
        break;
    }

    elements_ = isolate->factory()->NewFrameArray(Min(limit, 10));
  }

  void AppendAsyncFrame(Handle<JSGeneratorObject> generator_object) {
    if (full()) return;
    Handle<JSFunction> function(generator_object->function(), isolate_);
    if (!IsVisibleInStackTrace(function)) return;
    int flags = FrameArray::kIsAsync;
    if (IsStrictFrame(function)) flags |= FrameArray::kIsStrict;

    Handle<Object> receiver(generator_object->receiver(), isolate_);
    Handle<AbstractCode> code(
        AbstractCode::cast(function->shared().GetBytecodeArray()), isolate_);
    int offset = Smi::ToInt(generator_object->input_or_debug_pos());
    // The stored bytecode offset is relative to a different base than what
    // is used in the source position table, hence the subtraction.
    offset -= BytecodeArray::kHeaderSize - kHeapObjectTag;

    Handle<FixedArray> parameters = isolate_->factory()->empty_fixed_array();
    if (V8_UNLIKELY(FLAG_detailed_error_stack_trace)) {
      int param_count = function->shared().internal_formal_parameter_count();
      parameters = isolate_->factory()->NewFixedArray(param_count);
      for (int i = 0; i < param_count; i++) {
        parameters->set(i, generator_object->parameters_and_registers().get(i));
      }
    }

    elements_ = FrameArray::AppendJSFrame(elements_, receiver, function, code,
                                          offset, flags, parameters);
  }

  void AppendPromiseAllFrame(Handle<Context> context, int offset) {
    if (full()) return;
    int flags = FrameArray::kIsAsync | FrameArray::kIsPromiseAll;

    Handle<Context> native_context(context->native_context(), isolate_);
    Handle<JSFunction> function(native_context->promise_all(), isolate_);
    if (!IsVisibleInStackTrace(function)) return;

    Handle<Object> receiver(native_context->promise_function(), isolate_);
    Handle<AbstractCode> code(AbstractCode::cast(function->code()), isolate_);

    // TODO(mmarchini) save Promises list from Promise.all()
    Handle<FixedArray> parameters = isolate_->factory()->empty_fixed_array();

    elements_ = FrameArray::AppendJSFrame(elements_, receiver, function, code,
                                          offset, flags, parameters);
  }

  void AppendJavaScriptFrame(
      FrameSummary::JavaScriptFrameSummary const& summary) {
    // Filter out internal frames that we do not want to show.
    if (!IsVisibleInStackTrace(summary.function())) return;

    Handle<AbstractCode> abstract_code = summary.abstract_code();
    const int offset = summary.code_offset();

    const bool is_constructor = summary.is_constructor();

    int flags = 0;
    Handle<JSFunction> function = summary.function();
    if (IsStrictFrame(function)) flags |= FrameArray::kIsStrict;
    if (is_constructor) flags |= FrameArray::kIsConstructor;

    Handle<FixedArray> parameters = isolate_->factory()->empty_fixed_array();
    if (V8_UNLIKELY(FLAG_detailed_error_stack_trace))
      parameters = summary.parameters();

    elements_ = FrameArray::AppendJSFrame(
        elements_, TheHoleToUndefined(isolate_, summary.receiver()), function,
        abstract_code, offset, flags, parameters);
  }

  void AppendWasmCompiledFrame(
      FrameSummary::WasmCompiledFrameSummary const& summary) {
    if (summary.code()->kind() != wasm::WasmCode::kFunction) return;
    Handle<WasmInstanceObject> instance = summary.wasm_instance();
    int flags = 0;
    if (instance->module_object().is_asm_js()) {
      flags |= FrameArray::kIsAsmJsWasmFrame;
      if (summary.at_to_number_conversion()) {
        flags |= FrameArray::kAsmJsAtNumberConversion;
      }
    } else {
      flags |= FrameArray::kIsWasmCompiledFrame;
    }

    elements_ = FrameArray::AppendWasmFrame(
        elements_, instance, summary.function_index(), summary.code(),
        summary.code_offset(), flags);
  }

  void AppendWasmInterpretedFrame(
      FrameSummary::WasmInterpretedFrameSummary const& summary) {
    Handle<WasmInstanceObject> instance = summary.wasm_instance();
    int flags = FrameArray::kIsWasmInterpretedFrame;
    DCHECK(!instance->module_object().is_asm_js());
    elements_ = FrameArray::AppendWasmFrame(elements_, instance,
                                            summary.function_index(), {},
                                            summary.byte_offset(), flags);
  }

  void AppendBuiltinExitFrame(BuiltinExitFrame* exit_frame) {
    Handle<JSFunction> function = handle(exit_frame->function(), isolate_);

    // Filter out internal frames that we do not want to show.
    if (!IsVisibleInStackTrace(function)) return;

    // TODO(szuend): Remove this check once the flag is enabled
    //               by default.
    if (!FLAG_experimental_stack_trace_frames &&
        function->shared().IsApiFunction()) {
      return;
    }

    Handle<Object> receiver(exit_frame->receiver(), isolate_);
    Handle<Code> code(exit_frame->LookupCode(), isolate_);
    const int offset =
        static_cast<int>(exit_frame->pc() - code->InstructionStart());

    int flags = 0;
    if (IsStrictFrame(function)) flags |= FrameArray::kIsStrict;
    if (exit_frame->IsConstructor()) flags |= FrameArray::kIsConstructor;

    Handle<FixedArray> parameters = isolate_->factory()->empty_fixed_array();
    if (V8_UNLIKELY(FLAG_detailed_error_stack_trace)) {
      int param_count = exit_frame->ComputeParametersCount();
      parameters = isolate_->factory()->NewFixedArray(param_count);
      for (int i = 0; i < param_count; i++) {
        parameters->set(i, exit_frame->GetParameter(i));
      }
    }

    elements_ = FrameArray::AppendJSFrame(elements_, receiver, function,
                                          Handle<AbstractCode>::cast(code),
                                          offset, flags, parameters);
  }

  bool full() { return elements_->FrameCount() >= limit_; }

  Handle<FrameArray> GetElements() {
    elements_->ShrinkToFit(isolate_);
    return elements_;
  }

  // Creates a StackTraceFrame object for each frame in the FrameArray.
  Handle<FixedArray> GetElementsAsStackTraceFrameArray(
      bool enable_frame_caching) {
    elements_->ShrinkToFit(isolate_);
    const int frame_count = elements_->FrameCount();
    Handle<FixedArray> stack_trace =
        isolate_->factory()->NewFixedArray(frame_count);

    for (int i = 0; i < frame_count; ++i) {
      // Caching stack frames only happens for user JS frames.
      const bool cache_frame =
          enable_frame_caching && !isolate_->serializer_enabled() &&
          !elements_->IsAnyWasmFrame(i) &&
          elements_->Function(i).shared().IsUserJavaScript();
      if (cache_frame) {
        MaybeHandle<StackTraceFrame> maybe_frame =
            StackFrameCacheHelper::LookupCachedFrame(
                isolate_, handle(elements_->Code(i), isolate_),
                Smi::ToInt(elements_->Offset(i)));
        if (!maybe_frame.is_null()) {
          Handle<StackTraceFrame> frame = maybe_frame.ToHandleChecked();
          stack_trace->set(i, *frame);
          continue;
        }
      }

      Handle<StackTraceFrame> frame =
          isolate_->factory()->NewStackTraceFrame(elements_, i);
      stack_trace->set(i, *frame);

      if (cache_frame) {
        StackFrameCacheHelper::CacheFrameAndUpdateCache(
            isolate_, handle(elements_->Code(i), isolate_),
            Smi::ToInt(elements_->Offset(i)), frame);
      }
    }
    return stack_trace;
  }

 private:
  // Poison stack frames below the first strict mode frame.
  // The stack trace API should not expose receivers and function
  // objects on frames deeper than the top-most one with a strict mode
  // function.
  bool IsStrictFrame(Handle<JSFunction> function) {
    if (!encountered_strict_function_) {
      encountered_strict_function_ =
          is_strict(function->shared().language_mode());
    }
    return encountered_strict_function_;
  }

  // Determines whether the given stack frame should be displayed in a stack
  // trace.
  bool IsVisibleInStackTrace(Handle<JSFunction> function) {
    return ShouldIncludeFrame(function) && IsNotHidden(function) &&
           IsInSameSecurityContext(function);
  }

  // This mechanism excludes a number of uninteresting frames from the stack
  // trace. This can be be the first frame (which will be a builtin-exit frame
  // for the error constructor builtin) or every frame until encountering a
  // user-specified function.
  bool ShouldIncludeFrame(Handle<JSFunction> function) {
    switch (mode_) {
      case SKIP_NONE:
        return true;
      case SKIP_FIRST:
        if (!skip_next_frame_) return true;
        skip_next_frame_ = false;
        return false;
      case SKIP_UNTIL_SEEN:
        if (skip_next_frame_ && (*function == *caller_)) {
          skip_next_frame_ = false;
          return false;
        }
        return !skip_next_frame_;
    }
    UNREACHABLE();
  }

  bool IsNotHidden(Handle<JSFunction> function) {
    // Functions defined not in user scripts are not visible unless directly
    // exposed, in which case the native flag is set.
    // The --builtins-in-stack-traces command line flag allows including
    // internal call sites in the stack trace for debugging purposes.
    if (!FLAG_builtins_in_stack_traces &&
        !function->shared().IsUserJavaScript()) {
      return function->shared().native() || function->shared().IsApiFunction();
    }
    return true;
  }

  bool IsInSameSecurityContext(Handle<JSFunction> function) {
    if (!check_security_context_) return true;
    return isolate_->context().HasSameSecurityTokenAs(function->context());
  }

  // TODO(jgruber): Fix all cases in which frames give us a hole value (e.g. the
  // receiver in RegExp constructor frames.
  Handle<Object> TheHoleToUndefined(Isolate* isolate, Handle<Object> in) {
    return (in->IsTheHole(isolate))
               ? Handle<Object>::cast(isolate->factory()->undefined_value())
               : in;
  }

  Isolate* isolate_;
  const FrameSkipMode mode_;
  int limit_;
  const Handle<Object> caller_;
  bool skip_next_frame_ = true;
  bool encountered_strict_function_ = false;
  const bool check_security_context_;
  Handle<FrameArray> elements_;
};

bool GetStackTraceLimit(Isolate* isolate, int* result) {
  Handle<JSObject> error = isolate->error_function();

  Handle<String> key = isolate->factory()->stackTraceLimit_string();
  Handle<Object> stack_trace_limit = JSReceiver::GetDataProperty(error, key);
  if (!stack_trace_limit->IsNumber()) return false;

  // Ensure that limit is not negative.
  *result = Max(FastD2IChecked(stack_trace_limit->Number()), 0);

  if (*result != FLAG_stack_trace_limit) {
    isolate->CountUsage(v8::Isolate::kErrorStackTraceLimit);
  }

  return true;
}

bool NoExtension(const v8::FunctionCallbackInfo<v8::Value>&) { return false; }

bool IsBuiltinFunction(Isolate* isolate, HeapObject object,
                       Builtins::Name builtin_index) {
  if (!object.IsJSFunction()) return false;
  JSFunction const function = JSFunction::cast(object);
  return function.code() == isolate->builtins()->builtin(builtin_index);
}

void CaptureAsyncStackTrace(Isolate* isolate, Handle<JSPromise> promise,
                            FrameArrayBuilder* builder) {
  while (!builder->full()) {
    // Check that the {promise} is not settled.
    if (promise->status() != Promise::kPending) return;

    // Check that we have exactly one PromiseReaction on the {promise}.
    if (!promise->reactions().IsPromiseReaction()) return;
    Handle<PromiseReaction> reaction(
        PromiseReaction::cast(promise->reactions()), isolate);
    if (!reaction->next().IsSmi()) return;

    // Check if the {reaction} has one of the known async function or
    // async generator continuations as its fulfill handler.
    if (IsBuiltinFunction(isolate, reaction->fulfill_handler(),
                          Builtins::kAsyncFunctionAwaitResolveClosure) ||
        IsBuiltinFunction(isolate, reaction->fulfill_handler(),
                          Builtins::kAsyncGeneratorAwaitResolveClosure) ||
        IsBuiltinFunction(isolate, reaction->fulfill_handler(),
                          Builtins::kAsyncGeneratorYieldResolveClosure)) {
      // Now peak into the handlers' AwaitContext to get to
      // the JSGeneratorObject for the async function.
      Handle<Context> context(
          JSFunction::cast(reaction->fulfill_handler()).context(), isolate);
      Handle<JSGeneratorObject> generator_object(
          JSGeneratorObject::cast(context->extension()), isolate);
      CHECK(generator_object->is_suspended());

      // Append async frame corresponding to the {generator_object}.
      builder->AppendAsyncFrame(generator_object);

      // Try to continue from here.
      if (generator_object->IsJSAsyncFunctionObject()) {
        Handle<JSAsyncFunctionObject> async_function_object =
            Handle<JSAsyncFunctionObject>::cast(generator_object);
        promise = handle(async_function_object->promise(), isolate);
      } else {
        Handle<JSAsyncGeneratorObject> async_generator_object =
            Handle<JSAsyncGeneratorObject>::cast(generator_object);
        if (async_generator_object->queue().IsUndefined(isolate)) return;
        Handle<AsyncGeneratorRequest> async_generator_request(
            AsyncGeneratorRequest::cast(async_generator_object->queue()),
            isolate);
        promise = handle(JSPromise::cast(async_generator_request->promise()),
                         isolate);
      }
    } else if (IsBuiltinFunction(isolate, reaction->fulfill_handler(),
                                 Builtins::kPromiseAllResolveElementClosure)) {
      Handle<JSFunction> function(JSFunction::cast(reaction->fulfill_handler()),
                                  isolate);
      Handle<Context> context(function->context(), isolate);

      // We store the offset of the promise into the {function}'s
      // hash field for promise resolve element callbacks.
      int const offset = Smi::ToInt(Smi::cast(function->GetIdentityHash())) - 1;
      builder->AppendPromiseAllFrame(context, offset);

      // Now peak into the Promise.all() resolve element context to
      // find the promise capability that's being resolved when all
      // the concurrent promises resolve.
      int const index =
          PromiseBuiltins::kPromiseAllResolveElementCapabilitySlot;
      Handle<PromiseCapability> capability(
          PromiseCapability::cast(context->get(index)), isolate);
      if (!capability->promise().IsJSPromise()) return;
      promise = handle(JSPromise::cast(capability->promise()), isolate);
    } else if (IsBuiltinFunction(isolate, reaction->fulfill_handler(),
                                 Builtins::kPromiseCapabilityDefaultResolve)) {
      Handle<JSFunction> function(JSFunction::cast(reaction->fulfill_handler()),
                                  isolate);
      Handle<Context> context(function->context(), isolate);
      promise =
          handle(JSPromise::cast(context->get(PromiseBuiltins::kPromiseSlot)),
                 isolate);
    } else {
      // We have some generic promise chain here, so try to
      // continue with the chained promise on the reaction
      // (only works for native promise chains).
      Handle<HeapObject> promise_or_capability(
          reaction->promise_or_capability(), isolate);
      if (promise_or_capability->IsJSPromise()) {
        promise = Handle<JSPromise>::cast(promise_or_capability);
      } else if (promise_or_capability->IsPromiseCapability()) {
        Handle<PromiseCapability> capability =
            Handle<PromiseCapability>::cast(promise_or_capability);
        if (!capability->promise().IsJSPromise()) return;
        promise = handle(JSPromise::cast(capability->promise()), isolate);
      } else {
        // Otherwise the {promise_or_capability} must be undefined here.
        CHECK(promise_or_capability->IsUndefined(isolate));
        return;
      }
    }
  }
}

namespace {

struct CaptureStackTraceOptions {
  int limit;
  // 'filter_mode' and 'skip_mode' are somewhat orthogonal. 'filter_mode'
  // specifies whether to capture all frames, or just frames in the same
  // security context. While 'skip_mode' allows skipping the first frame.
  FrameSkipMode skip_mode;
  FrameArrayBuilder::FrameFilterMode filter_mode;

  bool capture_builtin_exit_frames;
  bool capture_only_frames_subject_to_debugging;
  bool async_stack_trace;
  bool enable_frame_caching;
};

Handle<Object> CaptureStackTrace(Isolate* isolate, Handle<Object> caller,
                                 CaptureStackTraceOptions options) {
  DisallowJavascriptExecution no_js(isolate);

  wasm::WasmCodeRefScope code_ref_scope;
  FrameArrayBuilder builder(isolate, options.skip_mode, options.limit, caller,
                            options.filter_mode);

  // Build the regular stack trace, and remember the last relevant
  // frame ID and inlined index (for the async stack trace handling
  // below, which starts from this last frame).
  for (StackFrameIterator it(isolate); !it.done() && !builder.full();
       it.Advance()) {
    StackFrame* const frame = it.frame();
    switch (frame->type()) {
      case StackFrame::JAVA_SCRIPT_BUILTIN_CONTINUATION:
      case StackFrame::JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH:
      case StackFrame::OPTIMIZED:
      case StackFrame::INTERPRETED:
      case StackFrame::BUILTIN:
      case StackFrame::WASM_COMPILED:
      case StackFrame::WASM_INTERPRETER_ENTRY: {
        // A standard frame may include many summarized frames (due to
        // inlining).
        std::vector<FrameSummary> frames;
        StandardFrame::cast(frame)->Summarize(&frames);
        for (size_t i = frames.size(); i-- != 0 && !builder.full();) {
          auto& summary = frames[i];
          if (options.capture_only_frames_subject_to_debugging &&
              !summary.is_subject_to_debugging()) {
            continue;
          }

          if (summary.IsJavaScript()) {
            //=========================================================
            // Handle a JavaScript frame.
            //=========================================================
            auto const& java_script = summary.AsJavaScript();
            builder.AppendJavaScriptFrame(java_script);
          } else if (summary.IsWasmCompiled()) {
            //=========================================================
            // Handle a WASM compiled frame.
            //=========================================================
            auto const& wasm_compiled = summary.AsWasmCompiled();
            builder.AppendWasmCompiledFrame(wasm_compiled);
          } else if (summary.IsWasmInterpreted()) {
            //=========================================================
            // Handle a WASM interpreted frame.
            //=========================================================
            auto const& wasm_interpreted = summary.AsWasmInterpreted();
            builder.AppendWasmInterpretedFrame(wasm_interpreted);
          }
        }
        break;
      }

      case StackFrame::BUILTIN_EXIT:
        if (!options.capture_builtin_exit_frames) continue;

        // BuiltinExitFrames are not standard frames, so they do not have
        // Summarize(). However, they may have one JS frame worth showing.
        builder.AppendBuiltinExitFrame(BuiltinExitFrame::cast(frame));
        break;

      default:
        break;
    }
  }

  // If --async-stack-traces are enabled and the "current microtask" is a
  // PromiseReactionJobTask, we try to enrich the stack trace with async
  // frames.
  if (options.async_stack_trace) {
    Handle<Object> current_microtask = isolate->factory()->current_microtask();
    if (current_microtask->IsPromiseReactionJobTask()) {
      Handle<PromiseReactionJobTask> promise_reaction_job_task =
          Handle<PromiseReactionJobTask>::cast(current_microtask);
      // Check if the {reaction} has one of the known async function or
      // async generator continuations as its fulfill handler.
      if (IsBuiltinFunction(isolate, promise_reaction_job_task->handler(),
                            Builtins::kAsyncFunctionAwaitResolveClosure) ||
          IsBuiltinFunction(isolate, promise_reaction_job_task->handler(),
                            Builtins::kAsyncGeneratorAwaitResolveClosure) ||
          IsBuiltinFunction(isolate, promise_reaction_job_task->handler(),
                            Builtins::kAsyncGeneratorYieldResolveClosure)) {
        // Now peak into the handlers' AwaitContext to get to
        // the JSGeneratorObject for the async function.
        Handle<Context> context(
            JSFunction::cast(promise_reaction_job_task->handler()).context(),
            isolate);
        Handle<JSGeneratorObject> generator_object(
            JSGeneratorObject::cast(context->extension()), isolate);
        if (generator_object->is_executing()) {
          if (generator_object->IsJSAsyncFunctionObject()) {
            Handle<JSAsyncFunctionObject> async_function_object =
                Handle<JSAsyncFunctionObject>::cast(generator_object);
            Handle<JSPromise> promise(async_function_object->promise(),
                                      isolate);
            CaptureAsyncStackTrace(isolate, promise, &builder);
          } else {
            Handle<JSAsyncGeneratorObject> async_generator_object =
                Handle<JSAsyncGeneratorObject>::cast(generator_object);
            Handle<Object> queue(async_generator_object->queue(), isolate);
            if (!queue->IsUndefined(isolate)) {
              Handle<AsyncGeneratorRequest> async_generator_request =
                  Handle<AsyncGeneratorRequest>::cast(queue);
              Handle<JSPromise> promise(
                  JSPromise::cast(async_generator_request->promise()), isolate);
              CaptureAsyncStackTrace(isolate, promise, &builder);
            }
          }
        }
      } else {
        // The {promise_reaction_job_task} doesn't belong to an await (or
        // yield inside an async generator), but we might still be able to
        // find an async frame if we follow along the chain of promises on
        // the {promise_reaction_job_task}.
        Handle<HeapObject> promise_or_capability(
            promise_reaction_job_task->promise_or_capability(), isolate);
        if (promise_or_capability->IsJSPromise()) {
          Handle<JSPromise> promise =
              Handle<JSPromise>::cast(promise_or_capability);
          CaptureAsyncStackTrace(isolate, promise, &builder);
        }
      }
    }
  }

  // TODO(yangguo): Queue this structured stack trace for preprocessing on GC.
  return builder.GetElementsAsStackTraceFrameArray(
      options.enable_frame_caching);
}

}  // namespace

Handle<Object> Isolate::CaptureSimpleStackTrace(Handle<JSReceiver> error_object,
                                                FrameSkipMode mode,
                                                Handle<Object> caller) {
  int limit;
  if (!GetStackTraceLimit(this, &limit)) return factory()->undefined_value();

  CaptureStackTraceOptions options;
  options.limit = limit;
  options.skip_mode = mode;
  options.capture_builtin_exit_frames = true;
  options.async_stack_trace = FLAG_async_stack_traces;
  options.filter_mode = FrameArrayBuilder::CURRENT_SECURITY_CONTEXT;
  options.capture_only_frames_subject_to_debugging = false;
  options.enable_frame_caching = false;

  return CaptureStackTrace(this, caller, options);
}

MaybeHandle<JSReceiver> Isolate::CaptureAndSetDetailedStackTrace(
    Handle<JSReceiver> error_object) {
  if (capture_stack_trace_for_uncaught_exceptions_) {
    // Capture stack trace for a detailed exception message.
    Handle<Name> key = factory()->detailed_stack_trace_symbol();
    Handle<FixedArray> stack_trace = CaptureCurrentStackTrace(
        stack_trace_for_uncaught_exceptions_frame_limit_,
        stack_trace_for_uncaught_exceptions_options_);
    RETURN_ON_EXCEPTION(
        this,
        Object::SetProperty(this, error_object, key, stack_trace,
                            StoreOrigin::kMaybeKeyed,
                            Just(ShouldThrow::kThrowOnError)),
        JSReceiver);
  }
  return error_object;
}

MaybeHandle<JSReceiver> Isolate::CaptureAndSetSimpleStackTrace(
    Handle<JSReceiver> error_object, FrameSkipMode mode,
    Handle<Object> caller) {
  // Capture stack trace for simple stack trace string formatting.
  Handle<Name> key = factory()->stack_trace_symbol();
  Handle<Object> stack_trace =
      CaptureSimpleStackTrace(error_object, mode, caller);
  RETURN_ON_EXCEPTION(this,
                      Object::SetProperty(this, error_object, key, stack_trace,
                                          StoreOrigin::kMaybeKeyed,
                                          Just(ShouldThrow::kThrowOnError)),
                      JSReceiver);
  return error_object;
}

Handle<FixedArray> Isolate::GetDetailedStackTrace(
    Handle<JSObject> error_object) {
  Handle<Name> key_detailed = factory()->detailed_stack_trace_symbol();
  Handle<Object> stack_trace =
      JSReceiver::GetDataProperty(error_object, key_detailed);
  if (stack_trace->IsFixedArray()) return Handle<FixedArray>::cast(stack_trace);
  return Handle<FixedArray>();
}

Address Isolate::GetAbstractPC(int* line, int* column) {
  JavaScriptFrameIterator it(this);

  if (it.done()) {
    *line = -1;
    *column = -1;
    return kNullAddress;
  }
  JavaScriptFrame* frame = it.frame();
  DCHECK(!frame->is_builtin());

  Handle<SharedFunctionInfo> shared = handle(frame->function().shared(), this);
  SharedFunctionInfo::EnsureSourcePositionsAvailable(this, shared);
  int position = frame->position();

  Object maybe_script = frame->function().shared().script();
  if (maybe_script.IsScript()) {
    Handle<Script> script(Script::cast(maybe_script), this);
    Script::PositionInfo info;
    Script::GetPositionInfo(script, position, &info, Script::WITH_OFFSET);
    *line = info.line + 1;
    *column = info.column + 1;
  } else {
    *line = position;
    *column = -1;
  }

  if (frame->is_interpreted()) {
    InterpretedFrame* iframe = static_cast<InterpretedFrame*>(frame);
    Address bytecode_start =
        iframe->GetBytecodeArray().GetFirstBytecodeAddress();
    return bytecode_start + iframe->GetBytecodeOffset();
  }

  return frame->pc();
}

Handle<FixedArray> Isolate::CaptureCurrentStackTrace(
    int frame_limit, StackTrace::StackTraceOptions stack_trace_options) {
  CaptureStackTraceOptions options;
  options.limit = Max(frame_limit, 0);  // Ensure no negative values.
  options.skip_mode = SKIP_NONE;
  options.capture_builtin_exit_frames = false;
  options.async_stack_trace = false;
  options.filter_mode =
      (stack_trace_options & StackTrace::kExposeFramesAcrossSecurityOrigins)
          ? FrameArrayBuilder::ALL
          : FrameArrayBuilder::CURRENT_SECURITY_CONTEXT;
  options.capture_only_frames_subject_to_debugging = true;
  options.enable_frame_caching = true;

  return Handle<FixedArray>::cast(
      CaptureStackTrace(this, factory()->undefined_value(), options));
}

void Isolate::PrintStack(FILE* out, PrintStackMode mode) {
  if (stack_trace_nesting_level_ == 0) {
    stack_trace_nesting_level_++;
    StringStream::ClearMentionedObjectCache(this);
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    incomplete_message_ = &accumulator;
    PrintStack(&accumulator, mode);
    accumulator.OutputToFile(out);
    InitializeLoggingAndCounters();
    accumulator.Log(this);
    incomplete_message_ = nullptr;
    stack_trace_nesting_level_ = 0;
  } else if (stack_trace_nesting_level_ == 1) {
    stack_trace_nesting_level_++;
    base::OS::PrintError(
        "\n\nAttempt to print stack while printing stack (double fault)\n");
    base::OS::PrintError(
        "If you are lucky you may find a partial stack dump on stdout.\n\n");
    incomplete_message_->OutputToFile(out);
  }
}

static void PrintFrames(Isolate* isolate, StringStream* accumulator,
                        StackFrame::PrintMode mode) {
  StackFrameIterator it(isolate);
  for (int i = 0; !it.done(); it.Advance()) {
    it.frame()->Print(accumulator, mode, i++);
  }
}

void Isolate::PrintStack(StringStream* accumulator, PrintStackMode mode) {
  HandleScope scope(this);
  wasm::WasmCodeRefScope wasm_code_ref_scope;
  DCHECK(accumulator->IsMentionedObjectCacheClear(this));

  // Avoid printing anything if there are no frames.
  if (c_entry_fp(thread_local_top()) == 0) return;

  accumulator->Add(
      "\n==== JS stack trace =========================================\n\n");
  PrintFrames(this, accumulator, StackFrame::OVERVIEW);
  if (mode == kPrintStackVerbose) {
    accumulator->Add(
        "\n==== Details ================================================\n\n");
    PrintFrames(this, accumulator, StackFrame::DETAILS);
    accumulator->PrintMentionedObjectCache(this);
  }
  accumulator->Add("=====================\n\n");
}

void Isolate::SetFailedAccessCheckCallback(
    v8::FailedAccessCheckCallback callback) {
  thread_local_top()->failed_access_check_callback_ = callback;
}

void Isolate::ReportFailedAccessCheck(Handle<JSObject> receiver) {
  if (!thread_local_top()->failed_access_check_callback_) {
    return ScheduleThrow(*factory()->NewTypeError(MessageTemplate::kNoAccess));
  }

  DCHECK(receiver->IsAccessCheckNeeded());
  DCHECK(!context().is_null());

  // Get the data object from access check info.
  HandleScope scope(this);
  Handle<Object> data;
  {
    DisallowHeapAllocation no_gc;
    AccessCheckInfo access_check_info = AccessCheckInfo::Get(this, receiver);
    if (access_check_info.is_null()) {
      AllowHeapAllocation doesnt_matter_anymore;
      return ScheduleThrow(
          *factory()->NewTypeError(MessageTemplate::kNoAccess));
    }
    data = handle(access_check_info.data(), this);
  }

  // Leaving JavaScript.
  VMState<EXTERNAL> state(this);
  thread_local_top()->failed_access_check_callback_(
      v8::Utils::ToLocal(receiver), v8::ACCESS_HAS, v8::Utils::ToLocal(data));
}

bool Isolate::MayAccess(Handle<Context> accessing_context,
                        Handle<JSObject> receiver) {
  DCHECK(receiver->IsJSGlobalProxy() || receiver->IsAccessCheckNeeded());

  // Check for compatibility between the security tokens in the
  // current lexical context and the accessed object.

  // During bootstrapping, callback functions are not enabled yet.
  if (bootstrapper()->IsActive()) return true;
  {
    DisallowHeapAllocation no_gc;

    if (receiver->IsJSGlobalProxy()) {
      Object receiver_context = JSGlobalProxy::cast(*receiver).native_context();
      if (!receiver_context.IsContext()) return false;

      // Get the native context of current top context.
      // avoid using Isolate::native_context() because it uses Handle.
      Context native_context =
          accessing_context->global_object().native_context();
      if (receiver_context == native_context) return true;

      if (Context::cast(receiver_context).security_token() ==
          native_context.security_token())
        return true;
    }
  }

  HandleScope scope(this);
  Handle<Object> data;
  v8::AccessCheckCallback callback = nullptr;
  {
    DisallowHeapAllocation no_gc;
    AccessCheckInfo access_check_info = AccessCheckInfo::Get(this, receiver);
    if (access_check_info.is_null()) return false;
    Object fun_obj = access_check_info.callback();
    callback = v8::ToCData<v8::AccessCheckCallback>(fun_obj);
    data = handle(access_check_info.data(), this);
  }

  LOG(this, ApiSecurityCheck());

  {
    // Leaving JavaScript.
    VMState<EXTERNAL> state(this);
    return callback(v8::Utils::ToLocal(accessing_context),
                    v8::Utils::ToLocal(receiver), v8::Utils::ToLocal(data));
  }
}

Object Isolate::StackOverflow() {
  if (FLAG_correctness_fuzzer_suppressions) {
    FATAL("Aborting on stack overflow");
  }

  DisallowJavascriptExecution no_js(this);
  HandleScope scope(this);

  Handle<JSFunction> fun = range_error_function();
  Handle<Object> msg = factory()->NewStringFromAsciiChecked(
      MessageFormatter::TemplateString(MessageTemplate::kStackOverflow));
  Handle<Object> no_caller;
  Handle<Object> exception;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      this, exception,
      ErrorUtils::Construct(this, fun, fun, msg, SKIP_NONE, no_caller,
                            ErrorUtils::StackTraceCollection::kSimple));

  Throw(*exception, nullptr);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && FLAG_stress_compaction) {
    heap()->CollectAllGarbage(Heap::kNoGCFlags,
                              GarbageCollectionReason::kTesting);
  }
#endif  // VERIFY_HEAP

  return ReadOnlyRoots(heap()).exception();
}

Object Isolate::TerminateExecution() {
  return Throw(ReadOnlyRoots(this).termination_exception(), nullptr);
}

void Isolate::CancelTerminateExecution() {
  if (try_catch_handler()) {
    try_catch_handler()->has_terminated_ = false;
  }
  if (has_pending_exception() &&
      pending_exception() == ReadOnlyRoots(this).termination_exception()) {
    thread_local_top()->external_caught_exception_ = false;
    clear_pending_exception();
  }
  if (has_scheduled_exception() &&
      scheduled_exception() == ReadOnlyRoots(this).termination_exception()) {
    thread_local_top()->external_caught_exception_ = false;
    clear_scheduled_exception();
  }
}

void Isolate::RequestInterrupt(InterruptCallback callback, void* data) {
  ExecutionAccess access(this);
  api_interrupts_queue_.push(InterruptEntry(callback, data));
  stack_guard()->RequestApiInterrupt();
}

void Isolate::InvokeApiInterruptCallbacks() {
  RuntimeCallTimerScope runtimeTimer(
      this, RuntimeCallCounterId::kInvokeApiInterruptCallbacks);
  // Note: callback below should be called outside of execution access lock.
  while (true) {
    InterruptEntry entry;
    {
      ExecutionAccess access(this);
      if (api_interrupts_queue_.empty()) return;
      entry = api_interrupts_queue_.front();
      api_interrupts_queue_.pop();
    }
    VMState<EXTERNAL> state(this);
    HandleScope handle_scope(this);
    entry.first(reinterpret_cast<v8::Isolate*>(this), entry.second);
  }
}

void ReportBootstrappingException(Handle<Object> exception,
                                  MessageLocation* location) {
  base::OS::PrintError("Exception thrown during bootstrapping\n");
  if (location == nullptr || location->script().is_null()) return;
  // We are bootstrapping and caught an error where the location is set
  // and we have a script for the location.
  // In this case we could have an extension (or an internal error
  // somewhere) and we print out the line number at which the error occurred
  // to the console for easier debugging.
  int line_number =
      location->script()->GetLineNumber(location->start_pos()) + 1;
  if (exception->IsString() && location->script()->name().IsString()) {
    base::OS::PrintError(
        "Extension or internal compilation error: %s in %s at line %d.\n",
        String::cast(*exception).ToCString().get(),
        String::cast(location->script()->name()).ToCString().get(),
        line_number);
  } else if (location->script()->name().IsString()) {
    base::OS::PrintError(
        "Extension or internal compilation error in %s at line %d.\n",
        String::cast(location->script()->name()).ToCString().get(),
        line_number);
  } else if (exception->IsString()) {
    base::OS::PrintError("Extension or internal compilation error: %s.\n",
                         String::cast(*exception).ToCString().get());
  } else {
    base::OS::PrintError("Extension or internal compilation error.\n");
  }
#ifdef OBJECT_PRINT
  // Since comments and empty lines have been stripped from the source of
  // builtins, print the actual source here so that line numbers match.
  if (location->script()->source().IsString()) {
    Handle<String> src(String::cast(location->script()->source()),
                       location->script()->GetIsolate());
    PrintF("Failing script:");
    int len = src->length();
    if (len == 0) {
      PrintF(" <not available>\n");
    } else {
      PrintF("\n");
      int line_number = 1;
      PrintF("%5d: ", line_number);
      for (int i = 0; i < len; i++) {
        uint16_t character = src->Get(i);
        PrintF("%c", character);
        if (character == '\n' && i < len - 2) {
          PrintF("%5d: ", ++line_number);
        }
      }
      PrintF("\n");
    }
  }
#endif
}

Object Isolate::Throw(Object raw_exception, MessageLocation* location) {
  DCHECK(!has_pending_exception());

  HandleScope scope(this);
  Handle<Object> exception(raw_exception, this);

  if (FLAG_print_all_exceptions) {
    printf("=========================================================\n");
    printf("Exception thrown:\n");
    if (location) {
      Handle<Script> script = location->script();
      Handle<Object> name(script->GetNameOrSourceURL(), this);
      printf("at ");
      if (name->IsString() && String::cast(*name).length() > 0)
        String::cast(*name).PrintOn(stdout);
      else
        printf("<anonymous>");
// Script::GetLineNumber and Script::GetColumnNumber can allocate on the heap to
// initialize the line_ends array, so be careful when calling them.
#ifdef DEBUG
      if (AllowHeapAllocation::IsAllowed()) {
#else
      if ((false)) {
#endif
        printf(", %d:%d - %d:%d\n",
               Script::GetLineNumber(script, location->start_pos()) + 1,
               Script::GetColumnNumber(script, location->start_pos()),
               Script::GetLineNumber(script, location->end_pos()) + 1,
               Script::GetColumnNumber(script, location->end_pos()));
        // Make sure to update the raw exception pointer in case it moved.
        raw_exception = *exception;
      } else {
        printf(", line %d\n", script->GetLineNumber(location->start_pos()) + 1);
      }
    }
    raw_exception.Print();
    printf("Stack Trace:\n");
    PrintStack(stdout);
    printf("=========================================================\n");
  }

  // Determine whether a message needs to be created for the given exception
  // depending on the following criteria:
  // 1) External v8::TryCatch missing: Always create a message because any
  //    JavaScript handler for a finally-block might re-throw to top-level.
  // 2) External v8::TryCatch exists: Only create a message if the handler
  //    captures messages or is verbose (which reports despite the catch).
  // 3) ReThrow from v8::TryCatch: The message from a previous throw still
  //    exists and we preserve it instead of creating a new message.
  bool requires_message = try_catch_handler() == nullptr ||
                          try_catch_handler()->is_verbose_ ||
                          try_catch_handler()->capture_message_;
  bool rethrowing_message = thread_local_top()->rethrowing_message_;

  thread_local_top()->rethrowing_message_ = false;

  // Notify debugger of exception.
  if (is_catchable_by_javascript(raw_exception)) {
    debug()->OnThrow(exception);
  }

  // Generate the message if required.
  if (requires_message && !rethrowing_message) {
    MessageLocation computed_location;
    // If no location was specified we try to use a computed one instead.
    if (location == nullptr && ComputeLocation(&computed_location)) {
      location = &computed_location;
    }

    if (bootstrapper()->IsActive()) {
      // It's not safe to try to make message objects or collect stack traces
      // while the bootstrapper is active since the infrastructure may not have
      // been properly initialized.
      ReportBootstrappingException(exception, location);
    } else {
      Handle<Object> message_obj = CreateMessage(exception, location);
      thread_local_top()->pending_message_obj_ = *message_obj;

      // For any exception not caught by JavaScript, even when an external
      // handler is present:
      // If the abort-on-uncaught-exception flag is specified, and if the
      // embedder didn't specify a custom uncaught exception callback,
      // or if the custom callback determined that V8 should abort, then
      // abort.
      if (FLAG_abort_on_uncaught_exception) {
        CatchType prediction = PredictExceptionCatcher();
        if ((prediction == NOT_CAUGHT || prediction == CAUGHT_BY_EXTERNAL) &&
            (!abort_on_uncaught_exception_callback_ ||
             abort_on_uncaught_exception_callback_(
                 reinterpret_cast<v8::Isolate*>(this)))) {
          // Prevent endless recursion.
          FLAG_abort_on_uncaught_exception = false;
          // This flag is intended for use by JavaScript developers, so
          // print a user-friendly stack trace (not an internal one).
          PrintF(stderr, "%s\n\nFROM\n",
                 MessageHandler::GetLocalizedMessage(this, message_obj).get());
          PrintCurrentStackTrace(stderr);
          base::OS::Abort();
        }
      }
    }
  }

  // Set the exception being thrown.
  set_pending_exception(*exception);
  return ReadOnlyRoots(heap()).exception();
}

Object Isolate::ReThrow(Object exception) {
  DCHECK(!has_pending_exception());

  // Set the exception being re-thrown.
  set_pending_exception(exception);
  return ReadOnlyRoots(heap()).exception();
}

Object Isolate::UnwindAndFindHandler() {
  Object exception = pending_exception();

  auto FoundHandler = [&](Context context, Address instruction_start,
                          intptr_t handler_offset,
                          Address constant_pool_address, Address handler_sp,
                          Address handler_fp) {
    // Store information to be consumed by the CEntry.
    thread_local_top()->pending_handler_context_ = context;
    thread_local_top()->pending_handler_entrypoint_ =
        instruction_start + handler_offset;
    thread_local_top()->pending_handler_constant_pool_ = constant_pool_address;
    thread_local_top()->pending_handler_fp_ = handler_fp;
    thread_local_top()->pending_handler_sp_ = handler_sp;

    // Return and clear pending exception. The contract is that:
    // (1) the pending exception is stored in one place (no duplication), and
    // (2) within generated-code land, that one place is the return register.
    // If/when we unwind back into C++ (returning to the JSEntry stub,
    // or to Execution::CallWasm), the returned exception will be sent
    // back to isolate->set_pending_exception(...).
    clear_pending_exception();
    return exception;
  };

  // Special handling of termination exceptions, uncatchable by JavaScript and
  // Wasm code, we unwind the handlers until the top ENTRY handler is found.
  bool catchable_by_js = is_catchable_by_javascript(exception);

  // Compute handler and stack unwinding information by performing a full walk
  // over the stack and dispatching according to the frame type.
  for (StackFrameIterator iter(this);; iter.Advance()) {
    // Handler must exist.
    DCHECK(!iter.done());

    StackFrame* frame = iter.frame();

    switch (frame->type()) {
      case StackFrame::ENTRY:
      case StackFrame::CONSTRUCT_ENTRY: {
        // For JSEntry frames we always have a handler.
        StackHandler* handler = frame->top_handler();

        // Restore the next handler.
        thread_local_top()->handler_ = handler->next_address();

        // Gather information from the handler.
        Code code = frame->LookupCode();
        HandlerTable table(code);
        return FoundHandler(Context(), code.InstructionStart(),
                            table.LookupReturn(0), code.constant_pool(),
                            handler->address() + StackHandlerConstants::kSize,
                            0);
      }

      case StackFrame::C_WASM_ENTRY: {
        StackHandler* handler = frame->top_handler();
        thread_local_top()->handler_ = handler->next_address();
        Code code = frame->LookupCode();
        HandlerTable table(code);
        Address instruction_start = code.InstructionStart();
        int return_offset = static_cast<int>(frame->pc() - instruction_start);
        int handler_offset = table.LookupReturn(return_offset);
        DCHECK_NE(-1, handler_offset);
        // Compute the stack pointer from the frame pointer. This ensures that
        // argument slots on the stack are dropped as returning would.
        Address return_sp = frame->fp() +
                            StandardFrameConstants::kFixedFrameSizeAboveFp -
                            code.stack_slots() * kSystemPointerSize;
        return FoundHandler(Context(), instruction_start, handler_offset,
                            code.constant_pool(), return_sp, frame->fp());
      }

      case StackFrame::WASM_COMPILED: {
        if (trap_handler::IsThreadInWasm()) {
          trap_handler::ClearThreadInWasm();
        }

        // For WebAssembly frames we perform a lookup in the handler table.
        if (!catchable_by_js) break;
        // This code ref scope is here to avoid a check failure when looking up
        // the code. It's not actually necessary to keep the code alive as it's
        // currently being executed.
        wasm::WasmCodeRefScope code_ref_scope;
        WasmCompiledFrame* wasm_frame = static_cast<WasmCompiledFrame*>(frame);
        wasm::WasmCode* wasm_code =
            wasm_engine()->code_manager()->LookupCode(frame->pc());
        int offset = wasm_frame->LookupExceptionHandlerInTable();
        if (offset < 0) break;
        // Compute the stack pointer from the frame pointer. This ensures that
        // argument slots on the stack are dropped as returning would.
        Address return_sp = frame->fp() +
                            StandardFrameConstants::kFixedFrameSizeAboveFp -
                            wasm_code->stack_slots() * kSystemPointerSize;

        // This is going to be handled by Wasm, so we need to set the TLS flag
        // again. It was cleared above assuming the frame would be unwound.
        trap_handler::SetThreadInWasm();

        return FoundHandler(Context(), wasm_code->instruction_start(), offset,
                            wasm_code->constant_pool(), return_sp, frame->fp());
      }

      case StackFrame::WASM_COMPILE_LAZY: {
        // Can only fail directly on invocation. This happens if an invalid
        // function was validated lazily.
        DCHECK_IMPLIES(trap_handler::IsTrapHandlerEnabled(),
                       trap_handler::IsThreadInWasm());
        DCHECK(FLAG_wasm_lazy_validation);
        trap_handler::ClearThreadInWasm();
        break;
      }

      case StackFrame::OPTIMIZED: {
        // For optimized frames we perform a lookup in the handler table.
        if (!catchable_by_js) break;
        OptimizedFrame* js_frame = static_cast<OptimizedFrame*>(frame);
        Code code = frame->LookupCode();
        int offset = js_frame->LookupExceptionHandlerInTable(nullptr, nullptr);
        if (offset < 0) break;
        // Compute the stack pointer from the frame pointer. This ensures
        // that argument slots on the stack are dropped as returning would.
        Address return_sp = frame->fp() +
                            StandardFrameConstants::kFixedFrameSizeAboveFp -
                            code.stack_slots() * kSystemPointerSize;

        // TODO(bmeurer): Turbofanned BUILTIN frames appear as OPTIMIZED,
        // but do not have a code kind of OPTIMIZED_FUNCTION.
        if (code.kind() == Code::OPTIMIZED_FUNCTION &&
            code.marked_for_deoptimization()) {
          // If the target code is lazy deoptimized, we jump to the original
          // return address, but we make a note that we are throwing, so
          // that the deoptimizer can do the right thing.
          offset = static_cast<int>(frame->pc() - code.entry());
          set_deoptimizer_lazy_throw(true);
        }

        return FoundHandler(Context(), code.InstructionStart(), offset,
                            code.constant_pool(), return_sp, frame->fp());
      }

      case StackFrame::STUB: {
        // Some stubs are able to handle exceptions.
        if (!catchable_by_js) break;
        StubFrame* stub_frame = static_cast<StubFrame*>(frame);
#ifdef DEBUG
        wasm::WasmCodeRefScope code_ref_scope;
        DCHECK_NULL(wasm_engine()->code_manager()->LookupCode(frame->pc()));
#endif  // DEBUG
        Code code = stub_frame->LookupCode();
        if (!code.IsCode() || code.kind() != Code::BUILTIN ||
            !code.has_handler_table() || !code.is_turbofanned()) {
          break;
        }

        int offset = stub_frame->LookupExceptionHandlerInTable();
        if (offset < 0) break;

        // Compute the stack pointer from the frame pointer. This ensures
        // that argument slots on the stack are dropped as returning would.
        Address return_sp = frame->fp() +
                            StandardFrameConstants::kFixedFrameSizeAboveFp -
                            code.stack_slots() * kSystemPointerSize;

        return FoundHandler(Context(), code.InstructionStart(), offset,
                            code.constant_pool(), return_sp, frame->fp());
      }

      case StackFrame::INTERPRETED: {
        // For interpreted frame we perform a range lookup in the handler table.
        if (!catchable_by_js) break;
        InterpretedFrame* js_frame = static_cast<InterpretedFrame*>(frame);
        int register_slots = InterpreterFrameConstants::RegisterStackSlotCount(
            js_frame->GetBytecodeArray().register_count());
        int context_reg = 0;  // Will contain register index holding context.
        int offset =
            js_frame->LookupExceptionHandlerInTable(&context_reg, nullptr);
        if (offset < 0) break;
        // Compute the stack pointer from the frame pointer. This ensures that
        // argument slots on the stack are dropped as returning would.
        // Note: This is only needed for interpreted frames that have been
        //       materialized by the deoptimizer. If there is a handler frame
        //       in between then {frame->sp()} would already be correct.
        Address return_sp = frame->fp() -
                            InterpreterFrameConstants::kFixedFrameSizeFromFp -
                            register_slots * kSystemPointerSize;

        // Patch the bytecode offset in the interpreted frame to reflect the
        // position of the exception handler. The special builtin below will
        // take care of continuing to dispatch at that position. Also restore
        // the correct context for the handler from the interpreter register.
        Context context =
            Context::cast(js_frame->ReadInterpreterRegister(context_reg));
        js_frame->PatchBytecodeOffset(static_cast<int>(offset));

        Code code =
            builtins()->builtin(Builtins::kInterpreterEnterBytecodeDispatch);
        return FoundHandler(context, code.InstructionStart(), 0,
                            code.constant_pool(), return_sp, frame->fp());
      }

      case StackFrame::BUILTIN:
        // For builtin frames we are guaranteed not to find a handler.
        if (catchable_by_js) {
          CHECK_EQ(-1,
                   JavaScriptFrame::cast(frame)->LookupExceptionHandlerInTable(
                       nullptr, nullptr));
        }
        break;

      case StackFrame::WASM_INTERPRETER_ENTRY: {
        if (trap_handler::IsThreadInWasm()) {
          trap_handler::ClearThreadInWasm();
        }
      } break;

      case StackFrame::JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH: {
        // Builtin continuation frames with catch can handle exceptions.
        if (!catchable_by_js) break;
        JavaScriptBuiltinContinuationWithCatchFrame* js_frame =
            JavaScriptBuiltinContinuationWithCatchFrame::cast(frame);
        js_frame->SetException(exception);

        // Reconstruct the stack pointer from the frame pointer.
        Address return_sp = js_frame->fp() - js_frame->GetSPToFPDelta();
        Code code = js_frame->LookupCode();
        return FoundHandler(Context(), code.InstructionStart(), 0,
                            code.constant_pool(), return_sp, frame->fp());
      } break;

      default:
        // All other types can not handle exception.
        break;
    }

    if (frame->is_optimized()) {
      // Remove per-frame stored materialized objects.
      bool removed = materialized_object_store_->Remove(frame->fp());
      USE(removed);
      // If there were any materialized objects, the code should be
      // marked for deopt.
      DCHECK_IMPLIES(removed, frame->LookupCode().marked_for_deoptimization());
    }
  }

  UNREACHABLE();
}

namespace {
HandlerTable::CatchPrediction PredictException(JavaScriptFrame* frame) {
  HandlerTable::CatchPrediction prediction;
  if (frame->is_optimized()) {
    if (frame->LookupExceptionHandlerInTable(nullptr, nullptr) > 0) {
      // This optimized frame will catch. It's handler table does not include
      // exception prediction, and we need to use the corresponding handler
      // tables on the unoptimized code objects.
      std::vector<FrameSummary> summaries;
      frame->Summarize(&summaries);
      for (size_t i = summaries.size(); i != 0; i--) {
        const FrameSummary& summary = summaries[i - 1];
        Handle<AbstractCode> code = summary.AsJavaScript().abstract_code();
        if (code->IsCode() && code->kind() == AbstractCode::BUILTIN) {
          prediction = code->GetCode().GetBuiltinCatchPrediction();
          if (prediction == HandlerTable::UNCAUGHT) continue;
          return prediction;
        }

        // Must have been constructed from a bytecode array.
        CHECK_EQ(AbstractCode::INTERPRETED_FUNCTION, code->kind());
        int code_offset = summary.code_offset();
        HandlerTable table(code->GetBytecodeArray());
        int index = table.LookupRange(code_offset, nullptr, &prediction);
        if (index <= 0) continue;
        if (prediction == HandlerTable::UNCAUGHT) continue;
        return prediction;
      }
    }
  } else if (frame->LookupExceptionHandlerInTable(nullptr, &prediction) > 0) {
    return prediction;
  }
  return HandlerTable::UNCAUGHT;
}

Isolate::CatchType ToCatchType(HandlerTable::CatchPrediction prediction) {
  switch (prediction) {
    case HandlerTable::UNCAUGHT:
      return Isolate::NOT_CAUGHT;
    case HandlerTable::CAUGHT:
      return Isolate::CAUGHT_BY_JAVASCRIPT;
    case HandlerTable::PROMISE:
      return Isolate::CAUGHT_BY_PROMISE;
    case HandlerTable::DESUGARING:
      return Isolate::CAUGHT_BY_DESUGARING;
    case HandlerTable::ASYNC_AWAIT:
      return Isolate::CAUGHT_BY_ASYNC_AWAIT;
    default:
      UNREACHABLE();
  }
}
}  // anonymous namespace

Isolate::CatchType Isolate::PredictExceptionCatcher() {
  Address external_handler = thread_local_top()->try_catch_handler_address();
  if (IsExternalHandlerOnTop(Object())) return CAUGHT_BY_EXTERNAL;

  // Search for an exception handler by performing a full walk over the stack.
  for (StackFrameIterator iter(this); !iter.done(); iter.Advance()) {
    StackFrame* frame = iter.frame();

    switch (frame->type()) {
      case StackFrame::ENTRY:
      case StackFrame::CONSTRUCT_ENTRY: {
        Address entry_handler = frame->top_handler()->next_address();
        // The exception has been externally caught if and only if there is an
        // external handler which is on top of the top-most JS_ENTRY handler.
        if (external_handler != kNullAddress &&
            !try_catch_handler()->is_verbose_) {
          if (entry_handler == kNullAddress ||
              entry_handler > external_handler) {
            return CAUGHT_BY_EXTERNAL;
          }
        }
      } break;

      // For JavaScript frames we perform a lookup in the handler table.
      case StackFrame::OPTIMIZED:
      case StackFrame::INTERPRETED:
      case StackFrame::BUILTIN: {
        JavaScriptFrame* js_frame = JavaScriptFrame::cast(frame);
        Isolate::CatchType prediction = ToCatchType(PredictException(js_frame));
        if (prediction == NOT_CAUGHT) break;
        return prediction;
      } break;

      case StackFrame::STUB: {
        Handle<Code> code(frame->LookupCode(), this);
        if (!code->IsCode() || code->kind() != Code::BUILTIN ||
            !code->has_handler_table() || !code->is_turbofanned()) {
          break;
        }

        CatchType prediction = ToCatchType(code->GetBuiltinCatchPrediction());
        if (prediction != NOT_CAUGHT) return prediction;
      } break;

      case StackFrame::JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH: {
        Handle<Code> code(frame->LookupCode(), this);
        CatchType prediction = ToCatchType(code->GetBuiltinCatchPrediction());
        if (prediction != NOT_CAUGHT) return prediction;
      } break;

      default:
        // All other types can not handle exception.
        break;
    }
  }

  // Handler not found.
  return NOT_CAUGHT;
}

Object Isolate::ThrowIllegalOperation() {
  if (FLAG_stack_trace_on_illegal) PrintStack(stdout);
  return Throw(ReadOnlyRoots(heap()).illegal_access_string());
}

void Isolate::ScheduleThrow(Object exception) {
  // When scheduling a throw we first throw the exception to get the
  // error reporting if it is uncaught before rescheduling it.
  Throw(exception);
  PropagatePendingExceptionToExternalTryCatch();
  if (has_pending_exception()) {
    thread_local_top()->scheduled_exception_ = pending_exception();
    thread_local_top()->external_caught_exception_ = false;
    clear_pending_exception();
  }
}

void Isolate::RestorePendingMessageFromTryCatch(v8::TryCatch* handler) {
  DCHECK(handler == try_catch_handler());
  DCHECK(handler->HasCaught());
  DCHECK(handler->rethrow_);
  DCHECK(handler->capture_message_);
  Object message(reinterpret_cast<Address>(handler->message_obj_));
  DCHECK(message.IsJSMessageObject() || message.IsTheHole(this));
  thread_local_top()->pending_message_obj_ = message;
}

void Isolate::CancelScheduledExceptionFromTryCatch(v8::TryCatch* handler) {
  DCHECK(has_scheduled_exception());
  if (reinterpret_cast<void*>(scheduled_exception().ptr()) ==
      handler->exception_) {
    DCHECK_NE(scheduled_exception(),
              ReadOnlyRoots(heap()).termination_exception());
    clear_scheduled_exception();
  } else {
    DCHECK_EQ(scheduled_exception(),
              ReadOnlyRoots(heap()).termination_exception());
    // Clear termination once we returned from all V8 frames.
    if (thread_local_top()->CallDepthIsZero()) {
      thread_local_top()->external_caught_exception_ = false;
      clear_scheduled_exception();
    }
  }
  if (reinterpret_cast<void*>(thread_local_top()->pending_message_obj_.ptr()) ==
      handler->message_obj_) {
    clear_pending_message();
  }
}

Object Isolate::PromoteScheduledException() {
  Object thrown = scheduled_exception();
  clear_scheduled_exception();
  // Re-throw the exception to avoid getting repeated error reporting.
  return ReThrow(thrown);
}

void Isolate::PrintCurrentStackTrace(FILE* out) {
  CaptureStackTraceOptions options;
  options.limit = 0;
  options.skip_mode = SKIP_NONE;
  options.capture_builtin_exit_frames = true;
  options.async_stack_trace = FLAG_async_stack_traces;
  options.filter_mode = FrameArrayBuilder::CURRENT_SECURITY_CONTEXT;
  options.capture_only_frames_subject_to_debugging = false;
  options.enable_frame_caching = false;

  Handle<FixedArray> frames = Handle<FixedArray>::cast(
      CaptureStackTrace(this, this->factory()->undefined_value(), options));

  IncrementalStringBuilder builder(this);
  for (int i = 0; i < frames->length(); ++i) {
    Handle<StackTraceFrame> frame(StackTraceFrame::cast(frames->get(i)), this);

    SerializeStackTraceFrame(this, frame, &builder);
  }

  Handle<String> stack_trace = builder.Finish().ToHandleChecked();
  stack_trace->PrintOn(out);
}

bool Isolate::ComputeLocation(MessageLocation* target) {
  StackTraceFrameIterator it(this);
  if (it.done()) return false;
  StandardFrame* frame = it.frame();
  // Compute the location from the function and the relocation info of the
  // baseline code. For optimized code this will use the deoptimization
  // information to get canonical location information.
  std::vector<FrameSummary> frames;
  wasm::WasmCodeRefScope code_ref_scope;
  frame->Summarize(&frames);
  FrameSummary& summary = frames.back();
  Handle<SharedFunctionInfo> shared;
  Handle<Object> script = summary.script();
  if (!script->IsScript() ||
      (Script::cast(*script).source().IsUndefined(this))) {
    return false;
  }

  if (summary.IsJavaScript()) {
    shared = handle(summary.AsJavaScript().function()->shared(), this);
  }
  if (summary.AreSourcePositionsAvailable()) {
    int pos = summary.SourcePosition();
    *target =
        MessageLocation(Handle<Script>::cast(script), pos, pos + 1, shared);
  } else {
    *target = MessageLocation(Handle<Script>::cast(script), shared,
                              summary.code_offset());
  }
  return true;
}

bool Isolate::ComputeLocationFromException(MessageLocation* target,
                                           Handle<Object> exception) {
  if (!exception->IsJSObject()) return false;

  Handle<Name> start_pos_symbol = factory()->error_start_pos_symbol();
  Handle<Object> start_pos = JSReceiver::GetDataProperty(
      Handle<JSObject>::cast(exception), start_pos_symbol);
  if (!start_pos->IsSmi()) return false;
  int start_pos_value = Handle<Smi>::cast(start_pos)->value();

  Handle<Name> end_pos_symbol = factory()->error_end_pos_symbol();
  Handle<Object> end_pos = JSReceiver::GetDataProperty(
      Handle<JSObject>::cast(exception), end_pos_symbol);
  if (!end_pos->IsSmi()) return false;
  int end_pos_value = Handle<Smi>::cast(end_pos)->value();

  Handle<Name> script_symbol = factory()->error_script_symbol();
  Handle<Object> script = JSReceiver::GetDataProperty(
      Handle<JSObject>::cast(exception), script_symbol);
  if (!script->IsScript()) return false;

  Handle<Script> cast_script(Script::cast(*script), this);
  *target = MessageLocation(cast_script, start_pos_value, end_pos_value);
  return true;
}

bool Isolate::ComputeLocationFromStackTrace(MessageLocation* target,
                                            Handle<Object> exception) {
  if (!exception->IsJSObject()) return false;
  Handle<Name> key = factory()->stack_trace_symbol();
  Handle<Object> property =
      JSReceiver::GetDataProperty(Handle<JSObject>::cast(exception), key);
  if (!property->IsFixedArray()) return false;

  Handle<FrameArray> elements =
      GetFrameArrayFromStackTrace(this, Handle<FixedArray>::cast(property));

  const int frame_count = elements->FrameCount();
  for (int i = 0; i < frame_count; i++) {
    if (elements->IsWasmFrame(i) || elements->IsAsmJsWasmFrame(i)) {
      Handle<WasmInstanceObject> instance(elements->WasmInstance(i), this);
      uint32_t func_index =
          static_cast<uint32_t>(elements->WasmFunctionIndex(i).value());
      int offset = elements->Offset(i).value();
      bool is_at_number_conversion =
          elements->IsAsmJsWasmFrame(i) &&
          elements->Flags(i).value() & FrameArray::kAsmJsAtNumberConversion;
      if (elements->IsWasmCompiledFrame(i)) {
        // WasmCode* held alive by the {GlobalWasmCodeRef}.
        wasm::WasmCode* code =
            Managed<wasm::GlobalWasmCodeRef>::cast(elements->WasmCodeObject(i))
                .get()
                ->code();
        offset = FrameSummary::WasmCompiledFrameSummary::GetWasmSourcePosition(
            code, offset);
      }
      int pos = WasmModuleObject::GetSourcePosition(
          handle(instance->module_object(), this), func_index, offset,
          is_at_number_conversion);
      Handle<Script> script(instance->module_object().script(), this);

      *target = MessageLocation(script, pos, pos + 1);
      return true;
    }

    Handle<JSFunction> fun = handle(elements->Function(i), this);
    if (!fun->shared().IsSubjectToDebugging()) continue;

    Object script = fun->shared().script();
    if (script.IsScript() &&
        !(Script::cast(script).source().IsUndefined(this))) {
      Handle<SharedFunctionInfo> shared = handle(fun->shared(), this);

      AbstractCode abstract_code = elements->Code(i);
      const int code_offset = elements->Offset(i).value();
      Handle<Script> casted_script(Script::cast(script), this);
      if (shared->HasBytecodeArray() &&
          shared->GetBytecodeArray().HasSourcePositionTable()) {
        int pos = abstract_code.SourcePosition(code_offset);
        *target = MessageLocation(casted_script, pos, pos + 1, shared);
      } else {
        *target = MessageLocation(casted_script, shared, code_offset);
      }

      return true;
    }
  }
  return false;
}

Handle<JSMessageObject> Isolate::CreateMessage(Handle<Object> exception,
                                               MessageLocation* location) {
  Handle<FixedArray> stack_trace_object;
  if (capture_stack_trace_for_uncaught_exceptions_) {
    if (exception->IsJSError()) {
      // We fetch the stack trace that corresponds to this error object.
      // If the lookup fails, the exception is probably not a valid Error
      // object. In that case, we fall through and capture the stack trace
      // at this throw site.
      stack_trace_object =
          GetDetailedStackTrace(Handle<JSObject>::cast(exception));
    }
    if (stack_trace_object.is_null()) {
      // Not an error object, we capture stack and location at throw site.
      stack_trace_object = CaptureCurrentStackTrace(
          stack_trace_for_uncaught_exceptions_frame_limit_,
          stack_trace_for_uncaught_exceptions_options_);
    }
  }
  MessageLocation computed_location;
  if (location == nullptr &&
      (ComputeLocationFromException(&computed_location, exception) ||
       ComputeLocationFromStackTrace(&computed_location, exception) ||
       ComputeLocation(&computed_location))) {
    location = &computed_location;
  }

  return MessageHandler::MakeMessageObject(
      this, MessageTemplate::kUncaughtException, location, exception,
      stack_trace_object);
}

bool Isolate::IsJavaScriptHandlerOnTop(Object exception) {
  DCHECK_NE(ReadOnlyRoots(heap()).the_hole_value(), exception);

  // For uncatchable exceptions, the JavaScript handler cannot be on top.
  if (!is_catchable_by_javascript(exception)) return false;

  // Get the top-most JS_ENTRY handler, cannot be on top if it doesn't exist.
  Address entry_handler = Isolate::handler(thread_local_top());
  if (entry_handler == kNullAddress) return false;

  // Get the address of the external handler so we can compare the address to
  // determine which one is closer to the top of the stack.
  Address external_handler = thread_local_top()->try_catch_handler_address();
  if (external_handler == kNullAddress) return true;

  // The exception has been externally caught if and only if there is an
  // external handler which is on top of the top-most JS_ENTRY handler.
  //
  // Note, that finally clauses would re-throw an exception unless it's aborted
  // by jumps in control flow (like return, break, etc.) and we'll have another
  // chance to set proper v8::TryCatch later.
  return (entry_handler < external_handler);
}

bool Isolate::IsExternalHandlerOnTop(Object exception) {
  DCHECK_NE(ReadOnlyRoots(heap()).the_hole_value(), exception);

  // Get the address of the external handler so we can compare the address to
  // determine which one is closer to the top of the stack.
  Address external_handler = thread_local_top()->try_catch_handler_address();
  if (external_handler == kNullAddress) return false;

  // For uncatchable exceptions, the external handler is always on top.
  if (!is_catchable_by_javascript(exception)) return true;

  // Get the top-most JS_ENTRY handler, cannot be on top if it doesn't exist.
  Address entry_handler = Isolate::handler(thread_local_top());
  if (entry_handler == kNullAddress) return true;

  // The exception has been externally caught if and only if there is an
  // external handler which is on top of the top-most JS_ENTRY handler.
  //
  // Note, that finally clauses would re-throw an exception unless it's aborted
  // by jumps in control flow (like return, break, etc.) and we'll have another
  // chance to set proper v8::TryCatch later.
  return (entry_handler > external_handler);
}

void Isolate::ReportPendingMessagesImpl(bool report_externally) {
  Object exception_obj = pending_exception();

  // Clear the pending message object early to avoid endless recursion.
  Object message_obj = thread_local_top()->pending_message_obj_;
  clear_pending_message();

  // For uncatchable exceptions we do nothing. If needed, the exception and the
  // message have already been propagated to v8::TryCatch.
  if (!is_catchable_by_javascript(exception_obj)) return;

  // Determine whether the message needs to be reported to all message handlers
  // depending on whether and external v8::TryCatch or an internal JavaScript
  // handler is on top.
  bool should_report_exception;
  if (report_externally) {
    // Only report the exception if the external handler is verbose.
    should_report_exception = try_catch_handler()->is_verbose_;
  } else {
    // Report the exception if it isn't caught by JavaScript code.
    should_report_exception = !IsJavaScriptHandlerOnTop(exception_obj);
  }

  // Actually report the pending message to all message handlers.
  if (!message_obj.IsTheHole(this) && should_report_exception) {
    HandleScope scope(this);
    Handle<JSMessageObject> message(JSMessageObject::cast(message_obj), this);
    Handle<Object> exception(exception_obj, this);
    Handle<Script> script(message->script(), this);
    // Clear the exception and restore it afterwards, otherwise
    // CollectSourcePositions will abort.
    clear_pending_exception();
    JSMessageObject::EnsureSourcePositionsAvailable(this, message);
    set_pending_exception(*exception);
    int start_pos = message->GetStartPosition();
    int end_pos = message->GetEndPosition();
    MessageLocation location(script, start_pos, end_pos);
    MessageHandler::ReportMessage(this, &location, message);
  }
}

void Isolate::ReportPendingMessages() {
  DCHECK(AllowExceptions::IsAllowed(this));

  // The embedder might run script in response to an exception.
  AllowJavascriptExecutionDebugOnly allow_script(this);

  Object exception = pending_exception();

  // Try to propagate the exception to an external v8::TryCatch handler. If
  // propagation was unsuccessful, then we will get another chance at reporting
  // the pending message if the exception is re-thrown.
  bool has_been_propagated = PropagatePendingExceptionToExternalTryCatch();
  if (!has_been_propagated) return;

  ReportPendingMessagesImpl(IsExternalHandlerOnTop(exception));
}

void Isolate::ReportPendingMessagesFromJavaScript() {
  DCHECK(AllowExceptions::IsAllowed(this));

  auto IsHandledByJavaScript = [=]() {
    // In this situation, the exception is always a non-terminating exception.

    // Get the top-most JS_ENTRY handler, cannot be on top if it doesn't exist.
    Address entry_handler = Isolate::handler(thread_local_top());
    DCHECK_NE(entry_handler, kNullAddress);
    entry_handler = StackHandler::FromAddress(entry_handler)->next_address();

    // Get the address of the external handler so we can compare the address to
    // determine which one is closer to the top of the stack.
    Address external_handler = thread_local_top()->try_catch_handler_address();
    if (external_handler == kNullAddress) return true;

    return (entry_handler < external_handler);
  };

  auto IsHandledExternally = [=]() {
    Address external_handler = thread_local_top()->try_catch_handler_address();
    if (external_handler == kNullAddress) return false;

    // Get the top-most JS_ENTRY handler, cannot be on top if it doesn't exist.
    Address entry_handler = Isolate::handler(thread_local_top());
    DCHECK_NE(entry_handler, kNullAddress);
    entry_handler = StackHandler::FromAddress(entry_handler)->next_address();
    return (entry_handler > external_handler);
  };

  auto PropagateToExternalHandler = [=]() {
    if (IsHandledByJavaScript()) {
      thread_local_top()->external_caught_exception_ = false;
      return false;
    }

    if (!IsHandledExternally()) {
      thread_local_top()->external_caught_exception_ = false;
      return true;
    }

    thread_local_top()->external_caught_exception_ = true;
    v8::TryCatch* handler = try_catch_handler();
    DCHECK(thread_local_top()->pending_message_obj_.IsJSMessageObject() ||
           thread_local_top()->pending_message_obj_.IsTheHole(this));
    handler->can_continue_ = true;
    handler->has_terminated_ = false;
    handler->exception_ = reinterpret_cast<void*>(pending_exception().ptr());
    // Propagate to the external try-catch only if we got an actual message.
    if (thread_local_top()->pending_message_obj_.IsTheHole(this)) return true;

    handler->message_obj_ =
        reinterpret_cast<void*>(thread_local_top()->pending_message_obj_.ptr());
    return true;
  };

  // Try to propagate to an external v8::TryCatch handler.
  if (!PropagateToExternalHandler()) return;

  ReportPendingMessagesImpl(true);
}

bool Isolate::OptionalRescheduleException(bool clear_exception) {
  DCHECK(has_pending_exception());
  PropagatePendingExceptionToExternalTryCatch();

  bool is_termination_exception =
      pending_exception() == ReadOnlyRoots(this).termination_exception();

  if (is_termination_exception) {
    if (clear_exception) {
      thread_local_top()->external_caught_exception_ = false;
      clear_pending_exception();
      return false;
    }
  } else if (thread_local_top()->external_caught_exception_) {
    // If the exception is externally caught, clear it if there are no
    // JavaScript frames on the way to the C++ frame that has the
    // external handler.
    DCHECK_NE(thread_local_top()->try_catch_handler_address(), kNullAddress);
    Address external_handler_address =
        thread_local_top()->try_catch_handler_address();
    JavaScriptFrameIterator it(this);
    if (it.done() || (it.frame()->sp() > external_handler_address)) {
      clear_exception = true;
    }
  }

  // Clear the exception if needed.
  if (clear_exception) {
    thread_local_top()->external_caught_exception_ = false;
    clear_pending_exception();
    return false;
  }

  // Reschedule the exception.
  thread_local_top()->scheduled_exception_ = pending_exception();
  clear_pending_exception();
  return true;
}

void Isolate::PushPromise(Handle<JSObject> promise) {
  ThreadLocalTop* tltop = thread_local_top();
  PromiseOnStack* prev = tltop->promise_on_stack_;
  Handle<JSObject> global_promise = global_handles()->Create(*promise);
  tltop->promise_on_stack_ = new PromiseOnStack(global_promise, prev);
}

void Isolate::PopPromise() {
  ThreadLocalTop* tltop = thread_local_top();
  if (tltop->promise_on_stack_ == nullptr) return;
  PromiseOnStack* prev = tltop->promise_on_stack_->prev();
  Handle<Object> global_promise = tltop->promise_on_stack_->promise();
  delete tltop->promise_on_stack_;
  tltop->promise_on_stack_ = prev;
  global_handles()->Destroy(global_promise.location());
}

namespace {
bool InternalPromiseHasUserDefinedRejectHandler(Isolate* isolate,
                                                Handle<JSPromise> promise);

bool PromiseHandlerCheck(Isolate* isolate, Handle<JSReceiver> handler,
                         Handle<JSReceiver> deferred_promise) {
  // Recurse to the forwarding Promise, if any. This may be due to
  //  - await reaction forwarding to the throwaway Promise, which has
  //    a dependency edge to the outer Promise.
  //  - PromiseIdResolveHandler forwarding to the output of .then
  //  - Promise.all/Promise.race forwarding to a throwaway Promise, which
  //    has a dependency edge to the generated outer Promise.
  // Otherwise, this is a real reject handler for the Promise.
  Handle<Symbol> key = isolate->factory()->promise_forwarding_handler_symbol();
  Handle<Object> forwarding_handler = JSReceiver::GetDataProperty(handler, key);
  if (forwarding_handler->IsUndefined(isolate)) {
    return true;
  }

  if (!deferred_promise->IsJSPromise()) {
    return true;
  }

  return InternalPromiseHasUserDefinedRejectHandler(
      isolate, Handle<JSPromise>::cast(deferred_promise));
}

bool InternalPromiseHasUserDefinedRejectHandler(Isolate* isolate,
                                                Handle<JSPromise> promise) {
  // If this promise was marked as being handled by a catch block
  // in an async function, then it has a user-defined reject handler.
  if (promise->handled_hint()) return true;

  // If this Promise is subsumed by another Promise (a Promise resolved
  // with another Promise, or an intermediate, hidden, throwaway Promise
  // within async/await), then recurse on the outer Promise.
  // In this case, the dependency is one possible way that the Promise
  // could be resolved, so it does not subsume the other following cases.
  Handle<Symbol> key = isolate->factory()->promise_handled_by_symbol();
  Handle<Object> outer_promise_obj = JSObject::GetDataProperty(promise, key);
  if (outer_promise_obj->IsJSPromise() &&
      InternalPromiseHasUserDefinedRejectHandler(
          isolate, Handle<JSPromise>::cast(outer_promise_obj))) {
    return true;
  }

  if (promise->status() == Promise::kPending) {
    for (Handle<Object> current(promise->reactions(), isolate);
         !current->IsSmi();) {
      Handle<PromiseReaction> reaction = Handle<PromiseReaction>::cast(current);
      Handle<HeapObject> promise_or_capability(
          reaction->promise_or_capability(), isolate);
      if (!promise_or_capability->IsUndefined(isolate)) {
        Handle<JSPromise> promise = Handle<JSPromise>::cast(
            promise_or_capability->IsJSPromise()
                ? promise_or_capability
                : handle(Handle<PromiseCapability>::cast(promise_or_capability)
                             ->promise(),
                         isolate));
        if (reaction->reject_handler().IsUndefined(isolate)) {
          if (InternalPromiseHasUserDefinedRejectHandler(isolate, promise)) {
            return true;
          }
        } else {
          Handle<JSReceiver> current_handler(
              JSReceiver::cast(reaction->reject_handler()), isolate);
          if (PromiseHandlerCheck(isolate, current_handler, promise)) {
            return true;
          }
        }
      }
      current = handle(reaction->next(), isolate);
    }
  }

  return false;
}

}  // namespace

bool Isolate::PromiseHasUserDefinedRejectHandler(Handle<Object> promise) {
  if (!promise->IsJSPromise()) return false;
  return InternalPromiseHasUserDefinedRejectHandler(
      this, Handle<JSPromise>::cast(promise));
}

Handle<Object> Isolate::GetPromiseOnStackOnThrow() {
  Handle<Object> undefined = factory()->undefined_value();
  ThreadLocalTop* tltop = thread_local_top();
  if (tltop->promise_on_stack_ == nullptr) return undefined;
  // Find the top-most try-catch or try-finally handler.
  CatchType prediction = PredictExceptionCatcher();
  if (prediction == NOT_CAUGHT || prediction == CAUGHT_BY_EXTERNAL) {
    return undefined;
  }
  Handle<Object> retval = undefined;
  PromiseOnStack* promise_on_stack = tltop->promise_on_stack_;
  for (StackFrameIterator it(this); !it.done(); it.Advance()) {
    StackFrame* frame = it.frame();
    HandlerTable::CatchPrediction catch_prediction;
    if (frame->is_java_script()) {
      catch_prediction = PredictException(JavaScriptFrame::cast(frame));
    } else if (frame->type() == StackFrame::STUB) {
      Code code = frame->LookupCode();
      if (!code.IsCode() || code.kind() != Code::BUILTIN ||
          !code.has_handler_table() || !code.is_turbofanned()) {
        continue;
      }
      catch_prediction = code.GetBuiltinCatchPrediction();
    } else {
      continue;
    }

    switch (catch_prediction) {
      case HandlerTable::UNCAUGHT:
        continue;
      case HandlerTable::CAUGHT:
      case HandlerTable::DESUGARING:
        if (retval->IsJSPromise()) {
          // Caught the result of an inner async/await invocation.
          // Mark the inner promise as caught in the "synchronous case" so
          // that Debug::OnException will see. In the synchronous case,
          // namely in the code in an async function before the first
          // await, the function which has this exception event has not yet
          // returned, so the generated Promise has not yet been marked
          // by AsyncFunctionAwaitCaught with promiseHandledHintSymbol.
          Handle<JSPromise>::cast(retval)->set_handled_hint(true);
        }
        return retval;
      case HandlerTable::PROMISE:
        return promise_on_stack
                   ? Handle<Object>::cast(promise_on_stack->promise())
                   : undefined;
      case HandlerTable::ASYNC_AWAIT: {
        // If in the initial portion of async/await, continue the loop to pop up
        // successive async/await stack frames until an asynchronous one with
        // dependents is found, or a non-async stack frame is encountered, in
        // order to handle the synchronous async/await catch prediction case:
        // assume that async function calls are awaited.
        if (!promise_on_stack) return retval;
        retval = promise_on_stack->promise();
        if (PromiseHasUserDefinedRejectHandler(retval)) {
          return retval;
        }
        promise_on_stack = promise_on_stack->prev();
        continue;
      }
    }
  }
  return retval;
}

void Isolate::SetCaptureStackTraceForUncaughtExceptions(
    bool capture, int frame_limit, StackTrace::StackTraceOptions options) {
  capture_stack_trace_for_uncaught_exceptions_ = capture;
  stack_trace_for_uncaught_exceptions_frame_limit_ = frame_limit;
  stack_trace_for_uncaught_exceptions_options_ = options;
}

void Isolate::SetAbortOnUncaughtExceptionCallback(
    v8::Isolate::AbortOnUncaughtExceptionCallback callback) {
  abort_on_uncaught_exception_callback_ = callback;
}

bool Isolate::AreWasmThreadsEnabled(Handle<Context> context) {
  if (wasm_threads_enabled_callback()) {
    v8::Local<v8::Context> api_context = v8::Utils::ToLocal(context);
    return wasm_threads_enabled_callback()(api_context);
  }
  return FLAG_experimental_wasm_threads;
}

Handle<Context> Isolate::GetIncumbentContext() {
  JavaScriptFrameIterator it(this);

  // 1st candidate: most-recently-entered author function's context
  // if it's newer than the last Context::BackupIncumbentScope entry.
  //
  // NOTE: This code assumes that the stack grows downward.
  Address top_backup_incumbent =
      top_backup_incumbent_scope()
          ? top_backup_incumbent_scope()->JSStackComparableAddress()
          : 0;
  if (!it.done() &&
      (!top_backup_incumbent || it.frame()->sp() < top_backup_incumbent)) {
    Context context = Context::cast(it.frame()->context());
    return Handle<Context>(context.native_context(), this);
  }

  // 2nd candidate: the last Context::Scope's incumbent context if any.
  if (top_backup_incumbent_scope()) {
    return Utils::OpenHandle(
        *top_backup_incumbent_scope()->backup_incumbent_context_);
  }

  // Last candidate: the entered context or microtask context.
  // Given that there is no other author function is running, there must be
  // no cross-context function running, then the incumbent realm must match
  // the entry realm.
  v8::Local<v8::Context> entered_context =
      reinterpret_cast<v8::Isolate*>(this)->GetEnteredOrMicrotaskContext();
  return Utils::OpenHandle(*entered_context);
}

char* Isolate::ArchiveThread(char* to) {
  MemCopy(to, reinterpret_cast<char*>(thread_local_top()),
          sizeof(ThreadLocalTop));
  return to + sizeof(ThreadLocalTop);
}

char* Isolate::RestoreThread(char* from) {
  MemCopy(reinterpret_cast<char*>(thread_local_top()), from,
          sizeof(ThreadLocalTop));
  DCHECK(context().is_null() || context().IsContext());
  return from + sizeof(ThreadLocalTop);
}

void Isolate::ReleaseSharedPtrs() {
  base::MutexGuard lock(&managed_ptr_destructors_mutex_);
  while (managed_ptr_destructors_head_) {
    ManagedPtrDestructor* l = managed_ptr_destructors_head_;
    ManagedPtrDestructor* n = nullptr;
    managed_ptr_destructors_head_ = nullptr;
    for (; l != nullptr; l = n) {
      l->destructor_(l->shared_ptr_ptr_);
      n = l->next_;
      delete l;
    }
  }
}

void Isolate::RegisterManagedPtrDestructor(ManagedPtrDestructor* destructor) {
  base::MutexGuard lock(&managed_ptr_destructors_mutex_);
  DCHECK_NULL(destructor->prev_);
  DCHECK_NULL(destructor->next_);
  if (managed_ptr_destructors_head_) {
    managed_ptr_destructors_head_->prev_ = destructor;
  }
  destructor->next_ = managed_ptr_destructors_head_;
  managed_ptr_destructors_head_ = destructor;
}

void Isolate::UnregisterManagedPtrDestructor(ManagedPtrDestructor* destructor) {
  base::MutexGuard lock(&managed_ptr_destructors_mutex_);
  if (destructor->prev_) {
    destructor->prev_->next_ = destructor->next_;
  } else {
    DCHECK_EQ(destructor, managed_ptr_destructors_head_);
    managed_ptr_destructors_head_ = destructor->next_;
  }
  if (destructor->next_) destructor->next_->prev_ = destructor->prev_;
  destructor->prev_ = nullptr;
  destructor->next_ = nullptr;
}

void Isolate::SetWasmEngine(std::shared_ptr<wasm::WasmEngine> engine) {
  DCHECK_NULL(wasm_engine_);  // Only call once before {Init}.
  wasm_engine_ = std::move(engine);
  wasm_engine_->AddIsolate(this);
}

// NOLINTNEXTLINE
Isolate::PerIsolateThreadData::~PerIsolateThreadData() {
#if defined(USE_SIMULATOR)
  delete simulator_;
#endif
}

Isolate::PerIsolateThreadData* Isolate::ThreadDataTable::Lookup(
    ThreadId thread_id) {
  auto t = table_.find(thread_id);
  if (t == table_.end()) return nullptr;
  return t->second;
}

void Isolate::ThreadDataTable::Insert(Isolate::PerIsolateThreadData* data) {
  bool inserted = table_.insert(std::make_pair(data->thread_id_, data)).second;
  CHECK(inserted);
}

void Isolate::ThreadDataTable::Remove(PerIsolateThreadData* data) {
  table_.erase(data->thread_id_);
  delete data;
}

void Isolate::ThreadDataTable::RemoveAllThreads() {
  for (auto& x : table_) {
    delete x.second;
  }
  table_.clear();
}

class VerboseAccountingAllocator : public AccountingAllocator {
 public:
  VerboseAccountingAllocator(Heap* heap, size_t allocation_sample_bytes)
      : heap_(heap), allocation_sample_bytes_(allocation_sample_bytes) {}

  v8::internal::Segment* AllocateSegment(size_t size) override {
    v8::internal::Segment* memory = AccountingAllocator::AllocateSegment(size);
    if (!memory) return nullptr;
    size_t malloced_current = GetCurrentMemoryUsage();

    if (last_memory_usage_ + allocation_sample_bytes_ < malloced_current) {
      PrintMemoryJSON(malloced_current);
      last_memory_usage_ = malloced_current;
    }
    return memory;
  }

  void ReturnSegment(v8::internal::Segment* memory) override {
    AccountingAllocator::ReturnSegment(memory);
    size_t malloced_current = GetCurrentMemoryUsage();

    if (malloced_current + allocation_sample_bytes_ < last_memory_usage_) {
      PrintMemoryJSON(malloced_current);
      last_memory_usage_ = malloced_current;
    }
  }

  void ZoneCreation(const Zone* zone) override {
    PrintZoneModificationSample(zone, "zonecreation");
    nesting_deepth_++;
  }

  void ZoneDestruction(const Zone* zone) override {
    nesting_deepth_--;
    PrintZoneModificationSample(zone, "zonedestruction");
  }

 private:
  void PrintZoneModificationSample(const Zone* zone, const char* type) {
    PrintF(
        "{"
        "\"type\": \"%s\", "
        "\"isolate\": \"%p\", "
        "\"time\": %f, "
        "\"ptr\": \"%p\", "
        "\"name\": \"%s\", "
        "\"size\": %zu,"
        "\"nesting\": %zu}\n",
        type, reinterpret_cast<void*>(heap_->isolate()),
        heap_->isolate()->time_millis_since_init(),
        reinterpret_cast<const void*>(zone), zone->name(),
        zone->allocation_size(), nesting_deepth_.load());
  }

  void PrintMemoryJSON(size_t malloced) {
    // Note: Neither isolate, nor heap is locked, so be careful with accesses
    // as the allocator is potentially used on a concurrent thread.
    double time = heap_->isolate()->time_millis_since_init();
    PrintF(
        "{"
        "\"type\": \"zone\", "
        "\"isolate\": \"%p\", "
        "\"time\": %f, "
        "\"allocated\": %zu}\n",
        reinterpret_cast<void*>(heap_->isolate()), time, malloced);
  }

  Heap* heap_;
  std::atomic<size_t> last_memory_usage_{0};
  std::atomic<size_t> nesting_deepth_{0};
  size_t allocation_sample_bytes_;
};

#ifdef DEBUG
std::atomic<size_t> Isolate::non_disposed_isolates_;
#endif  // DEBUG

// static
Isolate* Isolate::New(IsolateAllocationMode mode) {
  // IsolateAllocator allocates the memory for the Isolate object according to
  // the given allocation mode.
  std::unique_ptr<IsolateAllocator> isolate_allocator =
      std::make_unique<IsolateAllocator>(mode);
  // Construct Isolate object in the allocated memory.
  void* isolate_ptr = isolate_allocator->isolate_memory();
  Isolate* isolate = new (isolate_ptr) Isolate(std::move(isolate_allocator));
#if V8_TARGET_ARCH_64_BIT
  DCHECK_IMPLIES(
      mode == IsolateAllocationMode::kInV8Heap,
      IsAligned(isolate->isolate_root(), kPtrComprIsolateRootAlignment));
#endif

#ifdef DEBUG
  non_disposed_isolates_++;
#endif  // DEBUG

  return isolate;
}

// static
void Isolate::Delete(Isolate* isolate) {
  DCHECK_NOT_NULL(isolate);
  // Temporarily set this isolate as current so that various parts of
  // the isolate can access it in their destructors without having a
  // direct pointer. We don't use Enter/Exit here to avoid
  // initializing the thread data.
  PerIsolateThreadData* saved_data = isolate->CurrentPerIsolateThreadData();
  DCHECK_EQ(true, isolate_key_created_.load(std::memory_order_relaxed));
  Isolate* saved_isolate = reinterpret_cast<Isolate*>(
      base::Thread::GetThreadLocal(isolate->isolate_key_));
  SetIsolateThreadLocals(isolate, nullptr);

  isolate->Deinit();

#ifdef DEBUG
  non_disposed_isolates_--;
#endif  // DEBUG

  // Take ownership of the IsolateAllocator to ensure the Isolate memory will
  // be available during Isolate descructor call.
  std::unique_ptr<IsolateAllocator> isolate_allocator =
      std::move(isolate->isolate_allocator_);
  isolate->~Isolate();
  // Now free the memory owned by the allocator.
  isolate_allocator.reset();

  // Restore the previous current isolate.
  SetIsolateThreadLocals(saved_isolate, saved_data);
}

void Isolate::SetUpFromReadOnlyHeap(ReadOnlyHeap* ro_heap) {
  DCHECK_NOT_NULL(ro_heap);
  DCHECK_IMPLIES(read_only_heap_ != nullptr, read_only_heap_ == ro_heap);
  read_only_heap_ = ro_heap;
  heap_.SetUpFromReadOnlyHeap(ro_heap);
}

v8::PageAllocator* Isolate::page_allocator() {
  return isolate_allocator_->page_allocator();
}

Isolate::Isolate(std::unique_ptr<i::IsolateAllocator> isolate_allocator)
    : isolate_data_(this),
      isolate_allocator_(std::move(isolate_allocator)),
      id_(isolate_counter.fetch_add(1, std::memory_order_relaxed)),
      allocator_(FLAG_trace_zone_stats
                     ? new VerboseAccountingAllocator(&heap_, 256 * KB)
                     : new AccountingAllocator()),
      builtins_(this),
      rail_mode_(PERFORMANCE_ANIMATION),
      code_event_dispatcher_(new CodeEventDispatcher()),
      cancelable_task_manager_(new CancelableTaskManager()) {
  TRACE_ISOLATE(constructor);
  CheckIsolateLayout();

  // ThreadManager is initialized early to support locking an isolate
  // before it is entered.
  thread_manager_ = new ThreadManager(this);

  handle_scope_data_.Initialize();

#define ISOLATE_INIT_EXECUTE(type, name, initial_value) \
  name##_ = (initial_value);
  ISOLATE_INIT_LIST(ISOLATE_INIT_EXECUTE)
#undef ISOLATE_INIT_EXECUTE

#define ISOLATE_INIT_ARRAY_EXECUTE(type, name, length) \
  memset(name##_, 0, sizeof(type) * length);
  ISOLATE_INIT_ARRAY_LIST(ISOLATE_INIT_ARRAY_EXECUTE)
#undef ISOLATE_INIT_ARRAY_EXECUTE

  InitializeLoggingAndCounters();
  debug_ = new Debug(this);

  InitializeDefaultEmbeddedBlob();

  MicrotaskQueue::SetUpDefaultMicrotaskQueue(this);
}

void Isolate::CheckIsolateLayout() {
  CHECK_EQ(OFFSET_OF(Isolate, isolate_data_), 0);
  CHECK_EQ(static_cast<int>(OFFSET_OF(Isolate, isolate_data_.embedder_data_)),
           Internals::kIsolateEmbedderDataOffset);
  CHECK_EQ(static_cast<int>(
               OFFSET_OF(Isolate, isolate_data_.fast_c_call_caller_fp_)),
           Internals::kIsolateFastCCallCallerFpOffset);
  CHECK_EQ(static_cast<int>(
               OFFSET_OF(Isolate, isolate_data_.fast_c_call_caller_pc_)),
           Internals::kIsolateFastCCallCallerPcOffset);
  CHECK_EQ(static_cast<int>(OFFSET_OF(Isolate, isolate_data_.stack_guard_)),
           Internals::kIsolateStackGuardOffset);
  CHECK_EQ(static_cast<int>(OFFSET_OF(Isolate, isolate_data_.roots_)),
           Internals::kIsolateRootsOffset);
  CHECK_EQ(Internals::kExternalMemoryOffset % 8, 0);
  CHECK_EQ(static_cast<int>(OFFSET_OF(Isolate, isolate_data_.external_memory_)),
           Internals::kExternalMemoryOffset);
  CHECK_EQ(Internals::kExternalMemoryLimitOffset % 8, 0);
  CHECK_EQ(static_cast<int>(
               OFFSET_OF(Isolate, isolate_data_.external_memory_limit_)),
           Internals::kExternalMemoryLimitOffset);
  CHECK_EQ(Internals::kExternalMemoryAtLastMarkCompactOffset % 8, 0);
  CHECK_EQ(static_cast<int>(OFFSET_OF(
               Isolate, isolate_data_.external_memory_at_last_mark_compact_)),
           Internals::kExternalMemoryAtLastMarkCompactOffset);
}

void Isolate::ClearSerializerData() {
  delete external_reference_map_;
  external_reference_map_ = nullptr;
}

bool Isolate::LogObjectRelocation() {
  return FLAG_verify_predictable || logger()->is_logging() || is_profiling() ||
         heap()->isolate()->logger()->is_listening_to_code_events() ||
         (heap_profiler() != nullptr &&
          heap_profiler()->is_tracking_object_moves()) ||
         heap()->has_heap_object_allocation_tracker();
}

void Isolate::Deinit() {
  TRACE_ISOLATE(deinit);

  tracing_cpu_profiler_.reset();
  if (FLAG_stress_sampling_allocation_profiler > 0) {
    heap_profiler()->StopSamplingHeapProfiler();
  }

#if defined(V8_OS_WIN64)
  if (win64_unwindinfo::CanRegisterUnwindInfoForNonABICompliantCodeRange() &&
      heap()->memory_allocator()) {
    const base::AddressRegion& code_range =
        heap()->memory_allocator()->code_range();
    void* start = reinterpret_cast<void*>(code_range.begin());
    win64_unwindinfo::UnregisterNonABICompliantCodeRange(start);
  }
#endif  // V8_OS_WIN64

  debug()->Unload();

  wasm_engine()->DeleteCompileJobsOnIsolate(this);

  if (concurrent_recompilation_enabled()) {
    optimizing_compile_dispatcher_->Stop();
    delete optimizing_compile_dispatcher_;
    optimizing_compile_dispatcher_ = nullptr;
  }

  BackingStore::RemoveSharedWasmMemoryObjects(this);

  heap_.mark_compact_collector()->EnsureSweepingCompleted();
  heap_.memory_allocator()->unmapper()->EnsureUnmappingCompleted();

  DumpAndResetStats();

  if (FLAG_print_deopt_stress) {
    PrintF(stdout, "=== Stress deopt counter: %u\n", stress_deopt_count_);
  }

  // We must stop the logger before we tear down other components.
  sampler::Sampler* sampler = logger_->sampler();
  if (sampler && sampler->IsActive()) sampler->Stop();

  FreeThreadResources();
  logger_->StopProfilerThread();

  // We start with the heap tear down so that releasing managed objects does
  // not cause a GC.
  heap_.StartTearDown();

  ReleaseSharedPtrs();

  delete deoptimizer_data_;
  deoptimizer_data_ = nullptr;
  builtins_.TearDown();
  bootstrapper_->TearDown();

  if (runtime_profiler_ != nullptr) {
    delete runtime_profiler_;
    runtime_profiler_ = nullptr;
  }

  delete heap_profiler_;
  heap_profiler_ = nullptr;

  compiler_dispatcher_->AbortAll();
  delete compiler_dispatcher_;
  compiler_dispatcher_ = nullptr;

  // This stops cancelable tasks (i.e. concurrent marking tasks)
  cancelable_task_manager()->CancelAndWait();

  heap_.TearDown();
  logger_->TearDown();

  if (wasm_engine_) {
    wasm_engine_->RemoveIsolate(this);
    wasm_engine_.reset();
  }

  TearDownEmbeddedBlob();

  delete interpreter_;
  interpreter_ = nullptr;

  delete ast_string_constants_;
  ast_string_constants_ = nullptr;

  code_event_dispatcher_.reset();

  delete root_index_map_;
  root_index_map_ = nullptr;

  delete compiler_zone_;
  compiler_zone_ = nullptr;
  compiler_cache_ = nullptr;

  ClearSerializerData();

  {
    base::MutexGuard lock_guard(&thread_data_table_mutex_);
    thread_data_table_.RemoveAllThreads();
  }
}

void Isolate::SetIsolateThreadLocals(Isolate* isolate,
                                     PerIsolateThreadData* data) {
  base::Thread::SetThreadLocal(isolate_key_, isolate);
  base::Thread::SetThreadLocal(per_isolate_thread_data_key_, data);
}

Isolate::~Isolate() {
  TRACE_ISOLATE(destructor);

  // The entry stack must be empty when we get here.
  DCHECK(entry_stack_ == nullptr || entry_stack_->previous_item == nullptr);

  delete entry_stack_;
  entry_stack_ = nullptr;

  delete date_cache_;
  date_cache_ = nullptr;

  delete regexp_stack_;
  regexp_stack_ = nullptr;

  delete descriptor_lookup_cache_;
  descriptor_lookup_cache_ = nullptr;

  delete load_stub_cache_;
  load_stub_cache_ = nullptr;
  delete store_stub_cache_;
  store_stub_cache_ = nullptr;

  delete materialized_object_store_;
  materialized_object_store_ = nullptr;

  delete logger_;
  logger_ = nullptr;

  delete handle_scope_implementer_;
  handle_scope_implementer_ = nullptr;

  delete code_tracer();
  set_code_tracer(nullptr);

  delete compilation_cache_;
  compilation_cache_ = nullptr;
  delete bootstrapper_;
  bootstrapper_ = nullptr;
  delete inner_pointer_to_code_cache_;
  inner_pointer_to_code_cache_ = nullptr;

  delete thread_manager_;
  thread_manager_ = nullptr;

  delete global_handles_;
  global_handles_ = nullptr;
  delete eternal_handles_;
  eternal_handles_ = nullptr;

  delete string_stream_debug_object_cache_;
  string_stream_debug_object_cache_ = nullptr;

  delete random_number_generator_;
  random_number_generator_ = nullptr;

  delete fuzzer_rng_;
  fuzzer_rng_ = nullptr;

  delete debug_;
  debug_ = nullptr;

  delete cancelable_task_manager_;
  cancelable_task_manager_ = nullptr;

  delete allocator_;
  allocator_ = nullptr;

  // Assert that |default_microtask_queue_| is the last MicrotaskQueue instance.
  DCHECK_IMPLIES(default_microtask_queue_,
                 default_microtask_queue_ == default_microtask_queue_->next());
  delete default_microtask_queue_;
  default_microtask_queue_ = nullptr;
}

void Isolate::InitializeThreadLocal() {
  thread_local_top()->Initialize(this);
  clear_pending_exception();
  clear_pending_message();
  clear_scheduled_exception();
}

void Isolate::SetTerminationOnExternalTryCatch() {
  if (try_catch_handler() == nullptr) return;
  try_catch_handler()->can_continue_ = false;
  try_catch_handler()->has_terminated_ = true;
  try_catch_handler()->exception_ =
      reinterpret_cast<void*>(ReadOnlyRoots(heap()).null_value().ptr());
}

bool Isolate::PropagatePendingExceptionToExternalTryCatch() {
  Object exception = pending_exception();

  if (IsJavaScriptHandlerOnTop(exception)) {
    thread_local_top()->external_caught_exception_ = false;
    return false;
  }

  if (!IsExternalHandlerOnTop(exception)) {
    thread_local_top()->external_caught_exception_ = false;
    return true;
  }

  thread_local_top()->external_caught_exception_ = true;
  if (!is_catchable_by_javascript(exception)) {
    SetTerminationOnExternalTryCatch();
  } else {
    v8::TryCatch* handler = try_catch_handler();
    DCHECK(thread_local_top()->pending_message_obj_.IsJSMessageObject() ||
           thread_local_top()->pending_message_obj_.IsTheHole(this));
    handler->can_continue_ = true;
    handler->has_terminated_ = false;
    handler->exception_ = reinterpret_cast<void*>(pending_exception().ptr());
    // Propagate to the external try-catch only if we got an actual message.
    if (thread_local_top()->pending_message_obj_.IsTheHole(this)) return true;

    handler->message_obj_ =
        reinterpret_cast<void*>(thread_local_top()->pending_message_obj_.ptr());
  }
  return true;
}

bool Isolate::InitializeCounters() {
  if (async_counters_) return false;
  async_counters_ = std::make_shared<Counters>(this);
  return true;
}

void Isolate::InitializeLoggingAndCounters() {
  if (logger_ == nullptr) {
    logger_ = new Logger(this);
  }
  InitializeCounters();
}

namespace {

void CreateOffHeapTrampolines(Isolate* isolate) {
  DCHECK_NOT_NULL(isolate->embedded_blob());
  DCHECK_NE(0, isolate->embedded_blob_size());

  HandleScope scope(isolate);
  Builtins* builtins = isolate->builtins();

  EmbeddedData d = EmbeddedData::FromBlob();

  for (int i = 0; i < Builtins::builtin_count; i++) {
    if (!Builtins::IsIsolateIndependent(i)) continue;

    Address instruction_start = d.InstructionStartOfBuiltin(i);
    Handle<Code> trampoline = isolate->factory()->NewOffHeapTrampolineFor(
        builtins->builtin_handle(i), instruction_start);

    // From this point onwards, the old builtin code object is unreachable and
    // will be collected by the next GC.
    builtins->set_builtin(i, *trampoline);
  }
}

#ifdef DEBUG
bool IsolateIsCompatibleWithEmbeddedBlob(Isolate* isolate) {
  EmbeddedData d = EmbeddedData::FromBlob(isolate);
  return (d.IsolateHash() == isolate->HashIsolateForEmbeddedBlob());
}
#endif  // DEBUG

}  // namespace

void Isolate::InitializeDefaultEmbeddedBlob() {
  const uint8_t* blob = DefaultEmbeddedBlob();
  uint32_t size = DefaultEmbeddedBlobSize();

#ifdef V8_MULTI_SNAPSHOTS
  if (!FLAG_untrusted_code_mitigations) {
    blob = TrustedEmbeddedBlob();
    size = TrustedEmbeddedBlobSize();
  }
#endif

  if (StickyEmbeddedBlob() != nullptr) {
    base::MutexGuard guard(current_embedded_blob_refcount_mutex_.Pointer());
    // Check again now that we hold the lock.
    if (StickyEmbeddedBlob() != nullptr) {
      blob = StickyEmbeddedBlob();
      size = StickyEmbeddedBlobSize();
      current_embedded_blob_refs_++;
    }
  }

  if (blob == nullptr) {
    CHECK_EQ(0, size);
  } else {
    SetEmbeddedBlob(blob, size);
  }
}

void Isolate::CreateAndSetEmbeddedBlob() {
  base::MutexGuard guard(current_embedded_blob_refcount_mutex_.Pointer());

  PrepareBuiltinSourcePositionMap();

  // If a sticky blob has been set, we reuse it.
  if (StickyEmbeddedBlob() != nullptr) {
    CHECK_EQ(embedded_blob(), StickyEmbeddedBlob());
    CHECK_EQ(CurrentEmbeddedBlob(), StickyEmbeddedBlob());
  } else {
    // Create and set a new embedded blob.
    uint8_t* data;
    uint32_t size;
    InstructionStream::CreateOffHeapInstructionStream(this, &data, &size);

    CHECK_EQ(0, current_embedded_blob_refs_);
    const uint8_t* const_data = const_cast<const uint8_t*>(data);
    SetEmbeddedBlob(const_data, size);
    current_embedded_blob_refs_++;

    SetStickyEmbeddedBlob(const_data, size);
  }

  CreateOffHeapTrampolines(this);
}

void Isolate::TearDownEmbeddedBlob() {
  // Nothing to do in case the blob is embedded into the binary or unset.
  if (StickyEmbeddedBlob() == nullptr) return;

  CHECK_EQ(embedded_blob(), StickyEmbeddedBlob());
  CHECK_EQ(CurrentEmbeddedBlob(), StickyEmbeddedBlob());

  base::MutexGuard guard(current_embedded_blob_refcount_mutex_.Pointer());
  current_embedded_blob_refs_--;
  if (current_embedded_blob_refs_ == 0 && enable_embedded_blob_refcounting_) {
    // We own the embedded blob and are the last holder. Free it.
    InstructionStream::FreeOffHeapInstructionStream(
        const_cast<uint8_t*>(embedded_blob()), embedded_blob_size());
    ClearEmbeddedBlob();
  }
}

bool Isolate::InitWithoutSnapshot() { return Init(nullptr, nullptr); }

bool Isolate::InitWithSnapshot(ReadOnlyDeserializer* read_only_deserializer,
                               StartupDeserializer* startup_deserializer) {
  DCHECK_NOT_NULL(read_only_deserializer);
  DCHECK_NOT_NULL(startup_deserializer);
  return Init(read_only_deserializer, startup_deserializer);
}

static std::string AddressToString(uintptr_t address) {
  std::stringstream stream_address;
  stream_address << "0x" << std::hex << address;
  return stream_address.str();
}

void Isolate::AddCrashKeysForIsolateAndHeapPointers() {
  DCHECK_NOT_NULL(add_crash_key_callback_);

  const uintptr_t isolate_address = reinterpret_cast<uintptr_t>(this);
  add_crash_key_callback_(v8::CrashKeyId::kIsolateAddress,
                          AddressToString(isolate_address));

  const uintptr_t ro_space_firstpage_address =
      reinterpret_cast<uintptr_t>(heap()->read_only_space()->first_page());
  add_crash_key_callback_(v8::CrashKeyId::kReadonlySpaceFirstPageAddress,
                          AddressToString(ro_space_firstpage_address));
  const uintptr_t map_space_firstpage_address =
      reinterpret_cast<uintptr_t>(heap()->map_space()->first_page());
  add_crash_key_callback_(v8::CrashKeyId::kMapSpaceFirstPageAddress,
                          AddressToString(map_space_firstpage_address));
  const uintptr_t code_space_firstpage_address =
      reinterpret_cast<uintptr_t>(heap()->code_space()->first_page());
  add_crash_key_callback_(v8::CrashKeyId::kCodeSpaceFirstPageAddress,
                          AddressToString(code_space_firstpage_address));
}

bool Isolate::Init(ReadOnlyDeserializer* read_only_deserializer,
                   StartupDeserializer* startup_deserializer) {
  TRACE_ISOLATE(init);
  const bool create_heap_objects = (read_only_deserializer == nullptr);
  // We either have both or neither.
  DCHECK_EQ(create_heap_objects, startup_deserializer == nullptr);

  base::ElapsedTimer timer;
  if (create_heap_objects && FLAG_profile_deserialization) timer.Start();

  time_millis_at_init_ = heap_.MonotonicallyIncreasingTimeInMs();

  stress_deopt_count_ = FLAG_deopt_every_n_times;
  force_slow_path_ = FLAG_force_slow_path;

  has_fatal_error_ = false;

  // The initialization process does not handle memory exhaustion.
  AlwaysAllocateScope always_allocate(this);

#define ASSIGN_ELEMENT(CamelName, hacker_name)                  \
  isolate_addresses_[IsolateAddressId::k##CamelName##Address] = \
      reinterpret_cast<Address>(hacker_name##_address());
  FOR_EACH_ISOLATE_ADDRESS_NAME(ASSIGN_ELEMENT)
#undef ASSIGN_ELEMENT

  compilation_cache_ = new CompilationCache(this);
  descriptor_lookup_cache_ = new DescriptorLookupCache();
  inner_pointer_to_code_cache_ = new InnerPointerToCodeCache(this);
  global_handles_ = new GlobalHandles(this);
  eternal_handles_ = new EternalHandles();
  bootstrapper_ = new Bootstrapper(this);
  handle_scope_implementer_ = new HandleScopeImplementer(this);
  load_stub_cache_ = new StubCache(this);
  store_stub_cache_ = new StubCache(this);
  materialized_object_store_ = new MaterializedObjectStore(this);
  regexp_stack_ = new RegExpStack();
  regexp_stack_->isolate_ = this;
  date_cache_ = new DateCache();
  heap_profiler_ = new HeapProfiler(heap());
  interpreter_ = new interpreter::Interpreter(this);

  compiler_dispatcher_ =
      new CompilerDispatcher(this, V8::GetCurrentPlatform(), FLAG_stack_size);

  // Enable logging before setting up the heap
  logger_->SetUp(this);

  {  // NOLINT
    // Ensure that the thread has a valid stack guard.  The v8::Locker object
    // will ensure this too, but we don't have to use lockers if we are only
    // using one thread.
    ExecutionAccess lock(this);
    stack_guard()->InitThread(lock);
  }

  // SetUp the object heap.
  DCHECK(!heap_.HasBeenSetUp());
  heap_.SetUp();
  ReadOnlyHeap::SetUp(this, read_only_deserializer);
  heap_.SetUpSpaces();

  isolate_data_.external_reference_table()->Init(this);

  // Setup the wasm engine.
  if (wasm_engine_ == nullptr) {
    SetWasmEngine(wasm::WasmEngine::GetWasmEngine());
  }
  DCHECK_NOT_NULL(wasm_engine_);

  deoptimizer_data_ = new DeoptimizerData(heap());

  if (setup_delegate_ == nullptr) {
    setup_delegate_ = new SetupIsolateDelegate(create_heap_objects);
  }

  if (!FLAG_inline_new) heap_.DisableInlineAllocation();

  if (!setup_delegate_->SetupHeap(&heap_)) {
    V8::FatalProcessOutOfMemory(this, "heap object creation");
    return false;
  }

  if (create_heap_objects) {
    // Terminate the partial snapshot cache so we can iterate.
    partial_snapshot_cache_.push_back(ReadOnlyRoots(this).undefined_value());
  }

  InitializeThreadLocal();

  // Profiler has to be created after ThreadLocal is initialized
  // because it makes use of interrupts.
  tracing_cpu_profiler_.reset(new TracingCpuProfilerImpl(this));

  bootstrapper_->Initialize(create_heap_objects);

  if (create_heap_objects) {
    builtins_constants_table_builder_ = new BuiltinsConstantsTableBuilder(this);

    setup_delegate_->SetupBuiltins(this);

#ifndef V8_TARGET_ARCH_ARM
    // Store the interpreter entry trampoline on the root list. It is used as a
    // template for further copies that may later be created to help profile
    // interpreted code.
    // We currently cannot do this on arm due to RELATIVE_CODE_TARGETs
    // assuming that all possible Code targets may be addressed with an int24
    // offset, effectively limiting code space size to 32MB. We can guarantee
    // this at mksnapshot-time, but not at runtime.
    // See also: https://crbug.com/v8/8713.
    heap_.SetInterpreterEntryTrampolineForProfiling(
        heap_.builtin(Builtins::kInterpreterEntryTrampoline));
#endif

    builtins_constants_table_builder_->Finalize();
    delete builtins_constants_table_builder_;
    builtins_constants_table_builder_ = nullptr;

    CreateAndSetEmbeddedBlob();
  } else {
    setup_delegate_->SetupBuiltins(this);
  }

  // Initialize custom memcopy and memmove functions (must happen after
  // embedded blob setup).
  init_memcopy_functions();

  if (FLAG_log_internal_timer_events) {
    set_event_logger(Logger::DefaultEventLoggerSentinel);
  }

  if (FLAG_trace_turbo || FLAG_trace_turbo_graph || FLAG_turbo_profiling) {
    PrintF("Concurrent recompilation has been disabled for tracing.\n");
  } else if (OptimizingCompileDispatcher::Enabled()) {
    optimizing_compile_dispatcher_ = new OptimizingCompileDispatcher(this);
  }

  // Initialize runtime profiler before deserialization, because collections may
  // occur, clearing/updating ICs.
  runtime_profiler_ = new RuntimeProfiler(this);

  // If we are deserializing, read the state into the now-empty heap.
  {
    AlwaysAllocateScope always_allocate(this);
    CodeSpaceMemoryModificationScope modification_scope(&heap_);

    if (create_heap_objects) {
      heap_.read_only_space()->ClearStringPaddingIfNeeded();
      read_only_heap_->OnCreateHeapObjectsComplete(this);
    } else {
      startup_deserializer->DeserializeInto(this);
    }
    load_stub_cache_->Initialize();
    store_stub_cache_->Initialize();
    interpreter_->Initialize();
    heap_.NotifyDeserializationComplete();
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    heap_.VerifyReadOnlyHeap();
  }
#endif

  delete setup_delegate_;
  setup_delegate_ = nullptr;

  Builtins::InitializeBuiltinEntryTable(this);
  Builtins::EmitCodeCreateEvents(this);

#ifdef DEBUG
  // Verify that the current heap state (usually deserialized from the snapshot)
  // is compatible with the embedded blob. If this DCHECK fails, we've likely
  // loaded a snapshot generated by a different V8 version or build-time
  // configuration.
  if (!IsolateIsCompatibleWithEmbeddedBlob(this)) {
    FATAL(
        "The Isolate is incompatible with the embedded blob. This is usually "
        "caused by incorrect usage of mksnapshot. When generating custom "
        "snapshots, embedders must ensure they pass the same flags as during "
        "the V8 build process (e.g.: --turbo-instruction-scheduling).");
  }
#endif  // DEBUG

#ifndef V8_TARGET_ARCH_ARM
  // The IET for profiling should always be a full on-heap Code object.
  DCHECK(!Code::cast(heap_.interpreter_entry_trampoline_for_profiling())
              .is_off_heap_trampoline());
#endif  // V8_TARGET_ARCH_ARM

  if (FLAG_print_builtin_code) builtins()->PrintBuiltinCode();
  if (FLAG_print_builtin_size) builtins()->PrintBuiltinSize();

  // Finish initialization of ThreadLocal after deserialization is done.
  clear_pending_exception();
  clear_pending_message();
  clear_scheduled_exception();

  // Quiet the heap NaN if needed on target platform.
  if (!create_heap_objects)
    Assembler::QuietNaN(ReadOnlyRoots(this).nan_value());

  if (FLAG_trace_turbo) {
    // Create an empty file.
    std::ofstream(GetTurboCfgFileName(this).c_str(), std::ios_base::trunc);
  }

  {
    HandleScope scope(this);
    ast_string_constants_ = new AstStringConstants(this, HashSeed(this));
  }

  initialized_from_snapshot_ = !create_heap_objects;

  if (FLAG_stress_sampling_allocation_profiler > 0) {
    uint64_t sample_interval = FLAG_stress_sampling_allocation_profiler;
    int stack_depth = 128;
    v8::HeapProfiler::SamplingFlags sampling_flags =
        v8::HeapProfiler::SamplingFlags::kSamplingForceGC;
    heap_profiler()->StartSamplingHeapProfiler(sample_interval, stack_depth,
                                               sampling_flags);
  }

#if defined(V8_OS_WIN64)
  if (win64_unwindinfo::CanRegisterUnwindInfoForNonABICompliantCodeRange()) {
    const base::AddressRegion& code_range =
        heap()->memory_allocator()->code_range();
    void* start = reinterpret_cast<void*>(code_range.begin());
    size_t size_in_bytes = code_range.size();
    win64_unwindinfo::RegisterNonABICompliantCodeRange(start, size_in_bytes);
  }
#endif  // V8_OS_WIN64

  if (create_heap_objects && FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    PrintF("[Initializing isolate from scratch took %0.3f ms]\n", ms);
  }

  return true;
}

void Isolate::Enter() {
  Isolate* current_isolate = nullptr;
  PerIsolateThreadData* current_data = CurrentPerIsolateThreadData();
  if (current_data != nullptr) {
    current_isolate = current_data->isolate_;
    DCHECK_NOT_NULL(current_isolate);
    if (current_isolate == this) {
      DCHECK(Current() == this);
      DCHECK_NOT_NULL(entry_stack_);
      DCHECK(entry_stack_->previous_thread_data == nullptr ||
             entry_stack_->previous_thread_data->thread_id() ==
                 ThreadId::Current());
      // Same thread re-enters the isolate, no need to re-init anything.
      entry_stack_->entry_count++;
      return;
    }
  }

  PerIsolateThreadData* data = FindOrAllocatePerThreadDataForThisThread();
  DCHECK_NOT_NULL(data);
  DCHECK(data->isolate_ == this);

  EntryStackItem* item =
      new EntryStackItem(current_data, current_isolate, entry_stack_);
  entry_stack_ = item;

  SetIsolateThreadLocals(this, data);

  // In case it's the first time some thread enters the isolate.
  set_thread_id(data->thread_id());
}

void Isolate::Exit() {
  DCHECK_NOT_NULL(entry_stack_);
  DCHECK(entry_stack_->previous_thread_data == nullptr ||
         entry_stack_->previous_thread_data->thread_id() ==
             ThreadId::Current());

  if (--entry_stack_->entry_count > 0) return;

  DCHECK_NOT_NULL(CurrentPerIsolateThreadData());
  DCHECK(CurrentPerIsolateThreadData()->isolate_ == this);

  // Pop the stack.
  EntryStackItem* item = entry_stack_;
  entry_stack_ = item->previous_item;

  PerIsolateThreadData* previous_thread_data = item->previous_thread_data;
  Isolate* previous_isolate = item->previous_isolate;

  delete item;

  // Reinit the current thread for the isolate it was running before this one.
  SetIsolateThreadLocals(previous_isolate, previous_thread_data);
}

void Isolate::LinkDeferredHandles(DeferredHandles* deferred) {
  deferred->next_ = deferred_handles_head_;
  if (deferred_handles_head_ != nullptr) {
    deferred_handles_head_->previous_ = deferred;
  }
  deferred_handles_head_ = deferred;
}

void Isolate::UnlinkDeferredHandles(DeferredHandles* deferred) {
#ifdef DEBUG
  // In debug mode assert that the linked list is well-formed.
  DeferredHandles* deferred_iterator = deferred;
  while (deferred_iterator->previous_ != nullptr) {
    deferred_iterator = deferred_iterator->previous_;
  }
  DCHECK(deferred_handles_head_ == deferred_iterator);
#endif
  if (deferred_handles_head_ == deferred) {
    deferred_handles_head_ = deferred_handles_head_->next_;
  }
  if (deferred->next_ != nullptr) {
    deferred->next_->previous_ = deferred->previous_;
  }
  if (deferred->previous_ != nullptr) {
    deferred->previous_->next_ = deferred->next_;
  }
}

void Isolate::DumpAndResetStats() {
  if (turbo_statistics() != nullptr) {
    DCHECK(FLAG_turbo_stats || FLAG_turbo_stats_nvp);
    StdoutStream os;
    if (FLAG_turbo_stats) {
      AsPrintableStatistics ps = {*turbo_statistics(), false};
      os << ps << std::endl;
    }
    if (FLAG_turbo_stats_nvp) {
      AsPrintableStatistics ps = {*turbo_statistics(), true};
      os << ps << std::endl;
    }
    delete turbo_statistics_;
    turbo_statistics_ = nullptr;
  }
  // TODO(7424): There is no public API for the {WasmEngine} yet. So for now we
  // just dump and reset the engines statistics together with the Isolate.
  if (FLAG_turbo_stats_wasm) {
    wasm_engine()->DumpAndResetTurboStatistics();
  }
  if (V8_UNLIKELY(TracingFlags::runtime_stats.load(std::memory_order_relaxed) ==
                  v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE)) {
    counters()->worker_thread_runtime_call_stats()->AddToMainTable(
        counters()->runtime_call_stats());
    counters()->runtime_call_stats()->Print();
    counters()->runtime_call_stats()->Reset();
  }
}

void Isolate::AbortConcurrentOptimization(BlockingBehavior behavior) {
  if (concurrent_recompilation_enabled()) {
    DisallowHeapAllocation no_recursive_gc;
    optimizing_compile_dispatcher()->Flush(behavior);
  }
}

CompilationStatistics* Isolate::GetTurboStatistics() {
  if (turbo_statistics() == nullptr)
    set_turbo_statistics(new CompilationStatistics());
  return turbo_statistics();
}

CodeTracer* Isolate::GetCodeTracer() {
  if (code_tracer() == nullptr) set_code_tracer(new CodeTracer(id()));
  return code_tracer();
}

bool Isolate::use_optimizer() {
  return FLAG_opt && !serializer_enabled_ && CpuFeatures::SupportsOptimizer() &&
         !is_precise_count_code_coverage();
}

bool Isolate::NeedsDetailedOptimizedCodeLineInfo() const {
  return NeedsSourcePositionsForProfiling() ||
         detailed_source_positions_for_profiling();
}

bool Isolate::NeedsSourcePositionsForProfiling() const {
  return FLAG_trace_deopt || FLAG_trace_turbo || FLAG_trace_turbo_graph ||
         FLAG_turbo_profiling || FLAG_perf_prof || is_profiling() ||
         debug_->is_active() || logger_->is_logging() || FLAG_trace_maps;
}

void Isolate::SetFeedbackVectorsForProfilingTools(Object value) {
  DCHECK(value.IsUndefined(this) || value.IsArrayList());
  heap()->set_feedback_vectors_for_profiling_tools(value);
}

void Isolate::MaybeInitializeVectorListFromHeap() {
  if (!heap()->feedback_vectors_for_profiling_tools().IsUndefined(this)) {
    // Already initialized, return early.
    DCHECK(heap()->feedback_vectors_for_profiling_tools().IsArrayList());
    return;
  }

  // Collect existing feedback vectors.
  std::vector<Handle<FeedbackVector>> vectors;

  {
    HeapObjectIterator heap_iterator(heap());
    for (HeapObject current_obj = heap_iterator.Next(); !current_obj.is_null();
         current_obj = heap_iterator.Next()) {
      if (!current_obj.IsFeedbackVector()) continue;

      FeedbackVector vector = FeedbackVector::cast(current_obj);
      SharedFunctionInfo shared = vector.shared_function_info();

      // No need to preserve the feedback vector for non-user-visible functions.
      if (!shared.IsSubjectToDebugging()) continue;

      vectors.emplace_back(vector, this);
    }
  }

  // Add collected feedback vectors to the root list lest we lose them to GC.
  Handle<ArrayList> list =
      ArrayList::New(this, static_cast<int>(vectors.size()));
  for (const auto& vector : vectors) list = ArrayList::Add(this, list, vector);
  SetFeedbackVectorsForProfilingTools(*list);
}

void Isolate::set_date_cache(DateCache* date_cache) {
  if (date_cache != date_cache_) {
    delete date_cache_;
  }
  date_cache_ = date_cache;
}

bool Isolate::IsArrayOrObjectOrStringPrototype(Object object) {
  Object context = heap()->native_contexts_list();
  while (!context.IsUndefined(this)) {
    Context current_context = Context::cast(context);
    if (current_context.initial_object_prototype() == object ||
        current_context.initial_array_prototype() == object ||
        current_context.initial_string_prototype() == object) {
      return true;
    }
    context = current_context.next_context_link();
  }
  return false;
}

bool Isolate::IsInAnyContext(Object object, uint32_t index) {
  DisallowHeapAllocation no_gc;
  Object context = heap()->native_contexts_list();
  while (!context.IsUndefined(this)) {
    Context current_context = Context::cast(context);
    if (current_context.get(index) == object) {
      return true;
    }
    context = current_context.next_context_link();
  }
  return false;
}

void Isolate::UpdateNoElementsProtectorOnSetElement(Handle<JSObject> object) {
  DisallowHeapAllocation no_gc;
  if (!object->map().is_prototype_map()) return;
  if (!Protectors::IsNoElementsIntact(this)) return;
  if (!IsArrayOrObjectOrStringPrototype(*object)) return;
  Protectors::InvalidateNoElements(this);
}

bool Isolate::IsAnyInitialArrayPrototype(Handle<JSArray> array) {
  DisallowHeapAllocation no_gc;
  return IsInAnyContext(*array, Context::INITIAL_ARRAY_PROTOTYPE_INDEX);
}

static base::RandomNumberGenerator* ensure_rng_exists(
    base::RandomNumberGenerator** rng, int seed) {
  if (*rng == nullptr) {
    if (seed != 0) {
      *rng = new base::RandomNumberGenerator(seed);
    } else {
      *rng = new base::RandomNumberGenerator();
    }
  }
  return *rng;
}

base::RandomNumberGenerator* Isolate::random_number_generator() {
  // TODO(bmeurer) Initialized lazily because it depends on flags; can
  // be fixed once the default isolate cleanup is done.
  return ensure_rng_exists(&random_number_generator_, FLAG_random_seed);
}

base::RandomNumberGenerator* Isolate::fuzzer_rng() {
  if (fuzzer_rng_ == nullptr) {
    int64_t seed = FLAG_fuzzer_random_seed;
    if (seed == 0) {
      seed = random_number_generator()->initial_seed();
    }

    fuzzer_rng_ = new base::RandomNumberGenerator(seed);
  }

  return fuzzer_rng_;
}

int Isolate::GenerateIdentityHash(uint32_t mask) {
  int hash;
  int attempts = 0;
  do {
    hash = random_number_generator()->NextInt() & mask;
  } while (hash == 0 && attempts++ < 30);
  return hash != 0 ? hash : 1;
}

Code Isolate::FindCodeObject(Address a) {
  return heap()->GcSafeFindCodeForInnerPointer(a);
}

#ifdef DEBUG
#define ISOLATE_FIELD_OFFSET(type, name, ignored) \
  const intptr_t Isolate::name##_debug_offset_ = OFFSET_OF(Isolate, name##_);
ISOLATE_INIT_LIST(ISOLATE_FIELD_OFFSET)
ISOLATE_INIT_ARRAY_LIST(ISOLATE_FIELD_OFFSET)
#undef ISOLATE_FIELD_OFFSET
#endif

Handle<Symbol> Isolate::SymbolFor(RootIndex dictionary_index,
                                  Handle<String> name, bool private_symbol) {
  Handle<String> key = factory()->InternalizeString(name);
  Handle<NameDictionary> dictionary =
      Handle<NameDictionary>::cast(root_handle(dictionary_index));
  InternalIndex entry = dictionary->FindEntry(this, key);
  Handle<Symbol> symbol;
  if (entry.is_not_found()) {
    symbol =
        private_symbol ? factory()->NewPrivateSymbol() : factory()->NewSymbol();
    symbol->set_description(*key);
    dictionary = NameDictionary::Add(this, dictionary, key, symbol,
                                     PropertyDetails::Empty(), &entry);
    switch (dictionary_index) {
      case RootIndex::kPublicSymbolTable:
        symbol->set_is_in_public_symbol_table(true);
        heap()->set_public_symbol_table(*dictionary);
        break;
      case RootIndex::kApiSymbolTable:
        heap()->set_api_symbol_table(*dictionary);
        break;
      case RootIndex::kApiPrivateSymbolTable:
        heap()->set_api_private_symbol_table(*dictionary);
        break;
      default:
        UNREACHABLE();
    }
  } else {
    symbol = Handle<Symbol>(Symbol::cast(dictionary->ValueAt(entry)), this);
  }
  return symbol;
}

void Isolate::AddBeforeCallEnteredCallback(BeforeCallEnteredCallback callback) {
  auto pos = std::find(before_call_entered_callbacks_.begin(),
                       before_call_entered_callbacks_.end(), callback);
  if (pos != before_call_entered_callbacks_.end()) return;
  before_call_entered_callbacks_.push_back(callback);
}

void Isolate::RemoveBeforeCallEnteredCallback(
    BeforeCallEnteredCallback callback) {
  auto pos = std::find(before_call_entered_callbacks_.begin(),
                       before_call_entered_callbacks_.end(), callback);
  if (pos == before_call_entered_callbacks_.end()) return;
  before_call_entered_callbacks_.erase(pos);
}

void Isolate::AddCallCompletedCallback(CallCompletedCallback callback) {
  auto pos = std::find(call_completed_callbacks_.begin(),
                       call_completed_callbacks_.end(), callback);
  if (pos != call_completed_callbacks_.end()) return;
  call_completed_callbacks_.push_back(callback);
}

void Isolate::RemoveCallCompletedCallback(CallCompletedCallback callback) {
  auto pos = std::find(call_completed_callbacks_.begin(),
                       call_completed_callbacks_.end(), callback);
  if (pos == call_completed_callbacks_.end()) return;
  call_completed_callbacks_.erase(pos);
}

void Isolate::FireCallCompletedCallback(MicrotaskQueue* microtask_queue) {
  if (!thread_local_top()->CallDepthIsZero()) return;

  bool run_microtasks =
      microtask_queue && microtask_queue->size() &&
      !microtask_queue->HasMicrotasksSuppressions() &&
      microtask_queue->microtasks_policy() == v8::MicrotasksPolicy::kAuto;

  if (run_microtasks) {
    microtask_queue->RunMicrotasks(this);
  }

  if (call_completed_callbacks_.empty()) return;
  // Fire callbacks.  Increase call depth to prevent recursive callbacks.
  v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(this);
  v8::Isolate::SuppressMicrotaskExecutionScope suppress(isolate);
  std::vector<CallCompletedCallback> callbacks(call_completed_callbacks_);
  for (auto& callback : callbacks) {
    callback(reinterpret_cast<v8::Isolate*>(this));
  }
}

void Isolate::PromiseHookStateUpdated() {
  bool promise_hook_or_async_event_delegate =
      promise_hook_ || async_event_delegate_;
  bool promise_hook_or_debug_is_active_or_async_event_delegate =
      promise_hook_or_async_event_delegate || debug()->is_active();
  if (promise_hook_or_debug_is_active_or_async_event_delegate &&
      Protectors::IsPromiseHookIntact(this)) {
    HandleScope scope(this);
    Protectors::InvalidatePromiseHook(this);
  }
  promise_hook_or_async_event_delegate_ = promise_hook_or_async_event_delegate;
  promise_hook_or_debug_is_active_or_async_event_delegate_ =
      promise_hook_or_debug_is_active_or_async_event_delegate;
}

namespace {

MaybeHandle<JSPromise> NewRejectedPromise(Isolate* isolate,
                                          v8::Local<v8::Context> api_context,
                                          Handle<Object> exception) {
  v8::Local<v8::Promise::Resolver> resolver;
  ASSIGN_RETURN_ON_SCHEDULED_EXCEPTION_VALUE(
      isolate, resolver, v8::Promise::Resolver::New(api_context),
      MaybeHandle<JSPromise>());

  RETURN_ON_SCHEDULED_EXCEPTION_VALUE(
      isolate, resolver->Reject(api_context, v8::Utils::ToLocal(exception)),
      MaybeHandle<JSPromise>());

  v8::Local<v8::Promise> promise = resolver->GetPromise();
  return v8::Utils::OpenHandle(*promise);
}

}  // namespace

MaybeHandle<JSPromise> Isolate::RunHostImportModuleDynamicallyCallback(
    Handle<Script> referrer, Handle<Object> specifier) {
  v8::Local<v8::Context> api_context =
      v8::Utils::ToLocal(Handle<Context>(native_context()));

  if (host_import_module_dynamically_callback_ == nullptr) {
    Handle<Object> exception =
        factory()->NewError(error_function(), MessageTemplate::kUnsupported);
    return NewRejectedPromise(this, api_context, exception);
  }

  Handle<String> specifier_str;
  MaybeHandle<String> maybe_specifier = Object::ToString(this, specifier);
  if (!maybe_specifier.ToHandle(&specifier_str)) {
    Handle<Object> exception(pending_exception(), this);
    clear_pending_exception();

    return NewRejectedPromise(this, api_context, exception);
  }
  DCHECK(!has_pending_exception());

  v8::Local<v8::Promise> promise;
  ASSIGN_RETURN_ON_SCHEDULED_EXCEPTION_VALUE(
      this, promise,
      host_import_module_dynamically_callback_(
          api_context, v8::Utils::ScriptOrModuleToLocal(referrer),
          v8::Utils::ToLocal(specifier_str)),
      MaybeHandle<JSPromise>());
  return v8::Utils::OpenHandle(*promise);
}

void Isolate::ClearKeptObjects() { heap()->ClearKeptObjects(); }

void Isolate::SetHostCleanupFinalizationGroupCallback(
    HostCleanupFinalizationGroupCallback callback) {
  host_cleanup_finalization_group_callback_ = callback;
}

void Isolate::RunHostCleanupFinalizationGroupCallback(
    Handle<JSFinalizationGroup> fg) {
  if (host_cleanup_finalization_group_callback_ != nullptr) {
    v8::Local<v8::Context> api_context =
        v8::Utils::ToLocal(handle(Context::cast(fg->native_context()), this));
    host_cleanup_finalization_group_callback_(api_context,
                                              v8::Utils::ToLocal(fg));
  }
}

void Isolate::SetHostImportModuleDynamicallyCallback(
    HostImportModuleDynamicallyCallback callback) {
  host_import_module_dynamically_callback_ = callback;
}

Handle<JSObject> Isolate::RunHostInitializeImportMetaObjectCallback(
    Handle<SourceTextModule> module) {
  Handle<HeapObject> host_meta(module->import_meta(), this);
  if (host_meta->IsTheHole(this)) {
    host_meta = factory()->NewJSObjectWithNullProto();
    if (host_initialize_import_meta_object_callback_ != nullptr) {
      v8::Local<v8::Context> api_context =
          v8::Utils::ToLocal(Handle<Context>(native_context()));
      host_initialize_import_meta_object_callback_(
          api_context, Utils::ToLocal(Handle<Module>::cast(module)),
          v8::Local<v8::Object>::Cast(v8::Utils::ToLocal(host_meta)));
    }
    module->set_import_meta(*host_meta);
  }
  return Handle<JSObject>::cast(host_meta);
}

void Isolate::SetHostInitializeImportMetaObjectCallback(
    HostInitializeImportMetaObjectCallback callback) {
  host_initialize_import_meta_object_callback_ = callback;
}

MaybeHandle<Object> Isolate::RunPrepareStackTraceCallback(
    Handle<Context> context, Handle<JSObject> error, Handle<JSArray> sites) {
  v8::Local<v8::Context> api_context = Utils::ToLocal(context);

  v8::Local<v8::Value> stack;
  ASSIGN_RETURN_ON_SCHEDULED_EXCEPTION_VALUE(
      this, stack,
      prepare_stack_trace_callback_(api_context, Utils::ToLocal(error),
                                    Utils::ToLocal(sites)),
      MaybeHandle<Object>());
  return Utils::OpenHandle(*stack);
}

int Isolate::LookupOrAddExternallyCompiledFilename(const char* filename) {
  if (embedded_file_writer_ != nullptr) {
    return embedded_file_writer_->LookupOrAddExternallyCompiledFilename(
        filename);
  }
  return 0;
}

const char* Isolate::GetExternallyCompiledFilename(int index) const {
  if (embedded_file_writer_ != nullptr) {
    return embedded_file_writer_->GetExternallyCompiledFilename(index);
  }
  return "";
}

int Isolate::GetExternallyCompiledFilenameCount() const {
  if (embedded_file_writer_ != nullptr) {
    return embedded_file_writer_->GetExternallyCompiledFilenameCount();
  }
  return 0;
}

void Isolate::PrepareBuiltinSourcePositionMap() {
  if (embedded_file_writer_ != nullptr) {
    return embedded_file_writer_->PrepareBuiltinSourcePositionMap(
        this->builtins());
  }
}

#if defined(V8_OS_WIN64)
void Isolate::SetBuiltinUnwindData(
    int builtin_index,
    const win64_unwindinfo::BuiltinUnwindInfo& unwinding_info) {
  if (embedded_file_writer_ != nullptr) {
    embedded_file_writer_->SetBuiltinUnwindData(builtin_index, unwinding_info);
  }
}
#endif  // V8_OS_WIN64

void Isolate::SetPrepareStackTraceCallback(PrepareStackTraceCallback callback) {
  prepare_stack_trace_callback_ = callback;
}

bool Isolate::HasPrepareStackTraceCallback() const {
  return prepare_stack_trace_callback_ != nullptr;
}

void Isolate::SetAddCrashKeyCallback(AddCrashKeyCallback callback) {
  add_crash_key_callback_ = callback;

  // Log the initial set of data.
  AddCrashKeysForIsolateAndHeapPointers();
}

void Isolate::SetAtomicsWaitCallback(v8::Isolate::AtomicsWaitCallback callback,
                                     void* data) {
  atomics_wait_callback_ = callback;
  atomics_wait_callback_data_ = data;
}

void Isolate::RunAtomicsWaitCallback(v8::Isolate::AtomicsWaitEvent event,
                                     Handle<JSArrayBuffer> array_buffer,
                                     size_t offset_in_bytes, int64_t value,
                                     double timeout_in_ms,
                                     AtomicsWaitWakeHandle* stop_handle) {
  DCHECK(array_buffer->is_shared());
  if (atomics_wait_callback_ == nullptr) return;
  HandleScope handle_scope(this);
  atomics_wait_callback_(
      event, v8::Utils::ToLocalShared(array_buffer), offset_in_bytes, value,
      timeout_in_ms,
      reinterpret_cast<v8::Isolate::AtomicsWaitWakeHandle*>(stop_handle),
      atomics_wait_callback_data_);
}

void Isolate::SetPromiseHook(PromiseHook hook) {
  promise_hook_ = hook;
  PromiseHookStateUpdated();
}

void Isolate::RunPromiseHook(PromiseHookType type, Handle<JSPromise> promise,
                             Handle<Object> parent) {
  RunPromiseHookForAsyncEventDelegate(type, promise);
  if (promise_hook_ == nullptr) return;
  promise_hook_(type, v8::Utils::PromiseToLocal(promise),
                v8::Utils::ToLocal(parent));
}

void Isolate::RunPromiseHookForAsyncEventDelegate(PromiseHookType type,
                                                  Handle<JSPromise> promise) {
  if (!async_event_delegate_) return;
  if (type == PromiseHookType::kResolve) return;

  if (type == PromiseHookType::kBefore) {
    if (!promise->async_task_id()) return;
    async_event_delegate_->AsyncEventOccurred(debug::kDebugWillHandle,
                                              promise->async_task_id(), false);
  } else if (type == PromiseHookType::kAfter) {
    if (!promise->async_task_id()) return;
    async_event_delegate_->AsyncEventOccurred(debug::kDebugDidHandle,
                                              promise->async_task_id(), false);
  } else {
    DCHECK(type == PromiseHookType::kInit);
    debug::DebugAsyncActionType type = debug::kDebugPromiseThen;
    bool last_frame_was_promise_builtin = false;
    JavaScriptFrameIterator it(this);
    while (!it.done()) {
      std::vector<Handle<SharedFunctionInfo>> infos;
      it.frame()->GetFunctions(&infos);
      for (size_t i = 1; i <= infos.size(); ++i) {
        Handle<SharedFunctionInfo> info = infos[infos.size() - i];
        if (info->IsUserJavaScript()) {
          // We should not report PromiseThen and PromiseCatch which is called
          // indirectly, e.g. Promise.all calls Promise.then internally.
          if (last_frame_was_promise_builtin) {
            if (!promise->async_task_id()) {
              promise->set_async_task_id(++async_task_count_);
            }
            async_event_delegate_->AsyncEventOccurred(
                type, promise->async_task_id(), debug()->IsBlackboxed(info));
          }
          return;
        }
        last_frame_was_promise_builtin = false;
        if (info->HasBuiltinId()) {
          if (info->builtin_id() == Builtins::kPromisePrototypeThen) {
            type = debug::kDebugPromiseThen;
            last_frame_was_promise_builtin = true;
          } else if (info->builtin_id() == Builtins::kPromisePrototypeCatch) {
            type = debug::kDebugPromiseCatch;
            last_frame_was_promise_builtin = true;
          } else if (info->builtin_id() == Builtins::kPromisePrototypeFinally) {
            type = debug::kDebugPromiseFinally;
            last_frame_was_promise_builtin = true;
          }
        }
      }
      it.Advance();
    }
  }
}

void Isolate::OnAsyncFunctionStateChanged(Handle<JSPromise> promise,
                                          debug::DebugAsyncActionType event) {
  if (!async_event_delegate_) return;
  if (!promise->async_task_id()) {
    promise->set_async_task_id(++async_task_count_);
  }
  async_event_delegate_->AsyncEventOccurred(event, promise->async_task_id(),
                                            false);
}

void Isolate::SetPromiseRejectCallback(PromiseRejectCallback callback) {
  promise_reject_callback_ = callback;
}

void Isolate::ReportPromiseReject(Handle<JSPromise> promise,
                                  Handle<Object> value,
                                  v8::PromiseRejectEvent event) {
  if (promise_reject_callback_ == nullptr) return;
  promise_reject_callback_(v8::PromiseRejectMessage(
      v8::Utils::PromiseToLocal(promise), event, v8::Utils::ToLocal(value)));
}

void Isolate::SetUseCounterCallback(v8::Isolate::UseCounterCallback callback) {
  DCHECK(!use_counter_callback_);
  use_counter_callback_ = callback;
}

void Isolate::CountUsage(v8::Isolate::UseCounterFeature feature) {
  // The counter callback may cause the embedder to call into V8, which is not
  // generally possible during GC.
  if (heap_.gc_state() == Heap::NOT_IN_GC) {
    if (use_counter_callback_) {
      HandleScope handle_scope(this);
      use_counter_callback_(reinterpret_cast<v8::Isolate*>(this), feature);
    }
  } else {
    heap_.IncrementDeferredCount(feature);
  }
}

// static
std::string Isolate::GetTurboCfgFileName(Isolate* isolate) {
  if (FLAG_trace_turbo_cfg_file == nullptr) {
    std::ostringstream os;
    os << "turbo-" << base::OS::GetCurrentProcessId() << "-";
    if (isolate != nullptr) {
      os << isolate->id();
    } else {
      os << "any";
    }
    os << ".cfg";
    return os.str();
  } else {
    return FLAG_trace_turbo_cfg_file;
  }
}

// Heap::detached_contexts tracks detached contexts as pairs
// (number of GC since the context was detached, the context).
void Isolate::AddDetachedContext(Handle<Context> context) {
  HandleScope scope(this);
  Handle<WeakArrayList> detached_contexts = factory()->detached_contexts();
  detached_contexts = WeakArrayList::AddToEnd(
      this, detached_contexts, MaybeObjectHandle(Smi::zero(), this),
      MaybeObjectHandle::Weak(context));
  heap()->set_detached_contexts(*detached_contexts);
}

void Isolate::AddSharedWasmMemory(Handle<WasmMemoryObject> memory_object) {
  HandleScope scope(this);
  Handle<WeakArrayList> shared_wasm_memories =
      factory()->shared_wasm_memories();
  shared_wasm_memories = WeakArrayList::AddToEnd(
      this, shared_wasm_memories, MaybeObjectHandle::Weak(memory_object));
  heap()->set_shared_wasm_memories(*shared_wasm_memories);
}

void Isolate::CheckDetachedContextsAfterGC() {
  HandleScope scope(this);
  Handle<WeakArrayList> detached_contexts = factory()->detached_contexts();
  int length = detached_contexts->length();
  if (length == 0) return;
  int new_length = 0;
  for (int i = 0; i < length; i += 2) {
    int mark_sweeps = detached_contexts->Get(i).ToSmi().value();
    MaybeObject context = detached_contexts->Get(i + 1);
    DCHECK(context->IsWeakOrCleared());
    if (!context->IsCleared()) {
      detached_contexts->Set(
          new_length, MaybeObject::FromSmi(Smi::FromInt(mark_sweeps + 1)));
      detached_contexts->Set(new_length + 1, context);
      new_length += 2;
    }
  }
  detached_contexts->set_length(new_length);
  while (new_length < length) {
    detached_contexts->Set(new_length, MaybeObject::FromSmi(Smi::zero()));
    ++new_length;
  }

  if (FLAG_trace_detached_contexts) {
    PrintF("%d detached contexts are collected out of %d\n",
           length - new_length, length);
    for (int i = 0; i < new_length; i += 2) {
      int mark_sweeps = detached_contexts->Get(i).ToSmi().value();
      MaybeObject context = detached_contexts->Get(i + 1);
      DCHECK(context->IsWeakOrCleared());
      if (mark_sweeps > 3) {
        PrintF("detached context %p\n survived %d GCs (leak?)\n",
               reinterpret_cast<void*>(context.ptr()), mark_sweeps);
      }
    }
  }
}

double Isolate::LoadStartTimeMs() {
  base::MutexGuard guard(&rail_mutex_);
  return load_start_time_ms_;
}

void Isolate::SetRAILMode(RAILMode rail_mode) {
  RAILMode old_rail_mode = rail_mode_.load();
  if (old_rail_mode != PERFORMANCE_LOAD && rail_mode == PERFORMANCE_LOAD) {
    base::MutexGuard guard(&rail_mutex_);
    load_start_time_ms_ = heap()->MonotonicallyIncreasingTimeInMs();
  }
  rail_mode_.store(rail_mode);
  if (old_rail_mode == PERFORMANCE_LOAD && rail_mode != PERFORMANCE_LOAD) {
    heap()->incremental_marking()->incremental_marking_job()->ScheduleTask(
        heap());
  }
  if (FLAG_trace_rail) {
    PrintIsolate(this, "RAIL mode: %s\n", RAILModeName(rail_mode));
  }
}

void Isolate::IsolateInBackgroundNotification() {
  is_isolate_in_background_ = true;
  heap()->ActivateMemoryReducerIfNeeded();
}

void Isolate::IsolateInForegroundNotification() {
  is_isolate_in_background_ = false;
}

void Isolate::PrintWithTimestamp(const char* format, ...) {
  base::OS::Print("[%d:%p] %8.0f ms: ", base::OS::GetCurrentProcessId(),
                  static_cast<void*>(this), time_millis_since_init());
  va_list arguments;
  va_start(arguments, format);
  base::OS::VPrint(format, arguments);
  va_end(arguments);
}

void Isolate::SetIdle(bool is_idle) {
  if (!is_profiling()) return;
  StateTag state = current_vm_state();
  DCHECK(state == EXTERNAL || state == IDLE);
  if (js_entry_sp() != kNullAddress) return;
  if (is_idle) {
    set_current_vm_state(IDLE);
  } else if (state == IDLE) {
    set_current_vm_state(EXTERNAL);
  }
}

void Isolate::CollectSourcePositionsForAllBytecodeArrays() {
  HandleScope scope(this);
  std::vector<Handle<SharedFunctionInfo>> sfis;
  {
    DisallowHeapAllocation no_gc;
    HeapObjectIterator iterator(heap());
    for (HeapObject obj = iterator.Next(); !obj.is_null();
         obj = iterator.Next()) {
      if (obj.IsSharedFunctionInfo()) {
        SharedFunctionInfo sfi = SharedFunctionInfo::cast(obj);
        if (sfi.HasBytecodeArray()) {
          sfis.push_back(Handle<SharedFunctionInfo>(sfi, this));
        }
      }
    }
  }
  for (auto sfi : sfis) {
    SharedFunctionInfo::EnsureSourcePositionsAvailable(this, sfi);
  }
}

#ifdef V8_INTL_SUPPORT
icu::UMemory* Isolate::get_cached_icu_object(ICUObjectCacheType cache_type) {
  return icu_object_cache_[cache_type].get();
}

void Isolate::set_icu_object_in_cache(ICUObjectCacheType cache_type,
                                      std::shared_ptr<icu::UMemory> obj) {
  icu_object_cache_[cache_type] = obj;
}

void Isolate::clear_cached_icu_object(ICUObjectCacheType cache_type) {
  icu_object_cache_.erase(cache_type);
}
#endif  // V8_INTL_SUPPORT

bool StackLimitCheck::JsHasOverflowed(uintptr_t gap) const {
  StackGuard* stack_guard = isolate_->stack_guard();
#ifdef USE_SIMULATOR
  // The simulator uses a separate JS stack.
  Address jssp_address = Simulator::current(isolate_)->get_sp();
  uintptr_t jssp = static_cast<uintptr_t>(jssp_address);
  if (jssp - gap < stack_guard->real_jslimit()) return true;
#endif  // USE_SIMULATOR
  return GetCurrentStackPosition() - gap < stack_guard->real_climit();
}

SaveContext::SaveContext(Isolate* isolate) : isolate_(isolate) {
  if (!isolate->context().is_null()) {
    context_ = Handle<Context>(isolate->context(), isolate);
  }

  c_entry_fp_ = isolate->c_entry_fp(isolate->thread_local_top());
}

SaveContext::~SaveContext() {
  isolate_->set_context(context_.is_null() ? Context() : *context_);
}

bool SaveContext::IsBelowFrame(StandardFrame* frame) {
  return (c_entry_fp_ == 0) || (c_entry_fp_ > frame->sp());
}

SaveAndSwitchContext::SaveAndSwitchContext(Isolate* isolate,
                                           Context new_context)
    : SaveContext(isolate) {
  isolate->set_context(new_context);
}

#ifdef DEBUG
AssertNoContextChange::AssertNoContextChange(Isolate* isolate)
    : isolate_(isolate), context_(isolate->context(), isolate) {}
#endif  // DEBUG

#undef TRACE_ISOLATE

}  // namespace internal
}  // namespace v8
