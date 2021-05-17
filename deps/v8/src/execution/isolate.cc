// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"

#include <stdlib.h>

#include <atomic>
#include <fstream>  // NOLINT(readability/streams)
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "src/api/api-inl.h"
#include "src/ast/ast-value-factory.h"
#include "src/ast/scopes.h"
#include "src/base/hashmap.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/base/sys-info.h"
#include "src/base/utils/random-number-generator.h"
#include "src/builtins/builtins-promise.h"
#include "src/builtins/constants-table-builder.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/flush-instruction-cache.h"
#include "src/common/assert-scope.h"
#include "src/common/ptr-compr.h"
#include "src/compiler-dispatcher/compiler-dispatcher.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/date/date.h"
#include "src/debug/debug-frames.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/deoptimizer/materialized-object-store.h"
#include "src/diagnostics/basic-block-profiler.h"
#include "src/diagnostics/compilation-statistics.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/local-isolate.h"
#include "src/execution/messages.h"
#include "src/execution/microtask-queue.h"
#include "src/execution/protectors-inl.h"
#include "src/execution/runtime-profiler.h"
#include "src/execution/simulator.h"
#include "src/execution/v8threads.h"
#include "src/execution/vm-state-inl.h"
#include "src/handles/persistent-handles.h"
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
#include "src/logging/metrics.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/backing-store.h"
#include "src/objects/elements.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/promise-inl.h"
#include "src/objects/prototype.h"
#include "src/objects/slots.h"
#include "src/objects/smi.h"
#include "src/objects/source-text-module-inl.h"
#include "src/objects/stack-frame-info-inl.h"
#include "src/objects/visitors.h"
#include "src/profiler/heap-profiler.h"
#include "src/profiler/tracing-cpu-profiler.h"
#include "src/regexp/regexp-stack.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/snapshot/embedded/embedded-file-writer-interface.h"
#include "src/snapshot/read-only-deserializer.h"
#include "src/snapshot/startup-deserializer.h"
#include "src/strings/string-builder-inl.h"
#include "src/strings/string-stream.h"
#include "src/tasks/cancelable-task.h"
#include "src/tracing/tracing-category-observer.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/address-map.h"
#include "src/utils/ostreams.h"
#include "src/utils/version.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/type-stats.h"
#ifdef V8_INTL_SUPPORT
#include "unicode/uobject.h"
#endif  // V8_INTL_SUPPORT

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

#if defined(V8_OS_WIN64)
#include "src/diagnostics/unwinding-info-win64.h"
#endif  // V8_OS_WIN64

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
#include "src/base/platform/wrappers.h"
#include "src/heap/conservative-stack-visitor.h"
#endif

extern "C" const uint8_t* v8_Default_embedded_blob_code_;
extern "C" uint32_t v8_Default_embedded_blob_code_size_;
extern "C" const uint8_t* v8_Default_embedded_blob_data_;
extern "C" uint32_t v8_Default_embedded_blob_data_size_;

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

const uint8_t* DefaultEmbeddedBlobCode() {
  return v8_Default_embedded_blob_code_;
}
uint32_t DefaultEmbeddedBlobCodeSize() {
  return v8_Default_embedded_blob_code_size_;
}
const uint8_t* DefaultEmbeddedBlobData() {
  return v8_Default_embedded_blob_data_;
}
uint32_t DefaultEmbeddedBlobDataSize() {
  return v8_Default_embedded_blob_data_size_;
}

#ifdef V8_MULTI_SNAPSHOTS
extern "C" const uint8_t* v8_Trusted_embedded_blob_code_;
extern "C" uint32_t v8_Trusted_embedded_blob_code_size_;
extern "C" const uint8_t* v8_Trusted_embedded_blob_data_;
extern "C" uint32_t v8_Trusted_embedded_blob_data_size_;

const uint8_t* TrustedEmbeddedBlobCode() {
  return v8_Trusted_embedded_blob_code_;
}
uint32_t TrustedEmbeddedBlobCodeSize() {
  return v8_Trusted_embedded_blob_code_size_;
}
const uint8_t* TrustedEmbeddedBlobData() {
  return v8_Trusted_embedded_blob_data_;
}
uint32_t TrustedEmbeddedBlobDataSize() {
  return v8_Trusted_embedded_blob_data_size_;
}
#endif

namespace {
// These variables provide access to the current embedded blob without requiring
// an isolate instance. This is needed e.g. by Code::InstructionStart, which may
// not have access to an isolate but still needs to access the embedded blob.
// The variables are initialized by each isolate in Init(). Writes and reads are
// relaxed since we can guarantee that the current thread has initialized these
// variables before accessing them. Different threads may race, but this is fine
// since they all attempt to set the same values of the blob pointer and size.

std::atomic<const uint8_t*> current_embedded_blob_code_(nullptr);
std::atomic<uint32_t> current_embedded_blob_code_size_(0);
std::atomic<const uint8_t*> current_embedded_blob_data_(nullptr);
std::atomic<uint32_t> current_embedded_blob_data_size_(0);

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
// - sticky_embedded_blob_code_
// - sticky_embedded_blob_code_size_
// - sticky_embedded_blob_data_
// - sticky_embedded_blob_data_size_
// - enable_embedded_blob_refcounting_
// - current_embedded_blob_refs_
base::LazyMutex current_embedded_blob_refcount_mutex_ = LAZY_MUTEX_INITIALIZER;

const uint8_t* sticky_embedded_blob_code_ = nullptr;
uint32_t sticky_embedded_blob_code_size_ = 0;
const uint8_t* sticky_embedded_blob_data_ = nullptr;
uint32_t sticky_embedded_blob_data_size_ = 0;

bool enable_embedded_blob_refcounting_ = true;
int current_embedded_blob_refs_ = 0;

const uint8_t* StickyEmbeddedBlobCode() { return sticky_embedded_blob_code_; }
uint32_t StickyEmbeddedBlobCodeSize() {
  return sticky_embedded_blob_code_size_;
}
const uint8_t* StickyEmbeddedBlobData() { return sticky_embedded_blob_data_; }
uint32_t StickyEmbeddedBlobDataSize() {
  return sticky_embedded_blob_data_size_;
}

void SetStickyEmbeddedBlob(const uint8_t* code, uint32_t code_size,
                           const uint8_t* data, uint32_t data_size) {
  sticky_embedded_blob_code_ = code;
  sticky_embedded_blob_code_size_ = code_size;
  sticky_embedded_blob_data_ = data;
  sticky_embedded_blob_data_size_ = data_size;
}

}  // namespace

void DisableEmbeddedBlobRefcounting() {
  base::MutexGuard guard(current_embedded_blob_refcount_mutex_.Pointer());
  enable_embedded_blob_refcounting_ = false;
}

void FreeCurrentEmbeddedBlob() {
  CHECK(!enable_embedded_blob_refcounting_);
  base::MutexGuard guard(current_embedded_blob_refcount_mutex_.Pointer());

  if (StickyEmbeddedBlobCode() == nullptr) return;

  CHECK_EQ(StickyEmbeddedBlobCode(), Isolate::CurrentEmbeddedBlobCode());
  CHECK_EQ(StickyEmbeddedBlobData(), Isolate::CurrentEmbeddedBlobData());

  InstructionStream::FreeOffHeapInstructionStream(
      const_cast<uint8_t*>(Isolate::CurrentEmbeddedBlobCode()),
      Isolate::CurrentEmbeddedBlobCodeSize(),
      const_cast<uint8_t*>(Isolate::CurrentEmbeddedBlobData()),
      Isolate::CurrentEmbeddedBlobDataSize());

  current_embedded_blob_code_.store(nullptr, std::memory_order_relaxed);
  current_embedded_blob_code_size_.store(0, std::memory_order_relaxed);
  current_embedded_blob_data_.store(nullptr, std::memory_order_relaxed);
  current_embedded_blob_data_size_.store(0, std::memory_order_relaxed);
  sticky_embedded_blob_code_ = nullptr;
  sticky_embedded_blob_code_size_ = 0;
  sticky_embedded_blob_data_ = nullptr;
  sticky_embedded_blob_data_size_ = 0;
}

// static
bool Isolate::CurrentEmbeddedBlobIsBinaryEmbedded() {
  // In some situations, we must be able to rely on the embedded blob being
  // immortal immovable. This is the case if the blob is binary-embedded.
  // See blob lifecycle controls above for descriptions of when the current
  // embedded blob may change (e.g. in tests or mksnapshot). If the blob is
  // binary-embedded, it is immortal immovable.
  const uint8_t* code =
      current_embedded_blob_code_.load(std::memory_order::memory_order_relaxed);
  if (code == nullptr) return false;
#ifdef V8_MULTI_SNAPSHOTS
  if (code == TrustedEmbeddedBlobCode()) return true;
#endif
  return code == DefaultEmbeddedBlobCode();
}

void Isolate::SetEmbeddedBlob(const uint8_t* code, uint32_t code_size,
                              const uint8_t* data, uint32_t data_size) {
  CHECK_NOT_NULL(code);
  CHECK_NOT_NULL(data);

  embedded_blob_code_ = code;
  embedded_blob_code_size_ = code_size;
  embedded_blob_data_ = data;
  embedded_blob_data_size_ = data_size;
  current_embedded_blob_code_.store(code, std::memory_order_relaxed);
  current_embedded_blob_code_size_.store(code_size, std::memory_order_relaxed);
  current_embedded_blob_data_.store(data, std::memory_order_relaxed);
  current_embedded_blob_data_size_.store(data_size, std::memory_order_relaxed);

#ifdef DEBUG
  // Verify that the contents of the embedded blob are unchanged from
  // serialization-time, just to ensure the compiler isn't messing with us.
  EmbeddedData d = EmbeddedData::FromBlob();
  if (d.EmbeddedBlobDataHash() != d.CreateEmbeddedBlobDataHash()) {
    FATAL(
        "Embedded blob data section checksum verification failed. This "
        "indicates that the embedded blob has been modified since compilation "
        "time.");
  }
  if (FLAG_text_is_readable) {
    if (d.EmbeddedBlobCodeHash() != d.CreateEmbeddedBlobCodeHash()) {
      FATAL(
          "Embedded blob code section checksum verification failed. This "
          "indicates that the embedded blob has been modified since "
          "compilation time. A common cause is a debugging breakpoint set "
          "within builtin code.");
    }
  }
#endif  // DEBUG

  if (FLAG_experimental_flush_embedded_blob_icache) {
    FlushInstructionCache(const_cast<uint8_t*>(code), code_size);
  }
}

void Isolate::ClearEmbeddedBlob() {
  CHECK(enable_embedded_blob_refcounting_);
  CHECK_EQ(embedded_blob_code_, CurrentEmbeddedBlobCode());
  CHECK_EQ(embedded_blob_code_, StickyEmbeddedBlobCode());
  CHECK_EQ(embedded_blob_data_, CurrentEmbeddedBlobData());
  CHECK_EQ(embedded_blob_data_, StickyEmbeddedBlobData());

  embedded_blob_code_ = nullptr;
  embedded_blob_code_size_ = 0;
  embedded_blob_data_ = nullptr;
  embedded_blob_data_size_ = 0;
  current_embedded_blob_code_.store(nullptr, std::memory_order_relaxed);
  current_embedded_blob_code_size_.store(0, std::memory_order_relaxed);
  current_embedded_blob_data_.store(nullptr, std::memory_order_relaxed);
  current_embedded_blob_data_size_.store(0, std::memory_order_relaxed);
  sticky_embedded_blob_code_ = nullptr;
  sticky_embedded_blob_code_size_ = 0;
  sticky_embedded_blob_data_ = nullptr;
  sticky_embedded_blob_data_size_ = 0;
}

const uint8_t* Isolate::embedded_blob_code() const {
  return embedded_blob_code_;
}
uint32_t Isolate::embedded_blob_code_size() const {
  return embedded_blob_code_size_;
}
const uint8_t* Isolate::embedded_blob_data() const {
  return embedded_blob_data_;
}
uint32_t Isolate::embedded_blob_data_size() const {
  return embedded_blob_data_size_;
}

// static
const uint8_t* Isolate::CurrentEmbeddedBlobCode() {
  return current_embedded_blob_code_.load(
      std::memory_order::memory_order_relaxed);
}

// static
uint32_t Isolate::CurrentEmbeddedBlobCodeSize() {
  return current_embedded_blob_code_size_.load(
      std::memory_order::memory_order_relaxed);
}

// static
const uint8_t* Isolate::CurrentEmbeddedBlobData() {
  return current_embedded_blob_data_.load(
      std::memory_order::memory_order_relaxed);
}

// static
uint32_t Isolate::CurrentEmbeddedBlobDataSize() {
  return current_embedded_blob_data_size_.load(
      std::memory_order::memory_order_relaxed);
}

size_t Isolate::HashIsolateForEmbeddedBlob() {
  DCHECK(builtins_.is_initialized());
  DCHECK(Builtins::AllBuiltinsAreIsolateIndependent());

  DisallowGarbageCollection no_gc;

  static constexpr size_t kSeed = 0;
  size_t hash = kSeed;

  // Hash data sections of builtin code objects.
  for (int i = 0; i < Builtins::builtin_count; i++) {
    Code code = heap_.builtin(i);

    DCHECK(Internals::HasHeapObjectTag(code.ptr()));
    uint8_t* const code_ptr =
        reinterpret_cast<uint8_t*>(code.ptr() - kHeapObjectTag);

    // These static asserts ensure we don't miss relevant fields. We don't hash
    // instruction/metadata size and flags since they change when creating the
    // off-heap trampolines. Other data fields must remain the same.
    STATIC_ASSERT(Code::kInstructionSizeOffset == Code::kDataStart);
    STATIC_ASSERT(Code::kMetadataSizeOffset ==
                  Code::kInstructionSizeOffsetEnd + 1);
    STATIC_ASSERT(Code::kFlagsOffset == Code::kMetadataSizeOffsetEnd + 1);
    STATIC_ASSERT(Code::kBuiltinIndexOffset == Code::kFlagsOffsetEnd + 1);
    static constexpr int kStartOffset = Code::kBuiltinIndexOffset;

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
  v->VisitRootPointer(Root::kStackRoots, nullptr,
                      FullObjectSlot(&thread->pending_exception_));
  v->VisitRootPointer(Root::kStackRoots, nullptr,
                      FullObjectSlot(&thread->pending_message_obj_));
  v->VisitRootPointer(Root::kStackRoots, nullptr,
                      FullObjectSlot(&thread->context_));
  v->VisitRootPointer(Root::kStackRoots, nullptr,
                      FullObjectSlot(&thread->scheduled_exception_));

  for (v8::TryCatch* block = thread->try_catch_handler_; block != nullptr;
       block = block->next_) {
    // TODO(3770): Make TryCatch::exception_ an Address (and message_obj_ too).
    v->VisitRootPointer(
        Root::kStackRoots, nullptr,
        FullObjectSlot(reinterpret_cast<Address>(&(block->exception_))));
    v->VisitRootPointer(
        Root::kStackRoots, nullptr,
        FullObjectSlot(reinterpret_cast<Address>(&(block->message_obj_))));
  }

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
  ConservativeStackVisitor stack_visitor(this, v);
  thread_local_top()->stack_.IteratePointers(&stack_visitor);
#endif

  // Iterate over pointers on native execution stack.
#if V8_ENABLE_WEBASSEMBLY
  wasm::WasmCodeRefScope wasm_code_ref_scope;
#endif  // V8_ENABLE_WEBASSEMBLY
  for (StackFrameIterator it(this, thread); !it.done(); it.Advance()) {
    it.frame()->Iterate(v);
  }
}

void Isolate::Iterate(RootVisitor* v) {
  ThreadLocalTop* current_t = thread_local_top();
  Iterate(v, current_t);
}

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

class StackTraceBuilder {
 public:
  enum FrameFilterMode { ALL, CURRENT_SECURITY_CONTEXT };

  StackTraceBuilder(Isolate* isolate, FrameSkipMode mode, int limit,
                    Handle<Object> caller, FrameFilterMode filter_mode)
      : isolate_(isolate),
        mode_(mode),
        limit_(limit),
        caller_(caller),
        skip_next_frame_(mode != SKIP_NONE),
        check_security_context_(filter_mode == CURRENT_SECURITY_CONTEXT) {
    DCHECK_IMPLIES(mode_ == SKIP_UNTIL_SEEN, caller_->IsJSFunction());
    // Modern web applications are usually built with multiple layers of
    // framework and library code, and stack depth tends to be more than
    // a dozen frames, so we over-allocate a bit here to avoid growing
    // the elements array in the common case.
    elements_ = isolate->factory()->NewFixedArray(std::min(64, limit));
  }

  void AppendAsyncFrame(Handle<JSGeneratorObject> generator_object) {
    Handle<JSFunction> function(generator_object->function(), isolate_);
    if (!IsVisibleInStackTrace(function)) return;
    int flags = StackFrameInfo::kIsAsync;
    if (IsStrictFrame(function)) flags |= StackFrameInfo::kIsStrict;

    Handle<Object> receiver(generator_object->receiver(), isolate_);
    Handle<BytecodeArray> code(function->shared().GetBytecodeArray(isolate_),
                               isolate_);
    // The stored bytecode offset is relative to a different base than what
    // is used in the source position table, hence the subtraction.
    int offset = Smi::ToInt(generator_object->input_or_debug_pos()) -
                 (BytecodeArray::kHeaderSize - kHeapObjectTag);

    Handle<FixedArray> parameters = isolate_->factory()->empty_fixed_array();
    if (V8_UNLIKELY(FLAG_detailed_error_stack_trace)) {
      parameters = isolate_->factory()->CopyFixedArrayUpTo(
          handle(generator_object->parameters_and_registers(), isolate_),
          function->shared().internal_formal_parameter_count());
    }

    AppendFrame(receiver, function, code, offset, flags, parameters);
  }

  void AppendPromiseCombinatorFrame(Handle<JSFunction> element_function,
                                    Handle<JSFunction> combinator) {
    if (!IsVisibleInStackTrace(combinator)) return;
    int flags =
        StackFrameInfo::kIsAsync | StackFrameInfo::kIsSourcePositionComputed;

    Handle<Object> receiver(combinator->native_context().promise_function(),
                            isolate_);
    Handle<Code> code(combinator->code(), isolate_);

    // TODO(mmarchini) save Promises list from the Promise combinator
    Handle<FixedArray> parameters = isolate_->factory()->empty_fixed_array();

    // We store the offset of the promise into the element function's
    // hash field for element callbacks.
    int promise_index =
        Smi::ToInt(Smi::cast(element_function->GetIdentityHash())) - 1;

    AppendFrame(receiver, combinator, code, promise_index, flags, parameters);
  }

  void AppendJavaScriptFrame(
      FrameSummary::JavaScriptFrameSummary const& summary) {
    // Filter out internal frames that we do not want to show.
    if (!IsVisibleInStackTrace(summary.function())) return;

    int flags = 0;
    Handle<JSFunction> function = summary.function();
    if (IsStrictFrame(function)) flags |= StackFrameInfo::kIsStrict;
    if (summary.is_constructor()) flags |= StackFrameInfo::kIsConstructor;

    AppendFrame(summary.receiver(), function, summary.abstract_code(),
                summary.code_offset(), flags, summary.parameters());
  }

#if V8_ENABLE_WEBASSEMBLY
  void AppendWasmFrame(FrameSummary::WasmFrameSummary const& summary) {
    if (summary.code()->kind() != wasm::WasmCode::kFunction) return;
    Handle<WasmInstanceObject> instance = summary.wasm_instance();
    int flags = StackFrameInfo::kIsWasm;
    if (instance->module_object().is_asm_js()) {
      flags |= StackFrameInfo::kIsAsmJsWasm;
      if (summary.at_to_number_conversion()) {
        flags |= StackFrameInfo::kIsAsmJsAtNumberConversion;
      }
    }

    auto code = Managed<wasm::GlobalWasmCodeRef>::Allocate(
        isolate_, 0, summary.code(),
        instance->module_object().shared_native_module());
    AppendFrame(instance,
                handle(Smi::FromInt(summary.function_index()), isolate_), code,
                summary.code_offset(), flags,
                isolate_->factory()->empty_fixed_array());
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  void AppendBuiltinExitFrame(BuiltinExitFrame* exit_frame) {
    Handle<JSFunction> function(exit_frame->function(), isolate_);
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
        code->GetOffsetFromInstructionStart(isolate_, exit_frame->pc());

    int flags = 0;
    if (IsStrictFrame(function)) flags |= StackFrameInfo::kIsStrict;
    if (exit_frame->IsConstructor()) flags |= StackFrameInfo::kIsConstructor;

    Handle<FixedArray> parameters = isolate_->factory()->empty_fixed_array();
    if (V8_UNLIKELY(FLAG_detailed_error_stack_trace)) {
      int param_count = exit_frame->ComputeParametersCount();
      parameters = isolate_->factory()->NewFixedArray(param_count);
      for (int i = 0; i < param_count; i++) {
        parameters->set(i, exit_frame->GetParameter(i));
      }
    }

    AppendFrame(receiver, function, code, offset, flags, parameters);
  }

  bool Full() { return index_ >= limit_; }

  Handle<FixedArray> Build() {
    return FixedArray::ShrinkOrEmpty(isolate_, elements_, index_);
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

  void AppendFrame(Handle<Object> receiver_or_instance, Handle<Object> function,
                   Handle<HeapObject> code, int offset, int flags,
                   Handle<FixedArray> parameters) {
    DCHECK_LE(index_, elements_->length());
    DCHECK_LE(elements_->length(), limit_);
    if (index_ == elements_->length()) {
      elements_ = isolate_->factory()->CopyFixedArrayAndGrow(
          elements_, std::min(16, limit_ - elements_->length()));
    }
    if (receiver_or_instance->IsTheHole(isolate_)) {
      // TODO(jgruber): Fix all cases in which frames give us a hole value
      // (e.g. the receiver in RegExp constructor frames).
      receiver_or_instance = isolate_->factory()->undefined_value();
    }
    auto info = isolate_->factory()->NewStackFrameInfo(
        receiver_or_instance, function, code, offset, flags, parameters);
    elements_->set(index_++, *info);
  }

  Isolate* isolate_;
  const FrameSkipMode mode_;
  int index_ = 0;
  const int limit_;
  const Handle<Object> caller_;
  bool skip_next_frame_;
  bool encountered_strict_function_ = false;
  const bool check_security_context_;
  Handle<FixedArray> elements_;
};

bool GetStackTraceLimit(Isolate* isolate, int* result) {
  Handle<JSObject> error = isolate->error_function();

  Handle<String> key = isolate->factory()->stackTraceLimit_string();
  Handle<Object> stack_trace_limit = JSReceiver::GetDataProperty(error, key);
  if (!stack_trace_limit->IsNumber()) return false;

  // Ensure that limit is not negative.
  *result = std::max(FastD2IChecked(stack_trace_limit->Number()), 0);

  if (*result != FLAG_stack_trace_limit) {
    isolate->CountUsage(v8::Isolate::kErrorStackTraceLimit);
  }

  return true;
}

bool NoExtension(const v8::FunctionCallbackInfo<v8::Value>&) { return false; }

namespace {

bool IsBuiltinFunction(Isolate* isolate, HeapObject object,
                       Builtins::Name builtin_index) {
  if (!object.IsJSFunction()) return false;
  JSFunction const function = JSFunction::cast(object);
  return function.code() == isolate->builtins()->builtin(builtin_index);
}

void CaptureAsyncStackTrace(Isolate* isolate, Handle<JSPromise> promise,
                            StackTraceBuilder* builder) {
  while (!builder->Full()) {
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
      Handle<JSFunction> combinator(context->native_context().promise_all(),
                                    isolate);
      builder->AppendPromiseCombinatorFrame(function, combinator);

      // Now peak into the Promise.all() resolve element context to
      // find the promise capability that's being resolved when all
      // the concurrent promises resolve.
      int const index =
          PromiseBuiltins::kPromiseAllResolveElementCapabilitySlot;
      Handle<PromiseCapability> capability(
          PromiseCapability::cast(context->get(index)), isolate);
      if (!capability->promise().IsJSPromise()) return;
      promise = handle(JSPromise::cast(capability->promise()), isolate);
    } else if (IsBuiltinFunction(isolate, reaction->reject_handler(),
                                 Builtins::kPromiseAnyRejectElementClosure)) {
      Handle<JSFunction> function(JSFunction::cast(reaction->reject_handler()),
                                  isolate);
      Handle<Context> context(function->context(), isolate);
      Handle<JSFunction> combinator(context->native_context().promise_any(),
                                    isolate);
      builder->AppendPromiseCombinatorFrame(function, combinator);

      // Now peak into the Promise.any() reject element context to
      // find the promise capability that's being resolved when any of
      // the concurrent promises resolve.
      int const index = PromiseBuiltins::kPromiseAnyRejectElementCapabilitySlot;
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

struct CaptureStackTraceOptions {
  int limit;
  // 'filter_mode' and 'skip_mode' are somewhat orthogonal. 'filter_mode'
  // specifies whether to capture all frames, or just frames in the same
  // security context. While 'skip_mode' allows skipping the first frame.
  FrameSkipMode skip_mode;
  StackTraceBuilder::FrameFilterMode filter_mode;

  bool capture_builtin_exit_frames;
  bool capture_only_frames_subject_to_debugging;
  bool async_stack_trace;
};

Handle<FixedArray> CaptureStackTrace(Isolate* isolate, Handle<Object> caller,
                                     CaptureStackTraceOptions options) {
  DisallowJavascriptExecution no_js(isolate);

  TRACE_EVENT_BEGIN1(TRACE_DISABLED_BY_DEFAULT("v8.stack_trace"),
                     "CaptureStackTrace", "maxFrameCount", options.limit);

#if V8_ENABLE_WEBASSEMBLY
  wasm::WasmCodeRefScope code_ref_scope;
#endif  // V8_ENABLE_WEBASSEMBLY

  StackTraceBuilder builder(isolate, options.skip_mode, options.limit, caller,
                            options.filter_mode);

  // Build the regular stack trace, and remember the last relevant
  // frame ID and inlined index (for the async stack trace handling
  // below, which starts from this last frame).
  for (StackFrameIterator it(isolate); !it.done() && !builder.Full();
       it.Advance()) {
    StackFrame* const frame = it.frame();
    switch (frame->type()) {
      case StackFrame::JAVA_SCRIPT_BUILTIN_CONTINUATION:
      case StackFrame::JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH:
      case StackFrame::OPTIMIZED:
      case StackFrame::INTERPRETED:
      case StackFrame::BASELINE:
      case StackFrame::BUILTIN:
#if V8_ENABLE_WEBASSEMBLY
      case StackFrame::WASM:
#endif  // V8_ENABLE_WEBASSEMBLY
      {
        // A standard frame may include many summarized frames (due to
        // inlining).
        std::vector<FrameSummary> frames;
        CommonFrame::cast(frame)->Summarize(&frames);
        for (size_t i = frames.size(); i-- != 0 && !builder.Full();) {
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
#if V8_ENABLE_WEBASSEMBLY
          } else if (summary.IsWasm()) {
            //=========================================================
            // Handle a Wasm frame.
            //=========================================================
            auto const& wasm = summary.AsWasm();
            builder.AppendWasmFrame(wasm);
#endif  // V8_ENABLE_WEBASSEMBLY
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
                            Builtins::kAsyncGeneratorYieldResolveClosure) ||
          IsBuiltinFunction(isolate, promise_reaction_job_task->handler(),
                            Builtins::kAsyncFunctionAwaitRejectClosure) ||
          IsBuiltinFunction(isolate, promise_reaction_job_task->handler(),
                            Builtins::kAsyncGeneratorAwaitRejectClosure)) {
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

  Handle<FixedArray> stack_trace = builder.Build();
  TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("v8.stack_trace"),
                   "CaptureStackTrace", "frameCount", stack_trace->length());
  return stack_trace;
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
  options.filter_mode = StackTraceBuilder::CURRENT_SECURITY_CONTEXT;
  options.capture_only_frames_subject_to_debugging = false;

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

  if (frame->is_unoptimized()) {
    UnoptimizedFrame* iframe = static_cast<UnoptimizedFrame*>(frame);
    Address bytecode_start =
        iframe->GetBytecodeArray().GetFirstBytecodeAddress();
    return bytecode_start + iframe->GetBytecodeOffset();
  }

  return frame->pc();
}

Handle<FixedArray> Isolate::CaptureCurrentStackTrace(
    int frame_limit, StackTrace::StackTraceOptions stack_trace_options) {
  CaptureStackTraceOptions options;
  options.limit = std::max(frame_limit, 0);  // Ensure no negative values.
  options.skip_mode = SKIP_NONE;
  options.capture_builtin_exit_frames = false;
  options.async_stack_trace = false;
  options.filter_mode =
      (stack_trace_options & StackTrace::kExposeFramesAcrossSecurityOrigins)
          ? StackTraceBuilder::ALL
          : StackTraceBuilder::CURRENT_SECURITY_CONTEXT;
  options.capture_only_frames_subject_to_debugging = true;

  return CaptureStackTrace(this, factory()->undefined_value(), options);
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
    DisallowGarbageCollection no_gc;
    AccessCheckInfo access_check_info = AccessCheckInfo::Get(this, receiver);
    if (access_check_info.is_null()) {
      no_gc.Release();
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
    DisallowGarbageCollection no_gc;

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
    DisallowGarbageCollection no_gc;
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

  Throw(*exception);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && FLAG_stress_compaction) {
    heap()->CollectAllGarbage(Heap::kNoGCFlags,
                              GarbageCollectionReason::kTesting);
  }
#endif  // VERIFY_HEAP

  return ReadOnlyRoots(heap()).exception();
}

Object Isolate::ThrowAt(Handle<JSObject> exception, MessageLocation* location) {
  Handle<Name> key_start_pos = factory()->error_start_pos_symbol();
  Object::SetProperty(this, exception, key_start_pos,
                      handle(Smi::FromInt(location->start_pos()), this),
                      StoreOrigin::kMaybeKeyed,
                      Just(ShouldThrow::kThrowOnError))
      .Check();

  Handle<Name> key_end_pos = factory()->error_end_pos_symbol();
  Object::SetProperty(this, exception, key_end_pos,
                      handle(Smi::FromInt(location->end_pos()), this),
                      StoreOrigin::kMaybeKeyed,
                      Just(ShouldThrow::kThrowOnError))
      .Check();

  Handle<Name> key_script = factory()->error_script_symbol();
  Object::SetProperty(this, exception, key_script, location->script(),
                      StoreOrigin::kMaybeKeyed,
                      Just(ShouldThrow::kThrowOnError))
      .Check();

  return ThrowInternal(*exception, location);
}

Object Isolate::TerminateExecution() {
  return Throw(ReadOnlyRoots(this).termination_exception());
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

namespace {

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

}  // anonymous namespace

Handle<JSMessageObject> Isolate::CreateMessageOrAbort(
    Handle<Object> exception, MessageLocation* location) {
  Handle<JSMessageObject> message_obj = CreateMessage(exception, location);

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

  return message_obj;
}

Object Isolate::ThrowInternal(Object raw_exception, MessageLocation* location) {
  DCHECK(!has_pending_exception());
  DCHECK_IMPLIES(trap_handler::IsTrapHandlerEnabled(),
                 !trap_handler::IsThreadInWasm());

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
      if (AllowGarbageCollection::IsAllowed()) {
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
    base::Optional<Object> maybe_exception = debug()->OnThrow(exception);
    if (maybe_exception.has_value()) {
      return *maybe_exception;
    }
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
      Handle<Object> message_obj = CreateMessageOrAbort(exception, location);
      thread_local_top()->pending_message_obj_ = *message_obj;
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
  DCHECK_IMPLIES(trap_handler::IsTrapHandlerEnabled(),
                 !trap_handler::IsThreadInWasm());

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
        return FoundHandler(Context(), code.InstructionStart(this, frame->pc()),
                            table.LookupReturn(0), code.constant_pool(),
                            handler->address() + StackHandlerConstants::kSize,
                            0);
      }

#if V8_ENABLE_WEBASSEMBLY
      case StackFrame::C_WASM_ENTRY: {
        StackHandler* handler = frame->top_handler();
        thread_local_top()->handler_ = handler->next_address();
        Code code = frame->LookupCode();
        HandlerTable table(code);
        Address instruction_start = code.InstructionStart(this, frame->pc());
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

      case StackFrame::WASM: {
        if (!is_catchable_by_wasm(exception)) break;

        // For WebAssembly frames we perform a lookup in the handler table.
        // This code ref scope is here to avoid a check failure when looking up
        // the code. It's not actually necessary to keep the code alive as it's
        // currently being executed.
        wasm::WasmCodeRefScope code_ref_scope;
        WasmFrame* wasm_frame = static_cast<WasmFrame*>(frame);
        wasm::WasmCode* wasm_code =
            wasm_engine()->code_manager()->LookupCode(frame->pc());
        int offset = wasm_frame->LookupExceptionHandlerInTable();
        if (offset < 0) break;
        wasm_engine()->SampleCatchEvent(this);
        // Compute the stack pointer from the frame pointer. This ensures that
        // argument slots on the stack are dropped as returning would.
        Address return_sp = frame->fp() +
                            StandardFrameConstants::kFixedFrameSizeAboveFp -
                            wasm_code->stack_slots() * kSystemPointerSize;

        // This is going to be handled by Wasm, so we need to set the TLS flag.
        trap_handler::SetThreadInWasm();

        return FoundHandler(Context(), wasm_code->instruction_start(), offset,
                            wasm_code->constant_pool(), return_sp, frame->fp());
      }

      case StackFrame::WASM_COMPILE_LAZY: {
        // Can only fail directly on invocation. This happens if an invalid
        // function was validated lazily.
        DCHECK(FLAG_wasm_lazy_validation);
        break;
      }
#endif  // V8_ENABLE_WEBASSEMBLY

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
        // but do not have a code kind of TURBOFAN.
        if (CodeKindCanDeoptimize(code.kind()) &&
            code.marked_for_deoptimization()) {
          // If the target code is lazy deoptimized, we jump to the original
          // return address, but we make a note that we are throwing, so
          // that the deoptimizer can do the right thing.
          offset = static_cast<int>(frame->pc() - code.entry());
          set_deoptimizer_lazy_throw(true);
        }

        return FoundHandler(Context(), code.InstructionStart(this, frame->pc()),
                            offset, code.constant_pool(), return_sp,
                            frame->fp());
      }

      case StackFrame::STUB: {
        // Some stubs are able to handle exceptions.
        if (!catchable_by_js) break;
        StubFrame* stub_frame = static_cast<StubFrame*>(frame);
#if defined(DEBUG) && V8_ENABLE_WEBASSEMBLY
        wasm::WasmCodeRefScope code_ref_scope;
        DCHECK_NULL(wasm_engine()->code_manager()->LookupCode(frame->pc()));
#endif  // defined(DEBUG) && V8_ENABLE_WEBASSEMBLY
        Code code = stub_frame->LookupCode();
        if (!code.IsCode() || code.kind() != CodeKind::BUILTIN ||
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

        return FoundHandler(Context(), code.InstructionStart(this, frame->pc()),
                            offset, code.constant_pool(), return_sp,
                            frame->fp());
      }

      case StackFrame::INTERPRETED:
      case StackFrame::BASELINE: {
        // For interpreted frame we perform a range lookup in the handler table.
        if (!catchable_by_js) break;
        UnoptimizedFrame* js_frame = UnoptimizedFrame::cast(frame);
        int register_slots = UnoptimizedFrameConstants::RegisterStackSlotCount(
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
        DCHECK(context.IsContext());

        if (frame->is_baseline()) {
          BaselineFrame* sp_frame = BaselineFrame::cast(js_frame);
          Code code = sp_frame->LookupCode();
          intptr_t pc_offset = sp_frame->GetPCForBytecodeOffset(offset);
          // Patch the context register directly on the frame, so that we don't
          // need to have a context read + write in the baseline code.
          sp_frame->PatchContext(context);
          return FoundHandler(
              Context(), code.InstructionStart(this, sp_frame->sp()), pc_offset,
              code.constant_pool(), return_sp, sp_frame->fp());
        } else {
          InterpretedFrame::cast(js_frame)->PatchBytecodeOffset(
              static_cast<int>(offset));

          Code code =
              builtins()->builtin(Builtins::kInterpreterEnterBytecodeDispatch);
          return FoundHandler(context, code.InstructionStart(), 0,
                              code.constant_pool(), return_sp, frame->fp());
        }
      }

      case StackFrame::BUILTIN:
        // For builtin frames we are guaranteed not to find a handler.
        if (catchable_by_js) {
          CHECK_EQ(-1, BuiltinFrame::cast(frame)->LookupExceptionHandlerInTable(
                           nullptr, nullptr));
        }
        break;

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
        if (code->IsCode() && code->kind() == CodeKind::BUILTIN) {
          prediction = code->GetCode().GetBuiltinCatchPrediction();
          if (prediction == HandlerTable::UNCAUGHT) continue;
          return prediction;
        }

        // Must have been constructed from a bytecode array.
        CHECK_EQ(CodeKind::INTERPRETED_FUNCTION, code->kind());
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
    case HandlerTable::UNCAUGHT_ASYNC_AWAIT:
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
      case StackFrame::BASELINE:
      case StackFrame::BUILTIN: {
        JavaScriptFrame* js_frame = JavaScriptFrame::cast(frame);
        Isolate::CatchType prediction = ToCatchType(PredictException(js_frame));
        if (prediction == NOT_CAUGHT) break;
        return prediction;
      } break;

      case StackFrame::STUB: {
        Handle<Code> code(frame->LookupCode(), this);
        if (!code->IsCode() || code->kind() != CodeKind::BUILTIN ||
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
  options.filter_mode = StackTraceBuilder::CURRENT_SECURITY_CONTEXT;
  options.capture_only_frames_subject_to_debugging = false;

  Handle<FixedArray> frames =
      CaptureStackTrace(this, this->factory()->undefined_value(), options);

  IncrementalStringBuilder builder(this);
  for (int i = 0; i < frames->length(); ++i) {
    Handle<StackFrameInfo> frame(StackFrameInfo::cast(frames->get(i)), this);
    SerializeStackFrameInfo(this, frame, &builder);
  }

  Handle<String> stack_trace = builder.Finish().ToHandleChecked();
  stack_trace->PrintOn(out);
}

bool Isolate::ComputeLocation(MessageLocation* target) {
  StackTraceFrameIterator it(this);
  if (it.done()) return false;
  CommonFrame* frame = it.frame();
  // Compute the location from the function and the relocation info of the
  // baseline code. For optimized code this will use the deoptimization
  // information to get canonical location information.
  std::vector<FrameSummary> frames;
#if V8_ENABLE_WEBASSEMBLY
  wasm::WasmCodeRefScope code_ref_scope;
#endif  // V8_ENABLE_WEBASSEMBLY
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
  Handle<FixedArray> stack = Handle<FixedArray>::cast(property);
  for (int i = 0; i < stack->length(); i++) {
    Handle<StackFrameInfo> frame(StackFrameInfo::cast(stack->get(i)), this);
    if (StackFrameInfo::ComputeLocation(frame, target)) return true;
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

std::vector<MemoryRange>* Isolate::GetCodePages() const {
  return code_pages_.load(std::memory_order_acquire);
}

void Isolate::SetCodePages(std::vector<MemoryRange>* new_code_pages) {
  code_pages_.store(new_code_pages, std::memory_order_release);
}

void Isolate::ReportPendingMessages() {
  DCHECK(AllowExceptions::IsAllowed(this));

  // The embedder might run script in response to an exception.
  AllowJavascriptExecutionDebugOnly allow_script(this);

  Object exception_obj = pending_exception();

  // Try to propagate the exception to an external v8::TryCatch handler. If
  // propagation was unsuccessful, then we will get another chance at reporting
  // the pending message if the exception is re-thrown.
  bool has_been_propagated = PropagatePendingExceptionToExternalTryCatch();
  if (!has_been_propagated) return;

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
  if (IsExternalHandlerOnTop(exception_obj)) {
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
bool PromiseIsRejectHandler(Isolate* isolate, Handle<JSReceiver> handler) {
  // Recurse to the forwarding Promise (e.g. return false) due to
  //  - await reaction forwarding to the throwaway Promise, which has
  //    a dependency edge to the outer Promise.
  //  - PromiseIdResolveHandler forwarding to the output of .then
  //  - Promise.all/Promise.race forwarding to a throwaway Promise, which
  //    has a dependency edge to the generated outer Promise.
  // Otherwise, this is a real reject handler for the Promise.
  Handle<Symbol> key = isolate->factory()->promise_forwarding_handler_symbol();
  Handle<Object> forwarding_handler = JSReceiver::GetDataProperty(handler, key);
  return forwarding_handler->IsUndefined(isolate);
}

bool PromiseHasUserDefinedRejectHandlerInternal(Isolate* isolate,
                                                Handle<JSPromise> promise) {
  Handle<Object> current(promise->reactions(), isolate);
  while (!current->IsSmi()) {
    Handle<PromiseReaction> reaction = Handle<PromiseReaction>::cast(current);
    Handle<HeapObject> promise_or_capability(reaction->promise_or_capability(),
                                             isolate);
    if (!promise_or_capability->IsUndefined(isolate)) {
      if (!promise_or_capability->IsJSPromise()) {
        promise_or_capability = handle(
            Handle<PromiseCapability>::cast(promise_or_capability)->promise(),
            isolate);
      }
      Handle<JSPromise> promise =
          Handle<JSPromise>::cast(promise_or_capability);
      if (!reaction->reject_handler().IsUndefined(isolate)) {
        Handle<JSReceiver> reject_handler(
            JSReceiver::cast(reaction->reject_handler()), isolate);
        if (PromiseIsRejectHandler(isolate, reject_handler)) return true;
      }
      if (isolate->PromiseHasUserDefinedRejectHandler(promise)) return true;
    }
    current = handle(reaction->next(), isolate);
  }
  return false;
}

}  // namespace

bool Isolate::PromiseHasUserDefinedRejectHandler(Handle<JSPromise> promise) {
  Handle<Symbol> key = factory()->promise_handled_by_symbol();
  std::stack<Handle<JSPromise>> promises;
  // First descend into the outermost promise and collect the stack of
  // Promises for reverse processing.
  while (true) {
    // If this promise was marked as being handled by a catch block
    // in an async function, then it has a user-defined reject handler.
    if (promise->handled_hint()) return true;
    if (promise->status() == Promise::kPending) {
      promises.push(promise);
    }
    Handle<Object> outer_promise_obj = JSObject::GetDataProperty(promise, key);
    if (!outer_promise_obj->IsJSPromise()) break;
    promise = Handle<JSPromise>::cast(outer_promise_obj);
  }

  while (!promises.empty()) {
    promise = promises.top();
    if (PromiseHasUserDefinedRejectHandlerInternal(this, promise)) return true;
    promises.pop();
  }
  return false;
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
      if (!code.IsCode() || code.kind() != CodeKind::BUILTIN ||
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
      case HandlerTable::UNCAUGHT_ASYNC_AWAIT:
      case HandlerTable::ASYNC_AWAIT: {
        // If in the initial portion of async/await, continue the loop to pop up
        // successive async/await stack frames until an asynchronous one with
        // dependents is found, or a non-async stack frame is encountered, in
        // order to handle the synchronous async/await catch prediction case:
        // assume that async function calls are awaited.
        if (!promise_on_stack) return retval;
        retval = promise_on_stack->promise();
        if (retval->IsJSPromise()) {
          if (PromiseHasUserDefinedRejectHandler(
                  Handle<JSPromise>::cast(retval))) {
            return retval;
          }
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

bool Isolate::get_capture_stack_trace_for_uncaught_exceptions() const {
  return capture_stack_trace_for_uncaught_exceptions_;
}

void Isolate::SetAbortOnUncaughtExceptionCallback(
    v8::Isolate::AbortOnUncaughtExceptionCallback callback) {
  abort_on_uncaught_exception_callback_ = callback;
}

bool Isolate::IsWasmSimdEnabled(Handle<Context> context) {
#if V8_ENABLE_WEBASSEMBLY
  if (wasm_simd_enabled_callback()) {
    v8::Local<v8::Context> api_context = v8::Utils::ToLocal(context);
    return wasm_simd_enabled_callback()(api_context);
  }
  return FLAG_experimental_wasm_simd;
#else
  return false;
#endif  // V8_ENABLE_WEBASSEMBLY
}

bool Isolate::AreWasmExceptionsEnabled(Handle<Context> context) {
#if V8_ENABLE_WEBASSEMBLY
  if (wasm_exceptions_enabled_callback()) {
    v8::Local<v8::Context> api_context = v8::Utils::ToLocal(context);
    return wasm_exceptions_enabled_callback()(api_context);
  }
  return FLAG_experimental_wasm_eh;
#else
  return false;
#endif  // V8_ENABLE_WEBASSEMBLY
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

bool Isolate::IsBuiltinsTableHandleLocation(Address* handle_location) {
  FullObjectSlot location(handle_location);
  FullObjectSlot first_root(builtins_table());
  FullObjectSlot last_root(builtins_table() + Builtins::builtin_count);
  if (location >= last_root) return false;
  if (location < first_root) return false;
  return true;
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

#if V8_ENABLE_WEBASSEMBLY
void Isolate::SetWasmEngine(std::shared_ptr<wasm::WasmEngine> engine) {
  DCHECK_NULL(wasm_engine_);  // Only call once before {Init}.
  wasm_engine_ = std::move(engine);
  wasm_engine_->AddIsolate(this);
}

void Isolate::AddSharedWasmMemory(Handle<WasmMemoryObject> memory_object) {
  HandleScope scope(this);
  Handle<WeakArrayList> shared_wasm_memories =
      factory()->shared_wasm_memories();
  shared_wasm_memories = WeakArrayList::AddToEnd(
      this, shared_wasm_memories, MaybeObjectHandle::Weak(memory_object));
  heap()->set_shared_wasm_memories(*shared_wasm_memories);
}
#endif  // V8_ENABLE_WEBASSEMBLY

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

class TracingAccountingAllocator : public AccountingAllocator {
 public:
  explicit TracingAccountingAllocator(Isolate* isolate) : isolate_(isolate) {}
  ~TracingAccountingAllocator() = default;

 protected:
  void TraceAllocateSegmentImpl(v8::internal::Segment* segment) override {
    base::MutexGuard lock(&mutex_);
    UpdateMemoryTrafficAndReportMemoryUsage(segment->total_size());
  }

  void TraceZoneCreationImpl(const Zone* zone) override {
    base::MutexGuard lock(&mutex_);
    active_zones_.insert(zone);
    nesting_depth_++;
  }

  void TraceZoneDestructionImpl(const Zone* zone) override {
    base::MutexGuard lock(&mutex_);
#ifdef V8_ENABLE_PRECISE_ZONE_STATS
    if (FLAG_trace_zone_type_stats) {
      type_stats_.MergeWith(zone->type_stats());
    }
#endif
    UpdateMemoryTrafficAndReportMemoryUsage(zone->segment_bytes_allocated());
    active_zones_.erase(zone);
    nesting_depth_--;

#ifdef V8_ENABLE_PRECISE_ZONE_STATS
    if (FLAG_trace_zone_type_stats && active_zones_.empty()) {
      type_stats_.Dump();
    }
#endif
  }

 private:
  void UpdateMemoryTrafficAndReportMemoryUsage(size_t memory_traffic_delta) {
    if (!FLAG_trace_zone_stats &&
        !(TracingFlags::zone_stats.load(std::memory_order_relaxed) &
          v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING)) {
      // Don't print anything if the zone tracing was enabled only because of
      // FLAG_trace_zone_type_stats.
      return;
    }

    memory_traffic_since_last_report_ += memory_traffic_delta;
    if (memory_traffic_since_last_report_ < FLAG_zone_stats_tolerance) return;
    memory_traffic_since_last_report_ = 0;

    Dump(buffer_, true);

    {
      std::string trace_str = buffer_.str();

      if (FLAG_trace_zone_stats) {
        PrintF(
            "{"
            "\"type\": \"v8-zone-trace\", "
            "\"stats\": %s"
            "}\n",
            trace_str.c_str());
      }
      if (V8_UNLIKELY(
              TracingFlags::zone_stats.load(std::memory_order_relaxed) &
              v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING)) {
        TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("v8.zone_stats"),
                             "V8.Zone_Stats", TRACE_EVENT_SCOPE_THREAD, "stats",
                             TRACE_STR_COPY(trace_str.c_str()));
      }
    }

    // Clear the buffer.
    buffer_.str(std::string());
  }

  void Dump(std::ostringstream& out, bool dump_details) {
    // Note: Neither isolate nor zones are locked, so be careful with accesses
    // as the allocator is potentially used on a concurrent thread.
    double time = isolate_->time_millis_since_init();
    out << "{"
        << "\"isolate\": \"" << reinterpret_cast<void*>(isolate_) << "\", "
        << "\"time\": " << time << ", ";
    size_t total_segment_bytes_allocated = 0;
    size_t total_zone_allocation_size = 0;
    size_t total_zone_freed_size = 0;

    if (dump_details) {
      // Print detailed zone stats if memory usage changes direction.
      out << "\"zones\": [";
      bool first = true;
      for (const Zone* zone : active_zones_) {
        size_t zone_segment_bytes_allocated = zone->segment_bytes_allocated();
        size_t zone_allocation_size = zone->allocation_size_for_tracing();
        size_t freed_size = zone->freed_size_for_tracing();
        if (first) {
          first = false;
        } else {
          out << ", ";
        }
        out << "{"
            << "\"name\": \"" << zone->name() << "\", "
            << "\"allocated\": " << zone_segment_bytes_allocated << ", "
            << "\"used\": " << zone_allocation_size << ", "
            << "\"freed\": " << freed_size << "}";
        total_segment_bytes_allocated += zone_segment_bytes_allocated;
        total_zone_allocation_size += zone_allocation_size;
        total_zone_freed_size += freed_size;
      }
      out << "], ";
    } else {
      // Just calculate total allocated/used memory values.
      for (const Zone* zone : active_zones_) {
        total_segment_bytes_allocated += zone->segment_bytes_allocated();
        total_zone_allocation_size += zone->allocation_size_for_tracing();
        total_zone_freed_size += zone->freed_size_for_tracing();
      }
    }
    out << "\"allocated\": " << total_segment_bytes_allocated << ", "
        << "\"used\": " << total_zone_allocation_size << ", "
        << "\"freed\": " << total_zone_freed_size << "}";
  }

  Isolate* const isolate_;
  std::atomic<size_t> nesting_depth_{0};

  base::Mutex mutex_;
  std::unordered_set<const Zone*> active_zones_;
#ifdef V8_ENABLE_PRECISE_ZONE_STATS
  TypeStats type_stats_;
#endif
  std::ostringstream buffer_;
  // This value is increased on both allocations and deallocations.
  size_t memory_traffic_since_last_report_ = 0;
};

#ifdef DEBUG
std::atomic<size_t> Isolate::non_disposed_isolates_;
#endif  // DEBUG

// static
Isolate* Isolate::New() {
  // IsolateAllocator allocates the memory for the Isolate object according to
  // the given allocation mode.
  std::unique_ptr<IsolateAllocator> isolate_allocator =
      std::make_unique<IsolateAllocator>();
  // Construct Isolate object in the allocated memory.
  void* isolate_ptr = isolate_allocator->isolate_memory();
  Isolate* isolate = new (isolate_ptr) Isolate(std::move(isolate_allocator));
#ifdef V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
  DCHECK(IsAligned(isolate->isolate_root(), kPtrComprCageBaseAlignment));
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
  isolate->set_thread_id(ThreadId::Current());

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

void Isolate::SetUpFromReadOnlyArtifacts(
    std::shared_ptr<ReadOnlyArtifacts> artifacts, ReadOnlyHeap* ro_heap) {
  if (ReadOnlyHeap::IsReadOnlySpaceShared()) {
    DCHECK_NOT_NULL(artifacts);
    artifacts_ = artifacts;
  } else {
    DCHECK_NULL(artifacts);
  }
  DCHECK_NOT_NULL(ro_heap);
  DCHECK_IMPLIES(read_only_heap_ != nullptr, read_only_heap_ == ro_heap);
  read_only_heap_ = ro_heap;
  heap_.SetUpFromReadOnlyHeap(read_only_heap_);
}

v8::PageAllocator* Isolate::page_allocator() {
  return isolate_allocator_->page_allocator();
}

Isolate::Isolate(std::unique_ptr<i::IsolateAllocator> isolate_allocator)
    : isolate_data_(this),
      isolate_allocator_(std::move(isolate_allocator)),
      id_(isolate_counter.fetch_add(1, std::memory_order_relaxed)),
      allocator_(new TracingAccountingAllocator(this)),
      builtins_(this),
#if defined(DEBUG) || defined(VERIFY_HEAP)
      num_active_deserializers_(0),
#endif
      rail_mode_(PERFORMANCE_ANIMATION),
      code_event_dispatcher_(new CodeEventDispatcher()),
      persistent_handles_list_(new PersistentHandlesList()),
      jitless_(FLAG_jitless),
#if V8_SFI_HAS_UNIQUE_ID
      next_unique_sfi_id_(0),
#endif
      next_module_async_evaluating_ordinal_(
          SourceTextModule::kFirstAsyncEvaluatingOrdinal),
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

#ifdef V8_HEAP_SANDBOX
  CHECK_EQ(static_cast<int>(OFFSET_OF(ExternalPointerTable, buffer_)),
           Internals::kExternalPointerTableBufferOffset);
  CHECK_EQ(static_cast<int>(OFFSET_OF(ExternalPointerTable, length_)),
           Internals::kExternalPointerTableLengthOffset);
  CHECK_EQ(static_cast<int>(OFFSET_OF(ExternalPointerTable, capacity_)),
           Internals::kExternalPointerTableCapacityOffset);
#endif
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

  metrics_recorder_->NotifyIsolateDisposal();
  recorder_context_id_map_.clear();

#if defined(V8_OS_WIN64)
  if (win64_unwindinfo::CanRegisterUnwindInfoForNonABICompliantCodeRange() &&
      heap()->memory_allocator() && RequiresCodeRange()) {
    const base::AddressRegion& code_range =
        heap()->memory_allocator()->code_range();
    void* start = reinterpret_cast<void*>(code_range.begin());
    win64_unwindinfo::UnregisterNonABICompliantCodeRange(start);
  }
#endif  // V8_OS_WIN64

  FutexEmulation::IsolateDeinit(this);

  debug()->Unload();

#if V8_ENABLE_WEBASSEMBLY
  wasm_engine()->DeleteCompileJobsOnIsolate(this);

  BackingStore::RemoveSharedWasmMemoryObjects(this);
#endif  // V8_ENABLE_WEBASSEMBLY

  if (concurrent_recompilation_enabled()) {
    optimizing_compile_dispatcher_->Stop();
    delete optimizing_compile_dispatcher_;
    optimizing_compile_dispatcher_ = nullptr;
  }

  // Help sweeper threads complete sweeping to stop faster.
  heap_.mark_compact_collector()->DrainSweepingWorklists();
  heap_.mark_compact_collector()->sweeper()->EnsureIterabilityCompleted();

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

  string_table_.reset();
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

  main_thread_local_isolate_->heap()->FreeLinearAllocationArea();

  heap_.TearDown();

  main_thread_local_isolate_.reset();

  FILE* logfile = logger_->TearDownAndGetLogFile();
  if (logfile != nullptr) base::Fclose(logfile);

#if V8_ENABLE_WEBASSEMBLY
  if (wasm_engine_) {
    wasm_engine_->RemoveIsolate(this);
    wasm_engine_.reset();
  }
#endif  // V8_ENABLE_WEBASSEMBLY

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

  SetCodePages(nullptr);

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

  // The ReadOnlyHeap should not be destroyed when sharing without pointer
  // compression as the object itself is shared.
  if (read_only_heap_->IsOwnedByIsolate()) {
    delete read_only_heap_;
    read_only_heap_ = nullptr;
  }
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
  DCHECK_NOT_NULL(isolate->embedded_blob_code());
  DCHECK_NE(0, isolate->embedded_blob_code_size());
  DCHECK_NOT_NULL(isolate->embedded_blob_data());
  DCHECK_NE(0, isolate->embedded_blob_data_size());

  HandleScope scope(isolate);
  Builtins* builtins = isolate->builtins();

  EmbeddedData d = EmbeddedData::FromBlob(isolate);

  STATIC_ASSERT(Builtins::kAllBuiltinsAreIsolateIndependent);
  for (int i = 0; i < Builtins::builtin_count; i++) {
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
  const uint8_t* code = DefaultEmbeddedBlobCode();
  uint32_t code_size = DefaultEmbeddedBlobCodeSize();
  const uint8_t* data = DefaultEmbeddedBlobData();
  uint32_t data_size = DefaultEmbeddedBlobDataSize();

#ifdef V8_MULTI_SNAPSHOTS
  if (!FLAG_untrusted_code_mitigations) {
    code = TrustedEmbeddedBlobCode();
    code_size = TrustedEmbeddedBlobCodeSize();
    data = TrustedEmbeddedBlobData();
    data_size = TrustedEmbeddedBlobDataSize();
  }
#endif

  if (StickyEmbeddedBlobCode() != nullptr) {
    base::MutexGuard guard(current_embedded_blob_refcount_mutex_.Pointer());
    // Check again now that we hold the lock.
    if (StickyEmbeddedBlobCode() != nullptr) {
      code = StickyEmbeddedBlobCode();
      code_size = StickyEmbeddedBlobCodeSize();
      data = StickyEmbeddedBlobData();
      data_size = StickyEmbeddedBlobDataSize();
      current_embedded_blob_refs_++;
    }
  }

  if (code == nullptr) {
    CHECK_EQ(0, code_size);
  } else {
    SetEmbeddedBlob(code, code_size, data, data_size);
  }
}

void Isolate::CreateAndSetEmbeddedBlob() {
  base::MutexGuard guard(current_embedded_blob_refcount_mutex_.Pointer());

  PrepareBuiltinSourcePositionMap();

  PrepareBuiltinLabelInfoMap();

  // If a sticky blob has been set, we reuse it.
  if (StickyEmbeddedBlobCode() != nullptr) {
    CHECK_EQ(embedded_blob_code(), StickyEmbeddedBlobCode());
    CHECK_EQ(embedded_blob_data(), StickyEmbeddedBlobData());
    CHECK_EQ(CurrentEmbeddedBlobCode(), StickyEmbeddedBlobCode());
    CHECK_EQ(CurrentEmbeddedBlobData(), StickyEmbeddedBlobData());
  } else {
    // Create and set a new embedded blob.
    uint8_t* code;
    uint32_t code_size;
    uint8_t* data;
    uint32_t data_size;
    InstructionStream::CreateOffHeapInstructionStream(this, &code, &code_size,
                                                      &data, &data_size);

    CHECK_EQ(0, current_embedded_blob_refs_);
    const uint8_t* const_code = const_cast<const uint8_t*>(code);
    const uint8_t* const_data = const_cast<const uint8_t*>(data);
    SetEmbeddedBlob(const_code, code_size, const_data, data_size);
    current_embedded_blob_refs_++;

    SetStickyEmbeddedBlob(code, code_size, data, data_size);
  }

  MaybeRemapEmbeddedBuiltinsIntoCodeRange();

  CreateOffHeapTrampolines(this);
}

void Isolate::MaybeRemapEmbeddedBuiltinsIntoCodeRange() {
  if (!is_short_builtin_calls_enabled() || !RequiresCodeRange()) return;

  CHECK_NOT_NULL(embedded_blob_code_);
  CHECK_NE(embedded_blob_code_size_, 0);

  embedded_blob_code_ = heap_.RemapEmbeddedBuiltinsIntoCodeRange(
      embedded_blob_code_, embedded_blob_code_size_);
  CHECK_NOT_NULL(embedded_blob_code_);
  // The un-embedded code blob is already a part of the registered code range
  // so it's not necessary to register it again.
}

void Isolate::TearDownEmbeddedBlob() {
  // Nothing to do in case the blob is embedded into the binary or unset.
  if (StickyEmbeddedBlobCode() == nullptr) return;

  if (!is_short_builtin_calls_enabled()) {
    CHECK_EQ(embedded_blob_code(), StickyEmbeddedBlobCode());
    CHECK_EQ(embedded_blob_data(), StickyEmbeddedBlobData());
  }
  CHECK_EQ(CurrentEmbeddedBlobCode(), StickyEmbeddedBlobCode());
  CHECK_EQ(CurrentEmbeddedBlobData(), StickyEmbeddedBlobData());

  base::MutexGuard guard(current_embedded_blob_refcount_mutex_.Pointer());
  current_embedded_blob_refs_--;
  if (current_embedded_blob_refs_ == 0 && enable_embedded_blob_refcounting_) {
    // We own the embedded blob and are the last holder. Free it.
    InstructionStream::FreeOffHeapInstructionStream(
        const_cast<uint8_t*>(CurrentEmbeddedBlobCode()),
        embedded_blob_code_size(),
        const_cast<uint8_t*>(CurrentEmbeddedBlobData()),
        embedded_blob_data_size());
    ClearEmbeddedBlob();
  }
}

bool Isolate::InitWithoutSnapshot() { return Init(nullptr, nullptr, false); }

bool Isolate::InitWithSnapshot(SnapshotData* startup_snapshot_data,
                               SnapshotData* read_only_snapshot_data,
                               bool can_rehash) {
  DCHECK_NOT_NULL(startup_snapshot_data);
  DCHECK_NOT_NULL(read_only_snapshot_data);
  return Init(startup_snapshot_data, read_only_snapshot_data, can_rehash);
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
      heap()->read_only_space()->FirstPageAddress();
  add_crash_key_callback_(v8::CrashKeyId::kReadonlySpaceFirstPageAddress,
                          AddressToString(ro_space_firstpage_address));
  const uintptr_t map_space_firstpage_address =
      heap()->map_space()->FirstPageAddress();
  add_crash_key_callback_(v8::CrashKeyId::kMapSpaceFirstPageAddress,
                          AddressToString(map_space_firstpage_address));
  const uintptr_t code_space_firstpage_address =
      heap()->code_space()->FirstPageAddress();
  add_crash_key_callback_(v8::CrashKeyId::kCodeSpaceFirstPageAddress,
                          AddressToString(code_space_firstpage_address));
}

void Isolate::InitializeCodeRanges() {
  DCHECK_NULL(GetCodePages());
  MemoryRange embedded_range{
      reinterpret_cast<const void*>(embedded_blob_code()),
      embedded_blob_code_size()};
  code_pages_buffer1_.push_back(embedded_range);
  SetCodePages(&code_pages_buffer1_);
}

namespace {

// This global counter contains number of stack loads/stores per optimized/wasm
// function.
using MapOfLoadsAndStoresPerFunction =
    std::map<std::string /* function_name */,
             std::pair<uint64_t /* loads */, uint64_t /* stores */>>;
MapOfLoadsAndStoresPerFunction* stack_access_count_map = nullptr;
}  // namespace

bool Isolate::Init(SnapshotData* startup_snapshot_data,
                   SnapshotData* read_only_snapshot_data, bool can_rehash) {
  TRACE_ISOLATE(init);
  const bool create_heap_objects = (read_only_snapshot_data == nullptr);
  // We either have both or neither.
  DCHECK_EQ(create_heap_objects, startup_snapshot_data == nullptr);

  base::ElapsedTimer timer;
  if (create_heap_objects && FLAG_profile_deserialization) timer.Start();

  time_millis_at_init_ = heap_.MonotonicallyIncreasingTimeInMs();

  stress_deopt_count_ = FLAG_deopt_every_n_times;
  force_slow_path_ = FLAG_force_slow_path;

  has_fatal_error_ = false;

  // The initialization process does not handle memory exhaustion.
  AlwaysAllocateScope always_allocate(heap());

#define ASSIGN_ELEMENT(CamelName, hacker_name)                  \
  isolate_addresses_[IsolateAddressId::k##CamelName##Address] = \
      reinterpret_cast<Address>(hacker_name##_address());
  FOR_EACH_ISOLATE_ADDRESS_NAME(ASSIGN_ELEMENT)
#undef ASSIGN_ELEMENT

  // We need to initialize code_pages_ before any on-heap code is allocated to
  // make sure we record all code allocations.
  InitializeCodeRanges();

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
  string_table_.reset(new StringTable(this));

  compiler_dispatcher_ =
      new CompilerDispatcher(this, V8::GetCurrentPlatform(), FLAG_stack_size);

  // Enable logging before setting up the heap
  logger_->SetUp(this);

  metrics_recorder_ = std::make_shared<metrics::Recorder>();

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
  ReadOnlyHeap::SetUp(this, read_only_snapshot_data, can_rehash);
  heap_.SetUpSpaces();

  if (V8_SHORT_BUILTIN_CALLS_BOOL && FLAG_short_builtin_calls) {
    // Check if the system has more than 4GB of physical memory by comaring
    // the old space size with respective threshod value.
    is_short_builtin_calls_enabled_ =
        heap_.MaxOldGenerationSize() >= kShortBuiltinCallsOldSpaceSizeThreshold;
  }

  // Create LocalIsolate/LocalHeap for the main thread and set state to Running.
  main_thread_local_isolate_.reset(new LocalIsolate(this, ThreadKind::kMain));
  main_thread_local_heap()->Unpark();

  isolate_data_.external_reference_table()->Init(this);

#if V8_ENABLE_WEBASSEMBLY
  // Setup the wasm engine.
  if (wasm_engine_ == nullptr) {
    SetWasmEngine(wasm::WasmEngine::GetWasmEngine());
  }
  DCHECK_NOT_NULL(wasm_engine_);
#endif  // V8_ENABLE_WEBASSEMBLY

  if (setup_delegate_ == nullptr) {
    setup_delegate_ = new SetupIsolateDelegate(create_heap_objects);
  }

  if (!FLAG_inline_new) heap_.DisableInlineAllocation();

  if (!setup_delegate_->SetupHeap(&heap_)) {
    V8::FatalProcessOutOfMemory(this, "heap object creation");
    return false;
  }

  if (create_heap_objects) {
    // Terminate the startup object cache so we can iterate.
    startup_object_cache_.push_back(ReadOnlyRoots(this).undefined_value());
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
    MaybeRemapEmbeddedBuiltinsIntoCodeRange();
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
    AlwaysAllocateScope always_allocate(heap());
    CodeSpaceMemoryModificationScope modification_scope(heap());

    if (create_heap_objects) {
      heap_.read_only_space()->ClearStringPaddingIfNeeded();
      read_only_heap_->OnCreateHeapObjectsComplete(this);
    } else {
      StartupDeserializer startup_deserializer(this, startup_snapshot_data,
                                               can_rehash);
      startup_deserializer.DeserializeIntoIsolate();
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

std::unique_ptr<PersistentHandles> Isolate::NewPersistentHandles() {
  return std::make_unique<PersistentHandles>(this);
}

void Isolate::DumpAndResetStats() {
  if (FLAG_trace_turbo_stack_accesses) {
    StdoutStream os;
    uint64_t total_loads = 0;
    uint64_t total_stores = 0;
    os << "=== Stack access counters === " << std::endl;
    if (!stack_access_count_map) {
      os << "No stack accesses in optimized/wasm functions found.";
    } else {
      DCHECK_NOT_NULL(stack_access_count_map);
      os << "Number of optimized/wasm stack-access functions: "
         << stack_access_count_map->size() << std::endl;
      for (auto it = stack_access_count_map->cbegin();
           it != stack_access_count_map->cend(); it++) {
        std::string function_name((*it).first);
        std::pair<uint64_t, uint64_t> per_func_count = (*it).second;
        os << "Name: " << function_name << ", Loads: " << per_func_count.first
           << ", Stores: " << per_func_count.second << std::endl;
        total_loads += per_func_count.first;
        total_stores += per_func_count.second;
      }
      os << "Total Loads: " << total_loads << ", Total Stores: " << total_stores
         << std::endl;
      stack_access_count_map = nullptr;
    }
  }
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
#if V8_ENABLE_WEBASSEMBLY
  // TODO(7424): There is no public API for the {WasmEngine} yet. So for now we
  // just dump and reset the engines statistics together with the Isolate.
  if (FLAG_turbo_stats_wasm) {
    wasm_engine()->DumpAndResetTurboStatistics();
  }
#endif  // V8_ENABLE_WEBASSEMBLY
  if (V8_UNLIKELY(TracingFlags::runtime_stats.load(std::memory_order_relaxed) ==
                  v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE)) {
    counters()->worker_thread_runtime_call_stats()->AddToMainTable(
        counters()->runtime_call_stats());
    counters()->runtime_call_stats()->Print();
    counters()->runtime_call_stats()->Reset();
  }
  if (BasicBlockProfiler::Get()->HasData(this)) {
    StdoutStream out;
    BasicBlockProfiler::Get()->Print(out, this);
    BasicBlockProfiler::Get()->ResetCounts(this);
  }
}

void Isolate::AbortConcurrentOptimization(BlockingBehavior behavior) {
  if (concurrent_recompilation_enabled()) {
    DisallowGarbageCollection no_recursive_gc;
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

void Isolate::IncreaseTotalRegexpCodeGenerated(Handle<HeapObject> code) {
  DCHECK(code->IsCode() || code->IsByteArray());
  total_regexp_code_generated_ += code->Size();
}

bool Isolate::NeedsDetailedOptimizedCodeLineInfo() const {
  return NeedsSourcePositionsForProfiling() ||
         detailed_source_positions_for_profiling();
}

bool Isolate::NeedsSourcePositionsForProfiling() const {
  return FLAG_trace_deopt || FLAG_trace_turbo || FLAG_trace_turbo_graph ||
         FLAG_turbo_profiling || FLAG_perf_prof || is_profiling() ||
         debug_->is_active() || logger_->is_logging() || FLAG_log_maps ||
         FLAG_log_ic;
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

Isolate::KnownPrototype Isolate::IsArrayOrObjectOrStringPrototype(
    Object object) {
  Object context = heap()->native_contexts_list();
  while (!context.IsUndefined(this)) {
    Context current_context = Context::cast(context);
    if (current_context.initial_object_prototype() == object) {
      return KnownPrototype::kObject;
    } else if (current_context.initial_array_prototype() == object) {
      return KnownPrototype::kArray;
    } else if (current_context.initial_string_prototype() == object) {
      return KnownPrototype::kString;
    }
    context = current_context.next_context_link();
  }
  return KnownPrototype::kNone;
}

bool Isolate::IsInAnyContext(Object object, uint32_t index) {
  DisallowGarbageCollection no_gc;
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
  DisallowGarbageCollection no_gc;
  if (!object->map().is_prototype_map()) return;
  if (!Protectors::IsNoElementsIntact(this)) return;
  KnownPrototype obj_type = IsArrayOrObjectOrStringPrototype(*object);
  if (obj_type == KnownPrototype::kNone) return;
  if (obj_type == KnownPrototype::kObject) {
    this->CountUsage(v8::Isolate::kObjectPrototypeHasElements);
  } else if (obj_type == KnownPrototype::kArray) {
    this->CountUsage(v8::Isolate::kArrayPrototypeHasElements);
  }
  Protectors::InvalidateNoElements(this);
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

  bool perform_checkpoint =
      microtask_queue &&
      microtask_queue->microtasks_policy() == v8::MicrotasksPolicy::kAuto;

  v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(this);
  if (perform_checkpoint) microtask_queue->PerformCheckpoint(isolate);

  if (call_completed_callbacks_.empty()) return;
  // Fire callbacks.  Increase call depth to prevent recursive callbacks.
  v8::Isolate::SuppressMicrotaskExecutionScope suppress(isolate);
  std::vector<CallCompletedCallback> callbacks(call_completed_callbacks_);
  for (auto& callback : callbacks) {
    callback(reinterpret_cast<v8::Isolate*>(this));
  }
}

void Isolate::UpdatePromiseHookProtector() {
  if (Protectors::IsPromiseHookIntact(this)) {
    HandleScope scope(this);
    Protectors::InvalidatePromiseHook(this);
  }
}

void Isolate::PromiseHookStateUpdated() {
  promise_hook_flags_ =
    (promise_hook_flags_ & PromiseHookFields::HasContextPromiseHook::kMask) |
    PromiseHookFields::HasIsolatePromiseHook::encode(promise_hook_) |
    PromiseHookFields::HasAsyncEventDelegate::encode(async_event_delegate_) |
    PromiseHookFields::IsDebugActive::encode(debug()->is_active());

  if (promise_hook_flags_ != 0) {
    UpdatePromiseHookProtector();
  }
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
    Handle<Script> referrer, Handle<Object> specifier,
    MaybeHandle<Object> maybe_import_assertions_argument) {
  v8::Local<v8::Context> api_context =
      v8::Utils::ToLocal(Handle<Context>(native_context()));
  DCHECK(host_import_module_dynamically_callback_ == nullptr ||
         host_import_module_dynamically_with_import_assertions_callback_ ==
             nullptr);

  if (host_import_module_dynamically_callback_ == nullptr &&
      host_import_module_dynamically_with_import_assertions_callback_ ==
          nullptr) {
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

  if (host_import_module_dynamically_with_import_assertions_callback_) {
    Handle<FixedArray> import_assertions_array;
    if (GetImportAssertionsFromArgument(maybe_import_assertions_argument)
            .ToHandle(&import_assertions_array)) {
      ASSIGN_RETURN_ON_SCHEDULED_EXCEPTION_VALUE(
          this, promise,
          host_import_module_dynamically_with_import_assertions_callback_(
              api_context, v8::Utils::ScriptOrModuleToLocal(referrer),
              v8::Utils::ToLocal(specifier_str),
              ToApiHandle<v8::FixedArray>(import_assertions_array)),
          MaybeHandle<JSPromise>());
      return v8::Utils::OpenHandle(*promise);
    } else {
      Handle<Object> exception(pending_exception(), this);
      clear_pending_exception();

      return NewRejectedPromise(this, api_context, exception);
    }

  } else {
    DCHECK_NOT_NULL(host_import_module_dynamically_callback_);
    ASSIGN_RETURN_ON_SCHEDULED_EXCEPTION_VALUE(
        this, promise,
        host_import_module_dynamically_callback_(
            api_context, v8::Utils::ScriptOrModuleToLocal(referrer),
            v8::Utils::ToLocal(specifier_str)),
        MaybeHandle<JSPromise>());
    return v8::Utils::OpenHandle(*promise);
  }
}

MaybeHandle<FixedArray> Isolate::GetImportAssertionsFromArgument(
    MaybeHandle<Object> maybe_import_assertions_argument) {
  Handle<FixedArray> import_assertions_array = factory()->empty_fixed_array();
  Handle<Object> import_assertions_argument;
  if (!maybe_import_assertions_argument.ToHandle(&import_assertions_argument) ||
      import_assertions_argument->IsUndefined()) {
    return import_assertions_array;
  }

  // The parser shouldn't have allowed the second argument to import() if
  // the flag wasn't enabled.
  DCHECK(FLAG_harmony_import_assertions);

  if (!import_assertions_argument->IsJSReceiver()) {
    this->Throw(
        *factory()->NewTypeError(MessageTemplate::kNonObjectImportArgument));
    return MaybeHandle<FixedArray>();
  }

  Handle<JSReceiver> import_assertions_argument_receiver =
      Handle<JSReceiver>::cast(import_assertions_argument);
  Handle<Name> key = factory()->assert_string();

  Handle<Object> import_assertions_object;
  if (!JSReceiver::GetProperty(this, import_assertions_argument_receiver, key)
           .ToHandle(&import_assertions_object)) {
    // This can happen if the property has a getter function that throws
    // an error.
    return MaybeHandle<FixedArray>();
  }

  // If there is no 'assert' option in the options bag, it's not an error. Just
  // do the import() as if no assertions were provided.
  if (import_assertions_object->IsUndefined()) return import_assertions_array;

  if (!import_assertions_object->IsJSReceiver()) {
    this->Throw(
        *factory()->NewTypeError(MessageTemplate::kNonObjectAssertOption));
    return MaybeHandle<FixedArray>();
  }

  Handle<JSReceiver> import_assertions_object_receiver =
      Handle<JSReceiver>::cast(import_assertions_object);

  Handle<FixedArray> assertion_keys =
      KeyAccumulator::GetKeys(import_assertions_object_receiver,
                              KeyCollectionMode::kOwnOnly, ENUMERABLE_STRINGS,
                              GetKeysConversion::kConvertToString)
          .ToHandleChecked();

  // The assertions will be passed to the host in the form: [key1,
  // value1, key2, value2, ...].
  constexpr size_t kAssertionEntrySizeForDynamicImport = 2;
  import_assertions_array = factory()->NewFixedArray(static_cast<int>(
      assertion_keys->length() * kAssertionEntrySizeForDynamicImport));
  for (int i = 0; i < assertion_keys->length(); i++) {
    Handle<String> assertion_key(String::cast(assertion_keys->get(i)), this);
    Handle<Object> assertion_value;
    if (!JSReceiver::GetProperty(this, import_assertions_object_receiver,
                                 assertion_key)
             .ToHandle(&assertion_value)) {
      // This can happen if the property has a getter function that throws
      // an error.
      return MaybeHandle<FixedArray>();
    }

    if (!assertion_value->IsString()) {
      this->Throw(*factory()->NewTypeError(
          MessageTemplate::kNonStringImportAssertionValue));
      return MaybeHandle<FixedArray>();
    }

    import_assertions_array->set((i * kAssertionEntrySizeForDynamicImport),
                                 *assertion_key);
    import_assertions_array->set((i * kAssertionEntrySizeForDynamicImport) + 1,
                                 *assertion_value);
  }

  return import_assertions_array;
}

void Isolate::ClearKeptObjects() { heap()->ClearKeptObjects(); }

void Isolate::SetHostImportModuleDynamicallyCallback(
    DeprecatedHostImportModuleDynamicallyCallback callback) {
  host_import_module_dynamically_callback_ = callback;
}

void Isolate::SetHostImportModuleDynamicallyCallback(
    HostImportModuleDynamicallyWithImportAssertionsCallback callback) {
  host_import_module_dynamically_with_import_assertions_callback_ = callback;
}

MaybeHandle<JSObject> Isolate::RunHostInitializeImportMetaObjectCallback(
    Handle<SourceTextModule> module) {
  CHECK(module->import_meta().IsTheHole(this));
  Handle<JSObject> import_meta = factory()->NewJSObjectWithNullProto();
  if (host_initialize_import_meta_object_callback_ != nullptr) {
    v8::Local<v8::Context> api_context =
        v8::Utils::ToLocal(Handle<Context>(native_context()));
    host_initialize_import_meta_object_callback_(
        api_context, Utils::ToLocal(Handle<Module>::cast(module)),
        v8::Local<v8::Object>::Cast(v8::Utils::ToLocal(import_meta)));
    if (has_scheduled_exception()) {
      PromoteScheduledException();
      return {};
    }
  }
  return import_meta;
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

void Isolate::PrepareBuiltinLabelInfoMap() {
  if (embedded_file_writer_ != nullptr) {
    embedded_file_writer_->PrepareBuiltinLabelInfoMap(
        heap()->construct_stub_create_deopt_pc_offset().value(),
        heap()->construct_stub_invoke_deopt_pc_offset().value());
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

void Isolate::RunAllPromiseHooks(PromiseHookType type,
                                 Handle<JSPromise> promise,
                                 Handle<Object> parent) {
  if (HasContextPromiseHooks()) {
    native_context()->RunPromiseHook(type, promise, parent);
  }
  if (HasIsolatePromiseHooks() || HasAsyncEventDelegate()) {
    RunPromiseHook(type, promise, parent);
  }
}

void Isolate::RunPromiseHook(PromiseHookType type, Handle<JSPromise> promise,
                             Handle<Object> parent) {
  RunPromiseHookForAsyncEventDelegate(type, promise);
  if (!HasIsolatePromiseHooks()) return;
  DCHECK(promise_hook_ != nullptr);
  promise_hook_(type, v8::Utils::PromiseToLocal(promise),
                v8::Utils::ToLocal(parent));
}

void Isolate::RunPromiseHookForAsyncEventDelegate(PromiseHookType type,
                                                  Handle<JSPromise> promise) {
  if (!HasAsyncEventDelegate()) return;
  DCHECK(async_event_delegate_ != nullptr);
  switch (type) {
    case PromiseHookType::kResolve:
      return;
    case PromiseHookType::kBefore:
      if (!promise->async_task_id()) return;
      async_event_delegate_->AsyncEventOccurred(
          debug::kDebugWillHandle, promise->async_task_id(), false);
      break;
    case PromiseHookType::kAfter:
      if (!promise->async_task_id()) return;
      async_event_delegate_->AsyncEventOccurred(
          debug::kDebugDidHandle, promise->async_task_id(), false);
      break;
    case PromiseHookType::kInit:
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
            } else if (info->builtin_id() ==
                       Builtins::kPromisePrototypeFinally) {
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
  // The counter callback
  // - may cause the embedder to call into V8, which is not generally possible
  //   during GC.
  // - requires a current native context, which may not always exist.
  // TODO(jgruber): Consider either removing the native context requirement in
  // blink, or passing it to the callback explicitly.
  if (heap_.gc_state() == Heap::NOT_IN_GC && !context().is_null()) {
    DCHECK(context().IsContext());
    DCHECK(context().native_context().IsNativeContext());
    if (use_counter_callback_) {
      HandleScope handle_scope(this);
      use_counter_callback_(reinterpret_cast<v8::Isolate*>(this), feature);
    }
  } else {
    heap_.IncrementDeferredCount(feature);
  }
}

int Isolate::GetNextScriptId() { return heap()->NextScriptId(); }

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

void Isolate::UpdateLoadStartTime() {
  base::MutexGuard guard(&rail_mutex_);
  load_start_time_ms_ = heap()->MonotonicallyIncreasingTimeInMs();
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
  StateTag state = current_vm_state();
  if (js_entry_sp() != kNullAddress) return;
  DCHECK(state == EXTERNAL || state == IDLE);
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
    DisallowGarbageCollection no_gc;
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
namespace {
std::string GetStringFromLocale(Handle<Object> locales_obj) {
  DCHECK(locales_obj->IsString() || locales_obj->IsUndefined());
  if (locales_obj->IsString()) {
    return std::string(String::cast(*locales_obj).ToCString().get());
  }

  return "";
}
}  // namespace

icu::UMemory* Isolate::get_cached_icu_object(ICUObjectCacheType cache_type,
                                             Handle<Object> locales_obj) {
  std::string locale = GetStringFromLocale(locales_obj);
  auto value = icu_object_cache_.find(cache_type);
  if (value == icu_object_cache_.end()) return nullptr;

  ICUCachePair pair = value->second;
  if (pair.first != locale) return nullptr;

  return pair.second.get();
}

void Isolate::set_icu_object_in_cache(
    ICUObjectCacheType cache_type, Handle<Object> locales_obj,
    std::shared_ptr<icu::UMemory> icu_formatter) {
  std::string locale = GetStringFromLocale(locales_obj);
  ICUCachePair pair = std::make_pair(locale, icu_formatter);

  auto it = icu_object_cache_.find(cache_type);
  if (it == icu_object_cache_.end()) {
    icu_object_cache_.insert({cache_type, pair});
  } else {
    it->second = pair;
  }
}

void Isolate::clear_cached_icu_object(ICUObjectCacheType cache_type) {
  icu_object_cache_.erase(cache_type);
}

void Isolate::ClearCachedIcuObjects() { icu_object_cache_.clear(); }

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

bool SaveContext::IsBelowFrame(CommonFrame* frame) {
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

namespace {

bool Overlapping(const MemoryRange& a, const MemoryRange& b) {
  uintptr_t a1 = reinterpret_cast<uintptr_t>(a.start);
  uintptr_t a2 = a1 + a.length_in_bytes;
  uintptr_t b1 = reinterpret_cast<uintptr_t>(b.start);
  uintptr_t b2 = b1 + b.length_in_bytes;
  // Either b1 or b2 are in the [a1, a2) range.
  return (a1 <= b1 && b1 < a2) || (a1 <= b2 && b2 < a2);
}

}  // anonymous namespace

#endif  // DEBUG

void Isolate::AddCodeMemoryRange(MemoryRange range) {
  std::vector<MemoryRange>* old_code_pages = GetCodePages();
  DCHECK_NOT_NULL(old_code_pages);
#ifdef DEBUG
  auto overlapping = [range](const MemoryRange& a) {
    return Overlapping(range, a);
  };
  DCHECK_EQ(old_code_pages->end(),
            std::find_if(old_code_pages->begin(), old_code_pages->end(),
                         overlapping));
#endif

  std::vector<MemoryRange>* new_code_pages;
  if (old_code_pages == &code_pages_buffer1_) {
    new_code_pages = &code_pages_buffer2_;
  } else {
    new_code_pages = &code_pages_buffer1_;
  }

  // Copy all existing data from the old vector to the new vector and insert the
  // new page.
  new_code_pages->clear();
  new_code_pages->reserve(old_code_pages->size() + 1);
  std::merge(old_code_pages->begin(), old_code_pages->end(), &range, &range + 1,
             std::back_inserter(*new_code_pages),
             [](const MemoryRange& a, const MemoryRange& b) {
               return a.start < b.start;
             });

  // Atomically switch out the pointer
  SetCodePages(new_code_pages);
}

// |chunk| is either a Page or an executable LargePage.
void Isolate::AddCodeMemoryChunk(MemoryChunk* chunk) {
  // We only keep track of individual code pages/allocations if we are on arm32,
  // because on x64 and arm64 we have a code range which makes this unnecessary.
#if !defined(V8_TARGET_ARCH_ARM)
  return;
#else
  void* new_page_start = reinterpret_cast<void*>(chunk->area_start());
  size_t new_page_size = chunk->area_size();

  MemoryRange new_range{new_page_start, new_page_size};

  AddCodeMemoryRange(new_range);
#endif  // !defined(V8_TARGET_ARCH_ARM)
}

void Isolate::AddCodeRange(Address begin, size_t length_in_bytes) {
  AddCodeMemoryRange(
      MemoryRange{reinterpret_cast<void*>(begin), length_in_bytes});
}

bool Isolate::RequiresCodeRange() const {
  return kPlatformRequiresCodeRange && !jitless_;
}

v8::metrics::Recorder::ContextId Isolate::GetOrRegisterRecorderContextId(
    Handle<NativeContext> context) {
  if (serializer_enabled_) return v8::metrics::Recorder::ContextId::Empty();
  i::Object id = context->recorder_context_id();
  if (id.IsNullOrUndefined()) {
    CHECK_LT(last_recorder_context_id_, i::Smi::kMaxValue);
    context->set_recorder_context_id(
        i::Smi::FromIntptr(++last_recorder_context_id_));
    v8::HandleScope handle_scope(reinterpret_cast<v8::Isolate*>(this));
    auto result = recorder_context_id_map_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(last_recorder_context_id_),
        std::forward_as_tuple(reinterpret_cast<v8::Isolate*>(this),
                              ToApiHandle<v8::Context>(context)));
    result.first->second.SetWeak(
        reinterpret_cast<void*>(last_recorder_context_id_),
        RemoveContextIdCallback, v8::WeakCallbackType::kParameter);
    return v8::metrics::Recorder::ContextId(last_recorder_context_id_);
  } else {
    DCHECK(id.IsSmi());
    return v8::metrics::Recorder::ContextId(
        static_cast<uintptr_t>(i::Smi::ToInt(id)));
  }
}

MaybeLocal<v8::Context> Isolate::GetContextFromRecorderContextId(
    v8::metrics::Recorder::ContextId id) {
  auto result = recorder_context_id_map_.find(id.id_);
  if (result == recorder_context_id_map_.end() || result->second.IsEmpty())
    return MaybeLocal<v8::Context>();
  return result->second.Get(reinterpret_cast<v8::Isolate*>(this));
}

void Isolate::RemoveContextIdCallback(const v8::WeakCallbackInfo<void>& data) {
  Isolate* isolate = reinterpret_cast<Isolate*>(data.GetIsolate());
  uintptr_t context_id = reinterpret_cast<uintptr_t>(data.GetParameter());
  isolate->recorder_context_id_map_.erase(context_id);
}

LocalHeap* Isolate::main_thread_local_heap() {
  return main_thread_local_isolate()->heap();
}

LocalHeap* Isolate::CurrentLocalHeap() {
  LocalHeap* local_heap = LocalHeap::Current();
  return local_heap ? local_heap : main_thread_local_heap();
}

// |chunk| is either a Page or an executable LargePage.
void Isolate::RemoveCodeMemoryChunk(MemoryChunk* chunk) {
  // We only keep track of individual code pages/allocations if we are on arm32,
  // because on x64 and arm64 we have a code range which makes this unnecessary.
#if !defined(V8_TARGET_ARCH_ARM)
  return;
#else
  void* removed_page_start = reinterpret_cast<void*>(chunk->area_start());
  std::vector<MemoryRange>* old_code_pages = GetCodePages();
  DCHECK_NOT_NULL(old_code_pages);

  std::vector<MemoryRange>* new_code_pages;
  if (old_code_pages == &code_pages_buffer1_) {
    new_code_pages = &code_pages_buffer2_;
  } else {
    new_code_pages = &code_pages_buffer1_;
  }

  // Copy all existing data from the old vector to the new vector except the
  // removed page.
  new_code_pages->clear();
  new_code_pages->reserve(old_code_pages->size() - 1);
  std::remove_copy_if(old_code_pages->begin(), old_code_pages->end(),
                      std::back_inserter(*new_code_pages),
                      [removed_page_start](const MemoryRange& range) {
                        return range.start == removed_page_start;
                      });
  DCHECK_EQ(old_code_pages->size(), new_code_pages->size() + 1);
  // Atomically switch out the pointer
  SetCodePages(new_code_pages);
#endif  // !defined(V8_TARGET_ARCH_ARM)
}

#undef TRACE_ISOLATE

// static
Address Isolate::load_from_stack_count_address(const char* function_name) {
  DCHECK_NOT_NULL(function_name);
  if (!stack_access_count_map) {
    stack_access_count_map = new MapOfLoadsAndStoresPerFunction{};
  }
  auto& map = *stack_access_count_map;
  std::string name(function_name);
  // It is safe to return the address of std::map values.
  // Only iterators and references to the erased elements are invalidated.
  return reinterpret_cast<Address>(&map[name].first);
}

// static
Address Isolate::store_to_stack_count_address(const char* function_name) {
  DCHECK_NOT_NULL(function_name);
  if (!stack_access_count_map) {
    stack_access_count_map = new MapOfLoadsAndStoresPerFunction{};
  }
  auto& map = *stack_access_count_map;
  std::string name(function_name);
  // It is safe to return the address of std::map values.
  // Only iterators and references to the erased elements are invalidated.
  return reinterpret_cast<Address>(&map[name].second);
}

}  // namespace internal
}  // namespace v8
