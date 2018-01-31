// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/isolate.h"

#include <stdlib.h>

#include <fstream>  // NOLINT(readability/streams)
#include <sstream>

#include "src/api.h"
#include "src/assembler-inl.h"
#include "src/ast/ast-value-factory.h"
#include "src/ast/context-slot-cache.h"
#include "src/base/adapters.h"
#include "src/base/hashmap.h"
#include "src/base/platform/platform.h"
#include "src/base/sys-info.h"
#include "src/base/utils/random-number-generator.h"
#include "src/basic-block-profiler.h"
#include "src/bootstrapper.h"
#include "src/cancelable-task.h"
#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/compilation-statistics.h"
#include "src/compiler-dispatcher/compiler-dispatcher.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/elements.h"
#include "src/external-reference-table.h"
#include "src/frames-inl.h"
#include "src/ic/stub-cache.h"
#include "src/interface-descriptors.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate-inl.h"
#include "src/libsampler/sampler.h"
#include "src/log.h"
#include "src/messages.h"
#include "src/objects/frame-array-inl.h"
#include "src/profiler/cpu-profiler.h"
#include "src/prototype.h"
#include "src/regexp/regexp-stack.h"
#include "src/runtime-profiler.h"
#include "src/setup-isolate.h"
#include "src/simulator.h"
#include "src/snapshot/startup-deserializer.h"
#include "src/tracing/tracing-category-observer.h"
#include "src/trap-handler/trap-handler.h"
#include "src/unicode-cache.h"
#include "src/v8.h"
#include "src/version.h"
#include "src/visitors.h"
#include "src/vm-state-inl.h"
#include "src/wasm/compilation-manager.h"
#include "src/wasm/wasm-heap.h"
#include "src/wasm/wasm-objects.h"
#include "src/zone/accounting-allocator.h"

namespace v8 {
namespace internal {

base::Atomic32 ThreadId::highest_thread_id_ = 0;

int ThreadId::AllocateThreadId() {
  int new_id = base::Relaxed_AtomicIncrement(&highest_thread_id_, 1);
  return new_id;
}


int ThreadId::GetCurrentThreadId() {
  int thread_id = base::Thread::GetThreadLocalInt(Isolate::thread_id_key_);
  if (thread_id == 0) {
    thread_id = AllocateThreadId();
    base::Thread::SetThreadLocalInt(Isolate::thread_id_key_, thread_id);
  }
  return thread_id;
}


ThreadLocalTop::ThreadLocalTop() {
  InitializeInternal();
}


void ThreadLocalTop::InitializeInternal() {
  c_entry_fp_ = 0;
  c_function_ = 0;
  handler_ = 0;
#ifdef USE_SIMULATOR
  simulator_ = nullptr;
#endif
  js_entry_sp_ = nullptr;
  external_callback_scope_ = nullptr;
  current_vm_state_ = EXTERNAL;
  try_catch_handler_ = nullptr;
  context_ = nullptr;
  thread_id_ = ThreadId::Invalid();
  external_caught_exception_ = false;
  failed_access_check_callback_ = nullptr;
  save_context_ = nullptr;
  promise_on_stack_ = nullptr;

  // These members are re-initialized later after deserialization
  // is complete.
  pending_exception_ = nullptr;
  wasm_caught_exception_ = nullptr;
  rethrowing_message_ = false;
  pending_message_obj_ = nullptr;
  scheduled_exception_ = nullptr;
}


void ThreadLocalTop::Initialize() {
  InitializeInternal();
#ifdef USE_SIMULATOR
  simulator_ = Simulator::current(isolate_);
#endif
  thread_id_ = ThreadId::Current();
}


void ThreadLocalTop::Free() {
  wasm_caught_exception_ = nullptr;
  // Match unmatched PopPromise calls.
  while (promise_on_stack_) isolate_->PopPromise();
}


base::Thread::LocalStorageKey Isolate::isolate_key_;
base::Thread::LocalStorageKey Isolate::thread_id_key_;
base::Thread::LocalStorageKey Isolate::per_isolate_thread_data_key_;
base::LazyMutex Isolate::thread_data_table_mutex_ = LAZY_MUTEX_INITIALIZER;
Isolate::ThreadDataTable* Isolate::thread_data_table_ = nullptr;
base::Atomic32 Isolate::isolate_counter_ = 0;
#if DEBUG
base::Atomic32 Isolate::isolate_key_created_ = 0;
#endif

Isolate::PerIsolateThreadData*
    Isolate::FindOrAllocatePerThreadDataForThisThread() {
  ThreadId thread_id = ThreadId::Current();
  PerIsolateThreadData* per_thread = nullptr;
  {
    base::LockGuard<base::Mutex> lock_guard(thread_data_table_mutex_.Pointer());
    per_thread = thread_data_table_->Lookup(this, thread_id);
    if (per_thread == nullptr) {
      per_thread = new PerIsolateThreadData(this, thread_id);
      thread_data_table_->Insert(per_thread);
    }
    DCHECK(thread_data_table_->Lookup(this, thread_id) == per_thread);
  }
  return per_thread;
}


void Isolate::DiscardPerThreadDataForThisThread() {
  int thread_id_int = base::Thread::GetThreadLocalInt(Isolate::thread_id_key_);
  if (thread_id_int) {
    ThreadId thread_id = ThreadId(thread_id_int);
    DCHECK(!thread_manager_->mutex_owner_.Equals(thread_id));
    base::LockGuard<base::Mutex> lock_guard(thread_data_table_mutex_.Pointer());
    PerIsolateThreadData* per_thread =
        thread_data_table_->Lookup(this, thread_id);
    if (per_thread) {
      DCHECK(!per_thread->thread_state_);
      thread_data_table_->Remove(per_thread);
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
    base::LockGuard<base::Mutex> lock_guard(thread_data_table_mutex_.Pointer());
    per_thread = thread_data_table_->Lookup(this, thread_id);
  }
  return per_thread;
}


void Isolate::InitializeOncePerProcess() {
  base::LockGuard<base::Mutex> lock_guard(thread_data_table_mutex_.Pointer());
  CHECK_NULL(thread_data_table_);
  isolate_key_ = base::Thread::CreateThreadLocalKey();
#if DEBUG
  base::Relaxed_Store(&isolate_key_created_, 1);
#endif
  thread_id_key_ = base::Thread::CreateThreadLocalKey();
  per_isolate_thread_data_key_ = base::Thread::CreateThreadLocalKey();
  thread_data_table_ = new Isolate::ThreadDataTable();
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
  v->VisitRootPointer(Root::kTop, &thread->pending_exception_);
  v->VisitRootPointer(Root::kTop, &thread->wasm_caught_exception_);
  v->VisitRootPointer(Root::kTop, &thread->pending_message_obj_);
  v->VisitRootPointer(Root::kTop, bit_cast<Object**>(&(thread->context_)));
  v->VisitRootPointer(Root::kTop, &thread->scheduled_exception_);

  for (v8::TryCatch* block = thread->try_catch_handler(); block != nullptr;
       block = block->next_) {
    v->VisitRootPointer(Root::kTop, bit_cast<Object**>(&(block->exception_)));
    v->VisitRootPointer(Root::kTop, bit_cast<Object**>(&(block->message_obj_)));
  }

  // Iterate over pointers on native execution stack.
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
bool Isolate::IsDeferredHandle(Object** handle) {
  // Each DeferredHandles instance keeps the handles to one job in the
  // concurrent recompilation queue, containing a list of blocks.  Each block
  // contains kHandleBlockSize handles except for the first block, which may
  // not be fully filled.
  // We iterate through all the blocks to see whether the argument handle
  // belongs to one of the blocks.  If so, it is deferred.
  for (DeferredHandles* deferred = deferred_handles_head_; deferred != nullptr;
       deferred = deferred->next_) {
    std::vector<Object**>* blocks = &deferred->blocks_;
    for (size_t i = 0; i < blocks->size(); i++) {
      Object** block_limit = (i == 0) ? deferred->first_block_limit_
                                      : blocks->at(i) + kHandleBlockSize;
      if (blocks->at(i) <= handle && handle < block_limit) return true;
    }
  }
  return false;
}
#endif  // DEBUG


void Isolate::RegisterTryCatchHandler(v8::TryCatch* that) {
  thread_local_top()->set_try_catch_handler(that);
}


void Isolate::UnregisterTryCatchHandler(v8::TryCatch* that) {
  DCHECK(thread_local_top()->try_catch_handler() == that);
  thread_local_top()->set_try_catch_handler(that->next_);
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

void Isolate::PushStackTraceAndDie(unsigned int magic1, void* ptr1, void* ptr2,
                                   unsigned int magic2) {
  PushStackTraceAndDie(magic1, ptr1, ptr2, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, magic2);
}

void Isolate::PushStackTraceAndDie(unsigned int magic1, void* ptr1, void* ptr2,
                                   void* ptr3, void* ptr4, void* ptr5,
                                   void* ptr6, void* ptr7, void* ptr8,
                                   unsigned int magic2) {
  const int kMaxStackTraceSize = 32 * KB;
  Handle<String> trace = StackTraceString();
  uint8_t buffer[kMaxStackTraceSize];
  int length = Min(kMaxStackTraceSize - 1, trace->length());
  String::WriteToFlat(*trace, buffer, 0, length);
  buffer[length] = '\0';
  // TODO(dcarney): convert buffer to utf8?
  base::OS::PrintError(
      "Stacktrace:"
      "\n   magic1=%x magic2=%x ptr1=%p ptr2=%p ptr3=%p ptr4=%p ptr5=%p "
      "ptr6=%p ptr7=%p ptr8=%p\n\n%s",
      magic1, magic2, ptr1, ptr2, ptr3, ptr4, ptr5, ptr6, ptr7, ptr8,
      reinterpret_cast<char*>(buffer));
  PushCodeObjectsAndDie(0xdeadc0de, ptr1, ptr2, ptr3, ptr4, ptr5, ptr6, ptr7,
                        ptr8, 0xdeadc0de);
}

void Isolate::PushCodeObjectsAndDie(unsigned int magic1, void* ptr1, void* ptr2,
                                    void* ptr3, void* ptr4, void* ptr5,
                                    void* ptr6, void* ptr7, void* ptr8,
                                    unsigned int magic2) {
  const int kMaxCodeObjects = 16;
  // Mark as volatile to lower the probability of optimizing code_objects
  // away. The first and last entries are set to the magic markers, making it
  // easier to spot the array on the stack.
  void* volatile code_objects[kMaxCodeObjects + 2];
  code_objects[0] = reinterpret_cast<void*>(magic1);
  code_objects[kMaxCodeObjects + 1] = reinterpret_cast<void*>(magic2);
  StackFrameIterator it(this);
  int numCodeObjects = 0;
  for (; !it.done() && numCodeObjects < kMaxCodeObjects; it.Advance()) {
    code_objects[1 + numCodeObjects++] = it.frame()->unchecked_code();
  }

  // Keep the top raw code object pointers on the stack in the hope that the
  // corresponding pages end up more frequently in the minidump.
  base::OS::PrintError(
      "\nCodeObjects (%p length=%i): 1:%p 2:%p 3:%p 4:%p..."
      "\n   magic1=%x magic2=%x ptr1=%p ptr2=%p ptr3=%p ptr4=%p ptr5=%p "
      "ptr6=%p ptr7=%p ptr8=%p\n\n",
      static_cast<void*>(code_objects[0]), numCodeObjects,
      static_cast<void*>(code_objects[1]), static_cast<void*>(code_objects[2]),
      static_cast<void*>(code_objects[3]), static_cast<void*>(code_objects[4]),
      magic1, magic2, ptr1, ptr2, ptr3, ptr4, ptr5, ptr6, ptr7, ptr8);
  base::OS::Abort();
}

namespace {

class FrameArrayBuilder {
 public:
  FrameArrayBuilder(Isolate* isolate, FrameSkipMode mode, int limit,
                    Handle<Object> caller)
      : isolate_(isolate), mode_(mode), limit_(limit), caller_(caller) {
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

  void AppendStandardFrame(StandardFrame* frame) {
    std::vector<FrameSummary> frames;
    frame->Summarize(&frames);
    // A standard frame may include many summarized frames (due to inlining).
    for (size_t i = frames.size(); i != 0 && !full(); i--) {
      const auto& summ = frames[i - 1];
      if (summ.IsJavaScript()) {
        //====================================================================
        // Handle a JavaScript frame.
        //====================================================================
        const auto& summary = summ.AsJavaScript();

        // Filter out internal frames that we do not want to show.
        if (!IsVisibleInStackTrace(summary.function())) continue;

        Handle<AbstractCode> abstract_code = summary.abstract_code();
        const int offset = summary.code_offset();

        bool is_constructor = summary.is_constructor();
        // Help CallSite::IsConstructor correctly detect hand-written
        // construct stubs.
        if (abstract_code->IsCode() &&
            Code::cast(*abstract_code)->is_construct_stub()) {
          is_constructor = true;
        }

        int flags = 0;
        Handle<JSFunction> function = summary.function();
        if (IsStrictFrame(function)) flags |= FrameArray::kIsStrict;
        if (is_constructor) flags |= FrameArray::kIsConstructor;

        elements_ = FrameArray::AppendJSFrame(
            elements_, TheHoleToUndefined(isolate_, summary.receiver()),
            function, abstract_code, offset, flags);
      } else if (summ.IsWasmCompiled()) {
        //====================================================================
        // Handle a WASM compiled frame.
        //====================================================================
        const auto& summary = summ.AsWasmCompiled();
        if (!summary.code().IsCodeObject() &&
            summary.code().GetWasmCode()->kind() != wasm::WasmCode::Function) {
          continue;
        }
        Handle<WasmInstanceObject> instance = summary.wasm_instance();
        int flags = 0;
        if (instance->compiled_module()->is_asm_js()) {
          flags |= FrameArray::kIsAsmJsWasmFrame;
          if (WasmCompiledFrame::cast(frame)->at_to_number_conversion()) {
            flags |= FrameArray::kAsmJsAtNumberConversion;
          }
        } else {
          flags |= FrameArray::kIsWasmFrame;
        }

        elements_ = FrameArray::AppendWasmFrame(
            elements_, instance, summary.function_index(), summary.code(),
            summary.code_offset(), flags);
      } else if (summ.IsWasmInterpreted()) {
        //====================================================================
        // Handle a WASM interpreted frame.
        //====================================================================
        const auto& summary = summ.AsWasmInterpreted();
        Handle<WasmInstanceObject> instance = summary.wasm_instance();
        int flags = FrameArray::kIsWasmInterpretedFrame;
        DCHECK(!instance->compiled_module()->is_asm_js());
        elements_ = FrameArray::AppendWasmFrame(elements_, instance,
                                                summary.function_index(), {},
                                                summary.byte_offset(), flags);
      }
    }
  }

  void AppendBuiltinExitFrame(BuiltinExitFrame* exit_frame) {
    Handle<JSFunction> function = handle(exit_frame->function(), isolate_);

    // Filter out internal frames that we do not want to show.
    if (!IsVisibleInStackTrace(function)) return;

    Handle<Object> receiver(exit_frame->receiver(), isolate_);
    Handle<Code> code(exit_frame->LookupCode(), isolate_);
    const int offset =
        static_cast<int>(exit_frame->pc() - code->instruction_start());

    int flags = 0;
    if (IsStrictFrame(function)) flags |= FrameArray::kIsStrict;
    if (exit_frame->IsConstructor()) flags |= FrameArray::kIsConstructor;

    elements_ = FrameArray::AppendJSFrame(elements_, receiver, function,
                                          Handle<AbstractCode>::cast(code),
                                          offset, flags);
  }

  bool full() { return elements_->FrameCount() >= limit_; }

  Handle<FrameArray> GetElements() {
    elements_->ShrinkToFit();
    return elements_;
  }

 private:
  // Poison stack frames below the first strict mode frame.
  // The stack trace API should not expose receivers and function
  // objects on frames deeper than the top-most one with a strict mode
  // function.
  bool IsStrictFrame(Handle<JSFunction> function) {
    if (!encountered_strict_function_) {
      encountered_strict_function_ =
          is_strict(function->shared()->language_mode());
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
        !function->shared()->IsUserJavaScript()) {
      return function->shared()->native();
    }
    return true;
  }

  bool IsInSameSecurityContext(Handle<JSFunction> function) {
    return isolate_->context()->HasSameSecurityTokenAs(function->context());
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
}  // namespace

Handle<Object> Isolate::CaptureSimpleStackTrace(Handle<JSReceiver> error_object,
                                                FrameSkipMode mode,
                                                Handle<Object> caller) {
  DisallowJavascriptExecution no_js(this);

  int limit;
  if (!GetStackTraceLimit(this, &limit)) return factory()->undefined_value();

  FrameArrayBuilder builder(this, mode, limit, caller);

  for (StackFrameIterator iter(this); !iter.done() && !builder.full();
       iter.Advance()) {
    StackFrame* frame = iter.frame();

    switch (frame->type()) {
      case StackFrame::JAVA_SCRIPT_BUILTIN_CONTINUATION:
      case StackFrame::OPTIMIZED:
      case StackFrame::INTERPRETED:
      case StackFrame::BUILTIN:
        builder.AppendStandardFrame(JavaScriptFrame::cast(frame));
        break;
      case StackFrame::BUILTIN_EXIT:
        // BuiltinExitFrames are not standard frames, so they do not have
        // Summarize(). However, they may have one JS frame worth showing.
        builder.AppendBuiltinExitFrame(BuiltinExitFrame::cast(frame));
        break;
      case StackFrame::WASM_COMPILED:
        builder.AppendStandardFrame(WasmCompiledFrame::cast(frame));
        break;
      case StackFrame::WASM_INTERPRETER_ENTRY:
        builder.AppendStandardFrame(WasmInterpreterEntryFrame::cast(frame));
        break;

      default:
        break;
    }
  }

  // TODO(yangguo): Queue this structured stack trace for preprocessing on GC.
  return factory()->NewJSArrayWithElements(builder.GetElements());
}

MaybeHandle<JSReceiver> Isolate::CaptureAndSetDetailedStackTrace(
    Handle<JSReceiver> error_object) {
  if (capture_stack_trace_for_uncaught_exceptions_) {
    // Capture stack trace for a detailed exception message.
    Handle<Name> key = factory()->detailed_stack_trace_symbol();
    Handle<FixedArray> stack_trace = CaptureCurrentStackTrace(
        stack_trace_for_uncaught_exceptions_frame_limit_,
        stack_trace_for_uncaught_exceptions_options_);
    RETURN_ON_EXCEPTION(this,
                        JSReceiver::SetProperty(error_object, key, stack_trace,
                                                LanguageMode::kStrict),
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
                      JSReceiver::SetProperty(error_object, key, stack_trace,
                                              LanguageMode::kStrict),
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

  JavaScriptFrame* frame = it.frame();
  DCHECK(!frame->is_builtin());
  int position = frame->position();

  Object* maybe_script = frame->function()->shared()->script();
  if (maybe_script->IsScript()) {
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
        reinterpret_cast<Address>(iframe->GetBytecodeArray()) - kHeapObjectTag +
        BytecodeArray::kHeaderSize;
    return bytecode_start + iframe->GetBytecodeOffset();
  }

  return frame->pc();
}

class CaptureStackTraceHelper {
 public:
  explicit CaptureStackTraceHelper(Isolate* isolate) : isolate_(isolate) {}

  Handle<StackFrameInfo> NewStackFrameObject(FrameSummary& summ) {
    if (summ.IsJavaScript()) return NewStackFrameObject(summ.AsJavaScript());
    if (summ.IsWasm()) return NewStackFrameObject(summ.AsWasm());
    UNREACHABLE();
  }

  Handle<StackFrameInfo> NewStackFrameObject(
      const FrameSummary::JavaScriptFrameSummary& summ) {
    int code_offset;
    Handle<ByteArray> source_position_table;
    Handle<Object> maybe_cache;
    Handle<NumberDictionary> cache;
    if (!FLAG_optimize_for_size) {
      code_offset = summ.code_offset();
      source_position_table =
          handle(summ.abstract_code()->source_position_table(), isolate_);
      maybe_cache = handle(summ.abstract_code()->stack_frame_cache(), isolate_);
      if (maybe_cache->IsNumberDictionary()) {
        cache = Handle<NumberDictionary>::cast(maybe_cache);
      } else {
        cache = NumberDictionary::New(isolate_, 1);
      }
      int entry = cache->FindEntry(code_offset);
      if (entry != NumberDictionary::kNotFound) {
        Handle<StackFrameInfo> frame(
            StackFrameInfo::cast(cache->ValueAt(entry)));
        DCHECK(frame->function_name()->IsString());
        Handle<String> function_name = summ.FunctionName();
        if (function_name->Equals(String::cast(frame->function_name()))) {
          return frame;
        }
      }
    }

    Handle<StackFrameInfo> frame = factory()->NewStackFrameInfo();
    Handle<Script> script = Handle<Script>::cast(summ.script());
    Script::PositionInfo info;
    bool valid_pos = Script::GetPositionInfo(script, summ.SourcePosition(),
                                             &info, Script::WITH_OFFSET);
    if (valid_pos) {
      frame->set_line_number(info.line + 1);
      frame->set_column_number(info.column + 1);
    }
    frame->set_script_id(script->id());
    frame->set_script_name(script->name());
    frame->set_script_name_or_source_url(script->GetNameOrSourceURL());
    frame->set_is_eval(script->compilation_type() ==
                       Script::COMPILATION_TYPE_EVAL);
    Handle<String> function_name = summ.FunctionName();
    frame->set_function_name(*function_name);
    frame->set_is_constructor(summ.is_constructor());
    frame->set_is_wasm(false);
    if (!FLAG_optimize_for_size) {
      auto new_cache = NumberDictionary::Set(cache, code_offset, frame);
      if (*new_cache != *cache || !maybe_cache->IsNumberDictionary()) {
        AbstractCode::SetStackFrameCache(summ.abstract_code(), new_cache);
      }
    }
    frame->set_id(next_id());
    return frame;
  }

  Handle<StackFrameInfo> NewStackFrameObject(
      const FrameSummary::WasmFrameSummary& summ) {
    Handle<StackFrameInfo> info = factory()->NewStackFrameInfo();

    Handle<WasmCompiledModule> compiled_module(
        summ.wasm_instance()->compiled_module(), isolate_);
    Handle<String> name = WasmCompiledModule::GetFunctionName(
        isolate_, compiled_module, summ.function_index());
    info->set_function_name(*name);
    // Encode the function index as line number (1-based).
    info->set_line_number(summ.function_index() + 1);
    // Encode the byte offset as column (1-based).
    int position = summ.byte_offset();
    // Make position 1-based.
    if (position >= 0) ++position;
    info->set_column_number(position);
    info->set_script_id(summ.script()->id());
    info->set_is_wasm(true);
    info->set_id(next_id());
    return info;
  }

 private:
  inline Factory* factory() { return isolate_->factory(); }

  int next_id() const {
    int id = isolate_->last_stack_frame_info_id() + 1;
    isolate_->set_last_stack_frame_info_id(id);
    return id;
  }

  Isolate* isolate_;
};

Handle<FixedArray> Isolate::CaptureCurrentStackTrace(
    int frame_limit, StackTrace::StackTraceOptions options) {
  DisallowJavascriptExecution no_js(this);
  CaptureStackTraceHelper helper(this);

  // Ensure no negative values.
  int limit = Max(frame_limit, 0);
  Handle<FixedArray> stack_trace_elems = factory()->NewFixedArray(limit);

  int frames_seen = 0;
  for (StackTraceFrameIterator it(this); !it.done() && (frames_seen < limit);
       it.Advance()) {
    StandardFrame* frame = it.frame();
    // Set initial size to the maximum inlining level + 1 for the outermost
    // function.
    std::vector<FrameSummary> frames;
    frame->Summarize(&frames);
    for (size_t i = frames.size(); i != 0 && frames_seen < limit; i--) {
      FrameSummary& frame = frames[i - 1];
      if (!frame.is_subject_to_debugging()) continue;
      // Filter frames from other security contexts.
      if (!(options & StackTrace::kExposeFramesAcrossSecurityOrigins) &&
          !this->context()->HasSameSecurityTokenAs(*frame.native_context()))
        continue;
      Handle<StackFrameInfo> new_frame_obj = helper.NewStackFrameObject(frame);
      stack_trace_elems->set(frames_seen, *new_frame_obj);
      frames_seen++;
    }
  }
  stack_trace_elems->Shrink(frames_seen);
  return stack_trace_elems;
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


static void PrintFrames(Isolate* isolate,
                        StringStream* accumulator,
                        StackFrame::PrintMode mode) {
  StackFrameIterator it(isolate);
  for (int i = 0; !it.done(); it.Advance()) {
    it.frame()->Print(accumulator, mode, i++);
  }
}

void Isolate::PrintStack(StringStream* accumulator, PrintStackMode mode) {
  // The MentionedObjectCache is not GC-proof at the moment.
  DisallowHeapAllocation no_gc;
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
  DCHECK(context());

  // Get the data object from access check info.
  HandleScope scope(this);
  Handle<Object> data;
  { DisallowHeapAllocation no_gc;
    AccessCheckInfo* access_check_info = AccessCheckInfo::Get(this, receiver);
    if (!access_check_info) {
      AllowHeapAllocation doesnt_matter_anymore;
      return ScheduleThrow(
          *factory()->NewTypeError(MessageTemplate::kNoAccess));
    }
    data = handle(access_check_info->data(), this);
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
      Object* receiver_context =
          JSGlobalProxy::cast(*receiver)->native_context();
      if (!receiver_context->IsContext()) return false;

      // Get the native context of current top context.
      // avoid using Isolate::native_context() because it uses Handle.
      Context* native_context =
          accessing_context->global_object()->native_context();
      if (receiver_context == native_context) return true;

      if (Context::cast(receiver_context)->security_token() ==
          native_context->security_token())
        return true;
    }
  }

  HandleScope scope(this);
  Handle<Object> data;
  v8::AccessCheckCallback callback = nullptr;
  { DisallowHeapAllocation no_gc;
    AccessCheckInfo* access_check_info = AccessCheckInfo::Get(this, receiver);
    if (!access_check_info) return false;
    Object* fun_obj = access_check_info->callback();
    callback = v8::ToCData<v8::AccessCheckCallback>(fun_obj);
    data = handle(access_check_info->data(), this);
  }

  LOG(this, ApiSecurityCheck());

  {
    // Leaving JavaScript.
    VMState<EXTERNAL> state(this);
    return callback(v8::Utils::ToLocal(accessing_context),
                    v8::Utils::ToLocal(receiver), v8::Utils::ToLocal(data));
  }
}


Object* Isolate::StackOverflow() {
  if (FLAG_abort_on_stack_or_string_length_overflow) {
    FATAL("Aborting on stack overflow");
  }

  DisallowJavascriptExecution no_js(this);
  HandleScope scope(this);

  Handle<JSFunction> fun = range_error_function();
  Handle<Object> msg = factory()->NewStringFromAsciiChecked(
      MessageTemplate::TemplateString(MessageTemplate::kStackOverflow));
  Handle<Object> no_caller;
  Handle<Object> exception;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      this, exception,
      ErrorUtils::Construct(this, fun, fun, msg, SKIP_NONE, no_caller, true));

  Throw(*exception, nullptr);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && FLAG_stress_compaction) {
    heap()->CollectAllGarbage(Heap::kNoGCFlags,
                              GarbageCollectionReason::kTesting);
  }
#endif  // VERIFY_HEAP

  return heap()->exception();
}


Object* Isolate::TerminateExecution() {
  return Throw(heap_.termination_exception(), nullptr);
}


void Isolate::CancelTerminateExecution() {
  if (try_catch_handler()) {
    try_catch_handler()->has_terminated_ = false;
  }
  if (has_pending_exception() &&
      pending_exception() == heap_.termination_exception()) {
    thread_local_top()->external_caught_exception_ = false;
    clear_pending_exception();
  }
  if (has_scheduled_exception() &&
      scheduled_exception() == heap_.termination_exception()) {
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
      this, &RuntimeCallStats::InvokeApiInterruptCallbacks);
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
  if (exception->IsString() && location->script()->name()->IsString()) {
    base::OS::PrintError(
        "Extension or internal compilation error: %s in %s at line %d.\n",
        String::cast(*exception)->ToCString().get(),
        String::cast(location->script()->name())->ToCString().get(),
        line_number);
  } else if (location->script()->name()->IsString()) {
    base::OS::PrintError(
        "Extension or internal compilation error in %s at line %d.\n",
        String::cast(location->script()->name())->ToCString().get(),
        line_number);
  } else if (exception->IsString()) {
    base::OS::PrintError("Extension or internal compilation error: %s.\n",
                         String::cast(*exception)->ToCString().get());
  } else {
    base::OS::PrintError("Extension or internal compilation error.\n");
  }
#ifdef OBJECT_PRINT
  // Since comments and empty lines have been stripped from the source of
  // builtins, print the actual source here so that line numbers match.
  if (location->script()->source()->IsString()) {
    Handle<String> src(String::cast(location->script()->source()));
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

bool Isolate::is_catchable_by_wasm(Object* exception) {
  if (!is_catchable_by_javascript(exception) || !exception->IsJSError())
    return false;
  HandleScope scope(this);
  Handle<Object> exception_handle(exception, this);
  return JSReceiver::HasProperty(Handle<JSReceiver>::cast(exception_handle),
                                 factory()->InternalizeUtf8String(
                                     wasm::WasmException::kRuntimeIdStr))
      .IsJust();
}

Object* Isolate::Throw(Object* exception, MessageLocation* location) {
  DCHECK(!has_pending_exception());

  HandleScope scope(this);
  Handle<Object> exception_handle(exception, this);

  if (FLAG_print_all_exceptions) {
    printf("=========================================================\n");
    printf("Exception thrown:\n");
    if (location) {
      Handle<Script> script = location->script();
      Handle<Object> name(script->GetNameOrSourceURL(), this);
      printf("at ");
      if (name->IsString() && String::cast(*name)->length() > 0)
        String::cast(*name)->PrintOn(stdout);
      else
        printf("<anonymous>");
// Script::GetLineNumber and Script::GetColumnNumber can allocate on the heap to
// initialize the line_ends array, so be careful when calling them.
#ifdef DEBUG
      if (AllowHeapAllocation::IsAllowed()) {
#else
      if (false) {
#endif
        printf(", %d:%d - %d:%d\n",
               Script::GetLineNumber(script, location->start_pos()) + 1,
               Script::GetColumnNumber(script, location->start_pos()),
               Script::GetLineNumber(script, location->end_pos()) + 1,
               Script::GetColumnNumber(script, location->end_pos()));
      } else {
        printf(", line %d\n", script->GetLineNumber(location->start_pos()) + 1);
      }
    }
    exception->Print();
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
  if (is_catchable_by_javascript(exception)) {
    debug()->OnThrow(exception_handle);
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
      ReportBootstrappingException(exception_handle, location);
    } else {
      Handle<Object> message_obj = CreateMessage(exception_handle, location);
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
  set_pending_exception(*exception_handle);
  return heap()->exception();
}


Object* Isolate::ReThrow(Object* exception) {
  DCHECK(!has_pending_exception());

  // Set the exception being re-thrown.
  set_pending_exception(exception);
  return heap()->exception();
}


Object* Isolate::UnwindAndFindHandler() {
  Object* exception = pending_exception();

  auto FoundHandler = [&](Context* context, Address instruction_start,
                          intptr_t handler_offset,
                          Address constant_pool_address, Address handler_sp,
                          Address handler_fp) {
    // Store information to be consumed by the CEntryStub.
    thread_local_top()->pending_handler_context_ = context;
    thread_local_top()->pending_handler_entrypoint_ =
        instruction_start + handler_offset;
    thread_local_top()->pending_handler_constant_pool_ = constant_pool_address;
    thread_local_top()->pending_handler_fp_ = handler_fp;
    thread_local_top()->pending_handler_sp_ = handler_sp;

    // Return and clear pending exception.
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
        // For JSEntryStub frames we always have a handler.
        StackHandler* handler = frame->top_handler();

        // Restore the next handler.
        thread_local_top()->handler_ = handler->next()->address();

        // Gather information from the handler.
        Code* code = frame->LookupCode();
        return FoundHandler(
            nullptr, code->instruction_start(),
            Smi::ToInt(code->handler_table()->get(0)), code->constant_pool(),
            handler->address() + StackHandlerConstants::kSize, 0);
      }

      case StackFrame::WASM_COMPILED: {
        if (trap_handler::IsThreadInWasm()) {
          trap_handler::ClearThreadInWasm();
        }

        if (!FLAG_experimental_wasm_eh || !is_catchable_by_wasm(exception)) {
          break;
        }
        int stack_slots = 0;  // Will contain stack slot count of frame.
        WasmCompiledFrame* wasm_frame = static_cast<WasmCompiledFrame*>(frame);
        int offset = wasm_frame->LookupExceptionHandlerInTable(&stack_slots);
        if (offset < 0) break;
        // Compute the stack pointer from the frame pointer. This ensures that
        // argument slots on the stack are dropped as returning would.
        Address return_sp = frame->fp() +
                            StandardFrameConstants::kFixedFrameSizeAboveFp -
                            stack_slots * kPointerSize;

        // This is going to be handled by Wasm, so we need to set the TLS flag
        // again.
        trap_handler::SetThreadInWasm();

        set_wasm_caught_exception(exception);
        if (FLAG_wasm_jit_to_native) {
          wasm::WasmCode* wasm_code =
              wasm_code_manager()->LookupCode(frame->pc());
          return FoundHandler(nullptr, wasm_code->instructions().start(),
                              offset, wasm_code->constant_pool(), return_sp,
                              frame->fp());
        } else {
          Code* code = frame->LookupCode();
          return FoundHandler(nullptr, code->instruction_start(), offset,
                              code->constant_pool(), return_sp, frame->fp());
        }
      }

      case StackFrame::OPTIMIZED: {
        // For optimized frames we perform a lookup in the handler table.
        if (!catchable_by_js) break;
        OptimizedFrame* js_frame = static_cast<OptimizedFrame*>(frame);
        int stack_slots = 0;  // Will contain stack slot count of frame.
        int offset =
            js_frame->LookupExceptionHandlerInTable(&stack_slots, nullptr);
        if (offset < 0) break;
        // Compute the stack pointer from the frame pointer. This ensures
        // that argument slots on the stack are dropped as returning would.
        Address return_sp = frame->fp() +
                            StandardFrameConstants::kFixedFrameSizeAboveFp -
                            stack_slots * kPointerSize;

        // Gather information from the frame.
        Code* code = frame->LookupCode();

        // TODO(bmeurer): Turbofanned BUILTIN frames appear as OPTIMIZED,
        // but do not have a code kind of OPTIMIZED_FUNCTION.
        if (code->kind() == Code::OPTIMIZED_FUNCTION &&
            code->marked_for_deoptimization()) {
          // If the target code is lazy deoptimized, we jump to the original
          // return address, but we make a note that we are throwing, so
          // that the deoptimizer can do the right thing.
          offset = static_cast<int>(frame->pc() - code->entry());
          set_deoptimizer_lazy_throw(true);
        }

        return FoundHandler(nullptr, code->instruction_start(), offset,
                            code->constant_pool(), return_sp, frame->fp());
      }

      case StackFrame::STUB: {
        // Some stubs are able to handle exceptions.
        if (!catchable_by_js) break;
        StubFrame* stub_frame = static_cast<StubFrame*>(frame);
        Code* code = stub_frame->LookupCode();
        if (!code->IsCode() || code->kind() != Code::BUILTIN ||
            !code->handler_table()->length() || !code->is_turbofanned()) {
          break;
        }

        int stack_slots = 0;  // Will contain stack slot count of frame.
        int offset = stub_frame->LookupExceptionHandlerInTable(&stack_slots);
        if (offset < 0) break;

        // Compute the stack pointer from the frame pointer. This ensures
        // that argument slots on the stack are dropped as returning would.
        Address return_sp = frame->fp() +
                            StandardFrameConstants::kFixedFrameSizeAboveFp -
                            stack_slots * kPointerSize;

        return FoundHandler(nullptr, code->instruction_start(), offset,
                            code->constant_pool(), return_sp, frame->fp());
      }

      case StackFrame::INTERPRETED: {
        // For interpreted frame we perform a range lookup in the handler table.
        if (!catchable_by_js) break;
        InterpretedFrame* js_frame = static_cast<InterpretedFrame*>(frame);
        int register_slots = InterpreterFrameConstants::RegisterStackSlotCount(
            js_frame->GetBytecodeArray()->register_count());
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
                            register_slots * kPointerSize;

        // Patch the bytecode offset in the interpreted frame to reflect the
        // position of the exception handler. The special builtin below will
        // take care of continuing to dispatch at that position. Also restore
        // the correct context for the handler from the interpreter register.
        Context* context =
            Context::cast(js_frame->ReadInterpreterRegister(context_reg));
        js_frame->PatchBytecodeOffset(static_cast<int>(offset));

        Code* code =
            builtins()->builtin(Builtins::kInterpreterEnterBytecodeDispatch);
        return FoundHandler(context, code->instruction_start(), 0,
                            code->constant_pool(), return_sp, frame->fp());
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
        WasmInterpreterEntryFrame* interpreter_frame =
            WasmInterpreterEntryFrame::cast(frame);
        // TODO(wasm): Implement try-catch in the interpreter.
        interpreter_frame->wasm_instance()->debug_info()->Unwind(frame->fp());
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
      DCHECK_IMPLIES(removed, frame->LookupCode()->marked_for_deoptimization());
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
          prediction = code->GetCode()->GetBuiltinCatchPrediction();
          if (prediction == HandlerTable::UNCAUGHT) continue;
          return prediction;
        }

        // Must have been constructed from a bytecode array.
        CHECK_EQ(AbstractCode::INTERPRETED_FUNCTION, code->kind());
        int code_offset = summary.code_offset();
        BytecodeArray* bytecode = code->GetBytecodeArray();
        HandlerTable* table = HandlerTable::cast(bytecode->handler_table());
        int index = table->LookupRange(code_offset, nullptr, &prediction);
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
  if (IsExternalHandlerOnTop(nullptr)) return CAUGHT_BY_EXTERNAL;

  // Search for an exception handler by performing a full walk over the stack.
  for (StackFrameIterator iter(this); !iter.done(); iter.Advance()) {
    StackFrame* frame = iter.frame();

    switch (frame->type()) {
      case StackFrame::ENTRY:
      case StackFrame::CONSTRUCT_ENTRY: {
        Address entry_handler = frame->top_handler()->next()->address();
        // The exception has been externally caught if and only if there is an
        // external handler which is on top of the top-most JS_ENTRY handler.
        if (external_handler != nullptr && !try_catch_handler()->is_verbose_) {
          if (entry_handler == nullptr || entry_handler > external_handler) {
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
        Handle<Code> code(frame->LookupCode());
        if (!code->IsCode() || code->kind() != Code::BUILTIN ||
            !code->handler_table()->length() || !code->is_turbofanned()) {
          break;
        }

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

Object* Isolate::ThrowIllegalOperation() {
  if (FLAG_stack_trace_on_illegal) PrintStack(stdout);
  return Throw(heap()->illegal_access_string());
}


void Isolate::ScheduleThrow(Object* exception) {
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
  Object* message = reinterpret_cast<Object*>(handler->message_obj_);
  DCHECK(message->IsJSMessageObject() || message->IsTheHole(this));
  thread_local_top()->pending_message_obj_ = message;
}


void Isolate::CancelScheduledExceptionFromTryCatch(v8::TryCatch* handler) {
  DCHECK(has_scheduled_exception());
  if (scheduled_exception() == handler->exception_) {
    DCHECK(scheduled_exception() != heap()->termination_exception());
    clear_scheduled_exception();
  }
  if (thread_local_top_.pending_message_obj_ == handler->message_obj_) {
    clear_pending_message();
  }
}


Object* Isolate::PromoteScheduledException() {
  Object* thrown = scheduled_exception();
  clear_scheduled_exception();
  // Re-throw the exception to avoid getting repeated error reporting.
  return ReThrow(thrown);
}


void Isolate::PrintCurrentStackTrace(FILE* out) {
  for (StackTraceFrameIterator it(this); !it.done(); it.Advance()) {
    if (!it.is_javascript()) continue;

    HandleScope scope(this);
    JavaScriptFrame* frame = it.javascript_frame();

    Handle<Object> receiver(frame->receiver(), this);
    Handle<JSFunction> function(frame->function(), this);
    Handle<AbstractCode> code(AbstractCode::cast(frame->LookupCode()), this);
    const int offset =
        static_cast<int>(frame->pc() - code->instruction_start());

    JSStackFrame site(this, receiver, function, code, offset);
    Handle<String> line = site.ToString().ToHandleChecked();
    if (line->length() > 0) {
      line->PrintOn(out);
      PrintF(out, "\n");
    }
  }
}

bool Isolate::ComputeLocation(MessageLocation* target) {
  StackTraceFrameIterator it(this);
  if (it.done()) return false;
  StandardFrame* frame = it.frame();
  // Compute the location from the function and the relocation info of the
  // baseline code. For optimized code this will use the deoptimization
  // information to get canonical location information.
  std::vector<FrameSummary> frames;
  frame->Summarize(&frames);
  FrameSummary& summary = frames.back();
  int pos = summary.SourcePosition();
  Handle<SharedFunctionInfo> shared;
  Handle<Object> script = summary.script();
  if (!script->IsScript() ||
      (Script::cast(*script)->source()->IsUndefined(this))) {
    return false;
  }

  if (summary.IsJavaScript()) {
    shared = handle(summary.AsJavaScript().function()->shared());
  }
  *target = MessageLocation(Handle<Script>::cast(script), pos, pos + 1, shared);
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

  Handle<Script> cast_script(Script::cast(*script));
  *target = MessageLocation(cast_script, start_pos_value, end_pos_value);
  return true;
}


bool Isolate::ComputeLocationFromStackTrace(MessageLocation* target,
                                            Handle<Object> exception) {
  if (!exception->IsJSObject()) return false;
  Handle<Name> key = factory()->stack_trace_symbol();
  Handle<Object> property =
      JSReceiver::GetDataProperty(Handle<JSObject>::cast(exception), key);
  if (!property->IsJSArray()) return false;
  Handle<JSArray> simple_stack_trace = Handle<JSArray>::cast(property);

  Handle<FrameArray> elements(FrameArray::cast(simple_stack_trace->elements()));

  const int frame_count = elements->FrameCount();
  for (int i = 0; i < frame_count; i++) {
    if (elements->IsWasmFrame(i) || elements->IsAsmJsWasmFrame(i)) {
      Handle<WasmCompiledModule> compiled_module(
          WasmInstanceObject::cast(elements->WasmInstance(i))
              ->compiled_module());
      uint32_t func_index =
          static_cast<uint32_t>(elements->WasmFunctionIndex(i)->value());
      int code_offset = elements->Offset(i)->value();

      // TODO(titzer): store a reference to the code object in FrameArray;
      // a second lookup here could lead to inconsistency.
      int byte_offset =
          FLAG_wasm_jit_to_native
              ? FrameSummary::WasmCompiledFrameSummary::GetWasmSourcePosition(
                    compiled_module->GetNativeModule()->GetCode(func_index),
                    code_offset)
              : elements->Code(i)->SourcePosition(code_offset);

      bool is_at_number_conversion =
          elements->IsAsmJsWasmFrame(i) &&
          elements->Flags(i)->value() & FrameArray::kAsmJsAtNumberConversion;
      int pos = WasmCompiledModule::GetSourcePosition(
          compiled_module, func_index, byte_offset, is_at_number_conversion);
      Handle<Script> script(compiled_module->script());

      *target = MessageLocation(script, pos, pos + 1);
      return true;
    }

    Handle<JSFunction> fun = handle(elements->Function(i), this);
    if (!fun->shared()->IsSubjectToDebugging()) continue;

    Object* script = fun->shared()->script();
    if (script->IsScript() &&
        !(Script::cast(script)->source()->IsUndefined(this))) {
      AbstractCode* abstract_code = elements->Code(i);
      const int code_offset = elements->Offset(i)->value();
      const int pos = abstract_code->SourcePosition(code_offset);

      Handle<Script> casted_script(Script::cast(script));
      *target = MessageLocation(casted_script, pos, pos + 1);
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


bool Isolate::IsJavaScriptHandlerOnTop(Object* exception) {
  DCHECK_NE(heap()->the_hole_value(), exception);

  // For uncatchable exceptions, the JavaScript handler cannot be on top.
  if (!is_catchable_by_javascript(exception)) return false;

  // Get the top-most JS_ENTRY handler, cannot be on top if it doesn't exist.
  Address entry_handler = Isolate::handler(thread_local_top());
  if (entry_handler == nullptr) return false;

  // Get the address of the external handler so we can compare the address to
  // determine which one is closer to the top of the stack.
  Address external_handler = thread_local_top()->try_catch_handler_address();
  if (external_handler == nullptr) return true;

  // The exception has been externally caught if and only if there is an
  // external handler which is on top of the top-most JS_ENTRY handler.
  //
  // Note, that finally clauses would re-throw an exception unless it's aborted
  // by jumps in control flow (like return, break, etc.) and we'll have another
  // chance to set proper v8::TryCatch later.
  return (entry_handler < external_handler);
}


bool Isolate::IsExternalHandlerOnTop(Object* exception) {
  DCHECK_NE(heap()->the_hole_value(), exception);

  // Get the address of the external handler so we can compare the address to
  // determine which one is closer to the top of the stack.
  Address external_handler = thread_local_top()->try_catch_handler_address();
  if (external_handler == nullptr) return false;

  // For uncatchable exceptions, the external handler is always on top.
  if (!is_catchable_by_javascript(exception)) return true;

  // Get the top-most JS_ENTRY handler, cannot be on top if it doesn't exist.
  Address entry_handler = Isolate::handler(thread_local_top());
  if (entry_handler == nullptr) return true;

  // The exception has been externally caught if and only if there is an
  // external handler which is on top of the top-most JS_ENTRY handler.
  //
  // Note, that finally clauses would re-throw an exception unless it's aborted
  // by jumps in control flow (like return, break, etc.) and we'll have another
  // chance to set proper v8::TryCatch later.
  return (entry_handler > external_handler);
}


void Isolate::ReportPendingMessages() {
  DCHECK(AllowExceptions::IsAllowed(this));

  // The embedder might run script in response to an exception.
  AllowJavascriptExecutionDebugOnly allow_script(this);

  Object* exception = pending_exception();

  // Try to propagate the exception to an external v8::TryCatch handler. If
  // propagation was unsuccessful, then we will get another chance at reporting
  // the pending message if the exception is re-thrown.
  bool has_been_propagated = PropagatePendingExceptionToExternalTryCatch();
  if (!has_been_propagated) return;

  // Clear the pending message object early to avoid endless recursion.
  Object* message_obj = thread_local_top_.pending_message_obj_;
  clear_pending_message();

  // For uncatchable exceptions we do nothing. If needed, the exception and the
  // message have already been propagated to v8::TryCatch.
  if (!is_catchable_by_javascript(exception)) return;

  // Determine whether the message needs to be reported to all message handlers
  // depending on whether and external v8::TryCatch or an internal JavaScript
  // handler is on top.
  bool should_report_exception;
  if (IsExternalHandlerOnTop(exception)) {
    // Only report the exception if the external handler is verbose.
    should_report_exception = try_catch_handler()->is_verbose_;
  } else {
    // Report the exception if it isn't caught by JavaScript code.
    should_report_exception = !IsJavaScriptHandlerOnTop(exception);
  }

  // Actually report the pending message to all message handlers.
  if (!message_obj->IsTheHole(this) && should_report_exception) {
    HandleScope scope(this);
    Handle<JSMessageObject> message(JSMessageObject::cast(message_obj), this);
    Handle<JSValue> script_wrapper(JSValue::cast(message->script()), this);
    Handle<Script> script(Script::cast(script_wrapper->value()), this);
    int start_pos = message->start_position();
    int end_pos = message->end_position();
    MessageLocation location(script, start_pos, end_pos);
    MessageHandler::ReportMessage(this, &location, message);
  }
}


MessageLocation Isolate::GetMessageLocation() {
  DCHECK(has_pending_exception());

  if (thread_local_top_.pending_exception_ != heap()->termination_exception() &&
      !thread_local_top_.pending_message_obj_->IsTheHole(this)) {
    Handle<JSMessageObject> message_obj(
        JSMessageObject::cast(thread_local_top_.pending_message_obj_), this);
    Handle<JSValue> script_wrapper(JSValue::cast(message_obj->script()), this);
    Handle<Script> script(Script::cast(script_wrapper->value()), this);
    int start_pos = message_obj->start_position();
    int end_pos = message_obj->end_position();
    return MessageLocation(script, start_pos, end_pos);
  }

  return MessageLocation();
}


bool Isolate::OptionalRescheduleException(bool is_bottom_call) {
  DCHECK(has_pending_exception());
  PropagatePendingExceptionToExternalTryCatch();

  bool is_termination_exception =
      pending_exception() == heap_.termination_exception();

  // Do not reschedule the exception if this is the bottom call.
  bool clear_exception = is_bottom_call;

  if (is_termination_exception) {
    if (is_bottom_call) {
      thread_local_top()->external_caught_exception_ = false;
      clear_pending_exception();
      return false;
    }
  } else if (thread_local_top()->external_caught_exception_) {
    // If the exception is externally caught, clear it if there are no
    // JavaScript frames on the way to the C++ frame that has the
    // external handler.
    DCHECK_NOT_NULL(thread_local_top()->try_catch_handler_address());
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

  Handle<Object> queue(promise->reject_reactions(), isolate);
  Handle<Object> deferred_promise(promise->deferred_promise(), isolate);

  if (queue->IsUndefined(isolate)) {
    return false;
  }

  if (queue->IsCallable()) {
    return PromiseHandlerCheck(isolate, Handle<JSReceiver>::cast(queue),
                               Handle<JSReceiver>::cast(deferred_promise));
  }

  if (queue->IsSymbol()) {
    return InternalPromiseHasUserDefinedRejectHandler(
        isolate, Handle<JSPromise>::cast(deferred_promise));
  }

  Handle<FixedArray> queue_arr = Handle<FixedArray>::cast(queue);
  Handle<FixedArray> deferred_promise_arr =
      Handle<FixedArray>::cast(deferred_promise);
  for (int i = 0; i < deferred_promise_arr->length(); i++) {
    Handle<JSReceiver> deferred_promise_item(
        JSReceiver::cast(deferred_promise_arr->get(i)));
    if (queue_arr->get(i)->IsSymbol()) {
      if (InternalPromiseHasUserDefinedRejectHandler(
              isolate, Handle<JSPromise>::cast(deferred_promise_item))) {
        return true;
      }
    } else {
      Handle<JSReceiver> queue_item(JSReceiver::cast(queue_arr->get(i)));
      if (PromiseHandlerCheck(isolate, queue_item, deferred_promise_item)) {
        return true;
      }
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
  for (JavaScriptFrameIterator it(this); !it.done(); it.Advance()) {
    switch (PredictException(it.frame())) {
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
      bool capture,
      int frame_limit,
      StackTrace::StackTraceOptions options) {
  capture_stack_trace_for_uncaught_exceptions_ = capture;
  stack_trace_for_uncaught_exceptions_frame_limit_ = frame_limit;
  stack_trace_for_uncaught_exceptions_options_ = options;
}


void Isolate::SetAbortOnUncaughtExceptionCallback(
    v8::Isolate::AbortOnUncaughtExceptionCallback callback) {
  abort_on_uncaught_exception_callback_ = callback;
}

namespace {
void AdvanceWhileDebugContext(JavaScriptFrameIterator& it, Debug* debug) {
  if (!debug->in_debug_scope()) return;

  while (!it.done()) {
    Context* context = Context::cast(it.frame()->context());
    if (context->native_context() == *debug->debug_context()) {
      it.Advance();
    } else {
      break;
    }
  }
}
}  // namespace

Handle<Context> Isolate::GetCallingNativeContext() {
  JavaScriptFrameIterator it(this);
  AdvanceWhileDebugContext(it, debug_);
  if (it.done()) return Handle<Context>::null();
  JavaScriptFrame* frame = it.frame();
  Context* context = Context::cast(frame->context());
  return Handle<Context>(context->native_context(), this);
}

Handle<Context> Isolate::GetIncumbentContext() {
  JavaScriptFrameIterator it(this);
  AdvanceWhileDebugContext(it, debug_);

  // 1st candidate: most-recently-entered author function's context
  // if it's newer than the last Context::BackupIncumbentScope entry.
  if (!it.done() &&
      static_cast<const void*>(it.frame()) >
          static_cast<const void*>(top_backup_incumbent_scope())) {
    Context* context = Context::cast(it.frame()->context());
    return Handle<Context>(context->native_context(), this);
  }

  // 2nd candidate: the last Context::Scope's incumbent context if any.
  if (top_backup_incumbent_scope()) {
    return Utils::OpenHandle(
        *top_backup_incumbent_scope()->backup_incumbent_context_);
  }

  // Last candidate: the entered context.
  // Given that there is no other author function is running, there must be
  // no cross-context function running, then the incumbent realm must match
  // the entry realm.
  v8::Local<v8::Context> entered_context =
      reinterpret_cast<v8::Isolate*>(this)->GetEnteredContext();
  return Utils::OpenHandle(*entered_context);
}

char* Isolate::ArchiveThread(char* to) {
  MemCopy(to, reinterpret_cast<char*>(thread_local_top()),
          sizeof(ThreadLocalTop));
  InitializeThreadLocal();
  clear_pending_exception();
  clear_pending_message();
  clear_scheduled_exception();
  return to + sizeof(ThreadLocalTop);
}


char* Isolate::RestoreThread(char* from) {
  MemCopy(reinterpret_cast<char*>(thread_local_top()), from,
          sizeof(ThreadLocalTop));
// This might be just paranoia, but it seems to be needed in case a
// thread_local_top_ is restored on a separate OS thread.
#ifdef USE_SIMULATOR
  thread_local_top()->simulator_ = Simulator::current(this);
#endif
  DCHECK(context() == nullptr || context()->IsContext());
  return from + sizeof(ThreadLocalTop);
}

Isolate::ThreadDataTable::ThreadDataTable() : list_(nullptr) {}

Isolate::ThreadDataTable::~ThreadDataTable() {
  // TODO(svenpanne) The assertion below would fire if an embedder does not
  // cleanly dispose all Isolates before disposing v8, so we are conservative
  // and leave it out for now.
  // DCHECK_NULL(list_);
}

void Isolate::ReleaseManagedObjects() {
  Isolate::ManagedObjectFinalizer* current =
      managed_object_finalizers_list_.next_;
  managed_object_finalizers_list_.next_ = nullptr;
  while (current != nullptr) {
    Isolate::ManagedObjectFinalizer* next = current->next_;
    current->Dispose();
    current = next;
  }
  // No new managed objects should pop up during finalization.
  DCHECK_NULL(managed_object_finalizers_list_.next_);
}

void Isolate::RegisterForReleaseAtTeardown(
    Isolate::ManagedObjectFinalizer* finalizer) {
  DCHECK_NOT_NULL(finalizer->value_);
  DCHECK_NOT_NULL(finalizer->deleter_);
  DCHECK_NULL(finalizer->prev_);
  DCHECK_NULL(finalizer->next_);

  // Insert at head. We keep the head alive for the lifetime of the Isolate
  // because otherwise we can't reset the head, should we delete it before
  // the isolate expires
  Isolate::ManagedObjectFinalizer* next = managed_object_finalizers_list_.next_;
  managed_object_finalizers_list_.next_ = finalizer;
  finalizer->prev_ = &managed_object_finalizers_list_;
  finalizer->next_ = next;
  if (next != nullptr) next->prev_ = finalizer;
}

void Isolate::UnregisterFromReleaseAtTeardown(
    Isolate::ManagedObjectFinalizer* finalizer) {
  DCHECK_NOT_NULL(finalizer);
  DCHECK_NOT_NULL(finalizer->prev_);

  finalizer->prev_->next_ = finalizer->next_;
  if (finalizer->next_ != nullptr) finalizer->next_->prev_ = finalizer->prev_;
}

Isolate::PerIsolateThreadData::~PerIsolateThreadData() {
#if defined(USE_SIMULATOR)
  delete simulator_;
#endif
}


Isolate::PerIsolateThreadData*
    Isolate::ThreadDataTable::Lookup(Isolate* isolate,
                                     ThreadId thread_id) {
  for (PerIsolateThreadData* data = list_; data != nullptr;
       data = data->next_) {
    if (data->Matches(isolate, thread_id)) return data;
  }
  return nullptr;
}


void Isolate::ThreadDataTable::Insert(Isolate::PerIsolateThreadData* data) {
  if (list_ != nullptr) list_->prev_ = data;
  data->next_ = list_;
  list_ = data;
}


void Isolate::ThreadDataTable::Remove(PerIsolateThreadData* data) {
  if (list_ == data) list_ = data->next_;
  if (data->next_ != nullptr) data->next_->prev_ = data->prev_;
  if (data->prev_ != nullptr) data->prev_->next_ = data->next_;
  delete data;
}


void Isolate::ThreadDataTable::RemoveAllThreads(Isolate* isolate) {
  PerIsolateThreadData* data = list_;
  while (data != nullptr) {
    PerIsolateThreadData* next = data->next_;
    if (data->isolate() == isolate) Remove(data);
    data = next;
  }
}


#ifdef DEBUG
#define TRACE_ISOLATE(tag)                                              \
  do {                                                                  \
    if (FLAG_trace_isolates) {                                          \
      PrintF("Isolate %p (id %d)" #tag "\n",                            \
             reinterpret_cast<void*>(this), id());                      \
    }                                                                   \
  } while (false)
#else
#define TRACE_ISOLATE(tag)
#endif

class VerboseAccountingAllocator : public AccountingAllocator {
 public:
  VerboseAccountingAllocator(Heap* heap, size_t allocation_sample_bytes,
                             size_t pool_sample_bytes)
      : heap_(heap),
        last_memory_usage_(0),
        last_pool_size_(0),
        nesting_deepth_(0),
        allocation_sample_bytes_(allocation_sample_bytes),
        pool_sample_bytes_(pool_sample_bytes) {}

  v8::internal::Segment* GetSegment(size_t size) override {
    v8::internal::Segment* memory = AccountingAllocator::GetSegment(size);
    if (memory) {
      size_t malloced_current = GetCurrentMemoryUsage();
      size_t pooled_current = GetCurrentPoolSize();

      if (last_memory_usage_.Value() + allocation_sample_bytes_ <
              malloced_current ||
          last_pool_size_.Value() + pool_sample_bytes_ < pooled_current) {
        PrintMemoryJSON(malloced_current, pooled_current);
        last_memory_usage_.SetValue(malloced_current);
        last_pool_size_.SetValue(pooled_current);
      }
    }
    return memory;
  }

  void ReturnSegment(v8::internal::Segment* memory) override {
    AccountingAllocator::ReturnSegment(memory);
    size_t malloced_current = GetCurrentMemoryUsage();
    size_t pooled_current = GetCurrentPoolSize();

    if (malloced_current + allocation_sample_bytes_ <
            last_memory_usage_.Value() ||
        pooled_current + pool_sample_bytes_ < last_pool_size_.Value()) {
      PrintMemoryJSON(malloced_current, pooled_current);
      last_memory_usage_.SetValue(malloced_current);
      last_pool_size_.SetValue(pooled_current);
    }
  }

  void ZoneCreation(const Zone* zone) override {
    PrintZoneModificationSample(zone, "zonecreation");
    nesting_deepth_.Increment(1);
  }

  void ZoneDestruction(const Zone* zone) override {
    nesting_deepth_.Decrement(1);
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
        "\"size\": %" PRIuS
        ","
        "\"nesting\": %" PRIuS "}\n",
        type, reinterpret_cast<void*>(heap_->isolate()),
        heap_->isolate()->time_millis_since_init(),
        reinterpret_cast<const void*>(zone), zone->name(),
        zone->allocation_size(), nesting_deepth_.Value());
  }

  void PrintMemoryJSON(size_t malloced, size_t pooled) {
    // Note: Neither isolate, nor heap is locked, so be careful with accesses
    // as the allocator is potentially used on a concurrent thread.
    double time = heap_->isolate()->time_millis_since_init();
    PrintF(
        "{"
        "\"type\": \"zone\", "
        "\"isolate\": \"%p\", "
        "\"time\": %f, "
        "\"allocated\": %" PRIuS
        ","
        "\"pooled\": %" PRIuS "}\n",
        reinterpret_cast<void*>(heap_->isolate()), time, malloced, pooled);
  }

  Heap* heap_;
  base::AtomicNumber<size_t> last_memory_usage_;
  base::AtomicNumber<size_t> last_pool_size_;
  base::AtomicNumber<size_t> nesting_deepth_;
  size_t allocation_sample_bytes_, pool_sample_bytes_;
};

#ifdef DEBUG
base::AtomicNumber<size_t> Isolate::non_disposed_isolates_;
#endif  // DEBUG

Isolate::Isolate(bool enable_serializer)
    : embedder_data_(),
      entry_stack_(nullptr),
      stack_trace_nesting_level_(0),
      incomplete_message_(nullptr),
      bootstrapper_(nullptr),
      runtime_profiler_(nullptr),
      compilation_cache_(nullptr),
      logger_(nullptr),
      load_stub_cache_(nullptr),
      store_stub_cache_(nullptr),
      deoptimizer_data_(nullptr),
      deoptimizer_lazy_throw_(false),
      materialized_object_store_(nullptr),
      capture_stack_trace_for_uncaught_exceptions_(false),
      stack_trace_for_uncaught_exceptions_frame_limit_(0),
      stack_trace_for_uncaught_exceptions_options_(StackTrace::kOverview),
      context_slot_cache_(nullptr),
      descriptor_lookup_cache_(nullptr),
      handle_scope_implementer_(nullptr),
      unicode_cache_(nullptr),
      allocator_(FLAG_trace_gc_object_stats ? new VerboseAccountingAllocator(
                                                  &heap_, 256 * KB, 128 * KB)
                                            : new AccountingAllocator()),
      inner_pointer_to_code_cache_(nullptr),
      global_handles_(nullptr),
      eternal_handles_(nullptr),
      thread_manager_(nullptr),
      setup_delegate_(nullptr),
      regexp_stack_(nullptr),
      date_cache_(nullptr),
      call_descriptor_data_(nullptr),
      // TODO(bmeurer) Initialized lazily because it depends on flags; can
      // be fixed once the default isolate cleanup is done.
      random_number_generator_(nullptr),
      fuzzer_rng_(nullptr),
      rail_mode_(PERFORMANCE_ANIMATION),
      promise_hook_or_debug_is_active_(false),
      promise_hook_(nullptr),
      load_start_time_ms_(0),
      serializer_enabled_(enable_serializer),
      has_fatal_error_(false),
      initialized_from_snapshot_(false),
      is_tail_call_elimination_enabled_(true),
      is_isolate_in_background_(false),
      cpu_profiler_(nullptr),
      heap_profiler_(nullptr),
      code_event_dispatcher_(new CodeEventDispatcher()),
      function_entry_hook_(nullptr),
      deferred_handles_head_(nullptr),
      optimizing_compile_dispatcher_(nullptr),
      stress_deopt_count_(0),
      force_slow_path_(false),
      next_optimization_id_(0),
#if V8_SFI_HAS_UNIQUE_ID
      next_unique_sfi_id_(0),
#endif
      is_running_microtasks_(false),
      use_counter_callback_(nullptr),
      basic_block_profiler_(nullptr),
      cancelable_task_manager_(new CancelableTaskManager()),
      wasm_compilation_manager_(new wasm::CompilationManager()),
      abort_on_uncaught_exception_callback_(nullptr),
      total_regexp_code_generated_(0) {
  {
    base::LockGuard<base::Mutex> lock_guard(thread_data_table_mutex_.Pointer());
    CHECK(thread_data_table_);
  }
  id_ = base::Relaxed_AtomicIncrement(&isolate_counter_, 1);
  TRACE_ISOLATE(constructor);

  memset(isolate_addresses_, 0,
      sizeof(isolate_addresses_[0]) * (kIsolateAddressCount + 1));

  heap_.isolate_ = this;
  stack_guard_.isolate_ = this;

  // ThreadManager is initialized early to support locking an isolate
  // before it is entered.
  thread_manager_ = new ThreadManager();
  thread_manager_->isolate_ = this;

#ifdef DEBUG
  // heap_histograms_ initializes itself.
  memset(&js_spill_information_, 0, sizeof(js_spill_information_));

  non_disposed_isolates_.Increment(1);
#endif  // DEBUG

  handle_scope_data_.Initialize();

#define ISOLATE_INIT_EXECUTE(type, name, initial_value)                        \
  name##_ = (initial_value);
  ISOLATE_INIT_LIST(ISOLATE_INIT_EXECUTE)
#undef ISOLATE_INIT_EXECUTE

#define ISOLATE_INIT_ARRAY_EXECUTE(type, name, length)                         \
  memset(name##_, 0, sizeof(type) * length);
  ISOLATE_INIT_ARRAY_LIST(ISOLATE_INIT_ARRAY_EXECUTE)
#undef ISOLATE_INIT_ARRAY_EXECUTE

  InitializeLoggingAndCounters();
  debug_ = new Debug(this);

  init_memcopy_functions(this);
}


void Isolate::TearDown() {
  TRACE_ISOLATE(tear_down);

  // Temporarily set this isolate as current so that various parts of
  // the isolate can access it in their destructors without having a
  // direct pointer. We don't use Enter/Exit here to avoid
  // initializing the thread data.
  PerIsolateThreadData* saved_data = CurrentPerIsolateThreadData();
  DCHECK_EQ(base::Relaxed_Load(&isolate_key_created_), 1);
  Isolate* saved_isolate =
      reinterpret_cast<Isolate*>(base::Thread::GetThreadLocal(isolate_key_));
  SetIsolateThreadLocals(this, nullptr);

  Deinit();

  {
    base::LockGuard<base::Mutex> lock_guard(thread_data_table_mutex_.Pointer());
    thread_data_table_->RemoveAllThreads(this);
  }

#ifdef DEBUG
  non_disposed_isolates_.Decrement(1);
#endif  // DEBUG

  delete this;

  // Restore the previous current isolate.
  SetIsolateThreadLocals(saved_isolate, saved_data);
}


void Isolate::GlobalTearDown() {
  delete thread_data_table_;
  thread_data_table_ = nullptr;
}


void Isolate::ClearSerializerData() {
  delete external_reference_table_;
  external_reference_table_ = nullptr;
  delete external_reference_map_;
  external_reference_map_ = nullptr;
}


void Isolate::Deinit() {
  TRACE_ISOLATE(deinit);

  debug()->Unload();

  if (concurrent_recompilation_enabled()) {
    optimizing_compile_dispatcher_->Stop();
    delete optimizing_compile_dispatcher_;
    optimizing_compile_dispatcher_ = nullptr;
  }

  wasm_compilation_manager_->TearDown();

  heap_.mark_compact_collector()->EnsureSweepingCompleted();
  heap_.memory_allocator()->unmapper()->WaitUntilCompleted();

  DumpAndResetStats();

  if (FLAG_print_deopt_stress) {
    PrintF(stdout, "=== Stress deopt counter: %u\n", stress_deopt_count_);
  }

  if (cpu_profiler_) {
    cpu_profiler_->DeleteAllProfiles();
  }

  // We must stop the logger before we tear down other components.
  sampler::Sampler* sampler = logger_->sampler();
  if (sampler && sampler->IsActive()) sampler->Stop();

  FreeThreadResources();
  // Release managed objects before shutting down the heap. The finalizer might
  // need to access heap objects.
  ReleaseManagedObjects();

  delete deoptimizer_data_;
  deoptimizer_data_ = nullptr;
  builtins_.TearDown();
  bootstrapper_->TearDown();

  if (runtime_profiler_ != nullptr) {
    delete runtime_profiler_;
    runtime_profiler_ = nullptr;
  }

  delete basic_block_profiler_;
  basic_block_profiler_ = nullptr;

  delete heap_profiler_;
  heap_profiler_ = nullptr;

  compiler_dispatcher_->AbortAll(CompilerDispatcher::BlockingBehavior::kBlock);
  delete compiler_dispatcher_;
  compiler_dispatcher_ = nullptr;

  cancelable_task_manager()->CancelAndWait();

  heap_.TearDown();
  logger_->TearDown();

  delete interpreter_;
  interpreter_ = nullptr;

  delete ast_string_constants_;
  ast_string_constants_ = nullptr;

  delete cpu_profiler_;
  cpu_profiler_ = nullptr;

  code_event_dispatcher_.reset();

  delete root_index_map_;
  root_index_map_ = nullptr;

  ClearSerializerData();
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

  delete unicode_cache_;
  unicode_cache_ = nullptr;

  delete date_cache_;
  date_cache_ = nullptr;

  delete[] call_descriptor_data_;
  call_descriptor_data_ = nullptr;

  delete regexp_stack_;
  regexp_stack_ = nullptr;

  delete descriptor_lookup_cache_;
  descriptor_lookup_cache_ = nullptr;
  delete context_slot_cache_;
  context_slot_cache_ = nullptr;

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

#if USE_SIMULATOR
  Simulator::TearDown(simulator_i_cache_, simulator_redirection_);
  simulator_i_cache_ = nullptr;
  simulator_redirection_ = nullptr;
#endif
}


void Isolate::InitializeThreadLocal() {
  thread_local_top_.isolate_ = this;
  thread_local_top_.Initialize();
}


bool Isolate::PropagatePendingExceptionToExternalTryCatch() {
  Object* exception = pending_exception();

  if (IsJavaScriptHandlerOnTop(exception)) {
    thread_local_top_.external_caught_exception_ = false;
    return false;
  }

  if (!IsExternalHandlerOnTop(exception)) {
    thread_local_top_.external_caught_exception_ = false;
    return true;
  }

  thread_local_top_.external_caught_exception_ = true;
  if (!is_catchable_by_javascript(exception)) {
    try_catch_handler()->can_continue_ = false;
    try_catch_handler()->has_terminated_ = true;
    try_catch_handler()->exception_ = heap()->null_value();
  } else {
    v8::TryCatch* handler = try_catch_handler();
    DCHECK(thread_local_top_.pending_message_obj_->IsJSMessageObject() ||
           thread_local_top_.pending_message_obj_->IsTheHole(this));
    handler->can_continue_ = true;
    handler->has_terminated_ = false;
    handler->exception_ = pending_exception();
    // Propagate to the external try-catch only if we got an actual message.
    if (thread_local_top_.pending_message_obj_->IsTheHole(this)) return true;

    handler->message_obj_ = thread_local_top_.pending_message_obj_;
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
void PrintBuiltinSizes(Isolate* isolate) {
  Builtins* builtins = isolate->builtins();
  for (int i = 0; i < Builtins::builtin_count; i++) {
    const char* name = builtins->name(i);
    const char* kind = Builtins::KindNameOf(i);
    Code* code = builtins->builtin(i);
    PrintF(stdout, "%s Builtin, %s, %d\n", kind, name,
           code->instruction_size());
  }
}
}  // namespace

bool Isolate::Init(StartupDeserializer* des) {
  TRACE_ISOLATE(init);

  time_millis_at_init_ = heap_.MonotonicallyIncreasingTimeInMs();

  stress_deopt_count_ = FLAG_deopt_every_n_times;
  force_slow_path_ = FLAG_force_slow_path;

  has_fatal_error_ = false;

  if (function_entry_hook() != nullptr) {
    // When function entry hooking is in effect, we have to create the code
    // stubs from scratch to get entry hooks, rather than loading the previously
    // generated stubs from disk.
    // If this assert fires, the initialization path has regressed.
    DCHECK_NULL(des);
  }

  // The initialization process does not handle memory exhaustion.
  AlwaysAllocateScope always_allocate(this);

  // Safe after setting Heap::isolate_, and initializing StackGuard
  heap_.SetStackLimits();

#define ASSIGN_ELEMENT(CamelName, hacker_name)                  \
  isolate_addresses_[IsolateAddressId::k##CamelName##Address] = \
      reinterpret_cast<Address>(hacker_name##_address());
  FOR_EACH_ISOLATE_ADDRESS_NAME(ASSIGN_ELEMENT)
#undef ASSIGN_ELEMENT

  compilation_cache_ = new CompilationCache(this);
  context_slot_cache_ = new ContextSlotCache();
  descriptor_lookup_cache_ = new DescriptorLookupCache();
  unicode_cache_ = new UnicodeCache();
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
  call_descriptor_data_ =
      new CallInterfaceDescriptorData[CallDescriptors::NUMBER_OF_DESCRIPTORS];
  cpu_profiler_ = new CpuProfiler(this);
  heap_profiler_ = new HeapProfiler(heap());
  interpreter_ = new interpreter::Interpreter(this);
  compiler_dispatcher_ =
      new CompilerDispatcher(this, V8::GetCurrentPlatform(), FLAG_stack_size);

  // Enable logging before setting up the heap
  logger_->SetUp(this);

  // Initialize other runtime facilities
#if defined(USE_SIMULATOR)
#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_S390
  Simulator::Initialize(this);
#endif
#endif

  { // NOLINT
    // Ensure that the thread has a valid stack guard.  The v8::Locker object
    // will ensure this too, but we don't have to use lockers if we are only
    // using one thread.
    ExecutionAccess lock(this);
    stack_guard_.InitThread(lock);
  }

  // SetUp the object heap.
  DCHECK(!heap_.HasBeenSetUp());
  if (!heap_.SetUp()) {
    V8::FatalProcessOutOfMemory("heap setup");
    return false;
  }

  // Setup the wasm code manager. Currently, there's one per Isolate.
  if (!wasm_code_manager_) {
    size_t max_code_size = kMaxWasmCodeMemory;
    if (kRequiresCodeRange) {
      max_code_size = std::min(max_code_size,
                               heap_.memory_allocator()->code_range()->size());
    }
    wasm_code_manager_.reset(new wasm::WasmCodeManager(
        reinterpret_cast<v8::Isolate*>(this), max_code_size));
  }

// Initialize the interface descriptors ahead of time.
#define INTERFACE_DESCRIPTOR(Name, ...) \
  { Name##Descriptor(this); }
  INTERFACE_DESCRIPTOR_LIST(INTERFACE_DESCRIPTOR)
#undef INTERFACE_DESCRIPTOR

  deoptimizer_data_ = new DeoptimizerData(heap());

  const bool create_heap_objects = (des == nullptr);
  if (setup_delegate_ == nullptr) {
    setup_delegate_ = new SetupIsolateDelegate(create_heap_objects);
  }

  if (!setup_delegate_->SetupHeap(&heap_)) {
    V8::FatalProcessOutOfMemory("heap object creation");
    return false;
  }

  if (create_heap_objects) {
    // Terminate the partial snapshot cache so we can iterate.
    partial_snapshot_cache_.push_back(heap_.undefined_value());
  }

  InitializeThreadLocal();

  bootstrapper_->Initialize(create_heap_objects);
  setup_delegate_->SetupBuiltins(this);
  if (create_heap_objects) heap_.CreateFixedStubs();

  if (FLAG_log_internal_timer_events) {
    set_event_logger(Logger::DefaultEventLoggerSentinel);
  }

  if (FLAG_trace_turbo || FLAG_trace_turbo_graph) {
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

    if (!create_heap_objects) des->DeserializeInto(this);
    load_stub_cache_->Initialize();
    store_stub_cache_->Initialize();
    setup_delegate_->SetupInterpreter(interpreter_);

    heap_.NotifyDeserializationComplete();
  }
  delete setup_delegate_;
  setup_delegate_ = nullptr;

  if (FLAG_print_builtin_size) PrintBuiltinSizes(this);

  // Finish initialization of ThreadLocal after deserialization is done.
  clear_pending_exception();
  clear_pending_message();
  clear_scheduled_exception();

  // Deserializing may put strange things in the root array's copy of the
  // stack guard.
  heap_.SetStackLimits();

  // Quiet the heap NaN if needed on target platform.
  if (!create_heap_objects) Assembler::QuietNaN(heap_.nan_value());

  if (FLAG_trace_turbo) {
    // Create an empty file.
    std::ofstream(GetTurboCfgFileName().c_str(), std::ios_base::trunc);
  }

  CHECK_EQ(static_cast<int>(OFFSET_OF(Isolate, embedder_data_)),
           Internals::kIsolateEmbedderDataOffset);
  CHECK_EQ(static_cast<int>(OFFSET_OF(Isolate, heap_.roots_)),
           Internals::kIsolateRootsOffset);
  CHECK_EQ(static_cast<int>(OFFSET_OF(Isolate, heap_.external_memory_)),
           Internals::kExternalMemoryOffset);
  CHECK_EQ(static_cast<int>(OFFSET_OF(Isolate, heap_.external_memory_limit_)),
           Internals::kExternalMemoryLimitOffset);
  CHECK_EQ(static_cast<int>(
               OFFSET_OF(Isolate, heap_.external_memory_at_last_mark_compact_)),
           Internals::kExternalMemoryAtLastMarkCompactOffset);

  {
    HandleScope scope(this);
    ast_string_constants_ = new AstStringConstants(this, heap()->HashSeed());
  }

  if (!serializer_enabled()) {
    // Ensure that all stubs which need to be generated ahead of time, but
    // cannot be serialized into the snapshot have been generated.
    HandleScope scope(this);
    CodeStub::GenerateFPStubs(this);
  }

  initialized_from_snapshot_ = (des != nullptr);

  if (!FLAG_inline_new) heap_.DisableInlineAllocation();

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
             entry_stack_->previous_thread_data->thread_id().Equals(
                 ThreadId::Current()));
      // Same thread re-enters the isolate, no need to re-init anything.
      entry_stack_->entry_count++;
      return;
    }
  }

  PerIsolateThreadData* data = FindOrAllocatePerThreadDataForThisThread();
  DCHECK_NOT_NULL(data);
  DCHECK(data->isolate_ == this);

  EntryStackItem* item = new EntryStackItem(current_data,
                                            current_isolate,
                                            entry_stack_);
  entry_stack_ = item;

  SetIsolateThreadLocals(this, data);

  // In case it's the first time some thread enters the isolate.
  set_thread_id(data->thread_id());
}


void Isolate::Exit() {
  DCHECK_NOT_NULL(entry_stack_);
  DCHECK(entry_stack_->previous_thread_data == nullptr ||
         entry_stack_->previous_thread_data->thread_id().Equals(
             ThreadId::Current()));

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

    OFStream os(stdout);
    if (FLAG_turbo_stats) {
      AsPrintableStatistics ps = {*turbo_statistics(), false};
      os << ps << std::endl;
    }
    if (FLAG_turbo_stats_nvp) {
      AsPrintableStatistics ps = {*turbo_statistics(), true};
      os << ps << std::endl;
    }
  }
  delete turbo_statistics_;
  turbo_statistics_ = nullptr;
  if (V8_UNLIKELY(FLAG_runtime_stats ==
                  v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE)) {
    counters()->runtime_call_stats()->Print();
    counters()->runtime_call_stats()->Reset();
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
  return FLAG_opt && !serializer_enabled_ &&
         CpuFeatures::SupportsCrankshaft() &&
         !is_precise_count_code_coverage() && !is_block_count_code_coverage();
}

bool Isolate::NeedsSourcePositionsForProfiling() const {
  return FLAG_trace_deopt || FLAG_trace_turbo || FLAG_trace_turbo_graph ||
         FLAG_turbo_profiling || FLAG_perf_prof || is_profiling() ||
         debug_->is_active() || logger_->is_logging();
}

void Isolate::SetFeedbackVectorsForProfilingTools(Object* value) {
  DCHECK(value->IsUndefined(this) || value->IsArrayList());
  heap()->set_feedback_vectors_for_profiling_tools(value);
}

void Isolate::InitializeVectorListFromHeap() {
  // Collect existing feedback vectors.
  std::vector<Handle<FeedbackVector>> vectors;
  {
    HeapIterator heap_iterator(heap());
    while (HeapObject* current_obj = heap_iterator.next()) {
      if (current_obj->IsSharedFunctionInfo()) {
        SharedFunctionInfo* shared = SharedFunctionInfo::cast(current_obj);
        shared->set_has_reported_binary_coverage(false);
      } else if (current_obj->IsFeedbackVector()) {
        FeedbackVector* vector = FeedbackVector::cast(current_obj);
        SharedFunctionInfo* shared = vector->shared_function_info();
        if (!shared->IsSubjectToDebugging()) continue;
        vector->clear_invocation_count();
        vectors.emplace_back(vector, this);
      }
    }
  }

  // Add collected feedback vectors to the root list lest we lose them to
  // GC.
  Handle<ArrayList> list =
      ArrayList::New(this, static_cast<int>(vectors.size()));
  for (const auto& vector : vectors) list = ArrayList::Add(list, vector);
  SetFeedbackVectorsForProfilingTools(*list);
}

bool Isolate::IsArrayOrObjectOrStringPrototype(Object* object) {
  Object* context = heap()->native_contexts_list();
  while (!context->IsUndefined(this)) {
    Context* current_context = Context::cast(context);
    if (current_context->initial_object_prototype() == object ||
        current_context->initial_array_prototype() == object ||
        current_context->initial_string_prototype() == object) {
      return true;
    }
    context = current_context->next_context_link();
  }
  return false;
}

bool Isolate::IsInAnyContext(Object* object, uint32_t index) {
  DisallowHeapAllocation no_gc;
  Object* context = heap()->native_contexts_list();
  while (!context->IsUndefined(this)) {
    Context* current_context = Context::cast(context);
    if (current_context->get(index) == object) {
      return true;
    }
    context = current_context->next_context_link();
  }
  return false;
}

bool Isolate::IsNoElementsProtectorIntact(Context* context) {
  PropertyCell* no_elements_cell = heap()->no_elements_protector();
  bool cell_reports_intact =
      no_elements_cell->value()->IsSmi() &&
      Smi::ToInt(no_elements_cell->value()) == kProtectorValid;

#ifdef DEBUG
  Context* native_context = context->native_context();

  Map* root_array_map =
      native_context->GetInitialJSArrayMap(GetInitialFastElementsKind());
  JSObject* initial_array_proto = JSObject::cast(
      native_context->get(Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
  JSObject* initial_object_proto = JSObject::cast(
      native_context->get(Context::INITIAL_OBJECT_PROTOTYPE_INDEX));
  JSObject* initial_string_proto = JSObject::cast(
      native_context->get(Context::INITIAL_STRING_PROTOTYPE_INDEX));

  if (root_array_map == nullptr ||
      initial_array_proto == initial_object_proto) {
    // We are in the bootstrapping process, and the entire check sequence
    // shouldn't be performed.
    return cell_reports_intact;
  }

  // Check that the array prototype hasn't been altered WRT empty elements.
  if (root_array_map->prototype() != initial_array_proto) {
    DCHECK_EQ(false, cell_reports_intact);
    return cell_reports_intact;
  }

  FixedArrayBase* elements = initial_array_proto->elements();
  if (elements != heap()->empty_fixed_array() &&
      elements != heap()->empty_slow_element_dictionary()) {
    DCHECK_EQ(false, cell_reports_intact);
    return cell_reports_intact;
  }

  // Check that the Object.prototype hasn't been altered WRT empty elements.
  elements = initial_object_proto->elements();
  if (elements != heap()->empty_fixed_array() &&
      elements != heap()->empty_slow_element_dictionary()) {
    DCHECK_EQ(false, cell_reports_intact);
    return cell_reports_intact;
  }

  // Check that the Array.prototype has the Object.prototype as its
  // [[Prototype]] and that the Object.prototype has a null [[Prototype]].
  PrototypeIterator iter(this, initial_array_proto);
  if (iter.IsAtEnd() || iter.GetCurrent() != initial_object_proto) {
    DCHECK_EQ(false, cell_reports_intact);
    DCHECK(!has_pending_exception());
    return cell_reports_intact;
  }
  iter.Advance();
  if (!iter.IsAtEnd()) {
    DCHECK_EQ(false, cell_reports_intact);
    DCHECK(!has_pending_exception());
    return cell_reports_intact;
  }
  DCHECK(!has_pending_exception());

  // Check that the String.prototype hasn't been altered WRT empty elements.
  elements = initial_string_proto->elements();
  if (elements != heap()->empty_fixed_array() &&
      elements != heap()->empty_slow_element_dictionary()) {
    DCHECK_EQ(false, cell_reports_intact);
    return cell_reports_intact;
  }

  // Check that the String.prototype has the Object.prototype
  // as its [[Prototype]] still.
  if (initial_string_proto->map()->prototype() != initial_object_proto) {
    DCHECK_EQ(false, cell_reports_intact);
    return cell_reports_intact;
  }
#endif

  return cell_reports_intact;
}

bool Isolate::IsNoElementsProtectorIntact() {
  return Isolate::IsNoElementsProtectorIntact(context());
}

bool Isolate::IsIsConcatSpreadableLookupChainIntact() {
  Cell* is_concat_spreadable_cell = heap()->is_concat_spreadable_protector();
  bool is_is_concat_spreadable_set =
      Smi::ToInt(is_concat_spreadable_cell->value()) == kProtectorInvalid;
#ifdef DEBUG
  Map* root_array_map =
      raw_native_context()->GetInitialJSArrayMap(GetInitialFastElementsKind());
  if (root_array_map == nullptr) {
    // Ignore the value of is_concat_spreadable during bootstrap.
    return !is_is_concat_spreadable_set;
  }
  Handle<Object> array_prototype(array_function()->prototype(), this);
  Handle<Symbol> key = factory()->is_concat_spreadable_symbol();
  Handle<Object> value;
  LookupIterator it(array_prototype, key);
  if (it.IsFound() && !JSReceiver::GetDataProperty(&it)->IsUndefined(this)) {
    // TODO(cbruni): Currently we do not revert if we unset the
    // @@isConcatSpreadable property on Array.prototype or Object.prototype
    // hence the reverse implication doesn't hold.
    DCHECK(is_is_concat_spreadable_set);
    return false;
  }
#endif  // DEBUG

  return !is_is_concat_spreadable_set;
}

bool Isolate::IsIsConcatSpreadableLookupChainIntact(JSReceiver* receiver) {
  if (!IsIsConcatSpreadableLookupChainIntact()) return false;
  return !receiver->HasProxyInPrototype(this);
}

void Isolate::UpdateNoElementsProtectorOnSetElement(Handle<JSObject> object) {
  DisallowHeapAllocation no_gc;
  if (!object->map()->is_prototype_map()) return;
  if (!IsNoElementsProtectorIntact()) return;
  if (!IsArrayOrObjectOrStringPrototype(*object)) return;
  PropertyCell::SetValueWithInvalidation(
      factory()->no_elements_protector(),
      handle(Smi::FromInt(kProtectorInvalid), this));
}

void Isolate::InvalidateIsConcatSpreadableProtector() {
  DCHECK(factory()->is_concat_spreadable_protector()->value()->IsSmi());
  DCHECK(IsIsConcatSpreadableLookupChainIntact());
  factory()->is_concat_spreadable_protector()->set_value(
      Smi::FromInt(kProtectorInvalid));
  DCHECK(!IsIsConcatSpreadableLookupChainIntact());
}

void Isolate::InvalidateArrayConstructorProtector() {
  DCHECK(factory()->array_constructor_protector()->value()->IsSmi());
  DCHECK(IsArrayConstructorIntact());
  factory()->array_constructor_protector()->set_value(
      Smi::FromInt(kProtectorInvalid));
  DCHECK(!IsArrayConstructorIntact());
}

void Isolate::InvalidateArraySpeciesProtector() {
  DCHECK(factory()->species_protector()->value()->IsSmi());
  DCHECK(IsArraySpeciesLookupChainIntact());
  factory()->species_protector()->set_value(Smi::FromInt(kProtectorInvalid));
  DCHECK(!IsArraySpeciesLookupChainIntact());
}

void Isolate::InvalidateStringLengthOverflowProtector() {
  DCHECK(factory()->string_length_protector()->value()->IsSmi());
  DCHECK(IsStringLengthOverflowIntact());
  factory()->string_length_protector()->set_value(
      Smi::FromInt(kProtectorInvalid));
  DCHECK(!IsStringLengthOverflowIntact());
}

void Isolate::InvalidateArrayIteratorProtector() {
  DCHECK(factory()->array_iterator_protector()->value()->IsSmi());
  DCHECK(IsArrayIteratorLookupChainIntact());
  PropertyCell::SetValueWithInvalidation(
      factory()->array_iterator_protector(),
      handle(Smi::FromInt(kProtectorInvalid), this));
  DCHECK(!IsArrayIteratorLookupChainIntact());
}

void Isolate::InvalidateArrayBufferNeuteringProtector() {
  DCHECK(factory()->array_buffer_neutering_protector()->value()->IsSmi());
  DCHECK(IsArrayBufferNeuteringIntact());
  PropertyCell::SetValueWithInvalidation(
      factory()->array_buffer_neutering_protector(),
      handle(Smi::FromInt(kProtectorInvalid), this));
  DCHECK(!IsArrayBufferNeuteringIntact());
}

bool Isolate::IsAnyInitialArrayPrototype(Handle<JSArray> array) {
  DisallowHeapAllocation no_gc;
  return IsInAnyContext(*array, Context::INITIAL_ARRAY_PROTOTYPE_INDEX);
}


CallInterfaceDescriptorData* Isolate::call_descriptor_data(int index) {
  DCHECK(0 <= index && index < CallDescriptors::NUMBER_OF_DESCRIPTORS);
  return &call_descriptor_data_[index];
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
  return ensure_rng_exists(&random_number_generator_, FLAG_random_seed);
}

base::RandomNumberGenerator* Isolate::fuzzer_rng() {
  return ensure_rng_exists(&fuzzer_rng_, FLAG_fuzzer_random_seed);
}

int Isolate::GenerateIdentityHash(uint32_t mask) {
  int hash;
  int attempts = 0;
  do {
    hash = random_number_generator()->NextInt() & mask;
  } while (hash == 0 && attempts++ < 30);
  return hash != 0 ? hash : 1;
}

Code* Isolate::FindCodeObject(Address a) {
  return heap()->GcSafeFindCodeForInnerPointer(a);
}


#ifdef DEBUG
#define ISOLATE_FIELD_OFFSET(type, name, ignored)                       \
const intptr_t Isolate::name##_debug_offset_ = OFFSET_OF(Isolate, name##_);
ISOLATE_INIT_LIST(ISOLATE_FIELD_OFFSET)
ISOLATE_INIT_ARRAY_LIST(ISOLATE_FIELD_OFFSET)
#undef ISOLATE_FIELD_OFFSET
#endif

Handle<Symbol> Isolate::SymbolFor(Heap::RootListIndex dictionary_index,
                                  Handle<String> name, bool private_symbol) {
  Handle<String> key = factory()->InternalizeString(name);
  Handle<NameDictionary> dictionary =
      Handle<NameDictionary>::cast(heap()->root_handle(dictionary_index));
  int entry = dictionary->FindEntry(key);
  Handle<Symbol> symbol;
  if (entry == NameDictionary::kNotFound) {
    symbol =
        private_symbol ? factory()->NewPrivateSymbol() : factory()->NewSymbol();
    symbol->set_name(*key);
    dictionary = NameDictionary::Add(dictionary, key, symbol,
                                     PropertyDetails::Empty(), &entry);
    switch (dictionary_index) {
      case Heap::kPublicSymbolTableRootIndex:
        symbol->set_is_public(true);
        heap()->set_public_symbol_table(*dictionary);
        break;
      case Heap::kApiSymbolTableRootIndex:
        heap()->set_api_symbol_table(*dictionary);
        break;
      case Heap::kApiPrivateSymbolTableRootIndex:
        heap()->set_api_private_symbol_table(*dictionary);
        break;
      default:
        UNREACHABLE();
    }
  } else {
    symbol = Handle<Symbol>(Symbol::cast(dictionary->ValueAt(entry)));
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

void Isolate::AddMicrotasksCompletedCallback(
    MicrotasksCompletedCallback callback) {
  auto pos = std::find(microtasks_completed_callbacks_.begin(),
                       microtasks_completed_callbacks_.end(), callback);
  if (pos != microtasks_completed_callbacks_.end()) return;
  microtasks_completed_callbacks_.push_back(callback);
}

void Isolate::RemoveMicrotasksCompletedCallback(
    MicrotasksCompletedCallback callback) {
  auto pos = std::find(microtasks_completed_callbacks_.begin(),
                       microtasks_completed_callbacks_.end(), callback);
  if (pos == microtasks_completed_callbacks_.end()) return;
  microtasks_completed_callbacks_.erase(pos);
}

void Isolate::FireCallCompletedCallback() {
  if (!handle_scope_implementer()->CallDepthIsZero()) return;

  bool run_microtasks =
      pending_microtask_count() &&
      !handle_scope_implementer()->HasMicrotasksSuppressions() &&
      handle_scope_implementer()->microtasks_policy() ==
          v8::MicrotasksPolicy::kAuto;

  if (run_microtasks) RunMicrotasks();

  if (call_completed_callbacks_.empty()) return;
  // Fire callbacks.  Increase call depth to prevent recursive callbacks.
  v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(this);
  v8::Isolate::SuppressMicrotaskExecutionScope suppress(isolate);
  for (auto& callback : call_completed_callbacks_) {
    callback(reinterpret_cast<v8::Isolate*>(this));
  }
}

void Isolate::DebugStateUpdated() {
  promise_hook_or_debug_is_active_ = promise_hook_ || debug()->is_active();
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
  v8::Local<v8::Context> api_context = v8::Utils::ToLocal(native_context());

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

void Isolate::SetHostImportModuleDynamicallyCallback(
    HostImportModuleDynamicallyCallback callback) {
  host_import_module_dynamically_callback_ = callback;
}

Handle<JSObject> Isolate::RunHostInitializeImportMetaObjectCallback(
    Handle<Module> module) {
  Handle<Object> host_meta(module->import_meta(), this);
  if (host_meta->IsTheHole(this)) {
    host_meta = factory()->NewJSObjectWithNullProto();
    if (host_initialize_import_meta_object_callback_ != nullptr) {
      v8::Local<v8::Context> api_context = v8::Utils::ToLocal(native_context());
      host_initialize_import_meta_object_callback_(
          api_context, Utils::ToLocal(module),
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

void Isolate::SetPromiseHook(PromiseHook hook) {
  promise_hook_ = hook;
  DebugStateUpdated();
}

void Isolate::RunPromiseHook(PromiseHookType type, Handle<JSPromise> promise,
                             Handle<Object> parent) {
  if (debug()->is_active()) debug()->RunPromiseHook(type, promise, parent);
  if (promise_hook_ == nullptr) return;
  promise_hook_(type, v8::Utils::PromiseToLocal(promise),
                v8::Utils::ToLocal(parent));
}

void Isolate::SetPromiseRejectCallback(PromiseRejectCallback callback) {
  promise_reject_callback_ = callback;
}

void Isolate::ReportPromiseReject(Handle<JSPromise> promise,
                                  Handle<Object> value,
                                  v8::PromiseRejectEvent event) {
  DCHECK_EQ(v8::Promise::kRejected, promise->status());
  if (promise_reject_callback_ == nullptr) return;
  Handle<FixedArray> stack_trace;
  if (event == v8::kPromiseRejectWithNoHandler && value->IsJSObject()) {
    stack_trace = GetDetailedStackTrace(Handle<JSObject>::cast(value));
  }
  promise_reject_callback_(v8::PromiseRejectMessage(
      v8::Utils::PromiseToLocal(promise), event, v8::Utils::ToLocal(value),
      v8::Utils::StackTraceToLocal(stack_trace)));
}

void Isolate::PromiseReactionJob(Handle<PromiseReactionJobInfo> info,
                                 MaybeHandle<Object>* result,
                                 MaybeHandle<Object>* maybe_exception) {
  Handle<Object> value(info->value(), this);
  Handle<Object> tasks(info->tasks(), this);
  Handle<JSFunction> promise_handle_fn = promise_handle();
  Handle<Object> undefined = factory()->undefined_value();
  Handle<Object> deferred_promise(info->deferred_promise(), this);

  if (deferred_promise->IsFixedArray()) {
    DCHECK(tasks->IsFixedArray());
    Handle<FixedArray> deferred_promise_arr =
        Handle<FixedArray>::cast(deferred_promise);
    Handle<FixedArray> deferred_on_resolve_arr(
        FixedArray::cast(info->deferred_on_resolve()), this);
    Handle<FixedArray> deferred_on_reject_arr(
        FixedArray::cast(info->deferred_on_reject()), this);
    Handle<FixedArray> tasks_arr = Handle<FixedArray>::cast(tasks);
    for (int i = 0; i < deferred_promise_arr->length(); i++) {
      Handle<Object> argv[] = {value, handle(tasks_arr->get(i), this),
                               handle(deferred_promise_arr->get(i), this),
                               handle(deferred_on_resolve_arr->get(i), this),
                               handle(deferred_on_reject_arr->get(i), this)};
      *result = Execution::TryCall(
          this, promise_handle_fn, undefined, arraysize(argv), argv,
          Execution::MessageHandling::kReport, maybe_exception);
      // If execution is terminating, just bail out.
      if (result->is_null() && maybe_exception->is_null()) {
        return;
      }
    }
  } else {
    Handle<Object> argv[] = {value, tasks, deferred_promise,
                             handle(info->deferred_on_resolve(), this),
                             handle(info->deferred_on_reject(), this)};
    *result = Execution::TryCall(
        this, promise_handle_fn, undefined, arraysize(argv), argv,
        Execution::MessageHandling::kReport, maybe_exception);
  }
}

void Isolate::PromiseResolveThenableJob(
    Handle<PromiseResolveThenableJobInfo> info, MaybeHandle<Object>* result,
    MaybeHandle<Object>* maybe_exception) {
  Handle<JSReceiver> thenable(info->thenable(), this);
  Handle<JSFunction> resolve(info->resolve(), this);
  Handle<JSFunction> reject(info->reject(), this);
  Handle<JSReceiver> then(info->then(), this);
  Handle<Object> argv[] = {resolve, reject};
  *result =
      Execution::TryCall(this, then, thenable, arraysize(argv), argv,
                         Execution::MessageHandling::kReport, maybe_exception);

  Handle<Object> reason;
  if (maybe_exception->ToHandle(&reason)) {
    DCHECK(result->is_null());
    Handle<Object> reason_arg[] = {reason};
    *result = Execution::TryCall(
        this, reject, factory()->undefined_value(), arraysize(reason_arg),
        reason_arg, Execution::MessageHandling::kReport, maybe_exception);
  }
}

void Isolate::EnqueueMicrotask(Handle<Object> microtask) {
  DCHECK(microtask->IsJSFunction() || microtask->IsCallHandlerInfo() ||
         microtask->IsPromiseResolveThenableJobInfo() ||
         microtask->IsPromiseReactionJobInfo());
  Handle<FixedArray> queue(heap()->microtask_queue(), this);
  int num_tasks = pending_microtask_count();
  DCHECK(num_tasks <= queue->length());
  if (num_tasks == 0) {
    queue = factory()->NewFixedArray(8);
    heap()->set_microtask_queue(*queue);
  } else if (num_tasks == queue->length()) {
    queue = factory()->CopyFixedArrayAndGrow(queue, num_tasks);
    heap()->set_microtask_queue(*queue);
  }
  DCHECK(queue->get(num_tasks)->IsUndefined(this));
  queue->set(num_tasks, *microtask);
  set_pending_microtask_count(num_tasks + 1);
}


void Isolate::RunMicrotasks() {
  // Increase call depth to prevent recursive callbacks.
  v8::Isolate::SuppressMicrotaskExecutionScope suppress(
      reinterpret_cast<v8::Isolate*>(this));
  is_running_microtasks_ = true;
  RunMicrotasksInternal();
  is_running_microtasks_ = false;
  FireMicrotasksCompletedCallback();
}


void Isolate::RunMicrotasksInternal() {
  if (!pending_microtask_count()) return;
  TRACE_EVENT0("v8.execute", "RunMicrotasks");
  TRACE_EVENT_CALL_STATS_SCOPED(this, "v8", "V8.RunMicrotasks");
  while (pending_microtask_count() > 0) {
    HandleScope scope(this);
    int num_tasks = pending_microtask_count();
    // Do not use factory()->microtask_queue() here; we need a fresh handle!
    Handle<FixedArray> queue(heap()->microtask_queue(), this);
    DCHECK(num_tasks <= queue->length());
    set_pending_microtask_count(0);
    heap()->set_microtask_queue(heap()->empty_fixed_array());

    Isolate* isolate = this;
    FOR_WITH_HANDLE_SCOPE(isolate, int, i = 0, i, i < num_tasks, i++, {
      Handle<Object> microtask(queue->get(i), this);

      if (microtask->IsCallHandlerInfo()) {
        Handle<CallHandlerInfo> callback_info =
            Handle<CallHandlerInfo>::cast(microtask);
        v8::MicrotaskCallback callback =
            v8::ToCData<v8::MicrotaskCallback>(callback_info->callback());
        void* data = v8::ToCData<void*>(callback_info->data());
        callback(data);
      } else {
        SaveContext save(this);
        Context* context;
        if (microtask->IsJSFunction()) {
          context = Handle<JSFunction>::cast(microtask)->context();
        } else if (microtask->IsPromiseResolveThenableJobInfo()) {
          context =
              Handle<PromiseResolveThenableJobInfo>::cast(microtask)->context();
        } else {
          context = Handle<PromiseReactionJobInfo>::cast(microtask)->context();
        }

        set_context(context->native_context());
        handle_scope_implementer_->EnterMicrotaskContext(
            Handle<Context>(context, this));

        MaybeHandle<Object> result;
        MaybeHandle<Object> maybe_exception;

        if (microtask->IsJSFunction()) {
          Handle<JSFunction> microtask_function =
              Handle<JSFunction>::cast(microtask);
          result = Execution::TryCall(
              this, microtask_function, factory()->undefined_value(), 0,
              nullptr, Execution::MessageHandling::kReport, &maybe_exception);
        } else if (microtask->IsPromiseResolveThenableJobInfo()) {
          PromiseResolveThenableJob(
              Handle<PromiseResolveThenableJobInfo>::cast(microtask), &result,
              &maybe_exception);
        } else {
          PromiseReactionJob(Handle<PromiseReactionJobInfo>::cast(microtask),
                             &result, &maybe_exception);
        }

        handle_scope_implementer_->LeaveMicrotaskContext();

        // If execution is terminating, just bail out.
        if (result.is_null() && maybe_exception.is_null()) {
          // Clear out any remaining callbacks in the queue.
          heap()->set_microtask_queue(heap()->empty_fixed_array());
          set_pending_microtask_count(0);
          return;
        }
      }
    });
  }
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


BasicBlockProfiler* Isolate::GetOrCreateBasicBlockProfiler() {
  if (basic_block_profiler_ == nullptr) {
    basic_block_profiler_ = new BasicBlockProfiler();
  }
  return basic_block_profiler_;
}


std::string Isolate::GetTurboCfgFileName() {
  if (FLAG_trace_turbo_cfg_file == nullptr) {
    std::ostringstream os;
    os << "turbo-" << base::OS::GetCurrentProcessId() << "-" << id() << ".cfg";
    return os.str();
  } else {
    return FLAG_trace_turbo_cfg_file;
  }
}

// Heap::detached_contexts tracks detached contexts as pairs
// (number of GC since the context was detached, the context).
void Isolate::AddDetachedContext(Handle<Context> context) {
  HandleScope scope(this);
  Handle<WeakCell> cell = factory()->NewWeakCell(context);
  Handle<FixedArray> detached_contexts =
      factory()->CopyFixedArrayAndGrow(factory()->detached_contexts(), 2);
  int new_length = detached_contexts->length();
  detached_contexts->set(new_length - 2, Smi::kZero);
  detached_contexts->set(new_length - 1, *cell);
  heap()->set_detached_contexts(*detached_contexts);
}


void Isolate::CheckDetachedContextsAfterGC() {
  HandleScope scope(this);
  Handle<FixedArray> detached_contexts = factory()->detached_contexts();
  int length = detached_contexts->length();
  if (length == 0) return;
  int new_length = 0;
  for (int i = 0; i < length; i += 2) {
    int mark_sweeps = Smi::ToInt(detached_contexts->get(i));
    DCHECK(detached_contexts->get(i + 1)->IsWeakCell());
    WeakCell* cell = WeakCell::cast(detached_contexts->get(i + 1));
    if (!cell->cleared()) {
      detached_contexts->set(new_length, Smi::FromInt(mark_sweeps + 1));
      detached_contexts->set(new_length + 1, cell);
      new_length += 2;
    }
    counters()->detached_context_age_in_gc()->AddSample(mark_sweeps + 1);
  }
  if (FLAG_trace_detached_contexts) {
    PrintF("%d detached contexts are collected out of %d\n",
           length - new_length, length);
    for (int i = 0; i < new_length; i += 2) {
      int mark_sweeps = Smi::ToInt(detached_contexts->get(i));
      DCHECK(detached_contexts->get(i + 1)->IsWeakCell());
      WeakCell* cell = WeakCell::cast(detached_contexts->get(i + 1));
      if (mark_sweeps > 3) {
        PrintF("detached context %p\n survived %d GCs (leak?)\n",
               static_cast<void*>(cell->value()), mark_sweeps);
      }
    }
  }
  if (new_length == 0) {
    heap()->set_detached_contexts(heap()->empty_fixed_array());
  } else if (new_length < length) {
    heap()->RightTrimFixedArray(*detached_contexts, length - new_length);
  }
}

double Isolate::LoadStartTimeMs() {
  base::LockGuard<base::Mutex> guard(&rail_mutex_);
  return load_start_time_ms_;
}

void Isolate::SetRAILMode(RAILMode rail_mode) {
  RAILMode old_rail_mode = rail_mode_.Value();
  if (old_rail_mode != PERFORMANCE_LOAD && rail_mode == PERFORMANCE_LOAD) {
    base::LockGuard<base::Mutex> guard(&rail_mutex_);
    load_start_time_ms_ = heap()->MonotonicallyIncreasingTimeInMs();
  }
  rail_mode_.SetValue(rail_mode);
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

wasm::WasmCodeManager* Isolate::wasm_code_manager() {
  return wasm_code_manager_.get();
}

bool StackLimitCheck::JsHasOverflowed(uintptr_t gap) const {
  StackGuard* stack_guard = isolate_->stack_guard();
#ifdef USE_SIMULATOR
  // The simulator uses a separate JS stack.
  Address jssp_address = Simulator::current(isolate_)->get_sp();
  uintptr_t jssp = reinterpret_cast<uintptr_t>(jssp_address);
  if (jssp - gap < stack_guard->real_jslimit()) return true;
#endif  // USE_SIMULATOR
  return GetCurrentStackPosition() - gap < stack_guard->real_climit();
}

SaveContext::SaveContext(Isolate* isolate)
    : isolate_(isolate), prev_(isolate->save_context()) {
  if (isolate->context() != nullptr) {
    context_ = Handle<Context>(isolate->context());
  }
  isolate->set_save_context(this);

  c_entry_fp_ = isolate->c_entry_fp(isolate->thread_local_top());
}

SaveContext::~SaveContext() {
  isolate_->set_context(context_.is_null() ? nullptr : *context_);
  isolate_->set_save_context(prev_);
}

bool SaveContext::IsBelowFrame(StandardFrame* frame) {
  return (c_entry_fp_ == 0) || (c_entry_fp_ > frame->sp());
}

#ifdef DEBUG
AssertNoContextChange::AssertNoContextChange(Isolate* isolate)
    : isolate_(isolate), context_(isolate->context(), isolate) {}
#endif  // DEBUG


bool PostponeInterruptsScope::Intercept(StackGuard::InterruptFlag flag) {
  // First check whether the previous scope intercepts.
  if (prev_ && prev_->Intercept(flag)) return true;
  // Then check whether this scope intercepts.
  if ((flag & intercept_mask_)) {
    intercepted_flags_ |= flag;
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace v8
