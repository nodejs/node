// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "api.h"
#include "bootstrapper.h"
#include "debug.h"
#include "execution.h"
#include "messages.h"
#include "platform.h"
#include "simulator.h"
#include "string-stream.h"

namespace v8 {
namespace internal {

ThreadLocalTop Top::thread_local_;
Mutex* Top::break_access_ = OS::CreateMutex();

NoAllocationStringAllocator* preallocated_message_space = NULL;

bool capture_stack_trace_for_uncaught_exceptions = false;
int stack_trace_for_uncaught_exceptions_frame_limit = 0;
StackTrace::StackTraceOptions stack_trace_for_uncaught_exceptions_options =
    StackTrace::kOverview;

Address top_addresses[] = {
#define C(name) reinterpret_cast<Address>(Top::name()),
    TOP_ADDRESS_LIST(C)
    TOP_ADDRESS_LIST_PROF(C)
#undef C
    NULL
};


v8::TryCatch* ThreadLocalTop::TryCatchHandler() {
  return TRY_CATCH_FROM_ADDRESS(try_catch_handler_address());
}


void ThreadLocalTop::Initialize() {
  c_entry_fp_ = 0;
  handler_ = 0;
#ifdef ENABLE_LOGGING_AND_PROFILING
  js_entry_sp_ = 0;
#endif
  stack_is_cooked_ = false;
  try_catch_handler_address_ = NULL;
  context_ = NULL;
  int id = ThreadManager::CurrentId();
  thread_id_ = (id == 0) ? ThreadManager::kInvalidId : id;
  external_caught_exception_ = false;
  failed_access_check_callback_ = NULL;
  save_context_ = NULL;
  catcher_ = NULL;
}


Address Top::get_address_from_id(Top::AddressId id) {
  return top_addresses[id];
}


char* Top::Iterate(ObjectVisitor* v, char* thread_storage) {
  ThreadLocalTop* thread = reinterpret_cast<ThreadLocalTop*>(thread_storage);
  Iterate(v, thread);
  return thread_storage + sizeof(ThreadLocalTop);
}


void Top::IterateThread(ThreadVisitor* v) {
  v->VisitThread(&thread_local_);
}


void Top::IterateThread(ThreadVisitor* v, char* t) {
  ThreadLocalTop* thread = reinterpret_cast<ThreadLocalTop*>(t);
  v->VisitThread(thread);
}


void Top::Iterate(ObjectVisitor* v, ThreadLocalTop* thread) {
  v->VisitPointer(&(thread->pending_exception_));
  v->VisitPointer(&(thread->pending_message_obj_));
  v->VisitPointer(BitCast<Object**>(&(thread->pending_message_script_)));
  v->VisitPointer(BitCast<Object**>(&(thread->context_)));
  v->VisitPointer(&(thread->scheduled_exception_));

  for (v8::TryCatch* block = thread->TryCatchHandler();
       block != NULL;
       block = TRY_CATCH_FROM_ADDRESS(block->next_)) {
    v->VisitPointer(BitCast<Object**>(&(block->exception_)));
    v->VisitPointer(BitCast<Object**>(&(block->message_)));
  }

  // Iterate over pointers on native execution stack.
  for (StackFrameIterator it(thread); !it.done(); it.Advance()) {
    it.frame()->Iterate(v);
  }
}


void Top::Iterate(ObjectVisitor* v) {
  ThreadLocalTop* current_t = &thread_local_;
  Iterate(v, current_t);
}


void Top::InitializeThreadLocal() {
  thread_local_.Initialize();
  clear_pending_exception();
  clear_pending_message();
  clear_scheduled_exception();
}


// Create a dummy thread that will wait forever on a semaphore. The only
// purpose for this thread is to have some stack area to save essential data
// into for use by a stacks only core dump (aka minidump).
class PreallocatedMemoryThread: public Thread {
 public:
  PreallocatedMemoryThread() : keep_running_(true) {
    wait_for_ever_semaphore_ = OS::CreateSemaphore(0);
    data_ready_semaphore_ = OS::CreateSemaphore(0);
  }

  // When the thread starts running it will allocate a fixed number of bytes
  // on the stack and publish the location of this memory for others to use.
  void Run() {
    EmbeddedVector<char, 15 * 1024> local_buffer;

    // Initialize the buffer with a known good value.
    OS::StrNCpy(local_buffer, "Trace data was not generated.\n",
                local_buffer.length());

    // Publish the local buffer and signal its availability.
    data_ = local_buffer.start();
    length_ = local_buffer.length();
    data_ready_semaphore_->Signal();

    while (keep_running_) {
      // This thread will wait here until the end of time.
      wait_for_ever_semaphore_->Wait();
    }

    // Make sure we access the buffer after the wait to remove all possibility
    // of it being optimized away.
    OS::StrNCpy(local_buffer, "PreallocatedMemoryThread shutting down.\n",
                local_buffer.length());
  }

  static char* data() {
    if (data_ready_semaphore_ != NULL) {
      // Initial access is guarded until the data has been published.
      data_ready_semaphore_->Wait();
      delete data_ready_semaphore_;
      data_ready_semaphore_ = NULL;
    }
    return data_;
  }

  static unsigned length() {
    if (data_ready_semaphore_ != NULL) {
      // Initial access is guarded until the data has been published.
      data_ready_semaphore_->Wait();
      delete data_ready_semaphore_;
      data_ready_semaphore_ = NULL;
    }
    return length_;
  }

  static void StartThread() {
    if (the_thread_ != NULL) return;

    the_thread_ = new PreallocatedMemoryThread();
    the_thread_->Start();
  }

  // Stop the PreallocatedMemoryThread and release its resources.
  static void StopThread() {
    if (the_thread_ == NULL) return;

    the_thread_->keep_running_ = false;
    wait_for_ever_semaphore_->Signal();

    // Wait for the thread to terminate.
    the_thread_->Join();

    if (data_ready_semaphore_ != NULL) {
      delete data_ready_semaphore_;
      data_ready_semaphore_ = NULL;
    }

    delete wait_for_ever_semaphore_;
    wait_for_ever_semaphore_ = NULL;

    // Done with the thread entirely.
    delete the_thread_;
    the_thread_ = NULL;
  }

 private:
  // Used to make sure that the thread keeps looping even for spurious wakeups.
  bool keep_running_;

  // The preallocated memory thread singleton.
  static PreallocatedMemoryThread* the_thread_;
  // This semaphore is used by the PreallocatedMemoryThread to wait for ever.
  static Semaphore* wait_for_ever_semaphore_;
  // Semaphore to signal that the data has been initialized.
  static Semaphore* data_ready_semaphore_;

  // Location and size of the preallocated memory block.
  static char* data_;
  static unsigned length_;

  DISALLOW_COPY_AND_ASSIGN(PreallocatedMemoryThread);
};

PreallocatedMemoryThread* PreallocatedMemoryThread::the_thread_ = NULL;
Semaphore* PreallocatedMemoryThread::wait_for_ever_semaphore_ = NULL;
Semaphore* PreallocatedMemoryThread::data_ready_semaphore_ = NULL;
char* PreallocatedMemoryThread::data_ = NULL;
unsigned PreallocatedMemoryThread::length_ = 0;

static bool initialized = false;

void Top::Initialize() {
  CHECK(!initialized);

  InitializeThreadLocal();

  // Only preallocate on the first initialization.
  if (FLAG_preallocate_message_memory && (preallocated_message_space == NULL)) {
    // Start the thread which will set aside some memory.
    PreallocatedMemoryThread::StartThread();
    preallocated_message_space =
        new NoAllocationStringAllocator(PreallocatedMemoryThread::data(),
                                        PreallocatedMemoryThread::length());
    PreallocatedStorage::Init(PreallocatedMemoryThread::length() / 4);
  }
  initialized = true;
}


void Top::TearDown() {
  if (initialized) {
    // Remove the external reference to the preallocated stack memory.
    if (preallocated_message_space != NULL) {
      delete preallocated_message_space;
      preallocated_message_space = NULL;
    }

    PreallocatedMemoryThread::StopThread();
    initialized = false;
  }
}


void Top::RegisterTryCatchHandler(v8::TryCatch* that) {
  // The ARM simulator has a separate JS stack.  We therefore register
  // the C++ try catch handler with the simulator and get back an
  // address that can be used for comparisons with addresses into the
  // JS stack.  When running without the simulator, the address
  // returned will be the address of the C++ try catch handler itself.
  Address address = reinterpret_cast<Address>(
      SimulatorStack::RegisterCTryCatch(reinterpret_cast<uintptr_t>(that)));
  thread_local_.set_try_catch_handler_address(address);
}


void Top::UnregisterTryCatchHandler(v8::TryCatch* that) {
  ASSERT(thread_local_.TryCatchHandler() == that);
  thread_local_.set_try_catch_handler_address(
      reinterpret_cast<Address>(that->next_));
  thread_local_.catcher_ = NULL;
  SimulatorStack::UnregisterCTryCatch();
}


void Top::MarkCompactPrologue(bool is_compacting) {
  MarkCompactPrologue(is_compacting, &thread_local_);
}


void Top::MarkCompactPrologue(bool is_compacting, char* data) {
  MarkCompactPrologue(is_compacting, reinterpret_cast<ThreadLocalTop*>(data));
}


void Top::MarkCompactPrologue(bool is_compacting, ThreadLocalTop* thread) {
  if (is_compacting) {
    StackFrame::CookFramesForThread(thread);
  }
}


void Top::MarkCompactEpilogue(bool is_compacting, char* data) {
  MarkCompactEpilogue(is_compacting, reinterpret_cast<ThreadLocalTop*>(data));
}


void Top::MarkCompactEpilogue(bool is_compacting) {
  MarkCompactEpilogue(is_compacting, &thread_local_);
}


void Top::MarkCompactEpilogue(bool is_compacting, ThreadLocalTop* thread) {
  if (is_compacting) {
    StackFrame::UncookFramesForThread(thread);
  }
}


static int stack_trace_nesting_level = 0;
static StringStream* incomplete_message = NULL;


Handle<String> Top::StackTraceString() {
  if (stack_trace_nesting_level == 0) {
    stack_trace_nesting_level++;
    HeapStringAllocator allocator;
    StringStream::ClearMentionedObjectCache();
    StringStream accumulator(&allocator);
    incomplete_message = &accumulator;
    PrintStack(&accumulator);
    Handle<String> stack_trace = accumulator.ToString();
    incomplete_message = NULL;
    stack_trace_nesting_level = 0;
    return stack_trace;
  } else if (stack_trace_nesting_level == 1) {
    stack_trace_nesting_level++;
    OS::PrintError(
      "\n\nAttempt to print stack while printing stack (double fault)\n");
    OS::PrintError(
      "If you are lucky you may find a partial stack dump on stdout.\n\n");
    incomplete_message->OutputToStdOut();
    return Factory::empty_symbol();
  } else {
    OS::Abort();
    // Unreachable
    return Factory::empty_symbol();
  }
}


Handle<JSArray> Top::CaptureCurrentStackTrace(
    int frame_limit, StackTrace::StackTraceOptions options) {
  // Ensure no negative values.
  int limit = Max(frame_limit, 0);
  Handle<JSArray> stack_trace = Factory::NewJSArray(frame_limit);

  Handle<String> column_key =  Factory::LookupAsciiSymbol("column");
  Handle<String> line_key =  Factory::LookupAsciiSymbol("lineNumber");
  Handle<String> script_key =  Factory::LookupAsciiSymbol("scriptName");
  Handle<String> function_key =  Factory::LookupAsciiSymbol("functionName");
  Handle<String> eval_key =  Factory::LookupAsciiSymbol("isEval");
  Handle<String> constructor_key =  Factory::LookupAsciiSymbol("isConstructor");

  StackTraceFrameIterator it;
  int frames_seen = 0;
  while (!it.done() && (frames_seen < limit)) {
    // Create a JSObject to hold the information for the StackFrame.
    Handle<JSObject> stackFrame = Factory::NewJSObject(object_function());

    JavaScriptFrame* frame = it.frame();
    JSFunction* fun(JSFunction::cast(frame->function()));
    Script* script = Script::cast(fun->shared()->script());

    if (options & StackTrace::kLineNumber) {
      int script_line_offset = script->line_offset()->value();
      int position = frame->code()->SourcePosition(frame->pc());
      int line_number = GetScriptLineNumber(Handle<Script>(script), position);
      // line_number is already shifted by the script_line_offset.
      int relative_line_number = line_number - script_line_offset;
      if (options & StackTrace::kColumnOffset && relative_line_number >= 0) {
        Handle<FixedArray> line_ends(FixedArray::cast(script->line_ends()));
        int start = (relative_line_number == 0) ? 0 :
            Smi::cast(line_ends->get(relative_line_number - 1))->value() + 1;
        int column_offset = position - start;
        if (relative_line_number == 0) {
          // For the case where the code is on the same line as the script tag.
          column_offset += script->column_offset()->value();
        }
        SetProperty(stackFrame, column_key,
                    Handle<Smi>(Smi::FromInt(column_offset + 1)), NONE);
      }
      SetProperty(stackFrame, line_key,
                  Handle<Smi>(Smi::FromInt(line_number + 1)), NONE);
    }

    if (options & StackTrace::kScriptName) {
      Handle<Object> script_name(script->name());
      SetProperty(stackFrame, script_key, script_name, NONE);
    }

    if (options & StackTrace::kFunctionName) {
      Handle<Object> fun_name(fun->shared()->name());
      if (fun_name->ToBoolean()->IsFalse()) {
        fun_name = Handle<Object>(fun->shared()->inferred_name());
      }
      SetProperty(stackFrame, function_key, fun_name, NONE);
    }

    if (options & StackTrace::kIsEval) {
      int type = Smi::cast(script->compilation_type())->value();
      Handle<Object> is_eval = (type == Script::COMPILATION_TYPE_EVAL) ?
          Factory::true_value() : Factory::false_value();
      SetProperty(stackFrame, eval_key, is_eval, NONE);
    }

    if (options & StackTrace::kIsConstructor) {
      Handle<Object> is_constructor = (frame->IsConstructor()) ?
          Factory::true_value() : Factory::false_value();
      SetProperty(stackFrame, constructor_key, is_constructor, NONE);
    }

    FixedArray::cast(stack_trace->elements())->set(frames_seen, *stackFrame);
    frames_seen++;
    it.Advance();
  }

  stack_trace->set_length(Smi::FromInt(frames_seen));
  return stack_trace;
}


void Top::PrintStack() {
  if (stack_trace_nesting_level == 0) {
    stack_trace_nesting_level++;

    StringAllocator* allocator;
    if (preallocated_message_space == NULL) {
      allocator = new HeapStringAllocator();
    } else {
      allocator = preallocated_message_space;
    }

    NativeAllocationChecker allocation_checker(
      !FLAG_preallocate_message_memory ?
      NativeAllocationChecker::ALLOW :
      NativeAllocationChecker::DISALLOW);

    StringStream::ClearMentionedObjectCache();
    StringStream accumulator(allocator);
    incomplete_message = &accumulator;
    PrintStack(&accumulator);
    accumulator.OutputToStdOut();
    accumulator.Log();
    incomplete_message = NULL;
    stack_trace_nesting_level = 0;
    if (preallocated_message_space == NULL) {
      // Remove the HeapStringAllocator created above.
      delete allocator;
    }
  } else if (stack_trace_nesting_level == 1) {
    stack_trace_nesting_level++;
    OS::PrintError(
      "\n\nAttempt to print stack while printing stack (double fault)\n");
    OS::PrintError(
      "If you are lucky you may find a partial stack dump on stdout.\n\n");
    incomplete_message->OutputToStdOut();
  }
}


static void PrintFrames(StringStream* accumulator,
                        StackFrame::PrintMode mode) {
  StackFrameIterator it;
  for (int i = 0; !it.done(); it.Advance()) {
    it.frame()->Print(accumulator, mode, i++);
  }
}


void Top::PrintStack(StringStream* accumulator) {
  // The MentionedObjectCache is not GC-proof at the moment.
  AssertNoAllocation nogc;
  ASSERT(StringStream::IsMentionedObjectCacheClear());

  // Avoid printing anything if there are no frames.
  if (c_entry_fp(GetCurrentThread()) == 0) return;

  accumulator->Add(
      "\n==== Stack trace ============================================\n\n");
  PrintFrames(accumulator, StackFrame::OVERVIEW);

  accumulator->Add(
      "\n==== Details ================================================\n\n");
  PrintFrames(accumulator, StackFrame::DETAILS);

  accumulator->PrintMentionedObjectCache();
  accumulator->Add("=====================\n\n");
}


void Top::SetFailedAccessCheckCallback(v8::FailedAccessCheckCallback callback) {
  ASSERT(thread_local_.failed_access_check_callback_ == NULL);
  thread_local_.failed_access_check_callback_ = callback;
}


void Top::ReportFailedAccessCheck(JSObject* receiver, v8::AccessType type) {
  if (!thread_local_.failed_access_check_callback_) return;

  ASSERT(receiver->IsAccessCheckNeeded());
  ASSERT(Top::context());
  // The callers of this method are not expecting a GC.
  AssertNoAllocation no_gc;

  // Get the data object from access check info.
  JSFunction* constructor = JSFunction::cast(receiver->map()->constructor());
  if (!constructor->shared()->IsApiFunction()) return;
  Object* data_obj =
      constructor->shared()->get_api_func_data()->access_check_info();
  if (data_obj == Heap::undefined_value()) return;

  HandleScope scope;
  Handle<JSObject> receiver_handle(receiver);
  Handle<Object> data(AccessCheckInfo::cast(data_obj)->data());
  thread_local_.failed_access_check_callback_(
    v8::Utils::ToLocal(receiver_handle),
    type,
    v8::Utils::ToLocal(data));
}


enum MayAccessDecision {
  YES, NO, UNKNOWN
};


static MayAccessDecision MayAccessPreCheck(JSObject* receiver,
                                           v8::AccessType type) {
  // During bootstrapping, callback functions are not enabled yet.
  if (Bootstrapper::IsActive()) return YES;

  if (receiver->IsJSGlobalProxy()) {
    Object* receiver_context = JSGlobalProxy::cast(receiver)->context();
    if (!receiver_context->IsContext()) return NO;

    // Get the global context of current top context.
    // avoid using Top::global_context() because it uses Handle.
    Context* global_context = Top::context()->global()->global_context();
    if (receiver_context == global_context) return YES;

    if (Context::cast(receiver_context)->security_token() ==
        global_context->security_token())
      return YES;
  }

  return UNKNOWN;
}


bool Top::MayNamedAccess(JSObject* receiver, Object* key, v8::AccessType type) {
  ASSERT(receiver->IsAccessCheckNeeded());

  // The callers of this method are not expecting a GC.
  AssertNoAllocation no_gc;

  // Skip checks for hidden properties access.  Note, we do not
  // require existence of a context in this case.
  if (key == Heap::hidden_symbol()) return true;

  // Check for compatibility between the security tokens in the
  // current lexical context and the accessed object.
  ASSERT(Top::context());

  MayAccessDecision decision = MayAccessPreCheck(receiver, type);
  if (decision != UNKNOWN) return decision == YES;

  // Get named access check callback
  JSFunction* constructor = JSFunction::cast(receiver->map()->constructor());
  if (!constructor->shared()->IsApiFunction()) return false;

  Object* data_obj =
     constructor->shared()->get_api_func_data()->access_check_info();
  if (data_obj == Heap::undefined_value()) return false;

  Object* fun_obj = AccessCheckInfo::cast(data_obj)->named_callback();
  v8::NamedSecurityCallback callback =
      v8::ToCData<v8::NamedSecurityCallback>(fun_obj);

  if (!callback) return false;

  HandleScope scope;
  Handle<JSObject> receiver_handle(receiver);
  Handle<Object> key_handle(key);
  Handle<Object> data(AccessCheckInfo::cast(data_obj)->data());
  LOG(ApiNamedSecurityCheck(key));
  bool result = false;
  {
    // Leaving JavaScript.
    VMState state(EXTERNAL);
    result = callback(v8::Utils::ToLocal(receiver_handle),
                      v8::Utils::ToLocal(key_handle),
                      type,
                      v8::Utils::ToLocal(data));
  }
  return result;
}


bool Top::MayIndexedAccess(JSObject* receiver,
                           uint32_t index,
                           v8::AccessType type) {
  ASSERT(receiver->IsAccessCheckNeeded());
  // Check for compatibility between the security tokens in the
  // current lexical context and the accessed object.
  ASSERT(Top::context());
  // The callers of this method are not expecting a GC.
  AssertNoAllocation no_gc;

  MayAccessDecision decision = MayAccessPreCheck(receiver, type);
  if (decision != UNKNOWN) return decision == YES;

  // Get indexed access check callback
  JSFunction* constructor = JSFunction::cast(receiver->map()->constructor());
  if (!constructor->shared()->IsApiFunction()) return false;

  Object* data_obj =
      constructor->shared()->get_api_func_data()->access_check_info();
  if (data_obj == Heap::undefined_value()) return false;

  Object* fun_obj = AccessCheckInfo::cast(data_obj)->indexed_callback();
  v8::IndexedSecurityCallback callback =
      v8::ToCData<v8::IndexedSecurityCallback>(fun_obj);

  if (!callback) return false;

  HandleScope scope;
  Handle<JSObject> receiver_handle(receiver);
  Handle<Object> data(AccessCheckInfo::cast(data_obj)->data());
  LOG(ApiIndexedSecurityCheck(index));
  bool result = false;
  {
    // Leaving JavaScript.
    VMState state(EXTERNAL);
    result = callback(v8::Utils::ToLocal(receiver_handle),
                      index,
                      type,
                      v8::Utils::ToLocal(data));
  }
  return result;
}


const char* Top::kStackOverflowMessage =
  "Uncaught RangeError: Maximum call stack size exceeded";


Failure* Top::StackOverflow() {
  HandleScope scope;
  Handle<String> key = Factory::stack_overflow_symbol();
  Handle<JSObject> boilerplate =
      Handle<JSObject>::cast(GetProperty(Top::builtins(), key));
  Handle<Object> exception = Copy(boilerplate);
  // TODO(1240995): To avoid having to call JavaScript code to compute
  // the message for stack overflow exceptions which is very likely to
  // double fault with another stack overflow exception, we use a
  // precomputed message.
  DoThrow(*exception, NULL, kStackOverflowMessage);
  return Failure::Exception();
}


Failure* Top::TerminateExecution() {
  DoThrow(Heap::termination_exception(), NULL, NULL);
  return Failure::Exception();
}


Failure* Top::Throw(Object* exception, MessageLocation* location) {
  DoThrow(exception, location, NULL);
  return Failure::Exception();
}


Failure* Top::ReThrow(Object* exception, MessageLocation* location) {
  // Set the exception being re-thrown.
  set_pending_exception(exception);
  return Failure::Exception();
}


Failure* Top::ThrowIllegalOperation() {
  return Throw(Heap::illegal_access_symbol());
}


void Top::ScheduleThrow(Object* exception) {
  // When scheduling a throw we first throw the exception to get the
  // error reporting if it is uncaught before rescheduling it.
  Throw(exception);
  thread_local_.scheduled_exception_ = pending_exception();
  thread_local_.external_caught_exception_ = false;
  clear_pending_exception();
}


Object* Top::PromoteScheduledException() {
  Object* thrown = scheduled_exception();
  clear_scheduled_exception();
  // Re-throw the exception to avoid getting repeated error reporting.
  return ReThrow(thrown);
}


void Top::PrintCurrentStackTrace(FILE* out) {
  StackTraceFrameIterator it;
  while (!it.done()) {
    HandleScope scope;
    // Find code position if recorded in relocation info.
    JavaScriptFrame* frame = it.frame();
    int pos = frame->code()->SourcePosition(frame->pc());
    Handle<Object> pos_obj(Smi::FromInt(pos));
    // Fetch function and receiver.
    Handle<JSFunction> fun(JSFunction::cast(frame->function()));
    Handle<Object> recv(frame->receiver());
    // Advance to the next JavaScript frame and determine if the
    // current frame is the top-level frame.
    it.Advance();
    Handle<Object> is_top_level = it.done()
        ? Factory::true_value()
        : Factory::false_value();
    // Generate and print stack trace line.
    Handle<String> line =
        Execution::GetStackTraceLine(recv, fun, pos_obj, is_top_level);
    if (line->length() > 0) {
      line->PrintOn(out);
      fprintf(out, "\n");
    }
  }
}


void Top::ComputeLocation(MessageLocation* target) {
  *target = MessageLocation(Handle<Script>(Heap::empty_script()), -1, -1);
  StackTraceFrameIterator it;
  if (!it.done()) {
    JavaScriptFrame* frame = it.frame();
    JSFunction* fun = JSFunction::cast(frame->function());
    Object* script = fun->shared()->script();
    if (script->IsScript() &&
        !(Script::cast(script)->source()->IsUndefined())) {
      int pos = frame->code()->SourcePosition(frame->pc());
      // Compute the location from the function and the reloc info.
      Handle<Script> casted_script(Script::cast(script));
      *target = MessageLocation(casted_script, pos, pos + 1);
    }
  }
}


bool Top::ShouldReturnException(bool* is_caught_externally,
                                bool catchable_by_javascript) {
  // Find the top-most try-catch handler.
  StackHandler* handler =
      StackHandler::FromAddress(Top::handler(Top::GetCurrentThread()));
  while (handler != NULL && !handler->is_try_catch()) {
    handler = handler->next();
  }

  // Get the address of the external handler so we can compare the address to
  // determine which one is closer to the top of the stack.
  Address external_handler_address = thread_local_.try_catch_handler_address();

  // The exception has been externally caught if and only if there is
  // an external handler which is on top of the top-most try-catch
  // handler.
  *is_caught_externally = external_handler_address != NULL &&
      (handler == NULL || handler->address() > external_handler_address ||
       !catchable_by_javascript);

  if (*is_caught_externally) {
    // Only report the exception if the external handler is verbose.
    return thread_local_.TryCatchHandler()->is_verbose_;
  } else {
    // Report the exception if it isn't caught by JavaScript code.
    return handler == NULL;
  }
}


void Top::DoThrow(Object* exception,
                  MessageLocation* location,
                  const char* message) {
  ASSERT(!has_pending_exception());

  HandleScope scope;
  Handle<Object> exception_handle(exception);

  // Determine reporting and whether the exception is caught externally.
  bool is_caught_externally = false;
  bool is_out_of_memory = exception == Failure::OutOfMemoryException();
  bool is_termination_exception = exception == Heap::termination_exception();
  bool catchable_by_javascript = !is_termination_exception && !is_out_of_memory;
  bool should_return_exception =
      ShouldReturnException(&is_caught_externally, catchable_by_javascript);
  bool report_exception = catchable_by_javascript && should_return_exception;

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Notify debugger of exception.
  if (catchable_by_javascript) {
    Debugger::OnException(exception_handle, report_exception);
  }
#endif

  // Generate the message.
  Handle<Object> message_obj;
  MessageLocation potential_computed_location;
  bool try_catch_needs_message =
      is_caught_externally &&
      thread_local_.TryCatchHandler()->capture_message_;
  if (report_exception || try_catch_needs_message) {
    if (location == NULL) {
      // If no location was specified we use a computed one instead
      ComputeLocation(&potential_computed_location);
      location = &potential_computed_location;
    }
    if (!Bootstrapper::IsActive()) {
      // It's not safe to try to make message objects or collect stack
      // traces while the bootstrapper is active since the infrastructure
      // may not have been properly initialized.
      Handle<String> stack_trace;
      if (FLAG_trace_exception) stack_trace = StackTraceString();
      Handle<JSArray> stack_trace_object;
      if (report_exception && capture_stack_trace_for_uncaught_exceptions) {
          stack_trace_object = Top::CaptureCurrentStackTrace(
              stack_trace_for_uncaught_exceptions_frame_limit,
              stack_trace_for_uncaught_exceptions_options);
      }
      message_obj = MessageHandler::MakeMessageObject("uncaught_exception",
          location, HandleVector<Object>(&exception_handle, 1), stack_trace,
          stack_trace_object);
    }
  }

  // Save the message for reporting if the the exception remains uncaught.
  thread_local_.has_pending_message_ = report_exception;
  thread_local_.pending_message_ = message;
  if (!message_obj.is_null()) {
    thread_local_.pending_message_obj_ = *message_obj;
    if (location != NULL) {
      thread_local_.pending_message_script_ = *location->script();
      thread_local_.pending_message_start_pos_ = location->start_pos();
      thread_local_.pending_message_end_pos_ = location->end_pos();
    }
  }

  if (is_caught_externally) {
    thread_local_.catcher_ = thread_local_.TryCatchHandler();
  }

  // NOTE: Notifying the debugger or generating the message
  // may have caused new exceptions. For now, we just ignore
  // that and set the pending exception to the original one.
  set_pending_exception(*exception_handle);
}


void Top::ReportPendingMessages() {
  ASSERT(has_pending_exception());
  setup_external_caught();
  // If the pending exception is OutOfMemoryException set out_of_memory in
  // the global context.  Note: We have to mark the global context here
  // since the GenerateThrowOutOfMemory stub cannot make a RuntimeCall to
  // set it.
  bool external_caught = thread_local_.external_caught_exception_;
  HandleScope scope;
  if (thread_local_.pending_exception_ == Failure::OutOfMemoryException()) {
    context()->mark_out_of_memory();
  } else if (thread_local_.pending_exception_ ==
             Heap::termination_exception()) {
    if (external_caught) {
      thread_local_.TryCatchHandler()->can_continue_ = false;
      thread_local_.TryCatchHandler()->exception_ = Heap::null_value();
    }
  } else {
    Handle<Object> exception(pending_exception());
    thread_local_.external_caught_exception_ = false;
    if (external_caught) {
      thread_local_.TryCatchHandler()->can_continue_ = true;
      thread_local_.TryCatchHandler()->exception_ =
        thread_local_.pending_exception_;
      if (!thread_local_.pending_message_obj_->IsTheHole()) {
        try_catch_handler()->message_ = thread_local_.pending_message_obj_;
      }
    }
    if (thread_local_.has_pending_message_) {
      thread_local_.has_pending_message_ = false;
      if (thread_local_.pending_message_ != NULL) {
        MessageHandler::ReportMessage(thread_local_.pending_message_);
      } else if (!thread_local_.pending_message_obj_->IsTheHole()) {
        Handle<Object> message_obj(thread_local_.pending_message_obj_);
        if (thread_local_.pending_message_script_ != NULL) {
          Handle<Script> script(thread_local_.pending_message_script_);
          int start_pos = thread_local_.pending_message_start_pos_;
          int end_pos = thread_local_.pending_message_end_pos_;
          MessageLocation location(script, start_pos, end_pos);
          MessageHandler::ReportMessage(&location, message_obj);
        } else {
          MessageHandler::ReportMessage(NULL, message_obj);
        }
      }
    }
    thread_local_.external_caught_exception_ = external_caught;
    set_pending_exception(*exception);
  }
  clear_pending_message();
}


void Top::TraceException(bool flag) {
  FLAG_trace_exception = flag;
}


bool Top::OptionalRescheduleException(bool is_bottom_call) {
  // Allways reschedule out of memory exceptions.
  if (!is_out_of_memory()) {
    bool is_termination_exception =
        pending_exception() == Heap::termination_exception();

    // Do not reschedule the exception if this is the bottom call.
    bool clear_exception = is_bottom_call;

    if (is_termination_exception) {
      if (is_bottom_call) {
        thread_local_.external_caught_exception_ = false;
        clear_pending_exception();
        return false;
      }
    } else if (thread_local_.external_caught_exception_) {
      // If the exception is externally caught, clear it if there are no
      // JavaScript frames on the way to the C++ frame that has the
      // external handler.
      ASSERT(thread_local_.try_catch_handler_address() != NULL);
      Address external_handler_address =
          thread_local_.try_catch_handler_address();
      JavaScriptFrameIterator it;
      if (it.done() || (it.frame()->sp() > external_handler_address)) {
        clear_exception = true;
      }
    }

    // Clear the exception if needed.
    if (clear_exception) {
      thread_local_.external_caught_exception_ = false;
      clear_pending_exception();
      return false;
    }
  }

  // Reschedule the exception.
  thread_local_.scheduled_exception_ = pending_exception();
  clear_pending_exception();
  return true;
}


void Top::SetCaptureStackTraceForUncaughtExceptions(
      bool capture,
      int frame_limit,
      StackTrace::StackTraceOptions options) {
  capture_stack_trace_for_uncaught_exceptions = capture;
  stack_trace_for_uncaught_exceptions_frame_limit = frame_limit;
  stack_trace_for_uncaught_exceptions_options = options;
}


bool Top::is_out_of_memory() {
  if (has_pending_exception()) {
    Object* e = pending_exception();
    if (e->IsFailure() && Failure::cast(e)->IsOutOfMemoryException()) {
      return true;
    }
  }
  if (has_scheduled_exception()) {
    Object* e = scheduled_exception();
    if (e->IsFailure() && Failure::cast(e)->IsOutOfMemoryException()) {
      return true;
    }
  }
  return false;
}


Handle<Context> Top::global_context() {
  GlobalObject* global = thread_local_.context_->global();
  return Handle<Context>(global->global_context());
}


Handle<Context> Top::GetCallingGlobalContext() {
  JavaScriptFrameIterator it;
#ifdef ENABLE_DEBUGGER_SUPPORT
  if (Debug::InDebugger()) {
    while (!it.done()) {
      JavaScriptFrame* frame = it.frame();
      Context* context = Context::cast(frame->context());
      if (context->global_context() == *Debug::debug_context()) {
        it.Advance();
      } else {
        break;
      }
    }
  }
#endif  // ENABLE_DEBUGGER_SUPPORT
  if (it.done()) return Handle<Context>::null();
  JavaScriptFrame* frame = it.frame();
  Context* context = Context::cast(frame->context());
  return Handle<Context>(context->global_context());
}


char* Top::ArchiveThread(char* to) {
  memcpy(to, reinterpret_cast<char*>(&thread_local_), sizeof(thread_local_));
  InitializeThreadLocal();
  return to + sizeof(thread_local_);
}


char* Top::RestoreThread(char* from) {
  memcpy(reinterpret_cast<char*>(&thread_local_), from, sizeof(thread_local_));
  return from + sizeof(thread_local_);
}


ExecutionAccess::ExecutionAccess() {
  Top::break_access_->Lock();
}


ExecutionAccess::~ExecutionAccess() {
  Top::break_access_->Unlock();
}


} }  // namespace v8::internal
