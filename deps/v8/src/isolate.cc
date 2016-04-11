// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/isolate.h"

#include <stdlib.h>

#include <fstream>  // NOLINT(readability/streams)
#include <sstream>

#include "src/ast/ast.h"
#include "src/ast/scopeinfo.h"
#include "src/base/platform/platform.h"
#include "src/base/sys-info.h"
#include "src/base/utils/random-number-generator.h"
#include "src/basic-block-profiler.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/compilation-cache.h"
#include "src/compilation-statistics.h"
#include "src/crankshaft/hydrogen.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/frames-inl.h"
#include "src/ic/stub-cache.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate-inl.h"
#include "src/log.h"
#include "src/messages.h"
#include "src/profiler/cpu-profiler.h"
#include "src/profiler/sampler.h"
#include "src/prototype.h"
#include "src/regexp/regexp-stack.h"
#include "src/runtime-profiler.h"
#include "src/simulator.h"
#include "src/snapshot/serialize.h"
#include "src/v8.h"
#include "src/version.h"
#include "src/vm-state-inl.h"


namespace v8 {
namespace internal {

base::Atomic32 ThreadId::highest_thread_id_ = 0;

int ThreadId::AllocateThreadId() {
  int new_id = base::NoBarrier_AtomicIncrement(&highest_thread_id_, 1);
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
  simulator_ = NULL;
#endif
  js_entry_sp_ = NULL;
  external_callback_scope_ = NULL;
  current_vm_state_ = EXTERNAL;
  try_catch_handler_ = NULL;
  context_ = NULL;
  thread_id_ = ThreadId::Invalid();
  external_caught_exception_ = false;
  failed_access_check_callback_ = NULL;
  save_context_ = NULL;
  promise_on_stack_ = NULL;

  // These members are re-initialized later after deserialization
  // is complete.
  pending_exception_ = NULL;
  rethrowing_message_ = false;
  pending_message_obj_ = NULL;
  scheduled_exception_ = NULL;
}


void ThreadLocalTop::Initialize() {
  InitializeInternal();
#ifdef USE_SIMULATOR
  simulator_ = Simulator::current(isolate_);
#endif
  thread_id_ = ThreadId::Current();
}


void ThreadLocalTop::Free() {
  // Match unmatched PopPromise calls.
  while (promise_on_stack_) isolate_->PopPromise();
}


base::Thread::LocalStorageKey Isolate::isolate_key_;
base::Thread::LocalStorageKey Isolate::thread_id_key_;
base::Thread::LocalStorageKey Isolate::per_isolate_thread_data_key_;
base::LazyMutex Isolate::thread_data_table_mutex_ = LAZY_MUTEX_INITIALIZER;
Isolate::ThreadDataTable* Isolate::thread_data_table_ = NULL;
base::Atomic32 Isolate::isolate_counter_ = 0;
#if DEBUG
base::Atomic32 Isolate::isolate_key_created_ = 0;
#endif

Isolate::PerIsolateThreadData*
    Isolate::FindOrAllocatePerThreadDataForThisThread() {
  ThreadId thread_id = ThreadId::Current();
  PerIsolateThreadData* per_thread = NULL;
  {
    base::LockGuard<base::Mutex> lock_guard(thread_data_table_mutex_.Pointer());
    per_thread = thread_data_table_->Lookup(this, thread_id);
    if (per_thread == NULL) {
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
  PerIsolateThreadData* per_thread = NULL;
  {
    base::LockGuard<base::Mutex> lock_guard(thread_data_table_mutex_.Pointer());
    per_thread = thread_data_table_->Lookup(this, thread_id);
  }
  return per_thread;
}


void Isolate::InitializeOncePerProcess() {
  base::LockGuard<base::Mutex> lock_guard(thread_data_table_mutex_.Pointer());
  CHECK(thread_data_table_ == NULL);
  isolate_key_ = base::Thread::CreateThreadLocalKey();
#if DEBUG
  base::NoBarrier_Store(&isolate_key_created_, 1);
#endif
  thread_id_key_ = base::Thread::CreateThreadLocalKey();
  per_isolate_thread_data_key_ = base::Thread::CreateThreadLocalKey();
  thread_data_table_ = new Isolate::ThreadDataTable();
}


Address Isolate::get_address_from_id(Isolate::AddressId id) {
  return isolate_addresses_[id];
}


char* Isolate::Iterate(ObjectVisitor* v, char* thread_storage) {
  ThreadLocalTop* thread = reinterpret_cast<ThreadLocalTop*>(thread_storage);
  Iterate(v, thread);
  return thread_storage + sizeof(ThreadLocalTop);
}


void Isolate::IterateThread(ThreadVisitor* v, char* t) {
  ThreadLocalTop* thread = reinterpret_cast<ThreadLocalTop*>(t);
  v->VisitThread(this, thread);
}


void Isolate::Iterate(ObjectVisitor* v, ThreadLocalTop* thread) {
  // Visit the roots from the top for a given thread.
  v->VisitPointer(&thread->pending_exception_);
  v->VisitPointer(&(thread->pending_message_obj_));
  v->VisitPointer(bit_cast<Object**>(&(thread->context_)));
  v->VisitPointer(&thread->scheduled_exception_);

  for (v8::TryCatch* block = thread->try_catch_handler();
       block != NULL;
       block = block->next_) {
    v->VisitPointer(bit_cast<Object**>(&(block->exception_)));
    v->VisitPointer(bit_cast<Object**>(&(block->message_obj_)));
  }

  // Iterate over pointers on native execution stack.
  for (StackFrameIterator it(this, thread); !it.done(); it.Advance()) {
    it.frame()->Iterate(v);
  }
}


void Isolate::Iterate(ObjectVisitor* v) {
  ThreadLocalTop* current_t = thread_local_top();
  Iterate(v, current_t);
}


void Isolate::IterateDeferredHandles(ObjectVisitor* visitor) {
  for (DeferredHandles* deferred = deferred_handles_head_;
       deferred != NULL;
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
  for (DeferredHandles* deferred = deferred_handles_head_;
       deferred != NULL;
       deferred = deferred->next_) {
    List<Object**>* blocks = &deferred->blocks_;
    for (int i = 0; i < blocks->length(); i++) {
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
    incomplete_message_ = NULL;
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


void Isolate::PushStackTraceAndDie(unsigned int magic, void* ptr1, void* ptr2,
                                   unsigned int magic2) {
  const int kMaxStackTraceSize = 32 * KB;
  Handle<String> trace = StackTraceString();
  uint8_t buffer[kMaxStackTraceSize];
  int length = Min(kMaxStackTraceSize - 1, trace->length());
  String::WriteToFlat(*trace, buffer, 0, length);
  buffer[length] = '\0';
  // TODO(dcarney): convert buffer to utf8?
  base::OS::PrintError("Stacktrace (%x-%x) %p %p: %s\n", magic, magic2, ptr1,
                       ptr2, reinterpret_cast<char*>(buffer));
  base::OS::Abort();
}


// Determines whether the given stack frame should be displayed in
// a stack trace.  The caller is the error constructor that asked
// for the stack trace to be collected.  The first time a construct
// call to this function is encountered it is skipped.  The seen_caller
// in/out parameter is used to remember if the caller has been seen
// yet.
static bool IsVisibleInStackTrace(JSFunction* fun,
                                  Object* caller,
                                  Object* receiver,
                                  bool* seen_caller) {
  if ((fun == caller) && !(*seen_caller)) {
    *seen_caller = true;
    return false;
  }
  // Skip all frames until we've seen the caller.
  if (!(*seen_caller)) return false;
  // Functions defined in native scripts are not visible unless directly
  // exposed, in which case the native flag is set.
  // The --builtins-in-stack-traces command line flag allows including
  // internal call sites in the stack trace for debugging purposes.
  if (!FLAG_builtins_in_stack_traces && fun->shared()->IsBuiltin()) {
    return fun->shared()->native();
  }
  return true;
}


Handle<Object> Isolate::CaptureSimpleStackTrace(Handle<JSObject> error_object,
                                                Handle<Object> caller) {
  // Get stack trace limit.
  Handle<JSObject> error = error_function();
  Handle<String> stackTraceLimit =
      factory()->InternalizeUtf8String("stackTraceLimit");
  DCHECK(!stackTraceLimit.is_null());
  Handle<Object> stack_trace_limit =
      JSReceiver::GetDataProperty(error, stackTraceLimit);
  if (!stack_trace_limit->IsNumber()) return factory()->undefined_value();
  int limit = FastD2IChecked(stack_trace_limit->Number());
  limit = Max(limit, 0);  // Ensure that limit is not negative.

  int initial_size = Min(limit, 10);
  Handle<FixedArray> elements =
      factory()->NewFixedArrayWithHoles(initial_size * 4 + 1);

  // If the caller parameter is a function we skip frames until we're
  // under it before starting to collect.
  bool seen_caller = !caller->IsJSFunction();
  // First element is reserved to store the number of sloppy frames.
  int cursor = 1;
  int frames_seen = 0;
  int sloppy_frames = 0;
  bool encountered_strict_function = false;
  for (JavaScriptFrameIterator iter(this);
       !iter.done() && frames_seen < limit;
       iter.Advance()) {
    JavaScriptFrame* frame = iter.frame();
    // Set initial size to the maximum inlining level + 1 for the outermost
    // function.
    List<FrameSummary> frames(FLAG_max_inlining_levels + 1);
    frame->Summarize(&frames);
    for (int i = frames.length() - 1; i >= 0; i--) {
      Handle<JSFunction> fun = frames[i].function();
      Handle<Object> recv = frames[i].receiver();
      // Filter out internal frames that we do not want to show.
      if (!IsVisibleInStackTrace(*fun, *caller, *recv, &seen_caller)) continue;
      // Filter out frames from other security contexts.
      if (!this->context()->HasSameSecurityTokenAs(fun->context())) continue;
      if (cursor + 4 > elements->length()) {
        int new_capacity = JSObject::NewElementsCapacity(elements->length());
        Handle<FixedArray> new_elements =
            factory()->NewFixedArrayWithHoles(new_capacity);
        for (int i = 0; i < cursor; i++) {
          new_elements->set(i, elements->get(i));
        }
        elements = new_elements;
      }
      DCHECK(cursor + 4 <= elements->length());

      Handle<Code> code = frames[i].code();
      Handle<Smi> offset(Smi::FromInt(frames[i].offset()), this);
      // The stack trace API should not expose receivers and function
      // objects on frames deeper than the top-most one with a strict
      // mode function.  The number of sloppy frames is stored as
      // first element in the result array.
      if (!encountered_strict_function) {
        if (is_strict(fun->shared()->language_mode())) {
          encountered_strict_function = true;
        } else {
          sloppy_frames++;
        }
      }
      elements->set(cursor++, *recv);
      elements->set(cursor++, *fun);
      elements->set(cursor++, *code);
      elements->set(cursor++, *offset);
      frames_seen++;
    }
  }
  elements->set(0, Smi::FromInt(sloppy_frames));
  elements->Shrink(cursor);
  Handle<JSArray> result = factory()->NewJSArrayWithElements(elements);
  result->set_length(Smi::FromInt(cursor));
  // TODO(yangguo): Queue this structured stack trace for preprocessing on GC.
  return result;
}


MaybeHandle<JSObject> Isolate::CaptureAndSetDetailedStackTrace(
    Handle<JSObject> error_object) {
  if (capture_stack_trace_for_uncaught_exceptions_) {
    // Capture stack trace for a detailed exception message.
    Handle<Name> key = factory()->detailed_stack_trace_symbol();
    Handle<JSArray> stack_trace = CaptureCurrentStackTrace(
        stack_trace_for_uncaught_exceptions_frame_limit_,
        stack_trace_for_uncaught_exceptions_options_);
    RETURN_ON_EXCEPTION(
        this, JSObject::SetProperty(error_object, key, stack_trace, STRICT),
        JSObject);
  }
  return error_object;
}


MaybeHandle<JSObject> Isolate::CaptureAndSetSimpleStackTrace(
    Handle<JSObject> error_object, Handle<Object> caller) {
  // Capture stack trace for simple stack trace string formatting.
  Handle<Name> key = factory()->stack_trace_symbol();
  Handle<Object> stack_trace = CaptureSimpleStackTrace(error_object, caller);
  RETURN_ON_EXCEPTION(
      this, JSObject::SetProperty(error_object, key, stack_trace, STRICT),
      JSObject);
  return error_object;
}


Handle<JSArray> Isolate::GetDetailedStackTrace(Handle<JSObject> error_object) {
  Handle<Name> key_detailed = factory()->detailed_stack_trace_symbol();
  Handle<Object> stack_trace =
      JSReceiver::GetDataProperty(error_object, key_detailed);
  if (stack_trace->IsJSArray()) return Handle<JSArray>::cast(stack_trace);

  if (!capture_stack_trace_for_uncaught_exceptions_) return Handle<JSArray>();

  // Try to get details from simple stack trace.
  Handle<JSArray> detailed_stack_trace =
      GetDetailedFromSimpleStackTrace(error_object);
  if (!detailed_stack_trace.is_null()) {
    // Save the detailed stack since the simple one might be withdrawn later.
    JSObject::SetProperty(error_object, key_detailed, detailed_stack_trace,
                          STRICT).Assert();
  }
  return detailed_stack_trace;
}


class CaptureStackTraceHelper {
 public:
  CaptureStackTraceHelper(Isolate* isolate,
                          StackTrace::StackTraceOptions options)
      : isolate_(isolate) {
    if (options & StackTrace::kColumnOffset) {
      column_key_ =
          factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("column"));
    }
    if (options & StackTrace::kLineNumber) {
      line_key_ =
          factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("lineNumber"));
    }
    if (options & StackTrace::kScriptId) {
      script_id_key_ =
          factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("scriptId"));
    }
    if (options & StackTrace::kScriptName) {
      script_name_key_ =
          factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("scriptName"));
    }
    if (options & StackTrace::kScriptNameOrSourceURL) {
      script_name_or_source_url_key_ = factory()->InternalizeOneByteString(
          STATIC_CHAR_VECTOR("scriptNameOrSourceURL"));
    }
    if (options & StackTrace::kFunctionName) {
      function_key_ = factory()->InternalizeOneByteString(
          STATIC_CHAR_VECTOR("functionName"));
    }
    if (options & StackTrace::kIsEval) {
      eval_key_ =
          factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("isEval"));
    }
    if (options & StackTrace::kIsConstructor) {
      constructor_key_ = factory()->InternalizeOneByteString(
          STATIC_CHAR_VECTOR("isConstructor"));
    }
  }

  Handle<JSObject> NewStackFrameObject(Handle<JSFunction> fun, int position,
                                       bool is_constructor) {
    Handle<JSObject> stack_frame =
        factory()->NewJSObject(isolate_->object_function());

    Handle<Script> script(Script::cast(fun->shared()->script()));

    if (!line_key_.is_null()) {
      int script_line_offset = script->line_offset();
      int line_number = Script::GetLineNumber(script, position);
      // line_number is already shifted by the script_line_offset.
      int relative_line_number = line_number - script_line_offset;
      if (!column_key_.is_null() && relative_line_number >= 0) {
        Handle<FixedArray> line_ends(FixedArray::cast(script->line_ends()));
        int start = (relative_line_number == 0) ? 0 :
            Smi::cast(line_ends->get(relative_line_number - 1))->value() + 1;
        int column_offset = position - start;
        if (relative_line_number == 0) {
          // For the case where the code is on the same line as the script
          // tag.
          column_offset += script->column_offset();
        }
        JSObject::AddProperty(stack_frame, column_key_,
                              handle(Smi::FromInt(column_offset + 1), isolate_),
                              NONE);
      }
      JSObject::AddProperty(stack_frame, line_key_,
                            handle(Smi::FromInt(line_number + 1), isolate_),
                            NONE);
    }

    if (!script_id_key_.is_null()) {
      JSObject::AddProperty(stack_frame, script_id_key_,
                            handle(Smi::FromInt(script->id()), isolate_), NONE);
    }

    if (!script_name_key_.is_null()) {
      JSObject::AddProperty(stack_frame, script_name_key_,
                            handle(script->name(), isolate_), NONE);
    }

    if (!script_name_or_source_url_key_.is_null()) {
      Handle<Object> result = Script::GetNameOrSourceURL(script);
      JSObject::AddProperty(stack_frame, script_name_or_source_url_key_, result,
                            NONE);
    }

    if (!function_key_.is_null()) {
      Handle<Object> fun_name = JSFunction::GetDebugName(fun);
      JSObject::AddProperty(stack_frame, function_key_, fun_name, NONE);
    }

    if (!eval_key_.is_null()) {
      Handle<Object> is_eval = factory()->ToBoolean(
          script->compilation_type() == Script::COMPILATION_TYPE_EVAL);
      JSObject::AddProperty(stack_frame, eval_key_, is_eval, NONE);
    }

    if (!constructor_key_.is_null()) {
      Handle<Object> is_constructor_obj = factory()->ToBoolean(is_constructor);
      JSObject::AddProperty(stack_frame, constructor_key_, is_constructor_obj,
                            NONE);
    }

    return stack_frame;
  }

 private:
  inline Factory* factory() { return isolate_->factory(); }

  Isolate* isolate_;
  Handle<String> column_key_;
  Handle<String> line_key_;
  Handle<String> script_id_key_;
  Handle<String> script_name_key_;
  Handle<String> script_name_or_source_url_key_;
  Handle<String> function_key_;
  Handle<String> eval_key_;
  Handle<String> constructor_key_;
};


int PositionFromStackTrace(Handle<FixedArray> elements, int index) {
  DisallowHeapAllocation no_gc;
  Object* maybe_code = elements->get(index + 2);
  if (maybe_code->IsSmi()) {
    return Smi::cast(maybe_code)->value();
  } else {
    Code* code = Code::cast(maybe_code);
    Address pc = code->address() + Smi::cast(elements->get(index + 3))->value();
    return code->SourcePosition(pc);
  }
}


Handle<JSArray> Isolate::GetDetailedFromSimpleStackTrace(
    Handle<JSObject> error_object) {
  Handle<Name> key = factory()->stack_trace_symbol();
  Handle<Object> property = JSReceiver::GetDataProperty(error_object, key);
  if (!property->IsJSArray()) return Handle<JSArray>();
  Handle<JSArray> simple_stack_trace = Handle<JSArray>::cast(property);

  CaptureStackTraceHelper helper(this,
                                 stack_trace_for_uncaught_exceptions_options_);

  int frames_seen = 0;
  Handle<FixedArray> elements(FixedArray::cast(simple_stack_trace->elements()));
  int elements_limit = Smi::cast(simple_stack_trace->length())->value();

  int frame_limit = stack_trace_for_uncaught_exceptions_frame_limit_;
  if (frame_limit < 0) frame_limit = (elements_limit - 1) / 4;

  Handle<JSArray> stack_trace = factory()->NewJSArray(frame_limit);
  for (int i = 1; i < elements_limit && frames_seen < frame_limit; i += 4) {
    Handle<Object> recv = handle(elements->get(i), this);
    Handle<JSFunction> fun =
        handle(JSFunction::cast(elements->get(i + 1)), this);
    bool is_constructor =
        recv->IsJSObject() &&
        Handle<JSObject>::cast(recv)->map()->GetConstructor() == *fun;
    int position = PositionFromStackTrace(elements, i);

    Handle<JSObject> stack_frame =
        helper.NewStackFrameObject(fun, position, is_constructor);

    FixedArray::cast(stack_trace->elements())->set(frames_seen, *stack_frame);
    frames_seen++;
  }

  stack_trace->set_length(Smi::FromInt(frames_seen));
  return stack_trace;
}


Handle<JSArray> Isolate::CaptureCurrentStackTrace(
    int frame_limit, StackTrace::StackTraceOptions options) {
  CaptureStackTraceHelper helper(this, options);

  // Ensure no negative values.
  int limit = Max(frame_limit, 0);
  Handle<JSArray> stack_trace = factory()->NewJSArray(frame_limit);

  StackTraceFrameIterator it(this);
  int frames_seen = 0;
  while (!it.done() && (frames_seen < limit)) {
    JavaScriptFrame* frame = it.frame();
    // Set initial size to the maximum inlining level + 1 for the outermost
    // function.
    List<FrameSummary> frames(FLAG_max_inlining_levels + 1);
    frame->Summarize(&frames);
    for (int i = frames.length() - 1; i >= 0 && frames_seen < limit; i--) {
      Handle<JSFunction> fun = frames[i].function();
      // Filter frames from other security contexts.
      if (!(options & StackTrace::kExposeFramesAcrossSecurityOrigins) &&
          !this->context()->HasSameSecurityTokenAs(fun->context())) continue;
      int position = frames[i].code()->SourcePosition(frames[i].pc());
      Handle<JSObject> stack_frame =
          helper.NewStackFrameObject(fun, position, frames[i].is_constructor());

      FixedArray::cast(stack_trace->elements())->set(frames_seen, *stack_frame);
      frames_seen++;
    }
    it.Advance();
  }

  stack_trace->set_length(Smi::FromInt(frames_seen));
  return stack_trace;
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
    incomplete_message_ = NULL;
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


static inline AccessCheckInfo* GetAccessCheckInfo(Isolate* isolate,
                                                  Handle<JSObject> receiver) {
  Object* maybe_constructor = receiver->map()->GetConstructor();
  if (!maybe_constructor->IsJSFunction()) return NULL;
  JSFunction* constructor = JSFunction::cast(maybe_constructor);
  if (!constructor->shared()->IsApiFunction()) return NULL;

  Object* data_obj =
     constructor->shared()->get_api_func_data()->access_check_info();
  if (data_obj == isolate->heap()->undefined_value()) return NULL;

  return AccessCheckInfo::cast(data_obj);
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
    AccessCheckInfo* access_check_info = GetAccessCheckInfo(this, receiver);
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


bool Isolate::IsInternallyUsedPropertyName(Handle<Object> name) {
  if (name->IsSymbol()) {
    return Handle<Symbol>::cast(name)->is_private();
  }
  return name.is_identical_to(factory()->hidden_string());
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
  v8::NamedSecurityCallback named_callback = nullptr;
  { DisallowHeapAllocation no_gc;
    AccessCheckInfo* access_check_info = GetAccessCheckInfo(this, receiver);
    if (!access_check_info) return false;
    Object* fun_obj = access_check_info->callback();
    callback = v8::ToCData<v8::AccessCheckCallback>(fun_obj);
    if (!callback) {
      fun_obj = access_check_info->named_callback();
      named_callback = v8::ToCData<v8::NamedSecurityCallback>(fun_obj);
      if (!named_callback) return false;
      data = handle(access_check_info->data(), this);
    }
  }

  LOG(this, ApiSecurityCheck());

  {
    // Leaving JavaScript.
    VMState<EXTERNAL> state(this);
    if (callback) {
      return callback(v8::Utils::ToLocal(accessing_context),
                      v8::Utils::ToLocal(receiver));
    }
    Handle<Object> key = factory()->undefined_value();
    return named_callback(v8::Utils::ToLocal(receiver), v8::Utils::ToLocal(key),
                          v8::ACCESS_HAS, v8::Utils::ToLocal(data));
  }
}


const char* const Isolate::kStackOverflowMessage =
  "Uncaught RangeError: Maximum call stack size exceeded";


Object* Isolate::StackOverflow() {
  HandleScope scope(this);
  // At this point we cannot create an Error object using its javascript
  // constructor.  Instead, we copy the pre-constructed boilerplate and
  // attach the stack trace as a hidden property.
  Handle<Object> exception;
  if (bootstrapper()->IsActive()) {
    // There is no boilerplate to use during bootstrapping.
    exception = factory()->NewStringFromAsciiChecked(
        MessageTemplate::TemplateString(MessageTemplate::kStackOverflow));
  } else {
    Handle<JSObject> boilerplate = stack_overflow_boilerplate();
    Handle<JSObject> copy = factory()->CopyJSObject(boilerplate);
    CaptureAndSetSimpleStackTrace(copy, factory()->undefined_value());
    exception = copy;
  }
  Throw(*exception, nullptr);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && FLAG_stress_compaction) {
    heap()->CollectAllAvailableGarbage("trigger compaction");
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
  if (location == NULL || location->script().is_null()) return;
  // We are bootstrapping and caught an error where the location is set
  // and we have a script for the location.
  // In this case we could have an extension (or an internal error
  // somewhere) and we print out the line number at which the error occured
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


Object* Isolate::Throw(Object* exception, MessageLocation* location) {
  DCHECK(!has_pending_exception());

  HandleScope scope(this);
  Handle<Object> exception_handle(exception, this);

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
    if (location == NULL && ComputeLocation(&computed_location)) {
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
      if (FLAG_abort_on_uncaught_exception &&
          PredictExceptionCatcher() != CAUGHT_BY_JAVASCRIPT &&
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

  Code* code = nullptr;
  Context* context = nullptr;
  intptr_t offset = 0;
  Address handler_sp = nullptr;
  Address handler_fp = nullptr;

  // Special handling of termination exceptions, uncatchable by JavaScript code,
  // we unwind the handlers until the top ENTRY handler is found.
  bool catchable_by_js = is_catchable_by_javascript(exception);

  // Compute handler and stack unwinding information by performing a full walk
  // over the stack and dispatching according to the frame type.
  for (StackFrameIterator iter(this); !iter.done(); iter.Advance()) {
    StackFrame* frame = iter.frame();

    // For JSEntryStub frames we always have a handler.
    if (frame->is_entry() || frame->is_entry_construct()) {
      StackHandler* handler = frame->top_handler();

      // Restore the next handler.
      thread_local_top()->handler_ = handler->next()->address();

      // Gather information from the handler.
      code = frame->LookupCode();
      handler_sp = handler->address() + StackHandlerConstants::kSize;
      offset = Smi::cast(code->handler_table()->get(0))->value();
      break;
    }

    // For optimized frames we perform a lookup in the handler table.
    if (frame->is_optimized() && catchable_by_js) {
      OptimizedFrame* js_frame = static_cast<OptimizedFrame*>(frame);
      int stack_slots = 0;  // Will contain stack slot count of frame.
      offset = js_frame->LookupExceptionHandlerInTable(&stack_slots, NULL);
      if (offset >= 0) {
        // Compute the stack pointer from the frame pointer. This ensures that
        // argument slots on the stack are dropped as returning would.
        Address return_sp = frame->fp() -
                            StandardFrameConstants::kFixedFrameSizeFromFp -
                            stack_slots * kPointerSize;

        // Gather information from the frame.
        code = frame->LookupCode();
        handler_sp = return_sp;
        handler_fp = frame->fp();
        break;
      }
    }

    // For JavaScript frames we perform a range lookup in the handler table.
    if (frame->is_java_script() && catchable_by_js) {
      JavaScriptFrame* js_frame = static_cast<JavaScriptFrame*>(frame);
      int stack_slots = 0;  // Will contain operand stack depth of handler.
      offset = js_frame->LookupExceptionHandlerInTable(&stack_slots, NULL);
      if (offset >= 0) {
        // Compute the stack pointer from the frame pointer. This ensures that
        // operand stack slots are dropped for nested statements. Also restore
        // correct context for the handler which is pushed within the try-block.
        Address return_sp = frame->fp() -
                            StandardFrameConstants::kFixedFrameSizeFromFp -
                            stack_slots * kPointerSize;
        STATIC_ASSERT(TryBlockConstant::kElementCount == 1);
        context = Context::cast(Memory::Object_at(return_sp - kPointerSize));

        // Gather information from the frame.
        code = frame->LookupCode();
        handler_sp = return_sp;
        handler_fp = frame->fp();
        break;
      }
    }

    RemoveMaterializedObjectsOnUnwind(frame);
  }

  // Handler must exist.
  CHECK(code != nullptr);

  // Store information to be consumed by the CEntryStub.
  thread_local_top()->pending_handler_context_ = context;
  thread_local_top()->pending_handler_code_ = code;
  thread_local_top()->pending_handler_offset_ = offset;
  thread_local_top()->pending_handler_fp_ = handler_fp;
  thread_local_top()->pending_handler_sp_ = handler_sp;

  // Return and clear pending exception.
  clear_pending_exception();
  return exception;
}


Isolate::CatchType Isolate::PredictExceptionCatcher() {
  Address external_handler = thread_local_top()->try_catch_handler_address();
  Address entry_handler = Isolate::handler(thread_local_top());
  if (IsExternalHandlerOnTop(nullptr)) return CAUGHT_BY_EXTERNAL;

  // Search for an exception handler by performing a full walk over the stack.
  for (StackFrameIterator iter(this); !iter.done(); iter.Advance()) {
    StackFrame* frame = iter.frame();

    // For JSEntryStub frames we update the JS_ENTRY handler.
    if (frame->is_entry() || frame->is_entry_construct()) {
      entry_handler = frame->top_handler()->next()->address();
    }

    // For JavaScript frames we perform a lookup in the handler table.
    if (frame->is_java_script()) {
      JavaScriptFrame* js_frame = static_cast<JavaScriptFrame*>(frame);
      int stack_slots = 0;  // The computed stack slot count is not used.
      HandlerTable::CatchPrediction prediction;
      if (js_frame->LookupExceptionHandlerInTable(&stack_slots, &prediction) >
          0) {
        // We are conservative with our prediction: try-finally is considered
        // to always rethrow, to meet the expectation of the debugger.
        if (prediction == HandlerTable::CAUGHT) return CAUGHT_BY_JAVASCRIPT;
      }
    }

    // The exception has been externally caught if and only if there is an
    // external handler which is on top of the top-most JS_ENTRY handler.
    if (external_handler != nullptr && !try_catch_handler()->is_verbose_) {
      if (entry_handler == nullptr || entry_handler > external_handler) {
        return CAUGHT_BY_EXTERNAL;
      }
    }
  }

  // Handler not found.
  return NOT_CAUGHT;
}


void Isolate::RemoveMaterializedObjectsOnUnwind(StackFrame* frame) {
  if (frame->is_optimized()) {
    bool removed = materialized_object_store_->Remove(frame->fp());
    USE(removed);
    // If there were any materialized objects, the code should be
    // marked for deopt.
    DCHECK(!removed || frame->LookupCode()->marked_for_deoptimization());
  }
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
  DCHECK(message->IsJSMessageObject() || message->IsTheHole());
  thread_local_top()->pending_message_obj_ = message;
}


void Isolate::CancelScheduledExceptionFromTryCatch(v8::TryCatch* handler) {
  DCHECK(has_scheduled_exception());
  if (scheduled_exception() == handler->exception_) {
    DCHECK(scheduled_exception() != heap()->termination_exception());
    clear_scheduled_exception();
  }
}


Object* Isolate::PromoteScheduledException() {
  Object* thrown = scheduled_exception();
  clear_scheduled_exception();
  // Re-throw the exception to avoid getting repeated error reporting.
  return ReThrow(thrown);
}


void Isolate::PrintCurrentStackTrace(FILE* out) {
  StackTraceFrameIterator it(this);
  while (!it.done()) {
    HandleScope scope(this);
    // Find code position if recorded in relocation info.
    JavaScriptFrame* frame = it.frame();
    int pos = frame->LookupCode()->SourcePosition(frame->pc());
    Handle<Object> pos_obj(Smi::FromInt(pos), this);
    // Fetch function and receiver.
    Handle<JSFunction> fun(frame->function());
    Handle<Object> recv(frame->receiver(), this);
    // Advance to the next JavaScript frame and determine if the
    // current frame is the top-level frame.
    it.Advance();
    Handle<Object> is_top_level = factory()->ToBoolean(it.done());
    // Generate and print stack trace line.
    Handle<String> line =
        Execution::GetStackTraceLine(recv, fun, pos_obj, is_top_level);
    if (line->length() > 0) {
      line->PrintOn(out);
      PrintF(out, "\n");
    }
  }
}


bool Isolate::ComputeLocation(MessageLocation* target) {
  StackTraceFrameIterator it(this);
  if (!it.done()) {
    JavaScriptFrame* frame = it.frame();
    JSFunction* fun = frame->function();
    Object* script = fun->shared()->script();
    if (script->IsScript() &&
        !(Script::cast(script)->source()->IsUndefined())) {
      Handle<Script> casted_script(Script::cast(script));
      // Compute the location from the function and the relocation info of the
      // baseline code. For optimized code this will use the deoptimization
      // information to get canonical location information.
      List<FrameSummary> frames(FLAG_max_inlining_levels + 1);
      it.frame()->Summarize(&frames);
      FrameSummary& summary = frames.last();
      int pos = summary.code()->SourcePosition(summary.pc());
      *target = MessageLocation(casted_script, pos, pos + 1, handle(fun));
      return true;
    }
  }
  return false;
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

  Handle<FixedArray> elements(FixedArray::cast(simple_stack_trace->elements()));
  int elements_limit = Smi::cast(simple_stack_trace->length())->value();

  for (int i = 1; i < elements_limit; i += 4) {
    Handle<JSFunction> fun =
        handle(JSFunction::cast(elements->get(i + 1)), this);
    if (!fun->shared()->IsSubjectToDebugging()) continue;

    Object* script = fun->shared()->script();
    if (script->IsScript() &&
        !(Script::cast(script)->source()->IsUndefined())) {
      int pos = PositionFromStackTrace(elements, i);
      Handle<Script> casted_script(Script::cast(script));
      *target = MessageLocation(casted_script, pos, pos + 1);
      return true;
    }
  }
  return false;
}


Handle<JSMessageObject> Isolate::CreateMessage(Handle<Object> exception,
                                               MessageLocation* location) {
  Handle<JSArray> stack_trace_object;
  if (capture_stack_trace_for_uncaught_exceptions_) {
    if (Object::IsErrorObject(this, exception)) {
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
  if (location == NULL &&
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
  if (!message_obj->IsTheHole() && should_report_exception) {
    HandleScope scope(this);
    Handle<JSMessageObject> message(JSMessageObject::cast(message_obj));
    Handle<JSValue> script_wrapper(JSValue::cast(message->script()));
    Handle<Script> script(Script::cast(script_wrapper->value()));
    int start_pos = message->start_position();
    int end_pos = message->end_position();
    MessageLocation location(script, start_pos, end_pos);
    MessageHandler::ReportMessage(this, &location, message);
  }
}


MessageLocation Isolate::GetMessageLocation() {
  DCHECK(has_pending_exception());

  if (thread_local_top_.pending_exception_ != heap()->termination_exception() &&
      !thread_local_top_.pending_message_obj_->IsTheHole()) {
    Handle<JSMessageObject> message_obj(
        JSMessageObject::cast(thread_local_top_.pending_message_obj_));
    Handle<JSValue> script_wrapper(JSValue::cast(message_obj->script()));
    Handle<Script> script(Script::cast(script_wrapper->value()));
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
    DCHECK(thread_local_top()->try_catch_handler_address() != NULL);
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


void Isolate::PushPromise(Handle<JSObject> promise,
                          Handle<JSFunction> function) {
  ThreadLocalTop* tltop = thread_local_top();
  PromiseOnStack* prev = tltop->promise_on_stack_;
  Handle<JSObject> global_promise =
      Handle<JSObject>::cast(global_handles()->Create(*promise));
  Handle<JSFunction> global_function =
      Handle<JSFunction>::cast(global_handles()->Create(*function));
  tltop->promise_on_stack_ =
      new PromiseOnStack(global_function, global_promise, prev);
}


void Isolate::PopPromise() {
  ThreadLocalTop* tltop = thread_local_top();
  if (tltop->promise_on_stack_ == NULL) return;
  PromiseOnStack* prev = tltop->promise_on_stack_->prev();
  Handle<Object> global_function = tltop->promise_on_stack_->function();
  Handle<Object> global_promise = tltop->promise_on_stack_->promise();
  delete tltop->promise_on_stack_;
  tltop->promise_on_stack_ = prev;
  global_handles()->Destroy(global_function.location());
  global_handles()->Destroy(global_promise.location());
}


Handle<Object> Isolate::GetPromiseOnStackOnThrow() {
  Handle<Object> undefined = factory()->undefined_value();
  ThreadLocalTop* tltop = thread_local_top();
  if (tltop->promise_on_stack_ == NULL) return undefined;
  Handle<JSFunction> promise_function = tltop->promise_on_stack_->function();
  // Find the top-most try-catch or try-finally handler.
  if (PredictExceptionCatcher() != CAUGHT_BY_JAVASCRIPT) return undefined;
  for (JavaScriptFrameIterator it(this); !it.done(); it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    int stack_slots = 0;  // The computed stack slot count is not used.
    if (frame->LookupExceptionHandlerInTable(&stack_slots, NULL) > 0) {
      // Throwing inside a Promise only leads to a reject if not caught by an
      // inner try-catch or try-finally.
      if (frame->function() == *promise_function) {
        return tltop->promise_on_stack_->promise();
      }
      return undefined;
    }
  }
  return undefined;
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


Handle<Context> Isolate::native_context() {
  return handle(context()->native_context());
}


Handle<Context> Isolate::GetCallingNativeContext() {
  JavaScriptFrameIterator it(this);
  if (debug_->in_debug_scope()) {
    while (!it.done()) {
      JavaScriptFrame* frame = it.frame();
      Context* context = Context::cast(frame->context());
      if (context->native_context() == *debug_->debug_context()) {
        it.Advance();
      } else {
        break;
      }
    }
  }
  if (it.done()) return Handle<Context>::null();
  JavaScriptFrame* frame = it.frame();
  Context* context = Context::cast(frame->context());
  return Handle<Context>(context->native_context());
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
  DCHECK(context() == NULL || context()->IsContext());
  return from + sizeof(ThreadLocalTop);
}


Isolate::ThreadDataTable::ThreadDataTable()
    : list_(NULL) {
}


Isolate::ThreadDataTable::~ThreadDataTable() {
  // TODO(svenpanne) The assertion below would fire if an embedder does not
  // cleanly dispose all Isolates before disposing v8, so we are conservative
  // and leave it out for now.
  // DCHECK_NULL(list_);
}


Isolate::PerIsolateThreadData::~PerIsolateThreadData() {
#if defined(USE_SIMULATOR)
  delete simulator_;
#endif
}


Isolate::PerIsolateThreadData*
    Isolate::ThreadDataTable::Lookup(Isolate* isolate,
                                     ThreadId thread_id) {
  for (PerIsolateThreadData* data = list_; data != NULL; data = data->next_) {
    if (data->Matches(isolate, thread_id)) return data;
  }
  return NULL;
}


void Isolate::ThreadDataTable::Insert(Isolate::PerIsolateThreadData* data) {
  if (list_ != NULL) list_->prev_ = data;
  data->next_ = list_;
  list_ = data;
}


void Isolate::ThreadDataTable::Remove(PerIsolateThreadData* data) {
  if (list_ == data) list_ = data->next_;
  if (data->next_ != NULL) data->next_->prev_ = data->prev_;
  if (data->prev_ != NULL) data->prev_->next_ = data->next_;
  delete data;
}


void Isolate::ThreadDataTable::RemoveAllThreads(Isolate* isolate) {
  PerIsolateThreadData* data = list_;
  while (data != NULL) {
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


Isolate::Isolate(bool enable_serializer)
    : embedder_data_(),
      entry_stack_(NULL),
      stack_trace_nesting_level_(0),
      incomplete_message_(NULL),
      bootstrapper_(NULL),
      runtime_profiler_(NULL),
      compilation_cache_(NULL),
      counters_(NULL),
      code_range_(NULL),
      logger_(NULL),
      stats_table_(NULL),
      stub_cache_(NULL),
      code_aging_helper_(NULL),
      deoptimizer_data_(NULL),
      materialized_object_store_(NULL),
      capture_stack_trace_for_uncaught_exceptions_(false),
      stack_trace_for_uncaught_exceptions_frame_limit_(0),
      stack_trace_for_uncaught_exceptions_options_(StackTrace::kOverview),
      memory_allocator_(NULL),
      keyed_lookup_cache_(NULL),
      context_slot_cache_(NULL),
      descriptor_lookup_cache_(NULL),
      handle_scope_implementer_(NULL),
      unicode_cache_(NULL),
      inner_pointer_to_code_cache_(NULL),
      global_handles_(NULL),
      eternal_handles_(NULL),
      thread_manager_(NULL),
      has_installed_extensions_(false),
      regexp_stack_(NULL),
      date_cache_(NULL),
      call_descriptor_data_(NULL),
      // TODO(bmeurer) Initialized lazily because it depends on flags; can
      // be fixed once the default isolate cleanup is done.
      random_number_generator_(NULL),
      serializer_enabled_(enable_serializer),
      has_fatal_error_(false),
      initialized_from_snapshot_(false),
      cpu_profiler_(NULL),
      heap_profiler_(NULL),
      function_entry_hook_(NULL),
      deferred_handles_head_(NULL),
      optimizing_compile_dispatcher_(NULL),
      stress_deopt_count_(0),
      virtual_handler_register_(NULL),
      virtual_slot_register_(NULL),
      next_optimization_id_(0),
      js_calls_from_api_counter_(0),
#if TRACE_MAPS
      next_unique_sfi_id_(0),
#endif
      use_counter_callback_(NULL),
      basic_block_profiler_(NULL),
      cancelable_task_manager_(new CancelableTaskManager()),
      abort_on_uncaught_exception_callback_(NULL) {
  {
    base::LockGuard<base::Mutex> lock_guard(thread_data_table_mutex_.Pointer());
    CHECK(thread_data_table_);
  }
  id_ = base::NoBarrier_AtomicIncrement(&isolate_counter_, 1);
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
#endif

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
  DCHECK(base::NoBarrier_Load(&isolate_key_created_) == 1);
  Isolate* saved_isolate =
      reinterpret_cast<Isolate*>(base::Thread::GetThreadLocal(isolate_key_));
  SetIsolateThreadLocals(this, NULL);

  Deinit();

  {
    base::LockGuard<base::Mutex> lock_guard(thread_data_table_mutex_.Pointer());
    thread_data_table_->RemoveAllThreads(this);
  }

  delete this;

  // Restore the previous current isolate.
  SetIsolateThreadLocals(saved_isolate, saved_data);
}


void Isolate::GlobalTearDown() {
  delete thread_data_table_;
  thread_data_table_ = NULL;
}


void Isolate::ClearSerializerData() {
  delete external_reference_table_;
  external_reference_table_ = NULL;
  delete external_reference_map_;
  external_reference_map_ = NULL;
}


void Isolate::Deinit() {
  TRACE_ISOLATE(deinit);

  debug()->Unload();

  FreeThreadResources();

  if (concurrent_recompilation_enabled()) {
    optimizing_compile_dispatcher_->Stop();
    delete optimizing_compile_dispatcher_;
    optimizing_compile_dispatcher_ = NULL;
  }

  if (heap_.mark_compact_collector()->sweeping_in_progress()) {
    heap_.mark_compact_collector()->EnsureSweepingCompleted();
  }

  DumpAndResetCompilationStats();

  if (FLAG_print_deopt_stress) {
    PrintF(stdout, "=== Stress deopt counter: %u\n", stress_deopt_count_);
  }

  if (cpu_profiler_) {
    cpu_profiler_->DeleteAllProfiles();
  }

  // We must stop the logger before we tear down other components.
  Sampler* sampler = logger_->sampler();
  if (sampler && sampler->IsActive()) sampler->Stop();

  delete interpreter_;
  interpreter_ = NULL;

  delete deoptimizer_data_;
  deoptimizer_data_ = NULL;
  builtins_.TearDown();
  bootstrapper_->TearDown();

  if (runtime_profiler_ != NULL) {
    delete runtime_profiler_;
    runtime_profiler_ = NULL;
  }

  delete basic_block_profiler_;
  basic_block_profiler_ = NULL;

  heap_.TearDown();
  logger_->TearDown();

  cancelable_task_manager()->CancelAndWait();

  delete heap_profiler_;
  heap_profiler_ = NULL;
  delete cpu_profiler_;
  cpu_profiler_ = NULL;

  delete root_index_map_;
  root_index_map_ = NULL;

  ClearSerializerData();
}


void Isolate::SetIsolateThreadLocals(Isolate* isolate,
                                     PerIsolateThreadData* data) {
  base::Thread::SetThreadLocal(isolate_key_, isolate);
  base::Thread::SetThreadLocal(per_isolate_thread_data_key_, data);
}


Isolate::~Isolate() {
  TRACE_ISOLATE(destructor);

  // Has to be called while counters_ are still alive
  runtime_zone_.DeleteKeptSegment();

  // The entry stack must be empty when we get here.
  DCHECK(entry_stack_ == NULL || entry_stack_->previous_item == NULL);

  delete entry_stack_;
  entry_stack_ = NULL;

  delete unicode_cache_;
  unicode_cache_ = NULL;

  delete date_cache_;
  date_cache_ = NULL;

  delete[] call_descriptor_data_;
  call_descriptor_data_ = NULL;

  delete regexp_stack_;
  regexp_stack_ = NULL;

  delete descriptor_lookup_cache_;
  descriptor_lookup_cache_ = NULL;
  delete context_slot_cache_;
  context_slot_cache_ = NULL;
  delete keyed_lookup_cache_;
  keyed_lookup_cache_ = NULL;

  delete stub_cache_;
  stub_cache_ = NULL;
  delete code_aging_helper_;
  code_aging_helper_ = NULL;
  delete stats_table_;
  stats_table_ = NULL;

  delete materialized_object_store_;
  materialized_object_store_ = NULL;

  delete logger_;
  logger_ = NULL;

  delete counters_;
  counters_ = NULL;

  delete handle_scope_implementer_;
  handle_scope_implementer_ = NULL;

  delete code_tracer();
  set_code_tracer(NULL);

  delete compilation_cache_;
  compilation_cache_ = NULL;
  delete bootstrapper_;
  bootstrapper_ = NULL;
  delete inner_pointer_to_code_cache_;
  inner_pointer_to_code_cache_ = NULL;

  delete thread_manager_;
  thread_manager_ = NULL;

  delete memory_allocator_;
  memory_allocator_ = NULL;
  delete code_range_;
  code_range_ = NULL;
  delete global_handles_;
  global_handles_ = NULL;
  delete eternal_handles_;
  eternal_handles_ = NULL;

  delete string_stream_debug_object_cache_;
  string_stream_debug_object_cache_ = NULL;

  delete random_number_generator_;
  random_number_generator_ = NULL;

  delete debug_;
  debug_ = NULL;

  delete cancelable_task_manager_;
  cancelable_task_manager_ = nullptr;

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
           thread_local_top_.pending_message_obj_->IsTheHole());
    handler->can_continue_ = true;
    handler->has_terminated_ = false;
    handler->exception_ = pending_exception();
    // Propagate to the external try-catch only if we got an actual message.
    if (thread_local_top_.pending_message_obj_->IsTheHole()) return true;

    handler->message_obj_ = thread_local_top_.pending_message_obj_;
  }
  return true;
}


void Isolate::InitializeLoggingAndCounters() {
  if (logger_ == NULL) {
    logger_ = new Logger(this);
  }
  if (counters_ == NULL) {
    counters_ = new Counters(this);
  }
}


bool Isolate::Init(Deserializer* des) {
  TRACE_ISOLATE(init);

  stress_deopt_count_ = FLAG_deopt_every_n_times;

  has_fatal_error_ = false;

  if (function_entry_hook() != NULL) {
    // When function entry hooking is in effect, we have to create the code
    // stubs from scratch to get entry hooks, rather than loading the previously
    // generated stubs from disk.
    // If this assert fires, the initialization path has regressed.
    DCHECK(des == NULL);
  }

  // The initialization process does not handle memory exhaustion.
  AlwaysAllocateScope always_allocate(this);

  memory_allocator_ = new MemoryAllocator(this);
  code_range_ = new CodeRange(this);

  // Safe after setting Heap::isolate_, and initializing StackGuard
  heap_.SetStackLimits();

#define ASSIGN_ELEMENT(CamelName, hacker_name)                  \
  isolate_addresses_[Isolate::k##CamelName##Address] =          \
      reinterpret_cast<Address>(hacker_name##_address());
  FOR_EACH_ISOLATE_ADDRESS_NAME(ASSIGN_ELEMENT)
#undef ASSIGN_ELEMENT

  compilation_cache_ = new CompilationCache(this);
  keyed_lookup_cache_ = new KeyedLookupCache();
  context_slot_cache_ = new ContextSlotCache();
  descriptor_lookup_cache_ = new DescriptorLookupCache();
  unicode_cache_ = new UnicodeCache();
  inner_pointer_to_code_cache_ = new InnerPointerToCodeCache(this);
  global_handles_ = new GlobalHandles(this);
  eternal_handles_ = new EternalHandles();
  bootstrapper_ = new Bootstrapper(this);
  handle_scope_implementer_ = new HandleScopeImplementer(this);
  stub_cache_ = new StubCache(this);
  materialized_object_store_ = new MaterializedObjectStore(this);
  regexp_stack_ = new RegExpStack();
  regexp_stack_->isolate_ = this;
  date_cache_ = new DateCache();
  call_descriptor_data_ =
      new CallInterfaceDescriptorData[CallDescriptors::NUMBER_OF_DESCRIPTORS];
  cpu_profiler_ = new CpuProfiler(this);
  heap_profiler_ = new HeapProfiler(heap());
  interpreter_ = new interpreter::Interpreter(this);

  // Enable logging before setting up the heap
  logger_->SetUp(this);

  // Initialize other runtime facilities
#if defined(USE_SIMULATOR)
#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_PPC
  Simulator::Initialize(this);
#endif
#endif

  code_aging_helper_ = new CodeAgingHelper(this);

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

  deoptimizer_data_ = new DeoptimizerData(memory_allocator_);

  const bool create_heap_objects = (des == NULL);
  if (create_heap_objects && !heap_.CreateHeapObjects()) {
    V8::FatalProcessOutOfMemory("heap object creation");
    return false;
  }

  if (create_heap_objects) {
    // Terminate the cache array with the sentinel so we can iterate.
    partial_snapshot_cache_.Add(heap_.undefined_value());
  }

  InitializeThreadLocal();

  bootstrapper_->Initialize(create_heap_objects);
  builtins_.SetUp(this, create_heap_objects);

  if (FLAG_log_internal_timer_events) {
    set_event_logger(Logger::DefaultEventLoggerSentinel);
  }

  if (FLAG_trace_hydrogen || FLAG_trace_hydrogen_stubs) {
    PrintF("Concurrent recompilation has been disabled for tracing.\n");
  } else if (OptimizingCompileDispatcher::Enabled()) {
    optimizing_compile_dispatcher_ = new OptimizingCompileDispatcher(this);
  }

  // Initialize runtime profiler before deserialization, because collections may
  // occur, clearing/updating ICs.
  runtime_profiler_ = new RuntimeProfiler(this);

  // If we are deserializing, read the state into the now-empty heap.
  if (!create_heap_objects) {
    des->Deserialize(this);
  }
  stub_cache_->Initialize();

  if (FLAG_ignition) {
    interpreter_->Initialize();
  }

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
  CHECK_EQ(static_cast<int>(
               OFFSET_OF(Isolate, heap_.amount_of_external_allocated_memory_)),
           Internals::kAmountOfExternalAllocatedMemoryOffset);
  CHECK_EQ(static_cast<int>(OFFSET_OF(
               Isolate,
               heap_.amount_of_external_allocated_memory_at_last_global_gc_)),
           Internals::kAmountOfExternalAllocatedMemoryAtLastGlobalGCOffset);

  time_millis_at_init_ = heap_.MonotonicallyIncreasingTimeInMs();

  heap_.NotifyDeserializationComplete();

  if (!create_heap_objects) {
    // Now that the heap is consistent, it's OK to generate the code for the
    // deopt entry table that might have been referred to by optimized code in
    // the snapshot.
    HandleScope scope(this);
    Deoptimizer::EnsureCodeForDeoptimizationEntry(
        this,
        Deoptimizer::LAZY,
        kDeoptTableSerializeEntryCount - 1);
  }

  if (!serializer_enabled()) {
    // Ensure that all stubs which need to be generated ahead of time, but
    // cannot be serialized into the snapshot have been generated.
    HandleScope scope(this);
    CodeStub::GenerateFPStubs(this);
    StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(this);
    StubFailureTrampolineStub::GenerateAheadOfTime(this);
  }

  initialized_from_snapshot_ = (des != NULL);

  if (!FLAG_inline_new) heap_.DisableInlineAllocation();

  return true;
}


// Initialized lazily to allow early
// v8::V8::SetAddHistogramSampleFunction calls.
StatsTable* Isolate::stats_table() {
  if (stats_table_ == NULL) {
    stats_table_ = new StatsTable;
  }
  return stats_table_;
}


void Isolate::Enter() {
  Isolate* current_isolate = NULL;
  PerIsolateThreadData* current_data = CurrentPerIsolateThreadData();
  if (current_data != NULL) {
    current_isolate = current_data->isolate_;
    DCHECK(current_isolate != NULL);
    if (current_isolate == this) {
      DCHECK(Current() == this);
      DCHECK(entry_stack_ != NULL);
      DCHECK(entry_stack_->previous_thread_data == NULL ||
             entry_stack_->previous_thread_data->thread_id().Equals(
                 ThreadId::Current()));
      // Same thread re-enters the isolate, no need to re-init anything.
      entry_stack_->entry_count++;
      return;
    }
  }

  PerIsolateThreadData* data = FindOrAllocatePerThreadDataForThisThread();
  DCHECK(data != NULL);
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
  DCHECK(entry_stack_ != NULL);
  DCHECK(entry_stack_->previous_thread_data == NULL ||
         entry_stack_->previous_thread_data->thread_id().Equals(
             ThreadId::Current()));

  if (--entry_stack_->entry_count > 0) return;

  DCHECK(CurrentPerIsolateThreadData() != NULL);
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
  if (deferred_handles_head_ != NULL) {
    deferred_handles_head_->previous_ = deferred;
  }
  deferred_handles_head_ = deferred;
}


void Isolate::UnlinkDeferredHandles(DeferredHandles* deferred) {
#ifdef DEBUG
  // In debug mode assert that the linked list is well-formed.
  DeferredHandles* deferred_iterator = deferred;
  while (deferred_iterator->previous_ != NULL) {
    deferred_iterator = deferred_iterator->previous_;
  }
  DCHECK(deferred_handles_head_ == deferred_iterator);
#endif
  if (deferred_handles_head_ == deferred) {
    deferred_handles_head_ = deferred_handles_head_->next_;
  }
  if (deferred->next_ != NULL) {
    deferred->next_->previous_ = deferred->previous_;
  }
  if (deferred->previous_ != NULL) {
    deferred->previous_->next_ = deferred->next_;
  }
}


void Isolate::DumpAndResetCompilationStats() {
  if (turbo_statistics() != nullptr) {
    OFStream os(stdout);
    os << *turbo_statistics() << std::endl;
  }
  if (hstatistics() != nullptr) hstatistics()->Print();
  delete turbo_statistics_;
  turbo_statistics_ = nullptr;
  delete hstatistics_;
  hstatistics_ = nullptr;
}


HStatistics* Isolate::GetHStatistics() {
  if (hstatistics() == NULL) set_hstatistics(new HStatistics());
  return hstatistics();
}


CompilationStatistics* Isolate::GetTurboStatistics() {
  if (turbo_statistics() == NULL)
    set_turbo_statistics(new CompilationStatistics());
  return turbo_statistics();
}


HTracer* Isolate::GetHTracer() {
  if (htracer() == NULL) set_htracer(new HTracer(id()));
  return htracer();
}


CodeTracer* Isolate::GetCodeTracer() {
  if (code_tracer() == NULL) set_code_tracer(new CodeTracer(id()));
  return code_tracer();
}


Map* Isolate::get_initial_js_array_map(ElementsKind kind, Strength strength) {
  if (IsFastElementsKind(kind)) {
    DisallowHeapAllocation no_gc;
    Object* const initial_js_array_map = context()->native_context()->get(
        Context::ArrayMapIndex(kind, strength));
    if (!initial_js_array_map->IsUndefined()) {
      return Map::cast(initial_js_array_map);
    }
  }
  return nullptr;
}


bool Isolate::use_crankshaft() const {
  return FLAG_crankshaft &&
         !serializer_enabled_ &&
         CpuFeatures::SupportsCrankshaft();
}


bool Isolate::IsFastArrayConstructorPrototypeChainIntact() {
  PropertyCell* no_elements_cell = heap()->array_protector();
  bool cell_reports_intact =
      no_elements_cell->value()->IsSmi() &&
      Smi::cast(no_elements_cell->value())->value() == kArrayProtectorValid;

#ifdef DEBUG
  Map* root_array_map =
      get_initial_js_array_map(GetInitialFastElementsKind());
  Context* native_context = context()->native_context();
  JSObject* initial_array_proto = JSObject::cast(
      native_context->get(Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
  JSObject* initial_object_proto = JSObject::cast(
      native_context->get(Context::INITIAL_OBJECT_PROTOTYPE_INDEX));

  if (root_array_map == NULL || initial_array_proto == initial_object_proto) {
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

  // Check that the object prototype hasn't been altered WRT empty elements.
  PrototypeIterator iter(this, initial_array_proto);
  if (iter.IsAtEnd() || iter.GetCurrent() != initial_object_proto) {
    DCHECK_EQ(false, cell_reports_intact);
    return cell_reports_intact;
  }

  elements = initial_object_proto->elements();
  if (elements != heap()->empty_fixed_array() &&
      elements != heap()->empty_slow_element_dictionary()) {
    DCHECK_EQ(false, cell_reports_intact);
    return cell_reports_intact;
  }

  iter.Advance();
  if (!iter.IsAtEnd()) {
    DCHECK_EQ(false, cell_reports_intact);
    return cell_reports_intact;
  }

#endif

  return cell_reports_intact;
}


void Isolate::UpdateArrayProtectorOnSetElement(Handle<JSObject> object) {
  if (IsFastArrayConstructorPrototypeChainIntact() &&
      object->map()->is_prototype_map()) {
    Object* context = heap()->native_contexts_list();
    while (!context->IsUndefined()) {
      Context* current_context = Context::cast(context);
      if (current_context->get(Context::INITIAL_OBJECT_PROTOTYPE_INDEX) ==
              *object ||
          current_context->get(Context::INITIAL_ARRAY_PROTOTYPE_INDEX) ==
              *object) {
        PropertyCell::SetValueWithInvalidation(
            factory()->array_protector(),
            handle(Smi::FromInt(kArrayProtectorInvalid), this));
        break;
      }
      context = current_context->get(Context::NEXT_CONTEXT_LINK);
    }
  }
}


bool Isolate::IsAnyInitialArrayPrototype(Handle<JSArray> array) {
  if (array->map()->is_prototype_map()) {
    Object* context = heap()->native_contexts_list();
    while (!context->IsUndefined()) {
      Context* current_context = Context::cast(context);
      if (current_context->get(Context::INITIAL_ARRAY_PROTOTYPE_INDEX) ==
          *array) {
        return true;
      }
      context = current_context->get(Context::NEXT_CONTEXT_LINK);
    }
  }
  return false;
}


CallInterfaceDescriptorData* Isolate::call_descriptor_data(int index) {
  DCHECK(0 <= index && index < CallDescriptors::NUMBER_OF_DESCRIPTORS);
  return &call_descriptor_data_[index];
}


base::RandomNumberGenerator* Isolate::random_number_generator() {
  if (random_number_generator_ == NULL) {
    if (FLAG_random_seed != 0) {
      random_number_generator_ =
          new base::RandomNumberGenerator(FLAG_random_seed);
    } else {
      random_number_generator_ = new base::RandomNumberGenerator();
    }
  }
  return random_number_generator_;
}


Object* Isolate::FindCodeObject(Address a) {
  return inner_pointer_to_code_cache()->GcSafeFindCodeForInnerPointer(a);
}


#ifdef DEBUG
#define ISOLATE_FIELD_OFFSET(type, name, ignored)                       \
const intptr_t Isolate::name##_debug_offset_ = OFFSET_OF(Isolate, name##_);
ISOLATE_INIT_LIST(ISOLATE_FIELD_OFFSET)
ISOLATE_INIT_ARRAY_LIST(ISOLATE_FIELD_OFFSET)
#undef ISOLATE_FIELD_OFFSET
#endif


Handle<JSObject> Isolate::SetUpSubregistry(Handle<JSObject> registry,
                                           Handle<Map> map, const char* cname) {
  Handle<String> name = factory()->InternalizeUtf8String(cname);
  Handle<JSObject> obj = factory()->NewJSObjectFromMap(map);
  JSObject::NormalizeProperties(obj, CLEAR_INOBJECT_PROPERTIES, 0,
                                "SetupSymbolRegistry");
  JSObject::AddProperty(registry, name, obj, NONE);
  return obj;
}


Handle<JSObject> Isolate::GetSymbolRegistry() {
  if (heap()->symbol_registry()->IsSmi()) {
    Handle<Map> map = factory()->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    Handle<JSObject> registry = factory()->NewJSObjectFromMap(map);
    heap()->set_symbol_registry(*registry);

    SetUpSubregistry(registry, map, "for");
    SetUpSubregistry(registry, map, "for_api");
    SetUpSubregistry(registry, map, "keyFor");
    SetUpSubregistry(registry, map, "private_api");
  }
  return Handle<JSObject>::cast(factory()->symbol_registry());
}


void Isolate::AddCallCompletedCallback(CallCompletedCallback callback) {
  for (int i = 0; i < call_completed_callbacks_.length(); i++) {
    if (callback == call_completed_callbacks_.at(i)) return;
  }
  call_completed_callbacks_.Add(callback);
}


void Isolate::RemoveCallCompletedCallback(CallCompletedCallback callback) {
  for (int i = 0; i < call_completed_callbacks_.length(); i++) {
    if (callback == call_completed_callbacks_.at(i)) {
      call_completed_callbacks_.Remove(i);
    }
  }
}


void Isolate::FireCallCompletedCallback() {
  bool has_call_completed_callbacks = !call_completed_callbacks_.is_empty();
  bool run_microtasks = autorun_microtasks() && pending_microtask_count();
  if (!has_call_completed_callbacks && !run_microtasks) return;

  if (!handle_scope_implementer()->CallDepthIsZero()) return;
  if (run_microtasks) RunMicrotasks();
  // Fire callbacks.  Increase call depth to prevent recursive callbacks.
  v8::Isolate::SuppressMicrotaskExecutionScope suppress(
      reinterpret_cast<v8::Isolate*>(this));
  for (int i = 0; i < call_completed_callbacks_.length(); i++) {
    call_completed_callbacks_.at(i)();
  }
}


void Isolate::SetPromiseRejectCallback(PromiseRejectCallback callback) {
  promise_reject_callback_ = callback;
}


void Isolate::ReportPromiseReject(Handle<JSObject> promise,
                                  Handle<Object> value,
                                  v8::PromiseRejectEvent event) {
  if (promise_reject_callback_ == NULL) return;
  Handle<JSArray> stack_trace;
  if (event == v8::kPromiseRejectWithNoHandler && value->IsJSObject()) {
    stack_trace = GetDetailedStackTrace(Handle<JSObject>::cast(value));
  }
  promise_reject_callback_(v8::PromiseRejectMessage(
      v8::Utils::PromiseToLocal(promise), event, v8::Utils::ToLocal(value),
      v8::Utils::StackTraceToLocal(stack_trace)));
}


void Isolate::EnqueueMicrotask(Handle<Object> microtask) {
  DCHECK(microtask->IsJSFunction() || microtask->IsCallHandlerInfo());
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
  DCHECK(queue->get(num_tasks)->IsUndefined());
  queue->set(num_tasks, *microtask);
  set_pending_microtask_count(num_tasks + 1);
}


void Isolate::RunMicrotasks() {
  // Increase call depth to prevent recursive callbacks.
  v8::Isolate::SuppressMicrotaskExecutionScope suppress(
      reinterpret_cast<v8::Isolate*>(this));

  while (pending_microtask_count() > 0) {
    HandleScope scope(this);
    int num_tasks = pending_microtask_count();
    Handle<FixedArray> queue(heap()->microtask_queue(), this);
    DCHECK(num_tasks <= queue->length());
    set_pending_microtask_count(0);
    heap()->set_microtask_queue(heap()->empty_fixed_array());

    for (int i = 0; i < num_tasks; i++) {
      HandleScope scope(this);
      Handle<Object> microtask(queue->get(i), this);
      if (microtask->IsJSFunction()) {
        Handle<JSFunction> microtask_function =
            Handle<JSFunction>::cast(microtask);
        SaveContext save(this);
        set_context(microtask_function->context()->native_context());
        MaybeHandle<Object> maybe_exception;
        MaybeHandle<Object> result = Execution::TryCall(
            this, microtask_function, factory()->undefined_value(), 0, NULL,
            &maybe_exception);
        // If execution is terminating, just bail out.
        Handle<Object> exception;
        if (result.is_null() && maybe_exception.is_null()) {
          // Clear out any remaining callbacks in the queue.
          heap()->set_microtask_queue(heap()->empty_fixed_array());
          set_pending_microtask_count(0);
          return;
        }
      } else {
        Handle<CallHandlerInfo> callback_info =
            Handle<CallHandlerInfo>::cast(microtask);
        v8::MicrotaskCallback callback =
            v8::ToCData<v8::MicrotaskCallback>(callback_info->callback());
        void* data = v8::ToCData<void*>(callback_info->data());
        callback(data);
      }
    }
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
  if (basic_block_profiler_ == NULL) {
    basic_block_profiler_ = new BasicBlockProfiler();
  }
  return basic_block_profiler_;
}


std::string Isolate::GetTurboCfgFileName() {
  if (FLAG_trace_turbo_cfg_file == NULL) {
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
  Handle<FixedArray> detached_contexts(heap()->detached_contexts());
  int length = detached_contexts->length();
  detached_contexts = factory()->CopyFixedArrayAndGrow(detached_contexts, 2);
  detached_contexts->set(length, Smi::FromInt(0));
  detached_contexts->set(length + 1, *cell);
  heap()->set_detached_contexts(*detached_contexts);
}


void Isolate::CheckDetachedContextsAfterGC() {
  HandleScope scope(this);
  Handle<FixedArray> detached_contexts(heap()->detached_contexts());
  int length = detached_contexts->length();
  if (length == 0) return;
  int new_length = 0;
  for (int i = 0; i < length; i += 2) {
    int mark_sweeps = Smi::cast(detached_contexts->get(i))->value();
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
      int mark_sweeps = Smi::cast(detached_contexts->get(i))->value();
      DCHECK(detached_contexts->get(i + 1)->IsWeakCell());
      WeakCell* cell = WeakCell::cast(detached_contexts->get(i + 1));
      if (mark_sweeps > 3) {
        PrintF("detached context 0x%p\n survived %d GCs (leak?)\n",
               static_cast<void*>(cell->value()), mark_sweeps);
      }
    }
  }
  if (new_length == 0) {
    heap()->set_detached_contexts(heap()->empty_fixed_array());
  } else if (new_length < length) {
    heap()->RightTrimFixedArray<Heap::CONCURRENT_TO_SWEEPER>(
        *detached_contexts, length - new_length);
  }
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
  if (isolate->context() != NULL) {
    context_ = Handle<Context>(isolate->context());
  }
  isolate->set_save_context(this);

  c_entry_fp_ = isolate->c_entry_fp(isolate->thread_local_top());
}


SaveContext::~SaveContext() {
  isolate_->set_context(context_.is_null() ? NULL : *context_);
  isolate_->set_save_context(prev_);
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
