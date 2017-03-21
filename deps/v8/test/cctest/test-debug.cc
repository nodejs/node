// Copyright 2012 the V8 project authors. All rights reserved.
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

#include <stdlib.h>

#include "src/v8.h"

#include "src/api.h"
#include "src/compilation-cache.h"
#include "src/debug/debug-interface.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/frames.h"
#include "src/utils.h"
#include "test/cctest/cctest.h"

using ::v8::internal::EmbeddedVector;
using ::v8::internal::Object;
using ::v8::internal::Handle;
using ::v8::internal::Heap;
using ::v8::internal::JSGlobalProxy;
using ::v8::internal::Code;
using ::v8::internal::Debug;
using ::v8::internal::StackFrame;
using ::v8::internal::StepAction;
using ::v8::internal::StepIn;  // From StepAction enum
using ::v8::internal::StepNext;  // From StepAction enum
using ::v8::internal::StepOut;  // From StepAction enum
using ::v8::internal::Vector;
using ::v8::internal::StrLength;

// Size of temp buffer for formatting small strings.
#define SMALL_STRING_BUFFER_SIZE 80

// --- H e l p e r   C l a s s e s


// Helper class for creating a V8 enviromnent for running tests
class DebugLocalContext {
 public:
  inline DebugLocalContext(
      v8::Isolate* isolate, v8::ExtensionConfiguration* extensions = 0,
      v8::Local<v8::ObjectTemplate> global_template =
          v8::Local<v8::ObjectTemplate>(),
      v8::Local<v8::Value> global_object = v8::Local<v8::Value>())
      : scope_(isolate),
        context_(v8::Context::New(isolate, extensions, global_template,
                                  global_object)) {
    context_->Enter();
  }
  inline DebugLocalContext(
      v8::ExtensionConfiguration* extensions = 0,
      v8::Local<v8::ObjectTemplate> global_template =
          v8::Local<v8::ObjectTemplate>(),
      v8::Local<v8::Value> global_object = v8::Local<v8::Value>())
      : scope_(CcTest::isolate()),
        context_(v8::Context::New(CcTest::isolate(), extensions,
                                  global_template, global_object)) {
    context_->Enter();
  }
  inline ~DebugLocalContext() {
    context_->Exit();
  }
  inline v8::Local<v8::Context> context() { return context_; }
  inline v8::Context* operator->() { return *context_; }
  inline v8::Context* operator*() { return *context_; }
  inline v8::Isolate* GetIsolate() { return context_->GetIsolate(); }
  inline bool IsReady() { return !context_.IsEmpty(); }
  void ExposeDebug() {
    v8::internal::Isolate* isolate =
        reinterpret_cast<v8::internal::Isolate*>(context_->GetIsolate());
    v8::internal::Factory* factory = isolate->factory();
    // Expose the debug context global object in the global object for testing.
    CHECK(isolate->debug()->Load());
    Handle<v8::internal::Context> debug_context =
        isolate->debug()->debug_context();
    debug_context->set_security_token(
        v8::Utils::OpenHandle(*context_)->security_token());

    Handle<JSGlobalProxy> global(Handle<JSGlobalProxy>::cast(
        v8::Utils::OpenHandle(*context_->Global())));
    Handle<v8::internal::String> debug_string =
        factory->InternalizeOneByteString(STATIC_CHAR_VECTOR("debug"));
    v8::internal::JSObject::SetOwnPropertyIgnoreAttributes(
        global, debug_string, handle(debug_context->global_proxy()),
        v8::internal::DONT_ENUM)
        .Check();
  }

 private:
  v8::HandleScope scope_;
  v8::Local<v8::Context> context_;
};


// --- H e l p e r   F u n c t i o n s

// Compile and run the supplied source and return the requested function.
static v8::Local<v8::Function> CompileFunction(v8::Isolate* isolate,
                                               const char* source,
                                               const char* function_name) {
  CompileRunChecked(isolate, source);
  v8::Local<v8::String> name = v8_str(isolate, function_name);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::MaybeLocal<v8::Value> maybe_function =
      context->Global()->Get(context, name);
  return v8::Local<v8::Function>::Cast(maybe_function.ToLocalChecked());
}


// Compile and run the supplied source and return the requested function.
static v8::Local<v8::Function> CompileFunction(DebugLocalContext* env,
                                               const char* source,
                                               const char* function_name) {
  return CompileFunction(env->GetIsolate(), source, function_name);
}


// Is there any debug info for the function?
static bool HasDebugInfo(v8::Local<v8::Function> fun) {
  Handle<v8::internal::JSFunction> f =
      Handle<v8::internal::JSFunction>::cast(v8::Utils::OpenHandle(*fun));
  Handle<v8::internal::SharedFunctionInfo> shared(f->shared());
  return shared->HasDebugInfo();
}

// Set a break point in a function with a position relative to function start,
// and return the associated break point number.
static int SetBreakPoint(v8::Local<v8::Function> fun, int position) {
  i::Handle<i::JSFunction> function =
      i::Handle<i::JSFunction>::cast(v8::Utils::OpenHandle(*fun));
  position += function->shared()->start_position();
  static int break_point = 0;
  v8::internal::Isolate* isolate = function->GetIsolate();
  v8::internal::Debug* debug = isolate->debug();
  debug->SetBreakPoint(
      function,
      Handle<Object>(v8::internal::Smi::FromInt(++break_point), isolate),
      &position);
  return break_point;
}


// Set a break point in a function using the Debug object and return the
// associated break point number.
static int SetBreakPointFromJS(v8::Isolate* isolate,
                               const char* function_name,
                               int line, int position) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  SNPrintF(buffer,
           "debug.Debug.setBreakPoint(%s,%d,%d)",
           function_name, line, position);
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  v8::Local<v8::Value> value = CompileRunChecked(isolate, buffer.start());
  return value->Int32Value(isolate->GetCurrentContext()).FromJust();
}


// Set a break point in a script identified by id using the global Debug object.
static int SetScriptBreakPointByIdFromJS(v8::Isolate* isolate, int script_id,
                                         int line, int column) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  if (column >= 0) {
    // Column specified set script break point on precise location.
    SNPrintF(buffer,
             "debug.Debug.setScriptBreakPointById(%d,%d,%d)",
             script_id, line, column);
  } else {
    // Column not specified set script break point on line.
    SNPrintF(buffer,
             "debug.Debug.setScriptBreakPointById(%d,%d)",
             script_id, line);
  }
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  {
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Value> value = CompileRunChecked(isolate, buffer.start());
    CHECK(!try_catch.HasCaught());
    return value->Int32Value(isolate->GetCurrentContext()).FromJust();
  }
}


// Set a break point in a script identified by name using the global Debug
// object.
static int SetScriptBreakPointByNameFromJS(v8::Isolate* isolate,
                                           const char* script_name, int line,
                                           int column) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  if (column >= 0) {
    // Column specified set script break point on precise location.
    SNPrintF(buffer,
             "debug.Debug.setScriptBreakPointByName(\"%s\",%d,%d)",
             script_name, line, column);
  } else {
    // Column not specified set script break point on line.
    SNPrintF(buffer,
             "debug.Debug.setScriptBreakPointByName(\"%s\",%d)",
             script_name, line);
  }
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  {
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Value> value = CompileRunChecked(isolate, buffer.start());
    CHECK(!try_catch.HasCaught());
    return value->Int32Value(isolate->GetCurrentContext()).FromJust();
  }
}


// Clear a break point.
static void ClearBreakPoint(int break_point) {
  v8::internal::Isolate* isolate = CcTest::i_isolate();
  v8::internal::Debug* debug = isolate->debug();
  debug->ClearBreakPoint(
      Handle<Object>(v8::internal::Smi::FromInt(break_point), isolate));
}


// Clear a break point using the global Debug object.
static void ClearBreakPointFromJS(v8::Isolate* isolate,
                                  int break_point_number) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  SNPrintF(buffer,
           "debug.Debug.clearBreakPoint(%d)",
           break_point_number);
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  CompileRunChecked(isolate, buffer.start());
}


static void EnableScriptBreakPointFromJS(v8::Isolate* isolate,
                                         int break_point_number) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  SNPrintF(buffer,
           "debug.Debug.enableScriptBreakPoint(%d)",
           break_point_number);
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  CompileRunChecked(isolate, buffer.start());
}


static void DisableScriptBreakPointFromJS(v8::Isolate* isolate,
                                          int break_point_number) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  SNPrintF(buffer,
           "debug.Debug.disableScriptBreakPoint(%d)",
           break_point_number);
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  CompileRunChecked(isolate, buffer.start());
}


static void ChangeScriptBreakPointConditionFromJS(v8::Isolate* isolate,
                                                  int break_point_number,
                                                  const char* condition) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  SNPrintF(buffer,
           "debug.Debug.changeScriptBreakPointCondition(%d, \"%s\")",
           break_point_number, condition);
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  CompileRunChecked(isolate, buffer.start());
}


// Change break on exception.
static void ChangeBreakOnException(bool caught, bool uncaught) {
  v8::internal::Debug* debug = CcTest::i_isolate()->debug();
  debug->ChangeBreakOnException(v8::internal::BreakException, caught);
  debug->ChangeBreakOnException(v8::internal::BreakUncaughtException, uncaught);
}


// Change break on exception using the global Debug object.
static void ChangeBreakOnExceptionFromJS(v8::Isolate* isolate, bool caught,
                                         bool uncaught) {
  if (caught) {
    CompileRunChecked(isolate, "debug.Debug.setBreakOnException()");
  } else {
    CompileRunChecked(isolate, "debug.Debug.clearBreakOnException()");
  }
  if (uncaught) {
    CompileRunChecked(isolate, "debug.Debug.setBreakOnUncaughtException()");
  } else {
    CompileRunChecked(isolate, "debug.Debug.clearBreakOnUncaughtException()");
  }
}

// Change break on exception using the native API call.
static void ChangeBreakOnExceptionFromAPI(
    v8::Isolate* isolate, v8::debug::ExceptionBreakState state) {
  v8::debug::ChangeBreakOnException(isolate, state);
}

// Prepare to step to next break location.
static void PrepareStep(StepAction step_action) {
  v8::internal::Debug* debug = CcTest::i_isolate()->debug();
  debug->PrepareStep(step_action);
}


static void ClearStepping() { CcTest::i_isolate()->debug()->ClearStepping(); }


// This function is in namespace v8::internal to be friend with class
// v8::internal::Debug.
namespace v8 {
namespace internal {

// Collect the currently debugged functions.
Handle<FixedArray> GetDebuggedFunctions() {
  Debug* debug = CcTest::i_isolate()->debug();

  v8::internal::DebugInfoListNode* node = debug->debug_info_list_;

  // Find the number of debugged functions.
  int count = 0;
  while (node) {
    count++;
    node = node->next();
  }

  // Allocate array for the debugged functions
  Handle<FixedArray> debugged_functions =
      CcTest::i_isolate()->factory()->NewFixedArray(count);

  // Run through the debug info objects and collect all functions.
  count = 0;
  while (node) {
    debugged_functions->set(count++, *node->debug_info());
    node = node->next();
  }

  return debugged_functions;
}


// Check that the debugger has been fully unloaded.
void CheckDebuggerUnloaded(bool check_functions) {
  // Check that the debugger context is cleared and that there is no debug
  // information stored for the debugger.
  CHECK(CcTest::i_isolate()->debug()->debug_context().is_null());
  CHECK(!CcTest::i_isolate()->debug()->debug_info_list_);

  // Collect garbage to ensure weak handles are cleared.
  CcTest::CollectAllGarbage(i::Heap::kFinalizeIncrementalMarkingMask);
  CcTest::CollectAllGarbage(Heap::kMakeHeapIterableMask);

  // Iterate the head and check that there are no debugger related objects left.
  HeapIterator iterator(CcTest::heap());
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    CHECK(!obj->IsDebugInfo());
    CHECK(!obj->IsBreakPointInfo());

    // If deep check of functions is requested check that no debug break code
    // is left in all functions.
    if (check_functions) {
      if (obj->IsJSFunction()) {
        JSFunction* fun = JSFunction::cast(obj);
        for (RelocIterator it(fun->shared()->code(),
                              RelocInfo::kDebugBreakSlotMask);
             !it.done(); it.next()) {
          CHECK(!it.rinfo()->IsPatchedDebugBreakSlotSequence());
        }
      }
    }
  }
}


}  // namespace internal
}  // namespace v8


// Check that the debugger has been fully unloaded.
static void CheckDebuggerUnloaded(v8::Isolate* isolate,
                                  bool check_functions = false) {
  v8::internal::CheckDebuggerUnloaded(check_functions);
}


// --- D e b u g   E v e n t   H a n d l e r s
// ---
// --- The different tests uses a number of debug event handlers.
// ---


// Source for the JavaScript function which picks out the function
// name of a frame.
const char* frame_function_name_source =
    "function frame_function_name(exec_state, frame_number) {"
    "  return exec_state.frame(frame_number).func().name();"
    "}";
v8::Local<v8::Function> frame_function_name;


// Source for the JavaScript function which pick out the name of the
// first argument of a frame.
const char* frame_argument_name_source =
    "function frame_argument_name(exec_state, frame_number) {"
    "  return exec_state.frame(frame_number).argumentName(0);"
    "}";
v8::Local<v8::Function> frame_argument_name;


// Source for the JavaScript function which pick out the value of the
// first argument of a frame.
const char* frame_argument_value_source =
    "function frame_argument_value(exec_state, frame_number) {"
    "  return exec_state.frame(frame_number).argumentValue(0).value_;"
    "}";
v8::Local<v8::Function> frame_argument_value;


// Source for the JavaScript function which pick out the name of the
// first argument of a frame.
const char* frame_local_name_source =
    "function frame_local_name(exec_state, frame_number) {"
    "  return exec_state.frame(frame_number).localName(0);"
    "}";
v8::Local<v8::Function> frame_local_name;


// Source for the JavaScript function which pick out the value of the
// first argument of a frame.
const char* frame_local_value_source =
    "function frame_local_value(exec_state, frame_number) {"
    "  return exec_state.frame(frame_number).localValue(0).value_;"
    "}";
v8::Local<v8::Function> frame_local_value;


// Source for the JavaScript function which picks out the source line for the
// top frame.
const char* frame_source_line_source =
    "function frame_source_line(exec_state) {"
    "  return exec_state.frame(0).sourceLine();"
    "}";
v8::Local<v8::Function> frame_source_line;


// Source for the JavaScript function which picks out the source column for the
// top frame.
const char* frame_source_column_source =
    "function frame_source_column(exec_state) {"
    "  return exec_state.frame(0).sourceColumn();"
    "}";
v8::Local<v8::Function> frame_source_column;


// Source for the JavaScript function which picks out the script name for the
// top frame.
const char* frame_script_name_source =
    "function frame_script_name(exec_state) {"
    "  return exec_state.frame(0).func().script().name();"
    "}";
v8::Local<v8::Function> frame_script_name;


// Source for the JavaScript function which returns the number of frames.
static const char* frame_count_source =
    "function frame_count(exec_state) {"
    "  return exec_state.frameCount();"
    "}";
v8::Local<v8::Function> frame_count;


// Global variable to store the last function hit - used by some tests.
char last_function_hit[80];

// Global variable to store the name for last script hit - used by some tests.
char last_script_name_hit[80];

// Global variables to store the last source position - used by some tests.
int last_source_line = -1;
int last_source_column = -1;

// Debug event handler which counts the break points which have been hit.
int break_point_hit_count = 0;
int break_point_hit_count_deoptimize = 0;
static void DebugEventBreakPointHitCount(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  v8::Local<v8::Object> exec_state = event_details.GetExecutionState();
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
  v8::internal::Isolate* isolate = CcTest::i_isolate();
  Debug* debug = isolate->debug();
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(debug->break_id(), 0);

  // Count the number of breaks.
  if (event == v8::Break) {
    break_point_hit_count++;
    if (!frame_function_name.IsEmpty()) {
      // Get the name of the function.
      const int argc = 2;
      v8::Local<v8::Value> argv[argc] = {
          exec_state, v8::Integer::New(CcTest::isolate(), 0)};
      v8::Local<v8::Value> result =
          frame_function_name->Call(context, exec_state, argc, argv)
              .ToLocalChecked();
      if (result->IsUndefined()) {
        last_function_hit[0] = '\0';
      } else {
        CHECK(result->IsString());
        v8::Local<v8::String> function_name(result.As<v8::String>());
        function_name->WriteUtf8(last_function_hit);
      }
    }

    if (!frame_source_line.IsEmpty()) {
      // Get the source line.
      const int argc = 1;
      v8::Local<v8::Value> argv[argc] = {exec_state};
      v8::Local<v8::Value> result =
          frame_source_line->Call(context, exec_state, argc, argv)
              .ToLocalChecked();
      CHECK(result->IsNumber());
      last_source_line = result->Int32Value(context).FromJust();
    }

    if (!frame_source_column.IsEmpty()) {
      // Get the source column.
      const int argc = 1;
      v8::Local<v8::Value> argv[argc] = {exec_state};
      v8::Local<v8::Value> result =
          frame_source_column->Call(context, exec_state, argc, argv)
              .ToLocalChecked();
      CHECK(result->IsNumber());
      last_source_column = result->Int32Value(context).FromJust();
    }

    if (!frame_script_name.IsEmpty()) {
      // Get the script name of the function script.
      const int argc = 1;
      v8::Local<v8::Value> argv[argc] = {exec_state};
      v8::Local<v8::Value> result =
          frame_script_name->Call(context, exec_state, argc, argv)
              .ToLocalChecked();
      if (result->IsUndefined()) {
        last_script_name_hit[0] = '\0';
      } else {
        CHECK(result->IsString());
        v8::Local<v8::String> script_name(result.As<v8::String>());
        script_name->WriteUtf8(last_script_name_hit);
      }
    }

    // Perform a full deoptimization when the specified number of
    // breaks have been hit.
    if (break_point_hit_count == break_point_hit_count_deoptimize) {
      i::Deoptimizer::DeoptimizeAll(isolate);
    }
  }
}


// Debug event handler which counts a number of events and collects the stack
// height if there is a function compiled for that.
int exception_hit_count = 0;
int uncaught_exception_hit_count = 0;
int last_js_stack_height = -1;
v8::Local<v8::Function> debug_event_listener_callback;
int debug_event_listener_callback_result;

static void DebugEventCounterClear() {
  break_point_hit_count = 0;
  exception_hit_count = 0;
  uncaught_exception_hit_count = 0;
}

static void DebugEventCounter(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  v8::Local<v8::Object> exec_state = event_details.GetExecutionState();
  v8::Local<v8::Object> event_data = event_details.GetEventData();
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
  v8::internal::Debug* debug = CcTest::i_isolate()->debug();

  // When hitting a debug event listener there must be a break set.
  CHECK_NE(debug->break_id(), 0);

  // Count the number of breaks.
  if (event == v8::Break) {
    break_point_hit_count++;
  } else if (event == v8::Exception) {
    exception_hit_count++;

    // Check whether the exception was uncaught.
    v8::Local<v8::String> fun_name = v8_str(CcTest::isolate(), "uncaught");
    v8::Local<v8::Function> fun = v8::Local<v8::Function>::Cast(
        event_data->Get(context, fun_name).ToLocalChecked());
    v8::Local<v8::Value> result =
        fun->Call(context, event_data, 0, NULL).ToLocalChecked();
    if (result->IsTrue()) {
      uncaught_exception_hit_count++;
    }
  }

  // Collect the JavsScript stack height if the function frame_count is
  // compiled.
  if (!frame_count.IsEmpty()) {
    static const int kArgc = 1;
    v8::Local<v8::Value> argv[kArgc] = {exec_state};
    // Using exec_state as receiver is just to have a receiver.
    v8::Local<v8::Value> result =
        frame_count->Call(context, exec_state, kArgc, argv).ToLocalChecked();
    last_js_stack_height = result->Int32Value(context).FromJust();
  }

  // Run callback from DebugEventListener and check the result.
  if (!debug_event_listener_callback.IsEmpty()) {
    v8::Local<v8::Value> result =
        debug_event_listener_callback->Call(context, event_data, 0, NULL)
            .ToLocalChecked();
    CHECK(!result.IsEmpty());
    CHECK_EQ(debug_event_listener_callback_result,
             result->Int32Value(context).FromJust());
  }
}


// Debug event handler which evaluates a number of expressions when a break
// point is hit. Each evaluated expression is compared with an expected value.
// For this debug event handler to work the following two global varaibles
// must be initialized.
//   checks: An array of expressions and expected results
//   evaluate_check_function: A JavaScript function (see below)

// Structure for holding checks to do.
struct EvaluateCheck {
  const char* expr;  // An expression to evaluate when a break point is hit.
  v8::Local<v8::Value> expected;  // The expected result.
};


// Array of checks to do.
struct EvaluateCheck* checks = NULL;
// Source for The JavaScript function which can do the evaluation when a break
// point is hit.
const char* evaluate_check_source =
    "function evaluate_check(exec_state, expr, expected) {"
    "  return exec_state.frame(0).evaluate(expr).value() === expected;"
    "}";
v8::Local<v8::Function> evaluate_check_function;

// The actual debug event described by the longer comment above.
static void DebugEventEvaluate(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  v8::Local<v8::Object> exec_state = event_details.GetExecutionState();
  v8::Isolate* isolate = CcTest::isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::internal::Debug* debug = CcTest::i_isolate()->debug();
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(debug->break_id(), 0);

  if (event == v8::Break) {
    break_point_hit_count++;
    for (int i = 0; checks[i].expr != NULL; i++) {
      const int argc = 3;
      v8::Local<v8::String> string = v8_str(isolate, checks[i].expr);
      v8::Local<v8::Value> argv[argc] = {exec_state, string,
                                         checks[i].expected};
      v8::Local<v8::Value> result =
          evaluate_check_function->Call(context, exec_state, argc, argv)
              .ToLocalChecked();
      if (!result->IsTrue()) {
        v8::String::Utf8Value utf8(checks[i].expected);
        V8_Fatal(__FILE__, __LINE__, "%s != %s", checks[i].expr, *utf8);
      }
    }
  }
}


// This debug event listener removes a breakpoint in a function
int debug_event_remove_break_point = 0;
static void DebugEventRemoveBreakPoint(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  v8::Local<v8::Value> data = event_details.GetCallbackData();
  v8::internal::Debug* debug = CcTest::i_isolate()->debug();
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(debug->break_id(), 0);

  if (event == v8::Break) {
    break_point_hit_count++;
    CHECK(data->IsFunction());
    ClearBreakPoint(debug_event_remove_break_point);
  }
}


// Debug event handler which counts break points hit and performs a step
// afterwards.
StepAction step_action = StepIn;  // Step action to perform when stepping.
static void DebugEventStep(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  v8::internal::Debug* debug = CcTest::i_isolate()->debug();
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(debug->break_id(), 0);

  if (event == v8::Break) {
    break_point_hit_count++;
    PrepareStep(step_action);
  }
}


// Debug event handler which counts break points hit and performs a step
// afterwards. For each call the expected function is checked.
// For this debug event handler to work the following two global varaibles
// must be initialized.
//   expected_step_sequence: An array of the expected function call sequence.
//   frame_function_name: A JavaScript function (see below).

// String containing the expected function call sequence. Note: this only works
// if functions have name length of one.
const char* expected_step_sequence = NULL;

// The actual debug event described by the longer comment above.
static void DebugEventStepSequence(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  v8::Local<v8::Object> exec_state = event_details.GetExecutionState();
  v8::internal::Debug* debug = CcTest::i_isolate()->debug();
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(debug->break_id(), 0);

  if (event == v8::Break || event == v8::Exception) {
    // Check that the current function is the expected.
    CHECK(break_point_hit_count <
          StrLength(expected_step_sequence));
    const int argc = 2;
    v8::Local<v8::Value> argv[argc] = {exec_state,
                                       v8::Integer::New(CcTest::isolate(), 0)};
    v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
    v8::Local<v8::Value> result =
        frame_function_name->Call(context, exec_state, argc, argv)
            .ToLocalChecked();
    CHECK(result->IsString());
    v8::String::Utf8Value function_name(
        result->ToString(context).ToLocalChecked());
    CHECK_EQ(1, StrLength(*function_name));
    CHECK_EQ((*function_name)[0],
              expected_step_sequence[break_point_hit_count]);

    // Perform step.
    break_point_hit_count++;
    PrepareStep(step_action);
  }
}


// Debug event handler which performs a garbage collection.
static void DebugEventBreakPointCollectGarbage(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  v8::internal::Debug* debug = CcTest::i_isolate()->debug();
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(debug->break_id(), 0);

  // Perform a garbage collection when break point is hit and continue. Based
  // on the number of break points hit either scavenge or mark compact
  // collector is used.
  if (event == v8::Break) {
    break_point_hit_count++;
    if (break_point_hit_count % 2 == 0) {
      // Scavenge.
      CcTest::CollectGarbage(v8::internal::NEW_SPACE);
    } else {
      // Mark sweep compact.
      CcTest::CollectAllGarbage(i::Heap::kFinalizeIncrementalMarkingMask);
    }
  }
}


// Debug event handler which re-issues a debug break and calls the garbage
// collector to have the heap verified.
static void DebugEventBreak(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  v8::internal::Debug* debug = CcTest::i_isolate()->debug();
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(debug->break_id(), 0);

  if (event == v8::Break) {
    // Count the number of breaks.
    break_point_hit_count++;

    // Run the garbage collector to enforce heap verification if option
    // --verify-heap is set.
    CcTest::CollectGarbage(v8::internal::NEW_SPACE);

    // Set the break flag again to come back here as soon as possible.
    v8::Debug::DebugBreak(CcTest::isolate());
  }
}


// Debug event handler which re-issues a debug break until a limit has been
// reached.
int max_break_point_hit_count = 0;
bool terminate_after_max_break_point_hit = false;
static void DebugEventBreakMax(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  v8::Isolate* v8_isolate = CcTest::isolate();
  v8::internal::Isolate* isolate = CcTest::i_isolate();
  v8::internal::Debug* debug = isolate->debug();
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(debug->break_id(), 0);

  if (event == v8::Break) {
    if (break_point_hit_count < max_break_point_hit_count) {
      // Count the number of breaks.
      break_point_hit_count++;

      // Set the break flag again to come back here as soon as possible.
      v8::Debug::DebugBreak(v8_isolate);

    } else if (terminate_after_max_break_point_hit) {
      // Terminate execution after the last break if requested.
      v8_isolate->TerminateExecution();
    }

    // Perform a full deoptimization when the specified number of
    // breaks have been hit.
    if (break_point_hit_count == break_point_hit_count_deoptimize) {
      i::Deoptimizer::DeoptimizeAll(isolate);
    }
  }
}


// --- M e s s a g e   C a l l b a c k


// Message callback which counts the number of messages.
int message_callback_count = 0;

static void MessageCallbackCountClear() {
  message_callback_count = 0;
}

static void MessageCallbackCount(v8::Local<v8::Message> message,
                                 v8::Local<v8::Value> data) {
  message_callback_count++;
}


// --- T h e   A c t u a l   T e s t s

// Test that the debug info in the VM is in sync with the functions being
// debugged.
TEST(DebugInfo) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  // Create a couple of functions for the test.
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){}", "foo");
  v8::Local<v8::Function> bar =
      CompileFunction(&env, "function bar(){}", "bar");
  // Initially no functions are debugged.
  CHECK_EQ(0, v8::internal::GetDebuggedFunctions()->length());
  CHECK(!HasDebugInfo(foo));
  CHECK(!HasDebugInfo(bar));
  EnableDebugger(env->GetIsolate());
  // One function (foo) is debugged.
  int bp1 = SetBreakPoint(foo, 0);
  CHECK_EQ(1, v8::internal::GetDebuggedFunctions()->length());
  CHECK(HasDebugInfo(foo));
  CHECK(!HasDebugInfo(bar));
  // Two functions are debugged.
  int bp2 = SetBreakPoint(bar, 0);
  CHECK_EQ(2, v8::internal::GetDebuggedFunctions()->length());
  CHECK(HasDebugInfo(foo));
  CHECK(HasDebugInfo(bar));
  // One function (bar) is debugged.
  ClearBreakPoint(bp1);
  CHECK_EQ(1, v8::internal::GetDebuggedFunctions()->length());
  CHECK(!HasDebugInfo(foo));
  CHECK(HasDebugInfo(bar));
  // No functions are debugged.
  ClearBreakPoint(bp2);
  DisableDebugger(env->GetIsolate());
  CHECK_EQ(0, v8::internal::GetDebuggedFunctions()->length());
  CHECK(!HasDebugInfo(foo));
  CHECK(!HasDebugInfo(bar));
}


// Test that a break point can be set at an IC store location.
TEST(BreakPointICStore) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){bar=0;}", "foo");

  // Run without breakpoints.
  foo->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint
  int bp = SetBreakPoint(foo, 0);
  foo->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  foo->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that a break point can be set at an IC load location.
TEST(BreakPointICLoad) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);

  CompileRunChecked(env->GetIsolate(), "bar=1");
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){var x=bar;}", "foo");

  // Run without breakpoints.
  foo->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  int bp = SetBreakPoint(foo, 0);
  foo->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  foo->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that a break point can be set at an IC call location.
TEST(BreakPointICCall) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);
  CompileRunChecked(env->GetIsolate(), "function bar(){}");
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){bar();}", "foo");

  // Run without breakpoints.
  foo->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint
  int bp = SetBreakPoint(foo, 0);
  foo->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  foo->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that a break point can be set at an IC call location and survive a GC.
TEST(BreakPointICCallWithGC) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointCollectGarbage);
  CompileRunChecked(env->GetIsolate(), "function bar(){return 1;}");
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){return bar();}", "foo");
  v8::Local<v8::Context> context = env.context();

  // Run without breakpoints.
  CHECK_EQ(1, foo->Call(context, env->Global(), 0, NULL)
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  int bp = SetBreakPoint(foo, 0);
  CHECK_EQ(1, foo->Call(context, env->Global(), 0, NULL)
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(1, break_point_hit_count);
  CHECK_EQ(1, foo->Call(context, env->Global(), 0, NULL)
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that a break point can be set at an IC call location and survive a GC.
TEST(BreakPointConstructCallWithGC) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointCollectGarbage);
  CompileRunChecked(env->GetIsolate(), "function bar(){ this.x = 1;}");
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){return new bar(1).x;}", "foo");
  v8::Local<v8::Context> context = env.context();

  // Run without breakpoints.
  CHECK_EQ(1, foo->Call(context, env->Global(), 0, NULL)
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  int bp = SetBreakPoint(foo, 0);
  CHECK_EQ(1, foo->Call(context, env->Global(), 0, NULL)
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(1, break_point_hit_count);
  CHECK_EQ(1, foo->Call(context, env->Global(), 0, NULL)
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that a break point can be set at a return store location.
TEST(BreakPointReturn) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Create a functions for checking the source line and column when hitting
  // a break point.
  frame_source_line = CompileFunction(&env,
                                      frame_source_line_source,
                                      "frame_source_line");
  frame_source_column = CompileFunction(&env,
                                        frame_source_column_source,
                                        "frame_source_column");


  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){}", "foo");
  v8::Local<v8::Context> context = env.context();

  // Run without breakpoints.
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint
  int bp = SetBreakPoint(foo, 0);
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  CHECK_EQ(0, last_source_line);
  CHECK_EQ(15, last_source_column);
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);
  CHECK_EQ(0, last_source_line);
  CHECK_EQ(15, last_source_column);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


static void CallWithBreakPoints(v8::Local<v8::Context> context,
                                v8::Local<v8::Object> recv,
                                v8::Local<v8::Function> f,
                                int break_point_count, int call_count) {
  break_point_hit_count = 0;
  for (int i = 0; i < call_count; i++) {
    f->Call(context, recv, 0, NULL).ToLocalChecked();
    CHECK_EQ((i + 1) * break_point_count, break_point_hit_count);
  }
}


// Test GC during break point processing.
TEST(GCDuringBreakPointProcessing) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.context();

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointCollectGarbage);
  v8::Local<v8::Function> foo;

  // Test IC store break point with garbage collection.
  foo = CompileFunction(&env, "function foo(){bar=0;}", "foo");
  SetBreakPoint(foo, 0);
  CallWithBreakPoints(context, env->Global(), foo, 1, 10);

  // Test IC load break point with garbage collection.
  foo = CompileFunction(&env, "bar=1;function foo(){var x=bar;}", "foo");
  SetBreakPoint(foo, 0);
  CallWithBreakPoints(context, env->Global(), foo, 1, 10);

  // Test IC call break point with garbage collection.
  foo = CompileFunction(&env, "function bar(){};function foo(){bar();}", "foo");
  SetBreakPoint(foo, 0);
  CallWithBreakPoints(context, env->Global(), foo, 1, 10);

  // Test return break point with garbage collection.
  foo = CompileFunction(&env, "function foo(){}", "foo");
  SetBreakPoint(foo, 0);
  CallWithBreakPoints(context, env->Global(), foo, 1, 25);

  // Test debug break slot break point with garbage collection.
  foo = CompileFunction(&env, "function foo(){var a;}", "foo");
  SetBreakPoint(foo, 0);
  CallWithBreakPoints(context, env->Global(), foo, 1, 25);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Call the function three times with different garbage collections in between
// and make sure that the break point survives.
static void CallAndGC(v8::Local<v8::Context> context,
                      v8::Local<v8::Object> recv, v8::Local<v8::Function> f) {
  break_point_hit_count = 0;

  for (int i = 0; i < 3; i++) {
    // Call function.
    f->Call(context, recv, 0, NULL).ToLocalChecked();
    CHECK_EQ(1 + i * 3, break_point_hit_count);

    // Scavenge and call function.
    CcTest::CollectGarbage(v8::internal::NEW_SPACE);
    f->Call(context, recv, 0, NULL).ToLocalChecked();
    CHECK_EQ(2 + i * 3, break_point_hit_count);

    // Mark sweep (and perhaps compact) and call function.
    CcTest::CollectAllGarbage(i::Heap::kFinalizeIncrementalMarkingMask);
    f->Call(context, recv, 0, NULL).ToLocalChecked();
    CHECK_EQ(3 + i * 3, break_point_hit_count);
  }
}


// Test that a break point can be set at a return store location.
TEST(BreakPointSurviveGC) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.context();

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);
  v8::Local<v8::Function> foo;

  // Test IC store break point with garbage collection.
  {
    CompileFunction(&env, "function foo(){}", "foo");
    foo = CompileFunction(&env, "function foo(){bar=0;}", "foo");
    SetBreakPoint(foo, 0);
  }
  CallAndGC(context, env->Global(), foo);

  // Test IC load break point with garbage collection.
  {
    CompileFunction(&env, "function foo(){}", "foo");
    foo = CompileFunction(&env, "bar=1;function foo(){var x=bar;}", "foo");
    SetBreakPoint(foo, 0);
  }
  CallAndGC(context, env->Global(), foo);

  // Test IC call break point with garbage collection.
  {
    CompileFunction(&env, "function foo(){}", "foo");
    foo = CompileFunction(&env,
                          "function bar(){};function foo(){bar();}",
                          "foo");
    SetBreakPoint(foo, 0);
  }
  CallAndGC(context, env->Global(), foo);

  // Test return break point with garbage collection.
  {
    CompileFunction(&env, "function foo(){}", "foo");
    foo = CompileFunction(&env, "function foo(){}", "foo");
    SetBreakPoint(foo, 0);
  }
  CallAndGC(context, env->Global(), foo);

  // Test non IC break point with garbage collection.
  {
    CompileFunction(&env, "function foo(){}", "foo");
    foo = CompileFunction(&env, "function foo(){var bar=0;}", "foo");
    SetBreakPoint(foo, 0);
  }
  CallAndGC(context, env->Global(), foo);


  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that break points can be set using the global Debug object.
TEST(BreakPointThroughJavaScript) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = env.context();
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(isolate, DebugEventBreakPointHitCount);
  CompileRunChecked(isolate, "function bar(){}");
  CompileFunction(isolate, "function foo(){bar();bar();}", "foo");
  //                        012345678901234567890
  //                                  1         2
  // Break points are set at position 3 and 9
  v8::Local<v8::String> source = v8_str(env->GetIsolate(), "foo()");
  v8::Local<v8::Script> foo =
      v8::Script::Compile(context, source).ToLocalChecked();

  CHECK_EQ(0, break_point_hit_count);

  // Run with one breakpoint
  int bp1 = SetBreakPointFromJS(env->GetIsolate(), "foo", 0, 3);
  foo->Run(context).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  foo->Run(context).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Run with two breakpoints
  int bp2 = SetBreakPointFromJS(env->GetIsolate(), "foo", 0, 9);
  foo->Run(context).ToLocalChecked();
  CHECK_EQ(4, break_point_hit_count);
  foo->Run(context).ToLocalChecked();
  CHECK_EQ(6, break_point_hit_count);

  // Run with one breakpoint
  ClearBreakPointFromJS(env->GetIsolate(), bp2);
  foo->Run(context).ToLocalChecked();
  CHECK_EQ(7, break_point_hit_count);
  foo->Run(context).ToLocalChecked();
  CHECK_EQ(8, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPointFromJS(env->GetIsolate(), bp1);
  foo->Run(context).ToLocalChecked();
  CHECK_EQ(8, break_point_hit_count);

  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);

  // Make sure that the break point numbers are consecutive.
  CHECK_EQ(1, bp1);
  CHECK_EQ(2, bp2);
}


// Test that break points on scripts identified by name can be set using the
// global Debug object.
TEST(ScriptBreakPointByNameThroughJavaScript) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = env.context();
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(isolate, DebugEventBreakPointHitCount);

  v8::Local<v8::String> script = v8_str(isolate,
                                        "function f() {\n"
                                        "  function h() {\n"
                                        "    a = 0;  // line 2\n"
                                        "  }\n"
                                        "  b = 1;  // line 4\n"
                                        "  return h();\n"
                                        "}\n"
                                        "\n"
                                        "function g() {\n"
                                        "  function h() {\n"
                                        "    a = 0;\n"
                                        "  }\n"
                                        "  b = 2;  // line 12\n"
                                        "  h();\n"
                                        "  b = 3;  // line 14\n"
                                        "  f();    // line 15\n"
                                        "}");

  // Compile the script and get the two functions.
  v8::ScriptOrigin origin = v8::ScriptOrigin(v8_str(isolate, "test"));
  v8::Script::Compile(context, script, &origin)
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(
      env->Global()->Get(context, v8_str(isolate, "f")).ToLocalChecked());
  v8::Local<v8::Function> g = v8::Local<v8::Function>::Cast(
      env->Global()->Get(context, v8_str(isolate, "g")).ToLocalChecked());

  // Call f and g without break points.
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Call f and g with break point on line 12.
  int sbp1 = SetScriptBreakPointByNameFromJS(isolate, "test", 12, 0);
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);

  // Remove the break point again.
  break_point_hit_count = 0;
  ClearBreakPointFromJS(env->GetIsolate(), sbp1);
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Call f and g with break point on line 2.
  int sbp2 = SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test", 2, 0);
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Call f and g with break point on line 2, 4, 12, 14 and 15.
  int sbp3 = SetScriptBreakPointByNameFromJS(isolate, "test", 4, 0);
  int sbp4 = SetScriptBreakPointByNameFromJS(isolate, "test", 12, 0);
  int sbp5 = SetScriptBreakPointByNameFromJS(isolate, "test", 14, 0);
  int sbp6 = SetScriptBreakPointByNameFromJS(isolate, "test", 15, 0);
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(7, break_point_hit_count);

  // Remove all the break points again.
  break_point_hit_count = 0;
  ClearBreakPointFromJS(isolate, sbp2);
  ClearBreakPointFromJS(isolate, sbp3);
  ClearBreakPointFromJS(isolate, sbp4);
  ClearBreakPointFromJS(isolate, sbp5);
  ClearBreakPointFromJS(isolate, sbp6);
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);

  // Make sure that the break point numbers are consecutive.
  CHECK_EQ(1, sbp1);
  CHECK_EQ(2, sbp2);
  CHECK_EQ(3, sbp3);
  CHECK_EQ(4, sbp4);
  CHECK_EQ(5, sbp5);
  CHECK_EQ(6, sbp6);
}


TEST(ScriptBreakPointByIdThroughJavaScript) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = env.context();
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(isolate, DebugEventBreakPointHitCount);

  v8::Local<v8::String> source = v8_str(isolate,
                                        "function f() {\n"
                                        "  function h() {\n"
                                        "    a = 0;  // line 2\n"
                                        "  }\n"
                                        "  b = 1;  // line 4\n"
                                        "  return h();\n"
                                        "}\n"
                                        "\n"
                                        "function g() {\n"
                                        "  function h() {\n"
                                        "    a = 0;\n"
                                        "  }\n"
                                        "  b = 2;  // line 12\n"
                                        "  h();\n"
                                        "  b = 3;  // line 14\n"
                                        "  f();    // line 15\n"
                                        "}");

  // Compile the script and get the two functions.
  v8::ScriptOrigin origin = v8::ScriptOrigin(v8_str(isolate, "test"));
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, source, &origin).ToLocalChecked();
  script->Run(context).ToLocalChecked();
  v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(
      env->Global()->Get(context, v8_str(isolate, "f")).ToLocalChecked());
  v8::Local<v8::Function> g = v8::Local<v8::Function>::Cast(
      env->Global()->Get(context, v8_str(isolate, "g")).ToLocalChecked());

  // Get the script id knowing that internally it is a 32 integer.
  int script_id = script->GetUnboundScript()->GetId();

  // Call f and g without break points.
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Call f and g with break point on line 12.
  int sbp1 = SetScriptBreakPointByIdFromJS(env->GetIsolate(), script_id, 12, 0);
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);

  // Remove the break point again.
  break_point_hit_count = 0;
  ClearBreakPointFromJS(env->GetIsolate(), sbp1);
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Call f and g with break point on line 2.
  int sbp2 = SetScriptBreakPointByIdFromJS(env->GetIsolate(), script_id, 2, 0);
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Call f and g with break point on line 2, 4, 12, 14 and 15.
  int sbp3 = SetScriptBreakPointByIdFromJS(env->GetIsolate(), script_id, 4, 0);
  int sbp4 = SetScriptBreakPointByIdFromJS(env->GetIsolate(), script_id, 12, 0);
  int sbp5 = SetScriptBreakPointByIdFromJS(env->GetIsolate(), script_id, 14, 0);
  int sbp6 = SetScriptBreakPointByIdFromJS(env->GetIsolate(), script_id, 15, 0);
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(7, break_point_hit_count);

  // Remove all the break points again.
  break_point_hit_count = 0;
  ClearBreakPointFromJS(env->GetIsolate(), sbp2);
  ClearBreakPointFromJS(env->GetIsolate(), sbp3);
  ClearBreakPointFromJS(env->GetIsolate(), sbp4);
  ClearBreakPointFromJS(env->GetIsolate(), sbp5);
  ClearBreakPointFromJS(env->GetIsolate(), sbp6);
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);

  // Make sure that the break point numbers are consecutive.
  CHECK_EQ(1, sbp1);
  CHECK_EQ(2, sbp2);
  CHECK_EQ(3, sbp3);
  CHECK_EQ(4, sbp4);
  CHECK_EQ(5, sbp5);
  CHECK_EQ(6, sbp6);
}


// Test conditional script break points.
TEST(EnableDisableScriptBreakPoint) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = env.context();
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(isolate, DebugEventBreakPointHitCount);

  v8::Local<v8::String> script = v8_str(isolate,
                                        "function f() {\n"
                                        "  a = 0;  // line 1\n"
                                        "};");

  // Compile the script and get function f.
  v8::ScriptOrigin origin = v8::ScriptOrigin(v8_str(isolate, "test"));
  v8::Script::Compile(context, script, &origin)
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(
      env->Global()->Get(context, v8_str(isolate, "f")).ToLocalChecked());

  // Set script break point on line 1 (in function f).
  int sbp = SetScriptBreakPointByNameFromJS(isolate, "test", 1, 0);

  // Call f while enabeling and disabling the script break point.
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);

  DisableScriptBreakPointFromJS(isolate, sbp);
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);

  EnableScriptBreakPointFromJS(isolate, sbp);
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  DisableScriptBreakPointFromJS(isolate, sbp);
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}


// Test conditional script break points.
TEST(ConditionalScriptBreakPoint) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);

  v8::Local<v8::String> script = v8_str(env->GetIsolate(),
                                        "count = 0;\n"
                                        "function f() {\n"
                                        "  g(count++);  // line 2\n"
                                        "};\n"
                                        "function g(x) {\n"
                                        "  var a=x;  // line 5\n"
                                        "};");

  // Compile the script and get function f.
  v8::Local<v8::Context> context = env.context();
  v8::ScriptOrigin origin = v8::ScriptOrigin(v8_str(env->GetIsolate(), "test"));
  v8::Script::Compile(context, script, &origin)
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "f"))
          .ToLocalChecked());

  // Set script break point on line 5 (in function g).
  int sbp1 = SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test", 5, 0);

  // Call f with different conditions on the script break point.
  break_point_hit_count = 0;
  ChangeScriptBreakPointConditionFromJS(env->GetIsolate(), sbp1, "false");
  f->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  ChangeScriptBreakPointConditionFromJS(env->GetIsolate(), sbp1, "true");
  break_point_hit_count = 0;
  f->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);

  ChangeScriptBreakPointConditionFromJS(env->GetIsolate(), sbp1, "x % 2 == 0");
  break_point_hit_count = 0;
  for (int i = 0; i < 10; i++) {
    f->Call(env.context(), env->Global(), 0, NULL).ToLocalChecked();
  }
  CHECK_EQ(5, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test when several scripts has the same script data
TEST(ScriptBreakPointMultiple) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);

  v8::Local<v8::Context> context = env.context();
  v8::Local<v8::Function> f;
  v8::Local<v8::String> script_f = v8_str(env->GetIsolate(),
                                          "function f() {\n"
                                          "  a = 0;  // line 1\n"
                                          "}");

  v8::Local<v8::Function> g;
  v8::Local<v8::String> script_g = v8_str(env->GetIsolate(),
                                          "function g() {\n"
                                          "  b = 0;  // line 1\n"
                                          "}");

  v8::ScriptOrigin origin = v8::ScriptOrigin(v8_str(env->GetIsolate(), "test"));

  // Compile the scripts with same script data and get the functions.
  v8::Script::Compile(context, script_f, &origin)
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  f = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "f"))
          .ToLocalChecked());
  v8::Script::Compile(context, script_g, &origin)
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  g = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "g"))
          .ToLocalChecked());

  int sbp = SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test", 1, 0);

  // Call f and g and check that the script break point is active.
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Clear the script break point.
  ClearBreakPointFromJS(env->GetIsolate(), sbp);

  // Call f and g and check that the script break point is no longer active.
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Set script break point with the scripts loaded.
  sbp = SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test", 1, 0);

  // Call f and g and check that the script break point is active.
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test the script origin which has both name and line offset.
TEST(ScriptBreakPointLineOffset) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);

  v8::Local<v8::Context> context = env.context();
  v8::Local<v8::Function> f;
  v8::Local<v8::String> script =
      v8_str(env->GetIsolate(),
             "function f() {\n"
             "  a = 0;  // line 8 as this script has line offset 7\n"
             "  b = 0;  // line 9 as this script has line offset 7\n"
             "}");

  // Create script origin both name and line offset.
  v8::ScriptOrigin origin(v8_str(env->GetIsolate(), "test.html"),
                          v8::Integer::New(env->GetIsolate(), 7));

  // Compile the script and get the function.
  v8::Script::Compile(context, script, &origin)
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();

  int sbp1 =
      SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test.html", 8, 0);
  int sbp2 =
      SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test.html", 9, 0);

  f = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "f"))
          .ToLocalChecked());

  // Call f and check that the script break point is active.
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Clear the script break points.
  ClearBreakPointFromJS(env->GetIsolate(), sbp1);
  ClearBreakPointFromJS(env->GetIsolate(), sbp2);

  // Call f and check that no script break points are active.
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Set a script break point with the script loaded.
  sbp1 = SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test.html", 9, 0);

  // Call f and check that the script break point is active.
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test script break points set on lines.
TEST(ScriptBreakPointLine) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();

  // Create a function for checking the function when hitting a break point.
  frame_function_name = CompileFunction(&env,
                                        frame_function_name_source,
                                        "frame_function_name");

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);

  v8::Local<v8::Context> context = env.context();
  v8::Local<v8::Function> f;
  v8::Local<v8::Function> g;
  v8::Local<v8::String> script =
      v8_str(env->GetIsolate(),
             "a = 0                      // line 0\n"
             "function f() {\n"
             "  a = 1;                   // line 2\n"
             "}\n"
             " a = 2;                    // line 4\n"
             "  /* xx */ function g() {  // line 5\n"
             "    function h() {         // line 6\n"
             "      a = 3;               // line 7\n"
             "    }\n"
             "    h();                   // line 9\n"
             "    a = 4;                 // line 10\n"
             "  }\n"
             " a=5;                      // line 12");

  // Compile the script and get the function.
  break_point_hit_count = 0;
  v8::ScriptOrigin origin(v8_str(env->GetIsolate(), "test.html"),
                          v8::Integer::New(env->GetIsolate(), 0));
  v8::Local<v8::Script> compiled =
      v8::Script::Compile(context, script, &origin).ToLocalChecked();

  int sbp1 =
      SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test.html", 0, -1);
  int sbp2 =
      SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test.html", 1, -1);
  int sbp3 =
      SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test.html", 5, -1);

  compiled->Run(context).ToLocalChecked();

  f = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "f"))
          .ToLocalChecked());
  g = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "g"))
          .ToLocalChecked());

  // Check that a break point was hit when the script was run.
  CHECK_EQ(1, break_point_hit_count);
  CHECK_EQ(0, StrLength(last_function_hit));

  // Call f and check that the script break point.
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);
  CHECK_EQ(0, strcmp("f", last_function_hit));

  // Call g and check that the script break point.
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(3, break_point_hit_count);
  CHECK_EQ(0, strcmp("g", last_function_hit));

  // Clear the script break point on g and set one on h.
  ClearBreakPointFromJS(env->GetIsolate(), sbp3);
  int sbp4 =
      SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test.html", 6, -1);

  // Call g and check that the script break point in h is hit.
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(4, break_point_hit_count);
  CHECK_EQ(0, strcmp("h", last_function_hit));

  // Clear break points in f and h. Set a new one in the script between
  // functions f and g and test that there is no break points in f and g any
  // more.
  ClearBreakPointFromJS(env->GetIsolate(), sbp2);
  ClearBreakPointFromJS(env->GetIsolate(), sbp4);
  int sbp5 =
      SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test.html", 4, -1);
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Set a break point in the code after the last function decleration.
  int sbp6 =
      SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test.html", 12, -1);

  // Reloading the script should not hit any break points.
  break_point_hit_count = 0;
  v8::Script::Compile(context, script, &origin)
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  ClearBreakPointFromJS(env->GetIsolate(), sbp1);
  ClearBreakPointFromJS(env->GetIsolate(), sbp5);
  ClearBreakPointFromJS(env->GetIsolate(), sbp6);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test top level script break points set on lines.
TEST(ScriptBreakPointLineTopLevel) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);

  v8::Local<v8::Context> context = env.context();
  v8::Local<v8::String> script =
      v8_str(env->GetIsolate(),
             "function f() {\n"
             "  a = 1;                   // line 1\n"
             "}\n"
             "a = 2;                     // line 3\n");
  v8::Local<v8::Function> f;
  {
    v8::HandleScope scope(env->GetIsolate());
    CompileRunWithOrigin(script, "test.html");
  }
  f = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "f"))
          .ToLocalChecked());

  CcTest::CollectAllGarbage(i::Heap::kFinalizeIncrementalMarkingMask);

  SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test.html", 3, -1);

  // Call f and check that there was no break points.
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Recompile and run script and check that break point was not reapplied.
  break_point_hit_count = 0;
  CompileRunWithOrigin(script, "test.html");
  CHECK_EQ(0, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that it is possible to add and remove break points in a top level
// function which has no references but has not been collected yet.
TEST(ScriptBreakPointTopLevelCrash) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);

  CompileRunWithOrigin(
      "function f() {\n"
      "  return 0;\n"
      "}\n",
      "test.html");
  int sbp1 =
      SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test.html", 1, -1);
  break_point_hit_count = 0;

  CompileRun("f();");

  CHECK_EQ(1, break_point_hit_count);

  int sbp2 =
      SetScriptBreakPointByNameFromJS(env->GetIsolate(), "test.html", 3, -1);
  ClearBreakPointFromJS(env->GetIsolate(), sbp1);
  ClearBreakPointFromJS(env->GetIsolate(), sbp2);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that it is possible to remove the last break point for a function
// inside the break handling of that break point.
TEST(RemoveBreakPointInBreak) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::Local<v8::Context> context = env.context();
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){a=1;}", "foo");

  // Register the debug event listener pasing the function
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventRemoveBreakPoint, foo);

  debug_event_remove_break_point = SetBreakPoint(foo, 0);

  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);

  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that the debugger statement causes a break.
TEST(DebuggerStatement) {
  break_point_hit_count = 0;
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);
  v8::Local<v8::Context> context = env.context();
  v8::Script::Compile(context,
                      v8_str(env->GetIsolate(), "function bar(){debugger}"))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::Script::Compile(
      context, v8_str(env->GetIsolate(), "function foo(){debugger;debugger;}"))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::Local<v8::Function> foo = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "foo"))
          .ToLocalChecked());
  v8::Local<v8::Function> bar = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "bar"))
          .ToLocalChecked());

  // Run function with debugger statement
  bar->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);

  // Run function with two debugger statement
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(3, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test setting a breakpoint on the debugger statement.
TEST(DebuggerStatementBreakpoint) {
    break_point_hit_count = 0;
    DebugLocalContext env;
    v8::HandleScope scope(env->GetIsolate());
    v8::Local<v8::Context> context = env.context();
    v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                     DebugEventBreakPointHitCount);
    v8::Script::Compile(context,
                        v8_str(env->GetIsolate(), "function foo(){debugger;}"))
        .ToLocalChecked()
        ->Run(context)
        .ToLocalChecked();
    v8::Local<v8::Function> foo = v8::Local<v8::Function>::Cast(
        env->Global()
            ->Get(context, v8_str(env->GetIsolate(), "foo"))
            .ToLocalChecked());

    // The debugger statement triggers breakpoint hit
    foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
    CHECK_EQ(1, break_point_hit_count);

    int bp = SetBreakPoint(foo, 0);

    // Set breakpoint does not duplicate hits
    foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
    CHECK_EQ(2, break_point_hit_count);

    ClearBreakPoint(bp);
    v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
    CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that the evaluation of expressions when a break point is hit generates
// the correct results.
TEST(DebugEvaluate) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  env.ExposeDebug();

  // Create a function for checking the evaluation when hitting a break point.
  evaluate_check_function = CompileFunction(&env,
                                            evaluate_check_source,
                                            "evaluate_check");
  // Register the debug event listener
  v8::Debug::SetDebugEventListener(isolate, DebugEventEvaluate);

  // Different expected vaules of x and a when in a break point (u = undefined,
  // d = Hello, world!).
  struct EvaluateCheck checks_uu[] = {{"x", v8::Undefined(isolate)},
                                      {"a", v8::Undefined(isolate)},
                                      {NULL, v8::Local<v8::Value>()}};
  struct EvaluateCheck checks_hu[] = {
      {"x", v8_str(env->GetIsolate(), "Hello, world!")},
      {"a", v8::Undefined(isolate)},
      {NULL, v8::Local<v8::Value>()}};
  struct EvaluateCheck checks_hh[] = {
      {"x", v8_str(env->GetIsolate(), "Hello, world!")},
      {"a", v8_str(env->GetIsolate(), "Hello, world!")},
      {NULL, v8::Local<v8::Value>()}};

  // Simple test function. The "y=0" is in the function foo to provide a break
  // location. For "y=0" the "y" is at position 15 in the foo function
  // therefore setting breakpoint at position 15 will break at "y=0" and
  // setting it higher will break after.
  v8::Local<v8::Function> foo = CompileFunction(&env,
    "function foo(x) {"
    "  var a;"
    "  y=0;"  // To ensure break location 1.
    "  a=x;"
    "  y=0;"  // To ensure break location 2.
    "}",
    "foo");
  const int foo_break_position_1 = 15;
  const int foo_break_position_2 = 29;

  v8::Local<v8::Context> context = env.context();
  // Arguments with one parameter "Hello, world!"
  v8::Local<v8::Value> argv_foo[1] = {
      v8_str(env->GetIsolate(), "Hello, world!")};

  // Call foo with breakpoint set before a=x and undefined as parameter.
  int bp = SetBreakPoint(foo, foo_break_position_1);
  checks = checks_uu;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // Call foo with breakpoint set before a=x and parameter "Hello, world!".
  checks = checks_hu;
  foo->Call(context, env->Global(), 1, argv_foo).ToLocalChecked();

  // Call foo with breakpoint set after a=x and parameter "Hello, world!".
  ClearBreakPoint(bp);
  SetBreakPoint(foo, foo_break_position_2);
  checks = checks_hh;
  foo->Call(context, env->Global(), 1, argv_foo).ToLocalChecked();

  // Test that overriding Object.prototype will not interfere into evaluation
  // on call frame.
  v8::Local<v8::Function> zoo =
      CompileFunction(&env,
                      "x = undefined;"
                      "function zoo(t) {"
                      "  var a=x;"
                      "  Object.prototype.x = 42;"
                      "  x=t;"
                      "  y=0;"  // To ensure break location.
                      "  delete Object.prototype.x;"
                      "  x=a;"
                      "}",
                      "zoo");
  const int zoo_break_position = 50;

  // Arguments with one parameter "Hello, world!"
  v8::Local<v8::Value> argv_zoo[1] = {
      v8_str(env->GetIsolate(), "Hello, world!")};

  // Call zoo with breakpoint set at y=0.
  DebugEventCounterClear();
  bp = SetBreakPoint(zoo, zoo_break_position);
  checks = checks_hu;
  zoo->Call(context, env->Global(), 1, argv_zoo).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  ClearBreakPoint(bp);

  // Test function with an inner function. The "y=0" is in function barbar
  // to provide a break location. For "y=0" the "y" is at position 8 in the
  // barbar function therefore setting breakpoint at position 8 will break at
  // "y=0" and setting it higher will break after.
  v8::Local<v8::Function> bar = CompileFunction(&env,
    "y = 0;"
    "x = 'Goodbye, world!';"
    "function bar(x, b) {"
    "  var a;"
    "  function barbar() {"
    "    y=0; /* To ensure break location.*/"
    "    a=x;"
    "  };"
    "  debug.Debug.clearAllBreakPoints();"
    "  barbar();"
    "  y=0;a=x;"
    "}",
    "bar");
  const int barbar_break_position = 8;

  // Call bar setting breakpoint before a=x in barbar and undefined as
  // parameter.
  checks = checks_uu;
  v8::Local<v8::Value> argv_bar_1[2] = {
      v8::Undefined(isolate), v8::Number::New(isolate, barbar_break_position)};
  bar->Call(context, env->Global(), 2, argv_bar_1).ToLocalChecked();

  // Call bar setting breakpoint before a=x in barbar and parameter
  // "Hello, world!".
  checks = checks_hu;
  v8::Local<v8::Value> argv_bar_2[2] = {
      v8_str(env->GetIsolate(), "Hello, world!"),
      v8::Number::New(env->GetIsolate(), barbar_break_position)};
  bar->Call(context, env->Global(), 2, argv_bar_2).ToLocalChecked();

  // Call bar setting breakpoint after a=x in barbar and parameter
  // "Hello, world!".
  checks = checks_hh;
  v8::Local<v8::Value> argv_bar_3[2] = {
      v8_str(env->GetIsolate(), "Hello, world!"),
      v8::Number::New(env->GetIsolate(), barbar_break_position + 1)};
  bar->Call(context, env->Global(), 2, argv_bar_3).ToLocalChecked();

  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}


int debugEventCount = 0;
static void CheckDebugEvent(const v8::Debug::EventDetails& eventDetails) {
  if (eventDetails.GetEvent() == v8::Break) ++debugEventCount;
}


// Test that the conditional breakpoints work event if code generation from
// strings is prohibited in the debugee context.
TEST(ConditionalBreakpointWithCodeGenerationDisallowed) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(env->GetIsolate(), CheckDebugEvent);

  v8::Local<v8::Context> context = env.context();
  v8::Local<v8::Function> foo = CompileFunction(&env,
    "function foo(x) {\n"
    "  var s = 'String value2';\n"
    "  return s + x;\n"
    "}",
    "foo");

  // Set conditional breakpoint with condition 'true'.
  CompileRun("debug.Debug.setBreakPoint(foo, 2, 0, 'true')");

  debugEventCount = 0;
  env->AllowCodeGenerationFromStrings(false);
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, debugEventCount);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


bool checkedDebugEvals = true;
v8::Local<v8::Function> checkGlobalEvalFunction;
v8::Local<v8::Function> checkFrameEvalFunction;
static void CheckDebugEval(const v8::Debug::EventDetails& eventDetails) {
  if (eventDetails.GetEvent() == v8::Break) {
    ++debugEventCount;
    v8::HandleScope handleScope(CcTest::isolate());

    v8::Local<v8::Value> args[] = {eventDetails.GetExecutionState()};
    CHECK(
        checkGlobalEvalFunction->Call(eventDetails.GetEventContext(),
                                      eventDetails.GetEventContext()->Global(),
                                      1, args)
            .ToLocalChecked()
            ->IsTrue());
    CHECK(checkFrameEvalFunction->Call(eventDetails.GetEventContext(),
                                       eventDetails.GetEventContext()->Global(),
                                       1, args)
              .ToLocalChecked()
              ->IsTrue());
  }
}


// Test that the evaluation of expressions when a break point is hit generates
// the correct results in case code generation from strings is disallowed in the
// debugee context.
TEST(DebugEvaluateWithCodeGenerationDisallowed) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(env->GetIsolate(), CheckDebugEval);

  v8::Local<v8::Context> context = env.context();
  v8::Local<v8::Function> foo = CompileFunction(&env,
    "var global = 'Global';\n"
    "function foo(x) {\n"
    "  var local = 'Local';\n"
    "  debugger;\n"
    "  return local + x;\n"
    "}",
    "foo");
  checkGlobalEvalFunction = CompileFunction(&env,
    "function checkGlobalEval(exec_state) {\n"
    "  return exec_state.evaluateGlobal('global').value() === 'Global';\n"
    "}",
    "checkGlobalEval");

  checkFrameEvalFunction = CompileFunction(&env,
    "function checkFrameEval(exec_state) {\n"
    "  return exec_state.frame(0).evaluate('local').value() === 'Local';\n"
    "}",
    "checkFrameEval");
  debugEventCount = 0;
  env->AllowCodeGenerationFromStrings(false);
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, debugEventCount);

  checkGlobalEvalFunction.Clear();
  checkFrameEvalFunction.Clear();
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Simple test of the stepping mechanism using only store ICs.
TEST(DebugStepLinear) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Create a function for testing stepping.
  v8::Local<v8::Function> foo = CompileFunction(&env,
                                                "function foo(){a=1;b=1;c=1;}",
                                                "foo");

  // Run foo to allow it to get optimized.
  CompileRun("a=0; b=0; c=0; foo();");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStep);

  SetBreakPoint(foo, 3);

  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Context> context = env.context();
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(4, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());

  // Register a debug event listener which just counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);

  SetBreakPoint(foo, 3);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // Without stepping only active break points are hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test of the stepping mechanism for keyed load in a loop.
TEST(DebugStepKeyedLoadLoop) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStep);

  // Create a function for testing stepping of keyed load. The statement 'y=1'
  // is there to have more than one breakable statement in the loop, TODO(315).
  v8::Local<v8::Function> foo = CompileFunction(
      &env,
      "function foo(a) {\n"
      "  var x;\n"
      "  var len = a.length;\n"
      "  for (var i = 0; i < len; i++) {\n"
      "    y = 1;\n"
      "    x = a[i];\n"
      "  }\n"
      "}\n"
      "y=0\n",
      "foo");

  v8::Local<v8::Context> context = env.context();
  // Create array [0,1,2,3,4,5,6,7,8,9]
  v8::Local<v8::Array> a = v8::Array::New(env->GetIsolate(), 10);
  for (int i = 0; i < 10; i++) {
    CHECK(a->Set(context, v8::Number::New(env->GetIsolate(), i),
                 v8::Number::New(env->GetIsolate(), i))
              .FromJust());
  }

  // Call function without any break points to ensure inlining is in place.
  const int kArgc = 1;
  v8::Local<v8::Value> args[kArgc] = {a};
  foo->Call(context, env->Global(), kArgc, args).ToLocalChecked();

  // Set up break point and step through the function.
  SetBreakPoint(foo, 3);
  step_action = StepNext;
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), kArgc, args).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(44, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test of the stepping mechanism for keyed store in a loop.
TEST(DebugStepKeyedStoreLoop) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStep);

  // Create a function for testing stepping of keyed store. The statement 'y=1'
  // is there to have more than one breakable statement in the loop, TODO(315).
  v8::Local<v8::Function> foo = CompileFunction(
      &env,
      "function foo(a) {\n"
      "  var len = a.length;\n"
      "  for (var i = 0; i < len; i++) {\n"
      "    y = 1;\n"
      "    a[i] = 42;\n"
      "  }\n"
      "}\n"
      "y=0\n",
      "foo");

  v8::Local<v8::Context> context = env.context();
  // Create array [0,1,2,3,4,5,6,7,8,9]
  v8::Local<v8::Array> a = v8::Array::New(env->GetIsolate(), 10);
  for (int i = 0; i < 10; i++) {
    CHECK(a->Set(context, v8::Number::New(env->GetIsolate(), i),
                 v8::Number::New(env->GetIsolate(), i))
              .FromJust());
  }

  // Call function without any break points to ensure inlining is in place.
  const int kArgc = 1;
  v8::Local<v8::Value> args[kArgc] = {a};
  foo->Call(context, env->Global(), kArgc, args).ToLocalChecked();

  // Set up break point and step through the function.
  SetBreakPoint(foo, 3);
  step_action = StepNext;
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), kArgc, args).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(44, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test of the stepping mechanism for named load in a loop.
TEST(DebugStepNamedLoadLoop) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping of named load.
  v8::Local<v8::Function> foo = CompileFunction(
      &env,
      "function foo() {\n"
          "  var a = [];\n"
          "  var s = \"\";\n"
          "  for (var i = 0; i < 10; i++) {\n"
          "    var v = new V(i, i + 1);\n"
          "    v.y;\n"
          "    a.length;\n"  // Special case: array length.
          "    s.length;\n"  // Special case: string length.
          "  }\n"
          "}\n"
          "function V(x, y) {\n"
          "  this.x = x;\n"
          "  this.y = y;\n"
          "}\n",
          "foo");

  // Call function without any break points to ensure inlining is in place.
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // Set up break point and step through the function.
  SetBreakPoint(foo, 4);
  step_action = StepNext;
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(65, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


static void DoDebugStepNamedStoreLoop(int expected) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStep);

  // Create a function for testing stepping of named store.
  v8::Local<v8::Context> context = env.context();
  v8::Local<v8::Function> foo = CompileFunction(
      &env,
      "function foo() {\n"
          "  var a = {a:1};\n"
          "  for (var i = 0; i < 10; i++) {\n"
          "    a.a = 2\n"
          "  }\n"
          "}\n",
          "foo");

  // Call function without any break points to ensure inlining is in place.
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // Set up break point and step through the function.
  SetBreakPoint(foo, 3);
  step_action = StepNext;
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // With stepping all expected break locations are hit.
  CHECK_EQ(expected, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test of the stepping mechanism for named load in a loop.
TEST(DebugStepNamedStoreLoop) { DoDebugStepNamedStoreLoop(34); }

// Test the stepping mechanism with different ICs.
TEST(DebugStepLinearMixedICs) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping.
  v8::Local<v8::Function> foo = CompileFunction(&env,
      "function bar() {};"
      "function foo() {"
      "  var x;"
      "  var index='name';"
      "  var y = {};"
      "  a=1;b=2;x=a;y[index]=3;x=y[index];bar();}", "foo");

  // Run functions to allow them to get optimized.
  CompileRun("a=0; b=0; bar(); foo();");

  SetBreakPoint(foo, 0);

  step_action = StepIn;
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(10, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());

  // Register a debug event listener which just counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);

  SetBreakPoint(foo, 0);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // Without stepping only active break points are hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


TEST(DebugStepDeclarations) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const char* src = "function foo() { "
                    "  var a;"
                    "  var b = 1;"
                    "  var c = foo;"
                    "  var d = Math.floor;"
                    "  var e = b + d(1.2);"
                    "}"
                    "foo()";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");

  SetBreakPoint(foo, 0);

  // Stepping through the declarations.
  step_action = StepIn;
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(5, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


TEST(DebugStepLocals) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const char* src = "function foo() { "
                    "  var a,b;"
                    "  a = 1;"
                    "  b = a + 2;"
                    "  b = 1 + 2 + 3;"
                    "  a = Math.floor(b);"
                    "}"
                    "foo()";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");

  SetBreakPoint(foo, 0);

  // Stepping through the declarations.
  step_action = StepIn;
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(5, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


TEST(DebugStepIf) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const int argc = 1;
  const char* src = "function foo(x) { "
                    "  a = 1;"
                    "  if (x) {"
                    "    b = 1;"
                    "  } else {"
                    "    c = 1;"
                    "    d = 1;"
                    "  }"
                    "}"
                    "a=0; b=0; c=0; d=0; foo()";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");
  SetBreakPoint(foo, 0);

  // Stepping through the true part.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_true[argc] = {v8::True(isolate)};
  foo->Call(context, env->Global(), argc, argv_true).ToLocalChecked();
  CHECK_EQ(4, break_point_hit_count);

  // Stepping through the false part.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_false[argc] = {v8::False(isolate)};
  foo->Call(context, env->Global(), argc, argv_false).ToLocalChecked();
  CHECK_EQ(5, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(isolate);
}


TEST(DebugStepSwitch) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(isolate, DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const int argc = 1;
  const char* src = "function foo(x) { "
                    "  a = 1;"
                    "  switch (x) {"
                    "    case 1:"
                    "      b = 1;"
                    "    case 2:"
                    "      c = 1;"
                    "      break;"
                    "    case 3:"
                    "      d = 1;"
                    "      e = 1;"
                    "      f = 1;"
                    "      break;"
                    "  }"
                    "}"
                    "a=0; b=0; c=0; d=0; e=0; f=0; foo()";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");
  SetBreakPoint(foo, 0);

  // One case with fall-through.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_1[argc] = {v8::Number::New(isolate, 1)};
  foo->Call(context, env->Global(), argc, argv_1).ToLocalChecked();
  CHECK_EQ(6, break_point_hit_count);

  // Another case.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_2[argc] = {v8::Number::New(isolate, 2)};
  foo->Call(context, env->Global(), argc, argv_2).ToLocalChecked();
  CHECK_EQ(5, break_point_hit_count);

  // Last case.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_3[argc] = {v8::Number::New(isolate, 3)};
  foo->Call(context, env->Global(), argc, argv_3).ToLocalChecked();
  CHECK_EQ(7, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}


TEST(DebugStepWhile) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(isolate, DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const int argc = 1;
  const char* src = "function foo(x) { "
                    "  var a = 0;"
                    "  while (a < x) {"
                    "    a++;"
                    "  }"
                    "}"
                    "foo()";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");
  SetBreakPoint(foo, 8);  // "var a = 0;"

  // Looping 0 times.  We still should break at the while-condition once.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_0[argc] = {v8::Number::New(isolate, 0)};
  foo->Call(context, env->Global(), argc, argv_0).ToLocalChecked();
  CHECK_EQ(3, break_point_hit_count);

  // Looping 10 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_10[argc] = {v8::Number::New(isolate, 10)};
  foo->Call(context, env->Global(), argc, argv_10).ToLocalChecked();
  CHECK_EQ(23, break_point_hit_count);

  // Looping 100 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_100[argc] = {v8::Number::New(isolate, 100)};
  foo->Call(context, env->Global(), argc, argv_100).ToLocalChecked();
  CHECK_EQ(203, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}


TEST(DebugStepDoWhile) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(isolate, DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const int argc = 1;
  const char* src = "function foo(x) { "
                    "  var a = 0;"
                    "  do {"
                    "    a++;"
                    "  } while (a < x)"
                    "}"
                    "foo()";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");
  SetBreakPoint(foo, 8);  // "var a = 0;"

  // Looping 0 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_0[argc] = {v8::Number::New(isolate, 0)};
  foo->Call(context, env->Global(), argc, argv_0).ToLocalChecked();
  CHECK_EQ(4, break_point_hit_count);

  // Looping 10 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_10[argc] = {v8::Number::New(isolate, 10)};
  foo->Call(context, env->Global(), argc, argv_10).ToLocalChecked();
  CHECK_EQ(22, break_point_hit_count);

  // Looping 100 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_100[argc] = {v8::Number::New(isolate, 100)};
  foo->Call(context, env->Global(), argc, argv_100).ToLocalChecked();
  CHECK_EQ(202, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}


TEST(DebugStepFor) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(isolate, DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const int argc = 1;
  const char* src = "function foo(x) { "
                    "  a = 1;"
                    "  for (i = 0; i < x; i++) {"
                    "    b = 1;"
                    "  }"
                    "}"
                    "a=0; b=0; i=0; foo()";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");

  SetBreakPoint(foo, 8);  // "a = 1;"

  // Looping 0 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_0[argc] = {v8::Number::New(isolate, 0)};
  foo->Call(context, env->Global(), argc, argv_0).ToLocalChecked();
  CHECK_EQ(4, break_point_hit_count);

  // Looping 10 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_10[argc] = {v8::Number::New(isolate, 10)};
  foo->Call(context, env->Global(), argc, argv_10).ToLocalChecked();
  CHECK_EQ(34, break_point_hit_count);

  // Looping 100 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_100[argc] = {v8::Number::New(isolate, 100)};
  foo->Call(context, env->Global(), argc, argv_100).ToLocalChecked();
  CHECK_EQ(304, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}


TEST(DebugStepForContinue) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(isolate, DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const int argc = 1;
  const char* src = "function foo(x) { "
                    "  var a = 0;"
                    "  var b = 0;"
                    "  var c = 0;"
                    "  for (var i = 0; i < x; i++) {"
                    "    a++;"
                    "    if (a % 2 == 0) continue;"
                    "    b++;"
                    "    c++;"
                    "  }"
                    "  return b;"
                    "}"
                    "foo()";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");
  v8::Local<v8::Value> result;
  SetBreakPoint(foo, 8);  // "var a = 0;"

  // Each loop generates 4 or 5 steps depending on whether a is equal.

  // Looping 10 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_10[argc] = {v8::Number::New(isolate, 10)};
  result = foo->Call(context, env->Global(), argc, argv_10).ToLocalChecked();
  CHECK_EQ(5, result->Int32Value(context).FromJust());
  CHECK_EQ(62, break_point_hit_count);

  // Looping 100 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_100[argc] = {v8::Number::New(isolate, 100)};
  result = foo->Call(context, env->Global(), argc, argv_100).ToLocalChecked();
  CHECK_EQ(50, result->Int32Value(context).FromJust());
  CHECK_EQ(557, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}


TEST(DebugStepForBreak) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(isolate, DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const int argc = 1;
  const char* src = "function foo(x) { "
                    "  var a = 0;"
                    "  var b = 0;"
                    "  var c = 0;"
                    "  for (var i = 0; i < 1000; i++) {"
                    "    a++;"
                    "    if (a == x) break;"
                    "    b++;"
                    "    c++;"
                    "  }"
                    "  return b;"
                    "}"
                    "foo()";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");
  v8::Local<v8::Value> result;
  SetBreakPoint(foo, 8);  // "var a = 0;"

  // Each loop generates 5 steps except for the last (when break is executed)
  // which only generates 4.

  // Looping 10 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_10[argc] = {v8::Number::New(isolate, 10)};
  result = foo->Call(context, env->Global(), argc, argv_10).ToLocalChecked();
  CHECK_EQ(9, result->Int32Value(context).FromJust());
  CHECK_EQ(64, break_point_hit_count);

  // Looping 100 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_100[argc] = {v8::Number::New(isolate, 100)};
  result = foo->Call(context, env->Global(), argc, argv_100).ToLocalChecked();
  CHECK_EQ(99, result->Int32Value(context).FromJust());
  CHECK_EQ(604, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}


TEST(DebugStepForIn) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  v8::Local<v8::Function> foo;
  const char* src_1 = "function foo() { "
                      "  var a = [1, 2];"
                      "  for (x in a) {"
                      "    b = 0;"
                      "  }"
                      "}"
                      "foo()";
  foo = CompileFunction(&env, src_1, "foo");
  SetBreakPoint(foo, 0);  // "var a = ..."

  step_action = StepIn;
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(8, break_point_hit_count);

  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const char* src_2 = "function foo() { "
                      "  var a = {a:[1, 2, 3]};"
                      "  for (x in a.a) {"
                      "    b = 0;"
                      "  }"
                      "}"
                      "foo()";
  foo = CompileFunction(&env, src_2, "foo");
  SetBreakPoint(foo, 0);  // "var a = ..."

  step_action = StepIn;
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(10, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


TEST(DebugStepWith) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const char* src = "function foo(x) { "
                    "  var a = {};"
                    "  with (a) {}"
                    "  with (b) {}"
                    "}"
                    "foo()";
  CHECK(env->Global()
            ->Set(context, v8_str(env->GetIsolate(), "b"),
                  v8::Object::New(env->GetIsolate()))
            .FromJust());
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");
  v8::Local<v8::Value> result;
  SetBreakPoint(foo, 8);  // "var a = {};"

  step_action = StepIn;
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(4, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


TEST(DebugConditional) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(isolate, DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const char* src =
      "function foo(x) { "
      "  return x ? 1 : 2;"
      "}"
      "foo()";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");
  SetBreakPoint(foo, 0);  // "var a;"

  step_action = StepIn;
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  step_action = StepIn;
  break_point_hit_count = 0;
  const int argc = 1;
  v8::Local<v8::Value> argv_true[argc] = {v8::True(isolate)};
  foo->Call(context, env->Global(), argc, argv_true).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}


TEST(StepInOutSimple) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Create a function for checking the function when hitting a break point.
  frame_function_name = CompileFunction(&env,
                                        frame_function_name_source,
                                        "frame_function_name");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStepSequence);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const char* src = "function a() {b();c();}; "
                    "function b() {c();}; "
                    "function c() {}; "
                    "a(); b(); c()";
  v8::Local<v8::Function> a = CompileFunction(&env, src, "a");
  SetBreakPoint(a, 0);

  // Step through invocation of a with step in.
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "abcbaca";
  a->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(StrLength(expected_step_sequence),
           break_point_hit_count);

  // Step through invocation of a with step next.
  step_action = StepNext;
  break_point_hit_count = 0;
  expected_step_sequence = "aaa";
  a->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(StrLength(expected_step_sequence),
           break_point_hit_count);

  // Step through invocation of a with step out.
  step_action = StepOut;
  break_point_hit_count = 0;
  expected_step_sequence = "a";
  a->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(StrLength(expected_step_sequence),
           break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


TEST(StepInOutTree) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Create a function for checking the function when hitting a break point.
  frame_function_name = CompileFunction(&env,
                                        frame_function_name_source,
                                        "frame_function_name");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStepSequence);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const char* src = "function a() {b(c(d()),d());c(d());d()}; "
                    "function b(x,y) {c();}; "
                    "function c(x) {}; "
                    "function d() {}; "
                    "a(); b(); c(); d()";
  v8::Local<v8::Function> a = CompileFunction(&env, src, "a");
  SetBreakPoint(a, 0);

  // Step through invocation of a with step in.
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "adacadabcbadacada";
  a->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(StrLength(expected_step_sequence),
           break_point_hit_count);

  // Step through invocation of a with step next.
  step_action = StepNext;
  break_point_hit_count = 0;
  expected_step_sequence = "aaaa";
  a->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(StrLength(expected_step_sequence),
           break_point_hit_count);

  // Step through invocation of a with step out.
  step_action = StepOut;
  break_point_hit_count = 0;
  expected_step_sequence = "a";
  a->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(StrLength(expected_step_sequence),
           break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate(), true);
}


TEST(StepInOutBranch) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Create a function for checking the function when hitting a break point.
  frame_function_name = CompileFunction(&env,
                                        frame_function_name_source,
                                        "frame_function_name");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStepSequence);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const char* src = "function a() {b(false);c();}; "
                    "function b(x) {if(x){c();};}; "
                    "function c() {}; "
                    "a(); b(); c()";
  v8::Local<v8::Function> a = CompileFunction(&env, src, "a");
  SetBreakPoint(a, 0);

  // Step through invocation of a.
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "abbaca";
  a->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(StrLength(expected_step_sequence),
           break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that step in does not step into native functions.
TEST(DebugStepNatives) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Create a function for testing stepping.
  v8::Local<v8::Function> foo = CompileFunction(
      &env,
      "function foo(){debugger;Math.sin(1);}",
      "foo");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  step_action = StepIn;
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(3, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());

  // Register a debug event listener which just counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);

  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // Without stepping only active break points are hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that step in works with function.apply.
TEST(DebugStepFunctionApply) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Create a function for testing stepping.
  v8::Local<v8::Function> foo = CompileFunction(
      &env,
      "function bar(x, y, z) { if (x == 1) { a = y; b = z; } }"
      "function foo(){ debugger; bar.apply(this, [1,2,3]); }",
      "foo");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStep);

  v8::Local<v8::Context> context = env.context();
  step_action = StepIn;
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(7, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());

  // Register a debug event listener which just counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);

  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // Without stepping only the debugger statement is hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that step in works with function.call.
TEST(DebugStepFunctionCall) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping.
  v8::Local<v8::Function> foo = CompileFunction(
      &env,
      "function bar(x, y, z) { if (x == 1) { a = y; b = z; } }"
      "function foo(a){ debugger;"
      "                 if (a) {"
      "                   bar.call(this, 1, 2, 3);"
      "                 } else {"
      "                   bar.call(this, 0);"
      "                 }"
      "}",
      "foo");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(isolate, DebugEventStep);
  step_action = StepIn;

  // Check stepping where the if condition in bar is false.
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(6, break_point_hit_count);

  // Check stepping where the if condition in bar is true.
  break_point_hit_count = 0;
  const int argc = 1;
  v8::Local<v8::Value> argv[argc] = {v8::True(isolate)};
  foo->Call(context, env->Global(), argc, argv).ToLocalChecked();
  CHECK_EQ(8, break_point_hit_count);

  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);

  // Register a debug event listener which just counts.
  v8::Debug::SetDebugEventListener(isolate, DebugEventBreakPointHitCount);

  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // Without stepping only the debugger statement is hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}


// Test that step in works with Function.call.apply.
TEST(DebugStepFunctionCallApply) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping.
  v8::Local<v8::Function> foo =
      CompileFunction(&env,
                      "function bar() { }"
                      "function foo(){ debugger;"
                      "                Function.call.apply(bar);"
                      "                Function.call.apply(Function.call, "
                      "[Function.call, bar]);"
                      "}",
                      "foo");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(isolate, DebugEventStep);
  step_action = StepIn;

  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(6, break_point_hit_count);

  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);

  // Register a debug event listener which just counts.
  v8::Debug::SetDebugEventListener(isolate, DebugEventBreakPointHitCount);

  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // Without stepping only the debugger statement is hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}


// Tests that breakpoint will be hit if it's set in script.
TEST(PauseInScript) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();

  // Register a debug event listener which counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventCounter);

  v8::Local<v8::Context> context = env.context();
  // Create a script that returns a function.
  const char* src = "(function (evt) {})";
  const char* script_name = "StepInHandlerTest";

  v8::ScriptOrigin origin(v8_str(env->GetIsolate(), script_name),
                          v8::Integer::New(env->GetIsolate(), 0));
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, v8_str(env->GetIsolate(), src), &origin)
          .ToLocalChecked();

  // Set breakpoint in the script.
  SetScriptBreakPointByNameFromJS(env->GetIsolate(), script_name, 0, -1);
  break_point_hit_count = 0;

  v8::Local<v8::Value> r = script->Run(context).ToLocalChecked();

  CHECK(r->IsFunction());
  CHECK_EQ(1, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


static void DebugEventCounterCheck(int caught, int uncaught, int message) {
  CHECK_EQ(caught, exception_hit_count);
  CHECK_EQ(uncaught, uncaught_exception_hit_count);
  CHECK_EQ(message, message_callback_count);
}


// Test break on exceptions. For each exception break combination the number
// of debug event exception callbacks and message callbacks are collected. The
// number of debug event exception callbacks are used to check that the
// debugger is called correctly and the number of message callbacks is used to
// check that uncaught exceptions are still returned even if there is a break
// for them.
TEST(BreakOnException) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();

  v8::Local<v8::Context> context = env.context();
  // Create functions for testing break on exception.
  CompileFunction(&env, "function throws(){throw 1;}", "throws");
  v8::Local<v8::Function> caught =
      CompileFunction(&env,
                      "function caught(){try {throws();} catch(e) {};}",
                      "caught");
  v8::Local<v8::Function> notCaught =
      CompileFunction(&env, "function notCaught(){throws();}", "notCaught");
  v8::Local<v8::Function> notCaughtFinally = CompileFunction(
      &env, "function notCaughtFinally(){try{throws();}finally{}}",
      "notCaughtFinally");
  // In this edge case, even though this finally does not propagate the
  // exception, the debugger considers this uncaught, since we want to break
  // at the first throw for the general case where finally implicitly rethrows.
  v8::Local<v8::Function> edgeCaseFinally = CompileFunction(
      &env, "function caughtFinally(){L:try{throws();}finally{break L;}}",
      "caughtFinally");

  env->GetIsolate()->AddMessageListener(MessageCallbackCount);
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventCounter);

  // Initial state should be no break on exceptions.
  DebugEventCounterClear();
  MessageCallbackCountClear();
  caught->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(0, 0, 0);
  CHECK(notCaught->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(0, 0, 1);
  CHECK(notCaughtFinally->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(0, 0, 2);
  edgeCaseFinally->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(0, 0, 2);

  // No break on exception
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnException(false, false);
  caught->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(0, 0, 0);
  CHECK(notCaught->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(0, 0, 1);
  CHECK(notCaughtFinally->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(0, 0, 2);
  edgeCaseFinally->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(0, 0, 2);

  // Break on uncaught exception
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnException(false, true);
  caught->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(0, 0, 0);
  CHECK(notCaught->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(1, 1, 1);
  CHECK(notCaughtFinally->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(2, 2, 2);
  edgeCaseFinally->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(3, 3, 2);

  // Break on exception and uncaught exception
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnException(true, true);
  caught->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(1, 0, 0);
  CHECK(notCaught->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(2, 1, 1);
  CHECK(notCaughtFinally->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(3, 2, 2);
  edgeCaseFinally->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(4, 3, 2);

  // Break on exception
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnException(true, false);
  caught->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(1, 0, 0);
  CHECK(notCaught->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(2, 1, 1);
  CHECK(notCaughtFinally->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(3, 2, 2);
  edgeCaseFinally->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(4, 3, 2);

  // No break on exception using JavaScript
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnExceptionFromJS(env->GetIsolate(), false, false);
  caught->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(0, 0, 0);
  CHECK(notCaught->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(0, 0, 1);
  CHECK(notCaughtFinally->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(0, 0, 2);
  edgeCaseFinally->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(0, 0, 2);

  // Break on uncaught exception using JavaScript
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnExceptionFromJS(env->GetIsolate(), false, true);
  caught->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(0, 0, 0);
  CHECK(notCaught->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(1, 1, 1);
  CHECK(notCaughtFinally->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(2, 2, 2);
  edgeCaseFinally->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(3, 3, 2);

  // Break on exception and uncaught exception using JavaScript
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnExceptionFromJS(env->GetIsolate(), true, true);
  caught->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(1, 0, 0);
  CHECK(notCaught->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(2, 1, 1);
  CHECK(notCaughtFinally->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(3, 2, 2);
  edgeCaseFinally->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(4, 3, 2);

  // Break on exception using JavaScript
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnExceptionFromJS(env->GetIsolate(), true, false);
  caught->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(1, 0, 0);
  CHECK(notCaught->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(2, 1, 1);
  CHECK(notCaughtFinally->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(3, 2, 2);
  edgeCaseFinally->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(4, 3, 2);

  // No break on exception using native API
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnExceptionFromAPI(env->GetIsolate(),
                                v8::debug::NoBreakOnException);
  caught->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(0, 0, 0);
  CHECK(notCaught->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(0, 0, 1);
  CHECK(notCaughtFinally->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(0, 0, 2);
  edgeCaseFinally->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(0, 0, 2);

  // // Break on uncaught exception using native API
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnExceptionFromAPI(env->GetIsolate(),
                                v8::debug::BreakOnUncaughtException);
  caught->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(0, 0, 0);
  CHECK(notCaught->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(1, 1, 1);
  CHECK(notCaughtFinally->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(2, 2, 2);
  edgeCaseFinally->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(3, 3, 2);

  // // Break on exception and uncaught exception using native API
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnExceptionFromAPI(env->GetIsolate(),
                                v8::debug::BreakOnAnyException);
  caught->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(1, 0, 0);
  CHECK(notCaught->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(2, 1, 1);
  CHECK(notCaughtFinally->Call(context, env->Global(), 0, NULL).IsEmpty());
  DebugEventCounterCheck(3, 2, 2);
  edgeCaseFinally->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  DebugEventCounterCheck(4, 3, 2);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
  env->GetIsolate()->RemoveMessageListeners(MessageCallbackCount);
}


static void try_finally_original_message(v8::Local<v8::Message> message,
                                         v8::Local<v8::Value> data) {
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
  CHECK_EQ(2, message->GetLineNumber(context).FromJust());
  CHECK_EQ(2, message->GetStartColumn(context).FromJust());
  message_callback_count++;
}


TEST(TryFinallyOriginalMessage) {
  // Test that the debugger plays nicely with the pending message.
  message_callback_count = 0;
  DebugEventCounterClear();
  DebugLocalContext env;
  v8::Isolate* isolate = CcTest::isolate();
  isolate->AddMessageListener(try_finally_original_message);
  v8::Debug::SetDebugEventListener(isolate, DebugEventCounter);
  ChangeBreakOnException(true, true);
  v8::HandleScope scope(isolate);
  CompileRun(
      "try {\n"
      "  throw 1;\n"
      "} finally {\n"
      "}\n");
  DebugEventCounterCheck(1, 1, 1);
  v8::Debug::SetDebugEventListener(isolate, nullptr);
  isolate->RemoveMessageListeners(try_finally_original_message);
}


TEST(EvalJSInDebugEventListenerOnNativeReThrownException) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();

  // Create functions for testing break on exception.
  v8::Local<v8::Function> noThrowJS = CompileFunction(
      &env, "function noThrowJS(){var a=[1]; a.push(2); return a.length;}",
      "noThrowJS");

  debug_event_listener_callback = noThrowJS;
  debug_event_listener_callback_result = 2;

  env->GetIsolate()->AddMessageListener(MessageCallbackCount);
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventCounter);
  // Break on uncaught exception
  ChangeBreakOnException(false, true);
  DebugEventCounterClear();
  MessageCallbackCountClear();

  // ReThrow native error
  {
    v8::TryCatch tryCatch(env->GetIsolate());
    env->GetIsolate()->ThrowException(
        v8::Exception::TypeError(v8_str(env->GetIsolate(), "Type error")));
    CHECK(tryCatch.HasCaught());
    tryCatch.ReThrow();
  }
  CHECK_EQ(1, exception_hit_count);
  CHECK_EQ(1, uncaught_exception_hit_count);
  CHECK_EQ(0, message_callback_count);  // FIXME: Should it be 1 ?
  CHECK(!debug_event_listener_callback.IsEmpty());

  debug_event_listener_callback.Clear();
}


// Test break on exception from compiler errors. When compiling using
// v8::Script::Compile there is no JavaScript stack whereas when compiling using
// eval there are JavaScript frames.
TEST(BreakOnCompileException) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::Local<v8::Context> context = env.context();
  // For this test, we want to break on uncaught exceptions:
  ChangeBreakOnException(false, true);

  // Create a function for checking the function when hitting a break point.
  frame_count = CompileFunction(&env, frame_count_source, "frame_count");

  env->GetIsolate()->AddMessageListener(MessageCallbackCount);
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventCounter);

  DebugEventCounterClear();
  MessageCallbackCountClear();

  // Check initial state.
  CHECK_EQ(0, exception_hit_count);
  CHECK_EQ(0, uncaught_exception_hit_count);
  CHECK_EQ(0, message_callback_count);
  CHECK_EQ(-1, last_js_stack_height);

  // Throws SyntaxError: Unexpected end of input
  CHECK(
      v8::Script::Compile(context, v8_str(env->GetIsolate(), "+++")).IsEmpty());
  CHECK_EQ(1, exception_hit_count);
  CHECK_EQ(1, uncaught_exception_hit_count);
  CHECK_EQ(1, message_callback_count);
  CHECK_EQ(0, last_js_stack_height);  // No JavaScript stack.

  // Throws SyntaxError: Unexpected identifier
  CHECK(
      v8::Script::Compile(context, v8_str(env->GetIsolate(), "x x")).IsEmpty());
  CHECK_EQ(2, exception_hit_count);
  CHECK_EQ(2, uncaught_exception_hit_count);
  CHECK_EQ(2, message_callback_count);
  CHECK_EQ(0, last_js_stack_height);  // No JavaScript stack.

  // Throws SyntaxError: Unexpected end of input
  CHECK(v8::Script::Compile(context, v8_str(env->GetIsolate(), "eval('+++')"))
            .ToLocalChecked()
            ->Run(context)
            .IsEmpty());
  CHECK_EQ(3, exception_hit_count);
  CHECK_EQ(3, uncaught_exception_hit_count);
  CHECK_EQ(3, message_callback_count);
  CHECK_EQ(1, last_js_stack_height);

  // Throws SyntaxError: Unexpected identifier
  CHECK(v8::Script::Compile(context, v8_str(env->GetIsolate(), "eval('x x')"))
            .ToLocalChecked()
            ->Run(context)
            .IsEmpty());
  CHECK_EQ(4, exception_hit_count);
  CHECK_EQ(4, uncaught_exception_hit_count);
  CHECK_EQ(4, message_callback_count);
  CHECK_EQ(1, last_js_stack_height);
}


TEST(StepWithException) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // For this test, we want to break on uncaught exceptions:
  ChangeBreakOnException(false, true);

  // Create a function for checking the function when hitting a break point.
  frame_function_name = CompileFunction(&env,
                                        frame_function_name_source,
                                        "frame_function_name");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStepSequence);

  v8::Local<v8::Context> context = env.context();
  // Create functions for testing stepping.
  const char* src = "function a() { n(); }; "
                    "function b() { c(); }; "
                    "function c() { n(); }; "
                    "function d() { x = 1; try { e(); } catch(x) { x = 2; } }; "
                    "function e() { n(); }; "
                    "function f() { x = 1; try { g(); } catch(x) { x = 2; } }; "
                    "function g() { h(); }; "
                    "function h() { x = 1; throw 1; }; ";

  // Step through invocation of a.
  ClearStepping();
  v8::Local<v8::Function> a = CompileFunction(&env, src, "a");
  SetBreakPoint(a, 0);
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "aa";
  CHECK(a->Call(context, env->Global(), 0, NULL).IsEmpty());
  CHECK_EQ(StrLength(expected_step_sequence),
           break_point_hit_count);

  // Step through invocation of b + c.
  ClearStepping();
  v8::Local<v8::Function> b = CompileFunction(&env, src, "b");
  SetBreakPoint(b, 0);
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "bcc";
  CHECK(b->Call(context, env->Global(), 0, NULL).IsEmpty());
  CHECK_EQ(StrLength(expected_step_sequence),
           break_point_hit_count);

  // Step through invocation of d + e.
  ClearStepping();
  v8::Local<v8::Function> d = CompileFunction(&env, src, "d");
  SetBreakPoint(d, 0);
  ChangeBreakOnException(false, true);
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "ddedd";
  d->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(StrLength(expected_step_sequence),
           break_point_hit_count);

  // Step through invocation of d + e now with break on caught exceptions.
  ChangeBreakOnException(true, true);
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "ddeedd";
  d->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(StrLength(expected_step_sequence),
           break_point_hit_count);

  // Step through invocation of f + g + h.
  ClearStepping();
  v8::Local<v8::Function> f = CompileFunction(&env, src, "f");
  SetBreakPoint(f, 0);
  ChangeBreakOnException(false, true);
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "ffghhff";
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(StrLength(expected_step_sequence),
           break_point_hit_count);

  // Step through invocation of f + g + h now with break on caught exceptions.
  ChangeBreakOnException(true, true);
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "ffghhhff";
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(StrLength(expected_step_sequence),
           break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


TEST(DebugBreak) {
  i::FLAG_stress_compaction = false;
#ifdef VERIFY_HEAP
  i::FLAG_verify_heap = true;
#endif
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which sets the break flag and counts.
  v8::Debug::SetDebugEventListener(isolate, DebugEventBreak);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping.
  const char* src = "function f0() {}"
                    "function f1(x1) {}"
                    "function f2(x1,x2) {}"
                    "function f3(x1,x2,x3) {}";
  v8::Local<v8::Function> f0 = CompileFunction(&env, src, "f0");
  v8::Local<v8::Function> f1 = CompileFunction(&env, src, "f1");
  v8::Local<v8::Function> f2 = CompileFunction(&env, src, "f2");
  v8::Local<v8::Function> f3 = CompileFunction(&env, src, "f3");

  // Call the function to make sure it is compiled.
  v8::Local<v8::Value> argv[] = {
      v8::Number::New(isolate, 1), v8::Number::New(isolate, 1),
      v8::Number::New(isolate, 1), v8::Number::New(isolate, 1)};

  // Call all functions to make sure that they are compiled.
  f0->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  f1->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  f2->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  f3->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // Set the debug break flag.
  v8::Debug::DebugBreak(env->GetIsolate());

  // Call all functions with different argument count.
  break_point_hit_count = 0;
  for (unsigned int i = 0; i < arraysize(argv); i++) {
    f0->Call(context, env->Global(), i, argv).ToLocalChecked();
    f1->Call(context, env->Global(), i, argv).ToLocalChecked();
    f2->Call(context, env->Global(), i, argv).ToLocalChecked();
    f3->Call(context, env->Global(), i, argv).ToLocalChecked();
  }

  // One break for each function called.
  CHECK(4 * arraysize(argv) == break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}


// Test to ensure that JavaScript code keeps running while the debug break
// through the stack limit flag is set but breaks are disabled.
TEST(DisableBreak) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which sets the break flag and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventCounter);

  v8::Local<v8::Context> context = env.context();
  // Create a function for testing stepping.
  const char* src = "function f() {g()};function g(){i=0; while(i<10){i++}}";
  v8::Local<v8::Function> f = CompileFunction(&env, src, "f");

  // Set, test and cancel debug break.
  v8::Debug::DebugBreak(env->GetIsolate());
  v8::Debug::CancelDebugBreak(env->GetIsolate());

  // Set the debug break flag.
  v8::Debug::DebugBreak(env->GetIsolate());

  // Call all functions with different argument count.
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);

  {
    v8::Debug::DebugBreak(env->GetIsolate());
    i::Isolate* isolate = reinterpret_cast<i::Isolate*>(env->GetIsolate());
    v8::internal::DisableBreak disable_break(isolate->debug());
    f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
    CHECK_EQ(1, break_point_hit_count);
  }

  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}

TEST(DisableDebuggerStatement) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which sets the break flag and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventCounter);
  CompileRun("debugger;");
  CHECK_EQ(1, break_point_hit_count);

  // Check that we ignore debugger statement when breakpoints aren't active.
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(env->GetIsolate());
  isolate->debug()->set_break_points_active(false);
  CompileRun("debugger;");
  CHECK_EQ(1, break_point_hit_count);
}

static const char* kSimpleExtensionSource =
  "(function Foo() {"
  "  return 4;"
  "})() ";

// http://crbug.com/28933
// Test that debug break is disabled when bootstrapper is active.
TEST(NoBreakWhenBootstrapping) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which sets the break flag and counts.
  v8::Debug::SetDebugEventListener(isolate, DebugEventCounter);

  // Set the debug break flag.
  v8::Debug::DebugBreak(isolate);
  break_point_hit_count = 0;
  {
    // Create a context with an extension to make sure that some JavaScript
    // code is executed during bootstrapping.
    v8::RegisterExtension(new v8::Extension("simpletest",
                                            kSimpleExtensionSource));
    const char* extension_names[] = { "simpletest" };
    v8::ExtensionConfiguration extensions(1, extension_names);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate, &extensions);
  }
  // Check that no DebugBreak events occured during the context creation.
  CHECK_EQ(0, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}


static void NamedEnum(const v8::PropertyCallbackInfo<v8::Array>& info) {
  v8::Local<v8::Array> result = v8::Array::New(info.GetIsolate(), 3);
  v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
  CHECK(result->Set(context, v8::Integer::New(info.GetIsolate(), 0),
                    v8_str(info.GetIsolate(), "a"))
            .FromJust());
  CHECK(result->Set(context, v8::Integer::New(info.GetIsolate(), 1),
                    v8_str(info.GetIsolate(), "b"))
            .FromJust());
  CHECK(result->Set(context, v8::Integer::New(info.GetIsolate(), 2),
                    v8_str(info.GetIsolate(), "c"))
            .FromJust());
  info.GetReturnValue().Set(result);
}


static void IndexedEnum(const v8::PropertyCallbackInfo<v8::Array>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Array> result = v8::Array::New(isolate, 2);
  v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
  CHECK(result->Set(context, v8::Integer::New(isolate, 0),
                    v8::Number::New(isolate, 1))
            .FromJust());
  CHECK(result->Set(context, v8::Integer::New(isolate, 1),
                    v8::Number::New(isolate, 10))
            .FromJust());
  info.GetReturnValue().Set(result);
}


static void NamedGetter(v8::Local<v8::Name> name,
                        const v8::PropertyCallbackInfo<v8::Value>& info) {
  if (name->IsSymbol()) return;
  v8::String::Utf8Value n(v8::Local<v8::String>::Cast(name));
  if (strcmp(*n, "a") == 0) {
    info.GetReturnValue().Set(v8_str(info.GetIsolate(), "AA"));
    return;
  } else if (strcmp(*n, "b") == 0) {
    info.GetReturnValue().Set(v8_str(info.GetIsolate(), "BB"));
    return;
  } else if (strcmp(*n, "c") == 0) {
    info.GetReturnValue().Set(v8_str(info.GetIsolate(), "CC"));
    return;
  } else {
    info.GetReturnValue().SetUndefined();
    return;
  }
  info.GetReturnValue().Set(name);
}


static void IndexedGetter(uint32_t index,
                          const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(static_cast<double>(index + 1));
}


TEST(InterceptorPropertyMirror) {
  // Create a V8 environment with debug access.
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  env.ExposeDebug();

  v8::Local<v8::Context> context = env.context();
  // Create object with named interceptor.
  v8::Local<v8::ObjectTemplate> named = v8::ObjectTemplate::New(isolate);
  named->SetHandler(v8::NamedPropertyHandlerConfiguration(
      NamedGetter, NULL, NULL, NULL, NamedEnum));
  CHECK(env->Global()
            ->Set(context, v8_str(isolate, "intercepted_named"),
                  named->NewInstance(context).ToLocalChecked())
            .FromJust());

  // Create object with indexed interceptor.
  v8::Local<v8::ObjectTemplate> indexed = v8::ObjectTemplate::New(isolate);
  indexed->SetHandler(v8::IndexedPropertyHandlerConfiguration(
      IndexedGetter, NULL, NULL, NULL, IndexedEnum));
  CHECK(env->Global()
            ->Set(context, v8_str(isolate, "intercepted_indexed"),
                  indexed->NewInstance(context).ToLocalChecked())
            .FromJust());

  // Create object with both named and indexed interceptor.
  v8::Local<v8::ObjectTemplate> both = v8::ObjectTemplate::New(isolate);
  both->SetHandler(v8::NamedPropertyHandlerConfiguration(
      NamedGetter, NULL, NULL, NULL, NamedEnum));
  both->SetHandler(v8::IndexedPropertyHandlerConfiguration(
      IndexedGetter, NULL, NULL, NULL, IndexedEnum));
  CHECK(env->Global()
            ->Set(context, v8_str(isolate, "intercepted_both"),
                  both->NewInstance(context).ToLocalChecked())
            .FromJust());

  // Get mirrors for the three objects with interceptor.
  CompileRun(
      "var named_mirror = debug.MakeMirror(intercepted_named);"
      "var indexed_mirror = debug.MakeMirror(intercepted_indexed);"
      "var both_mirror = debug.MakeMirror(intercepted_both)");
  CHECK(CompileRun("named_mirror instanceof debug.ObjectMirror")
            ->BooleanValue(context)
            .FromJust());
  CHECK(CompileRun("indexed_mirror instanceof debug.ObjectMirror")
            ->BooleanValue(context)
            .FromJust());
  CHECK(CompileRun("both_mirror instanceof debug.ObjectMirror")
            ->BooleanValue(context)
            .FromJust());

  // Get the property names from the interceptors
  CompileRun(
      "named_names = named_mirror.propertyNames();"
      "indexed_names = indexed_mirror.propertyNames();"
      "both_names = both_mirror.propertyNames()");
  CHECK_EQ(3, CompileRun("named_names.length")->Int32Value(context).FromJust());
  CHECK_EQ(2,
           CompileRun("indexed_names.length")->Int32Value(context).FromJust());
  CHECK_EQ(5, CompileRun("both_names.length")->Int32Value(context).FromJust());

  // Check the expected number of properties.
  const char* source;
  source = "named_mirror.properties().length";
  CHECK_EQ(3, CompileRun(source)->Int32Value(context).FromJust());

  source = "indexed_mirror.properties().length";
  CHECK_EQ(2, CompileRun(source)->Int32Value(context).FromJust());

  source = "both_mirror.properties().length";
  CHECK_EQ(5, CompileRun(source)->Int32Value(context).FromJust());

  // Get the interceptor properties for the object with only named interceptor.
  CompileRun("var named_values = named_mirror.properties()");

  // Check that the properties are interceptor properties.
  for (int i = 0; i < 3; i++) {
    EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
    SNPrintF(buffer,
             "named_values[%d] instanceof debug.PropertyMirror", i);
    CHECK(CompileRun(buffer.start())->BooleanValue(context).FromJust());

    SNPrintF(buffer, "named_values[%d].isNative()", i);
    CHECK(CompileRun(buffer.start())->BooleanValue(context).FromJust());
  }

  // Get the interceptor properties for the object with only indexed
  // interceptor.
  CompileRun("var indexed_values = indexed_mirror.properties()");

  // Check that the properties are interceptor properties.
  for (int i = 0; i < 2; i++) {
    EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
    SNPrintF(buffer,
             "indexed_values[%d] instanceof debug.PropertyMirror", i);
    CHECK(CompileRun(buffer.start())->BooleanValue(context).FromJust());
  }

  // Get the interceptor properties for the object with both types of
  // interceptors.
  CompileRun("var both_values = both_mirror.properties()");

  // Check that the properties are interceptor properties.
  for (int i = 0; i < 5; i++) {
    EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
    SNPrintF(buffer, "both_values[%d] instanceof debug.PropertyMirror", i);
    CHECK(CompileRun(buffer.start())->BooleanValue(context).FromJust());
  }

  // Check the property names.
  source = "both_values[0].name() == '1'";
  CHECK(CompileRun(source)->BooleanValue(context).FromJust());

  source = "both_values[1].name() == '10'";
  CHECK(CompileRun(source)->BooleanValue(context).FromJust());

  source = "both_values[2].name() == 'a'";
  CHECK(CompileRun(source)->BooleanValue(context).FromJust());

  source = "both_values[3].name() == 'b'";
  CHECK(CompileRun(source)->BooleanValue(context).FromJust());

  source = "both_values[4].name() == 'c'";
  CHECK(CompileRun(source)->BooleanValue(context).FromJust());
}


TEST(HiddenPrototypePropertyMirror) {
  // Create a V8 environment with debug access.
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  env.ExposeDebug();

  v8::Local<v8::FunctionTemplate> t0 = v8::FunctionTemplate::New(isolate);
  t0->InstanceTemplate()->Set(v8_str(isolate, "x"),
                              v8::Number::New(isolate, 0));
  v8::Local<v8::FunctionTemplate> t1 = v8::FunctionTemplate::New(isolate);
  t1->SetHiddenPrototype(true);
  t1->InstanceTemplate()->Set(v8_str(isolate, "y"),
                              v8::Number::New(isolate, 1));
  v8::Local<v8::FunctionTemplate> t2 = v8::FunctionTemplate::New(isolate);
  t2->SetHiddenPrototype(true);
  t2->InstanceTemplate()->Set(v8_str(isolate, "z"),
                              v8::Number::New(isolate, 2));
  v8::Local<v8::FunctionTemplate> t3 = v8::FunctionTemplate::New(isolate);
  t3->InstanceTemplate()->Set(v8_str(isolate, "u"),
                              v8::Number::New(isolate, 3));

  v8::Local<v8::Context> context = env.context();
  // Create object and set them on the global object.
  v8::Local<v8::Object> o0 = t0->GetFunction(context)
                                 .ToLocalChecked()
                                 ->NewInstance(context)
                                 .ToLocalChecked();
  CHECK(env->Global()->Set(context, v8_str(isolate, "o0"), o0).FromJust());
  v8::Local<v8::Object> o1 = t1->GetFunction(context)
                                 .ToLocalChecked()
                                 ->NewInstance(context)
                                 .ToLocalChecked();
  CHECK(env->Global()->Set(context, v8_str(isolate, "o1"), o1).FromJust());
  v8::Local<v8::Object> o2 = t2->GetFunction(context)
                                 .ToLocalChecked()
                                 ->NewInstance(context)
                                 .ToLocalChecked();
  CHECK(env->Global()->Set(context, v8_str(isolate, "o2"), o2).FromJust());
  v8::Local<v8::Object> o3 = t3->GetFunction(context)
                                 .ToLocalChecked()
                                 ->NewInstance(context)
                                 .ToLocalChecked();
  CHECK(env->Global()->Set(context, v8_str(isolate, "o3"), o3).FromJust());

  // Get mirrors for the four objects.
  CompileRun(
      "var o0_mirror = debug.MakeMirror(o0);"
      "var o1_mirror = debug.MakeMirror(o1);"
      "var o2_mirror = debug.MakeMirror(o2);"
      "var o3_mirror = debug.MakeMirror(o3)");
  CHECK(CompileRun("o0_mirror instanceof debug.ObjectMirror")
            ->BooleanValue(context)
            .FromJust());
  CHECK(CompileRun("o1_mirror instanceof debug.ObjectMirror")
            ->BooleanValue(context)
            .FromJust());
  CHECK(CompileRun("o2_mirror instanceof debug.ObjectMirror")
            ->BooleanValue(context)
            .FromJust());
  CHECK(CompileRun("o3_mirror instanceof debug.ObjectMirror")
            ->BooleanValue(context)
            .FromJust());

  // Check that each object has one property.
  CHECK_EQ(1, CompileRun("o0_mirror.propertyNames().length")
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(1, CompileRun("o1_mirror.propertyNames().length")
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(1, CompileRun("o2_mirror.propertyNames().length")
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(1, CompileRun("o3_mirror.propertyNames().length")
                  ->Int32Value(context)
                  .FromJust());

  // Set o1 as prototype for o0. o1 has the hidden prototype flag so all
  // properties on o1 should be seen on o0.
  CHECK(o0->Set(context, v8_str(isolate, "__proto__"), o1).FromJust());
  CHECK_EQ(2, CompileRun("o0_mirror.propertyNames().length")
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(0, CompileRun("o0_mirror.property('x').value().value()")
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(1, CompileRun("o0_mirror.property('y').value().value()")
                  ->Int32Value(context)
                  .FromJust());

  // Set o2 as prototype for o0 (it will end up after o1 as o1 has the hidden
  // prototype flag. o2 also has the hidden prototype flag so all properties
  // on o2 should be seen on o0 as well as properties on o1.
  CHECK(o0->Set(context, v8_str(isolate, "__proto__"), o2).FromJust());
  CHECK_EQ(3, CompileRun("o0_mirror.propertyNames().length")
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(0, CompileRun("o0_mirror.property('x').value().value()")
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(1, CompileRun("o0_mirror.property('y').value().value()")
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(2, CompileRun("o0_mirror.property('z').value().value()")
                  ->Int32Value(context)
                  .FromJust());

  // Set o3 as prototype for o0 (it will end up after o1 and o2 as both o1 and
  // o2 has the hidden prototype flag. o3 does not have the hidden prototype
  // flag so properties on o3 should not be seen on o0 whereas the properties
  // from o1 and o2 should still be seen on o0.
  // Final prototype chain: o0 -> o1 -> o2 -> o3
  // Hidden prototypes:           ^^    ^^
  CHECK(o0->Set(context, v8_str(isolate, "__proto__"), o3).FromJust());
  CHECK_EQ(3, CompileRun("o0_mirror.propertyNames().length")
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(1, CompileRun("o3_mirror.propertyNames().length")
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(0, CompileRun("o0_mirror.property('x').value().value()")
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(1, CompileRun("o0_mirror.property('y').value().value()")
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(2, CompileRun("o0_mirror.property('z').value().value()")
                  ->Int32Value(context)
                  .FromJust());
  CHECK(CompileRun("o0_mirror.property('u').isUndefined()")
            ->BooleanValue(context)
            .FromJust());

  // The prototype (__proto__) for o0 should be o3 as o1 and o2 are hidden.
  CHECK(CompileRun("o0_mirror.protoObject() == o3_mirror")
            ->BooleanValue(context)
            .FromJust());
}


static void ProtperyXNativeGetter(
    v8::Local<v8::String> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(10);
}


TEST(NativeGetterPropertyMirror) {
  // Create a V8 environment with debug access.
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  env.ExposeDebug();

  v8::Local<v8::Context> context = env.context();
  v8::Local<v8::String> name = v8_str(isolate, "x");
  // Create object with named accessor.
  v8::Local<v8::ObjectTemplate> named = v8::ObjectTemplate::New(isolate);
  named->SetAccessor(name, &ProtperyXNativeGetter, NULL, v8::Local<v8::Value>(),
                     v8::DEFAULT, v8::None);

  // Create object with named property getter.
  CHECK(env->Global()
            ->Set(context, v8_str(isolate, "instance"),
                  named->NewInstance(context).ToLocalChecked())
            .FromJust());
  CHECK_EQ(10, CompileRun("instance.x")->Int32Value(context).FromJust());

  // Get mirror for the object with property getter.
  CompileRun("var instance_mirror = debug.MakeMirror(instance);");
  CHECK(CompileRun("instance_mirror instanceof debug.ObjectMirror")
            ->BooleanValue(context)
            .FromJust());

  CompileRun("var named_names = instance_mirror.propertyNames();");
  CHECK_EQ(1, CompileRun("named_names.length")->Int32Value(context).FromJust());
  CHECK(CompileRun("named_names[0] == 'x'")->BooleanValue(context).FromJust());
  CHECK(CompileRun("instance_mirror.property('x').value().isNumber()")
            ->BooleanValue(context)
            .FromJust());
  CHECK(CompileRun("instance_mirror.property('x').value().value() == 10")
            ->BooleanValue(context)
            .FromJust());
}


static void ProtperyXNativeGetterThrowingError(
    v8::Local<v8::String> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  CompileRun("throw new Error('Error message');");
}


TEST(NativeGetterThrowingErrorPropertyMirror) {
  // Create a V8 environment with debug access.
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  env.ExposeDebug();

  v8::Local<v8::Context> context = env.context();
  v8::Local<v8::String> name = v8_str(isolate, "x");
  // Create object with named accessor.
  v8::Local<v8::ObjectTemplate> named = v8::ObjectTemplate::New(isolate);
  named->SetAccessor(name, &ProtperyXNativeGetterThrowingError, NULL,
                     v8::Local<v8::Value>(), v8::DEFAULT, v8::None);

  // Create object with named property getter.
  CHECK(env->Global()
            ->Set(context, v8_str(isolate, "instance"),
                  named->NewInstance(context).ToLocalChecked())
            .FromJust());

  // Get mirror for the object with property getter.
  CompileRun("var instance_mirror = debug.MakeMirror(instance);");
  CHECK(CompileRun("instance_mirror instanceof debug.ObjectMirror")
            ->BooleanValue(context)
            .FromJust());
  CompileRun("named_names = instance_mirror.propertyNames();");
  CHECK_EQ(1, CompileRun("named_names.length")->Int32Value(context).FromJust());
  CHECK(CompileRun("named_names[0] == 'x'")->BooleanValue(context).FromJust());
  CHECK(CompileRun("instance_mirror.property('x').value().isError()")
            ->BooleanValue(context)
            .FromJust());

  // Check that the message is that passed to the Error constructor.
  CHECK(
      CompileRun(
          "instance_mirror.property('x').value().message() == 'Error message'")
          ->BooleanValue(context)
          .FromJust());
}


// Test that hidden properties object is not returned as an unnamed property
// among regular properties.
// See http://crbug.com/26491
TEST(NoHiddenProperties) {
  // Create a V8 environment with debug access.
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  env.ExposeDebug();

  v8::Local<v8::Context> context = env.context();
  // Create an object in the global scope.
  const char* source = "var obj = {a: 1};";
  v8::Script::Compile(context, v8_str(isolate, source))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(
      env->Global()->Get(context, v8_str(isolate, "obj")).ToLocalChecked());
  // Set a hidden property on the object.
  obj->SetPrivate(
         env.context(),
         v8::Private::New(isolate, v8_str(isolate, "v8::test-debug::a")),
         v8::Int32::New(isolate, 11))
      .FromJust();

  // Get mirror for the object with property getter.
  CompileRun("var obj_mirror = debug.MakeMirror(obj);");
  CHECK(CompileRun("obj_mirror instanceof debug.ObjectMirror")
            ->BooleanValue(context)
            .FromJust());
  CompileRun("var named_names = obj_mirror.propertyNames();");
  // There should be exactly one property. But there is also an unnamed
  // property whose value is hidden properties dictionary. The latter
  // property should not be in the list of reguar properties.
  CHECK_EQ(1, CompileRun("named_names.length")->Int32Value(context).FromJust());
  CHECK(CompileRun("named_names[0] == 'a'")->BooleanValue(context).FromJust());
  CHECK(CompileRun("obj_mirror.property('a').value().value() == 1")
            ->BooleanValue(context)
            .FromJust());

  // Object created by t0 will become hidden prototype of object 'obj'.
  v8::Local<v8::FunctionTemplate> t0 = v8::FunctionTemplate::New(isolate);
  t0->InstanceTemplate()->Set(v8_str(isolate, "b"),
                              v8::Number::New(isolate, 2));
  t0->SetHiddenPrototype(true);
  v8::Local<v8::FunctionTemplate> t1 = v8::FunctionTemplate::New(isolate);
  t1->InstanceTemplate()->Set(v8_str(isolate, "c"),
                              v8::Number::New(isolate, 3));

  // Create proto objects, add hidden properties to them and set them on
  // the global object.
  v8::Local<v8::Object> protoObj = t0->GetFunction(context)
                                       .ToLocalChecked()
                                       ->NewInstance(context)
                                       .ToLocalChecked();
  protoObj->SetPrivate(
              env.context(),
              v8::Private::New(isolate, v8_str(isolate, "v8::test-debug::b")),
              v8::Int32::New(isolate, 12))
      .FromJust();
  CHECK(env->Global()
            ->Set(context, v8_str(isolate, "protoObj"), protoObj)
            .FromJust());
  v8::Local<v8::Object> grandProtoObj = t1->GetFunction(context)
                                            .ToLocalChecked()
                                            ->NewInstance(context)
                                            .ToLocalChecked();
  grandProtoObj->SetPrivate(env.context(),
                            v8::Private::New(
                                isolate, v8_str(isolate, "v8::test-debug::c")),
                            v8::Int32::New(isolate, 13))
      .FromJust();
  CHECK(env->Global()
            ->Set(context, v8_str(isolate, "grandProtoObj"), grandProtoObj)
            .FromJust());

  // Setting prototypes: obj->protoObj->grandProtoObj
  CHECK(protoObj->Set(context, v8_str(isolate, "__proto__"), grandProtoObj)
            .FromJust());
  CHECK(obj->Set(context, v8_str(isolate, "__proto__"), protoObj).FromJust());

  // Get mirror for the object with property getter.
  CompileRun("var obj_mirror = debug.MakeMirror(obj);");
  CHECK(CompileRun("obj_mirror instanceof debug.ObjectMirror")
            ->BooleanValue(context)
            .FromJust());
  CompileRun("var named_names = obj_mirror.propertyNames();");
  // There should be exactly two properties - one from the object itself and
  // another from its hidden prototype.
  CHECK_EQ(2, CompileRun("named_names.length")->Int32Value(context).FromJust());
  CHECK(CompileRun("named_names.sort(); named_names[0] == 'a' &&"
                   "named_names[1] == 'b'")
            ->BooleanValue(context)
            .FromJust());
  CHECK(CompileRun("obj_mirror.property('a').value().value() == 1")
            ->BooleanValue(context)
            .FromJust());
  CHECK(CompileRun("obj_mirror.property('b').value().value() == 2")
            ->BooleanValue(context)
            .FromJust());
}


TEST(SetDebugEventListenerOnUninitializedVM) {
  v8::Debug::SetDebugEventListener(CcTest::isolate(), DummyDebugEventListener);
}

// Source for a JavaScript function which returns the data parameter of a
// function called in the context of the debugger. If no data parameter is
// passed it throws an exception.
static const char* debugger_call_with_data_source =
    "function debugger_call_with_data(exec_state, data) {"
    "  if (data) return data;"
    "  throw 'No data!'"
    "}";
v8::Local<v8::Function> debugger_call_with_data;


// Source for a JavaScript function which returns the data parameter of a
// function called in the context of the debugger. If no data parameter is
// passed it throws an exception.
static const char* debugger_call_with_closure_source =
    "var x = 3;"
    "(function (exec_state) {"
    "  if (exec_state.y) return x - 1;"
    "  exec_state.y = x;"
    "  return exec_state.y"
    "})";
v8::Local<v8::Function> debugger_call_with_closure;

// Function to retrieve the number of JavaScript frames by calling a JavaScript
// in the debugger.
static void CheckFrameCount(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
  CHECK(v8::Debug::Call(context, frame_count).ToLocalChecked()->IsNumber());
  CHECK_EQ(args[0]->Int32Value(context).FromJust(),
           v8::Debug::Call(context, frame_count)
               .ToLocalChecked()
               ->Int32Value(context)
               .FromJust());
}


// Function to retrieve the source line of the top JavaScript frame by calling a
// JavaScript function in the debugger.
static void CheckSourceLine(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
  CHECK(
      v8::Debug::Call(context, frame_source_line).ToLocalChecked()->IsNumber());
  CHECK_EQ(args[0]->Int32Value(context).FromJust(),
           v8::Debug::Call(context, frame_source_line)
               .ToLocalChecked()
               ->Int32Value(context)
               .FromJust());
}


// Function to test passing an additional parameter to a JavaScript function
// called in the debugger. It also tests that functions called in the debugger
// can throw exceptions.
static void CheckDataParameter(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Local<v8::String> data = v8_str(args.GetIsolate(), "Test");
  v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
  CHECK(v8::Debug::Call(context, debugger_call_with_data, data)
            .ToLocalChecked()
            ->IsString());

  for (int i = 0; i < 3; i++) {
    v8::TryCatch catcher(args.GetIsolate());
    CHECK(v8::Debug::Call(context, debugger_call_with_data).IsEmpty());
    CHECK(catcher.HasCaught());
    CHECK(catcher.Exception()->IsString());
  }
}


// Function to test using a JavaScript with closure in the debugger.
static void CheckClosure(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
  CHECK(v8::Debug::Call(context, debugger_call_with_closure)
            .ToLocalChecked()
            ->IsNumber());
  CHECK_EQ(3, v8::Debug::Call(context, debugger_call_with_closure)
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
}


// Test functions called through the debugger.
TEST(CallFunctionInDebugger) {
  // Create and enter a context with the functions CheckFrameCount,
  // CheckSourceLine and CheckDataParameter installed.
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  global_template->Set(v8_str(isolate, "CheckFrameCount"),
                       v8::FunctionTemplate::New(isolate, CheckFrameCount));
  global_template->Set(v8_str(isolate, "CheckSourceLine"),
                       v8::FunctionTemplate::New(isolate, CheckSourceLine));
  global_template->Set(v8_str(isolate, "CheckDataParameter"),
                       v8::FunctionTemplate::New(isolate, CheckDataParameter));
  global_template->Set(v8_str(isolate, "CheckClosure"),
                       v8::FunctionTemplate::New(isolate, CheckClosure));
  v8::Local<v8::Context> context =
      v8::Context::New(isolate, NULL, global_template);
  v8::Context::Scope context_scope(context);

  // Compile a function for checking the number of JavaScript frames.
  v8::Script::Compile(context, v8_str(isolate, frame_count_source))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  frame_count = v8::Local<v8::Function>::Cast(
      context->Global()
          ->Get(context, v8_str(isolate, "frame_count"))
          .ToLocalChecked());

  // Compile a function for returning the source line for the top frame.
  v8::Script::Compile(context, v8_str(isolate, frame_source_line_source))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  frame_source_line = v8::Local<v8::Function>::Cast(
      context->Global()
          ->Get(context, v8_str(isolate, "frame_source_line"))
          .ToLocalChecked());

  // Compile a function returning the data parameter.
  v8::Script::Compile(context, v8_str(isolate, debugger_call_with_data_source))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  debugger_call_with_data = v8::Local<v8::Function>::Cast(
      context->Global()
          ->Get(context, v8_str(isolate, "debugger_call_with_data"))
          .ToLocalChecked());

  // Compile a function capturing closure.
  debugger_call_with_closure = v8::Local<v8::Function>::Cast(
      v8::Script::Compile(context,
                          v8_str(isolate, debugger_call_with_closure_source))
          .ToLocalChecked()
          ->Run(context)
          .ToLocalChecked());

  // Calling a function through the debugger returns 0 frames if there are
  // no JavaScript frames.
  CHECK(v8::Integer::New(isolate, 0)
            ->Equals(context,
                     v8::Debug::Call(context, frame_count).ToLocalChecked())
            .FromJust());

  // Test that the number of frames can be retrieved.
  v8::Script::Compile(context, v8_str(isolate, "CheckFrameCount(1)"))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::Script::Compile(context, v8_str(isolate,
                                      "function f() {"
                                      "  CheckFrameCount(2);"
                                      "}; f()"))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();

  // Test that the source line can be retrieved.
  v8::Script::Compile(context, v8_str(isolate, "CheckSourceLine(0)"))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::Script::Compile(context, v8_str(isolate,
                                      "function f() {\n"
                                      "  CheckSourceLine(1)\n"
                                      "  CheckSourceLine(2)\n"
                                      "  CheckSourceLine(3)\n"
                                      "}; f()"))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();

  // Test that a parameter can be passed to a function called in the debugger.
  v8::Script::Compile(context, v8_str(isolate, "CheckDataParameter()"))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();

  // Test that a function with closure can be run in the debugger.
  v8::Script::Compile(context, v8_str(isolate, "CheckClosure()"))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();

  // Test that the source line is correct when there is a line offset.
  v8::ScriptOrigin origin(v8_str(isolate, "test"),
                          v8::Integer::New(isolate, 7));
  v8::Script::Compile(context, v8_str(isolate, "CheckSourceLine(7)"), &origin)
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::Script::Compile(context, v8_str(isolate,
                                      "function f() {\n"
                                      "  CheckSourceLine(8)\n"
                                      "  CheckSourceLine(9)\n"
                                      "  CheckSourceLine(10)\n"
                                      "}; f()"),
                      &origin)
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
}


// Test that clearing the debug event listener actually clears all break points
// and related information.
TEST(DebuggerUnload) {
  DebugLocalContext env;

  // Check debugger is unloaded before it is used.
  CheckDebuggerUnloaded(env->GetIsolate());

  // Set a debug event listener.
  break_point_hit_count = 0;
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);
  v8::Local<v8::Context> context = env.context();
  {
    v8::HandleScope scope(env->GetIsolate());
    // Create a couple of functions for the test.
    v8::Local<v8::Function> foo =
        CompileFunction(&env, "function foo(){x=1}", "foo");
    v8::Local<v8::Function> bar =
        CompileFunction(&env, "function bar(){y=2}", "bar");

    // Set some break points.
    SetBreakPoint(foo, 0);
    SetBreakPoint(foo, 4);
    SetBreakPoint(bar, 0);
    SetBreakPoint(bar, 4);

    // Make sure that the break points are there.
    break_point_hit_count = 0;
    foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();
    CHECK_EQ(2, break_point_hit_count);
    bar->Call(context, env->Global(), 0, NULL).ToLocalChecked();
    CHECK_EQ(4, break_point_hit_count);
  }

  // Remove the debug event listener without clearing breakpoints. Do this
  // outside a handle scope.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate(), true);
}

int event_listener_hit_count = 0;

// Debugger event listener which clears itself while active.
static void EventListenerClearingItself(
    const v8::Debug::EventDetails& details) {
  event_listener_hit_count++;

  // Clear debug event listener.
  v8::Debug::SetDebugEventListener(details.GetIsolate(), nullptr);
}


// Test clearing the debug message handler while processing a debug event.
TEST(DebuggerClearEventListenerWhileActive) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Check debugger is unloaded before it is used.
  CheckDebuggerUnloaded(env->GetIsolate());

  // Set a debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   EventListenerClearingItself);

  // Run code to throw an uncaught exception. This should trigger the listener.
  CompileRun("throw 1");

  // The event listener should have been called.
  CHECK_EQ(1, event_listener_hit_count);

  CheckDebuggerUnloaded(env->GetIsolate(), true);
}

// Test for issue http://code.google.com/p/v8/issues/detail?id=289.
// Make sure that DebugGetLoadedScripts doesn't return scripts
// with disposed external source.
class EmptyExternalStringResource : public v8::String::ExternalStringResource {
 public:
  EmptyExternalStringResource() { empty_[0] = 0; }
  virtual ~EmptyExternalStringResource() {}
  virtual size_t length() const { return empty_.length(); }
  virtual const uint16_t* data() const { return empty_.start(); }
 private:
  ::v8::internal::EmbeddedVector<uint16_t, 1> empty_;
};

TEST(DebugScriptLineEndsAreAscending) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  env.ExposeDebug();

  // Compile a test script.
  v8::Local<v8::String> script = v8_str(isolate,
                                        "function f() {\n"
                                        "  debugger;\n"
                                        "}\n");

  v8::ScriptOrigin origin1 = v8::ScriptOrigin(v8_str(isolate, "name"));
  v8::Local<v8::Script> script1 =
      v8::Script::Compile(env.context(), script, &origin1).ToLocalChecked();
  USE(script1);

  Handle<v8::internal::FixedArray> instances;
  {
    v8::internal::Debug* debug = CcTest::i_isolate()->debug();
    v8::internal::DebugScope debug_scope(debug);
    CHECK(!debug_scope.failed());
    instances = debug->GetLoadedScripts();
  }

  CHECK_GT(instances->length(), 0);
  for (int i = 0; i < instances->length(); i++) {
    Handle<v8::internal::Script> script = Handle<v8::internal::Script>(
        v8::internal::Script::cast(instances->get(i)));

    v8::internal::Script::InitLineEnds(script);
    v8::internal::FixedArray* ends =
        v8::internal::FixedArray::cast(script->line_ends());
    CHECK_GT(ends->length(), 0);

    int prev_end = -1;
    for (int j = 0; j < ends->length(); j++) {
      const int curr_end = v8::internal::Smi::cast(ends->get(j))->value();
      CHECK_GT(curr_end, prev_end);
      prev_end = curr_end;
    }
  }
}


// Test script break points set on lines.
TEST(ScriptNameAndData) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();

  // Create functions for retrieving script name and data for the function on
  // the top frame when hitting a break point.
  frame_script_name = CompileFunction(&env,
                                      frame_script_name_source,
                                      "frame_script_name");

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakPointHitCount);

  v8::Local<v8::Context> context = env.context();
  // Test function source.
  v8::Local<v8::String> script = v8_str(env->GetIsolate(),
                                        "function f() {\n"
                                        "  debugger;\n"
                                        "}\n");

  v8::ScriptOrigin origin1 =
      v8::ScriptOrigin(v8_str(env->GetIsolate(), "name"));
  v8::Local<v8::Script> script1 =
      v8::Script::Compile(context, script, &origin1).ToLocalChecked();
  script1->Run(context).ToLocalChecked();
  v8::Local<v8::Function> f;
  f = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "f"))
          .ToLocalChecked());

  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  CHECK_EQ(0, strcmp("name", last_script_name_hit));

  // Compile the same script again without setting data. As the compilation
  // cache is disabled when debugging expect the data to be missing.
  v8::Script::Compile(context, script, &origin1)
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  f = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "f"))
          .ToLocalChecked());
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);
  CHECK_EQ(0, strcmp("name", last_script_name_hit));

  v8::Local<v8::String> data_obj_source =
      v8_str(env->GetIsolate(),
             "({ a: 'abc',\n"
             "  b: 123,\n"
             "  toString: function() { return this.a + ' ' + this.b; }\n"
             "})\n");
  v8::Script::Compile(context, data_obj_source)
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::ScriptOrigin origin2 =
      v8::ScriptOrigin(v8_str(env->GetIsolate(), "new name"));
  v8::Local<v8::Script> script2 =
      v8::Script::Compile(context, script, &origin2).ToLocalChecked();
  script2->Run(context).ToLocalChecked();
  f = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "f"))
          .ToLocalChecked());
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(3, break_point_hit_count);
  CHECK_EQ(0, strcmp("new name", last_script_name_hit));

  v8::Local<v8::Script> script3 =
      v8::Script::Compile(context, script, &origin2).ToLocalChecked();
  script3->Run(context).ToLocalChecked();
  f = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "f"))
          .ToLocalChecked());
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(4, break_point_hit_count);
}


static v8::Local<v8::Context> expected_context;
static v8::Local<v8::Value> expected_context_data;


// Check that the expected context is the one generating the debug event.
static void ContextCheckEventListener(
    const v8::Debug::EventDetails& event_details) {
  CHECK(event_details.GetEventContext() == expected_context);
  CHECK(event_details.GetEventContext()->GetEmbedderData(0)->StrictEquals(
      expected_context_data));
  event_listener_hit_count++;
}


// Test which creates two contexts and sets different embedder data on each.
// Checks that this data is set correctly and that when the debug event
// listener is called the expected context is the one active.
TEST(ContextData) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  // Create two contexts.
  v8::Local<v8::Context> context_1;
  v8::Local<v8::Context> context_2;
  v8::Local<v8::ObjectTemplate> global_template =
      v8::Local<v8::ObjectTemplate>();
  v8::Local<v8::Value> global_object = v8::Local<v8::Value>();
  context_1 = v8::Context::New(isolate, NULL, global_template, global_object);
  context_2 = v8::Context::New(isolate, NULL, global_template, global_object);

  v8::Debug::SetDebugEventListener(isolate, ContextCheckEventListener);

  // Default data value is undefined.
  CHECK(context_1->GetEmbedderData(0)->IsUndefined());
  CHECK(context_2->GetEmbedderData(0)->IsUndefined());

  // Set and check different data values.
  v8::Local<v8::String> data_1 = v8_str(isolate, "1");
  v8::Local<v8::String> data_2 = v8_str(isolate, "2");
  context_1->SetEmbedderData(0, data_1);
  context_2->SetEmbedderData(0, data_2);
  CHECK(context_1->GetEmbedderData(0)->StrictEquals(data_1));
  CHECK(context_2->GetEmbedderData(0)->StrictEquals(data_2));

  // Simple test function which causes a break.
  const char* source = "function f() { debugger; }";

  // Enter and run function in the first context.
  {
    v8::Context::Scope context_scope(context_1);
    expected_context = context_1;
    expected_context_data = data_1;
    v8::Local<v8::Function> f = CompileFunction(isolate, source, "f");
    f->Call(context_1, context_1->Global(), 0, NULL).ToLocalChecked();
  }


  // Enter and run function in the second context.
  {
    v8::Context::Scope context_scope(context_2);
    expected_context = context_2;
    expected_context_data = data_2;
    v8::Local<v8::Function> f = CompileFunction(isolate, source, "f");
    f->Call(context_2, context_2->Global(), 0, NULL).ToLocalChecked();
  }

  // Two times compile event and two times break event.
  CHECK_GT(event_listener_hit_count, 3);

  v8::Debug::SetDebugEventListener(isolate, nullptr);
  CheckDebuggerUnloaded(isolate);
}

// Debug event listener which issues a debug break when it hits a break event.
static int event_listener_break_hit_count = 0;
static void DebugBreakEventListener(const v8::Debug::EventDetails& details) {
  // Schedule a debug break for break events.
  if (details.GetEvent() == v8::Break) {
    event_listener_break_hit_count++;
    if (event_listener_break_hit_count == 1) {
      v8::Debug::DebugBreak(details.GetIsolate());
    }
  }
}

// Test that a debug break can be scheduled while in a event listener.
TEST(DebugBreakInEventListener) {
  i::FLAG_turbo_inlining = false;  // Make sure g is not inlined into f.
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugBreakEventListener);

  v8::Local<v8::Context> context = env.context();
  // Test functions.
  const char* script = "function f() { debugger; g(); } function g() { }";
  CompileRun(script);
  v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "f"))
          .ToLocalChecked());
  v8::Local<v8::Function> g = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "g"))
          .ToLocalChecked());

  // Call f then g. The debugger statement in f will cause a break which will
  // cause another break.
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, event_listener_break_hit_count);
  // Calling g will not cause any additional breaks.
  g->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(2, event_listener_break_hit_count);
}


#ifndef V8_INTERPRETED_REGEXP
// Debug event handler which gets the function on the top frame and schedules a
// break a number of times.
static void DebugEventDebugBreak(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  v8::Local<v8::Object> exec_state = event_details.GetExecutionState();
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
  if (event == v8::Break) {
    break_point_hit_count++;

    // Get the name of the top frame function.
    if (!frame_function_name.IsEmpty()) {
      // Get the name of the function.
      const int argc = 2;
      v8::Local<v8::Value> argv[argc] = {
          exec_state, v8::Integer::New(CcTest::isolate(), 0)};
      v8::Local<v8::Value> result =
          frame_function_name->Call(context, exec_state, argc, argv)
              .ToLocalChecked();
      if (result->IsUndefined()) {
        last_function_hit[0] = '\0';
      } else {
        CHECK(result->IsString());
        v8::Local<v8::String> function_name(
            result->ToString(context).ToLocalChecked());
        function_name->WriteUtf8(last_function_hit);
      }
    }

    // Keep forcing breaks.
    if (break_point_hit_count < 20) {
      v8::Debug::DebugBreak(CcTest::isolate());
    }
  }
}


TEST(RegExpDebugBreak) {
  // This test only applies to native regexps.
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.context();
  // Create a function for checking the function when hitting a break point.
  frame_function_name = CompileFunction(&env,
                                        frame_function_name_source,
                                        "frame_function_name");

  // Test RegExp which matches white spaces and comments at the begining of a
  // source line.
  const char* script =
    "var sourceLineBeginningSkip = /^(?:[ \\v\\h]*(?:\\/\\*.*?\\*\\/)*)*/;\n"
    "function f(s) { return s.match(sourceLineBeginningSkip)[0].length; }";

  v8::Local<v8::Function> f = CompileFunction(env->GetIsolate(), script, "f");
  const int argc = 1;
  v8::Local<v8::Value> argv[argc] = {
      v8_str(env->GetIsolate(), "  /* xxx */ a=0;")};
  v8::Local<v8::Value> result =
      f->Call(context, env->Global(), argc, argv).ToLocalChecked();
  CHECK_EQ(12, result->Int32Value(context).FromJust());

  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventDebugBreak);
  v8::Debug::DebugBreak(env->GetIsolate());
  result = f->Call(context, env->Global(), argc, argv).ToLocalChecked();

  // Check that there was only one break event. Matching RegExp should not
  // cause Break events.
  CHECK_EQ(1, break_point_hit_count);
  CHECK_EQ(0, strcmp("f", last_function_hit));
}
#endif  // V8_INTERPRETED_REGEXP

// Test which creates a context and sets embedder data on it. Checks that this
// data is set correctly and that when the debug event listener is called for
// break event in an eval statement the expected context is the one returned by
// Message.GetEventContext.
TEST(EvalContextData) {
  v8::HandleScope scope(CcTest::isolate());

  v8::Local<v8::Context> context_1;
  v8::Local<v8::ObjectTemplate> global_template =
      v8::Local<v8::ObjectTemplate>();
  context_1 =
      v8::Context::New(CcTest::isolate(), NULL, global_template);

  v8::Debug::SetDebugEventListener(CcTest::isolate(),
                                   ContextCheckEventListener);

  // Default data value is undefined.
  CHECK(context_1->GetEmbedderData(0)->IsUndefined());

  // Set and check a data value.
  v8::Local<v8::String> data_1 = v8_str(CcTest::isolate(), "1");
  context_1->SetEmbedderData(0, data_1);
  CHECK(context_1->GetEmbedderData(0)->StrictEquals(data_1));

  // Simple test function with eval that causes a break.
  const char* source = "function f() { eval('debugger;'); }";

  // Enter and run function in the context.
  {
    v8::Context::Scope context_scope(context_1);
    expected_context = context_1;
    expected_context_data = data_1;
    v8::Local<v8::Function> f = CompileFunction(CcTest::isolate(), source, "f");
    f->Call(context_1, context_1->Global(), 0, NULL).ToLocalChecked();
  }

  v8::Debug::SetDebugEventListener(CcTest::isolate(), nullptr);

  // One time compile event and one time break event.
  CHECK_GT(event_listener_hit_count, 2);
  CheckDebuggerUnloaded(CcTest::isolate());
}


// Debug event listener which counts the after compile events.
int after_compile_event_count = 0;
static void AfterCompileEventListener(const v8::Debug::EventDetails& details) {
  // Count the number of scripts collected.
  if (details.GetEvent() == v8::AfterCompile) {
    after_compile_event_count++;
  }
}


// Tests that after compile event is sent as many times as there are scripts
// compiled.
TEST(AfterCompileEventWhenEventListenerIsReset) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.context();
  const char* script = "var a=1";

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   AfterCompileEventListener);
  v8::Script::Compile(context, v8_str(env->GetIsolate(), script))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   AfterCompileEventListener);
  v8::Debug::DebugBreak(env->GetIsolate());
  v8::Script::Compile(context, v8_str(env->GetIsolate(), script))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();

  // Setting listener to NULL should cause debugger unload.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());

  // Compilation cache should be disabled when debugger is active.
  CHECK_EQ(2, after_compile_event_count);
}


// Syntax error event handler which counts a number of events.
int compile_error_event_count = 0;

static void CompileErrorEventCounterClear() {
  compile_error_event_count = 0;
}

static void CompileErrorEventCounter(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();

  if (event == v8::CompileError) {
    compile_error_event_count++;
  }
}


// Tests that syntax error event is sent as many times as there are scripts
// with syntax error compiled.
TEST(SyntaxErrorEventOnSyntaxException) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // For this test, we want to break on uncaught exceptions:
  ChangeBreakOnException(false, true);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), CompileErrorEventCounter);
  v8::Local<v8::Context> context = env.context();

  CompileErrorEventCounterClear();

  // Check initial state.
  CHECK_EQ(0, compile_error_event_count);

  // Throws SyntaxError: Unexpected end of input
  CHECK(
      v8::Script::Compile(context, v8_str(env->GetIsolate(), "+++")).IsEmpty());
  CHECK_EQ(1, compile_error_event_count);

  CHECK(v8::Script::Compile(context, v8_str(env->GetIsolate(), "/sel\\/: \\"))
            .IsEmpty());
  CHECK_EQ(2, compile_error_event_count);

  v8::Local<v8::Script> script =
      v8::Script::Compile(context,
                          v8_str(env->GetIsolate(), "JSON.parse('1234:')"))
          .ToLocalChecked();
  CHECK_EQ(2, compile_error_event_count);
  CHECK(script->Run(context).IsEmpty());
  CHECK_EQ(3, compile_error_event_count);

  v8::Script::Compile(context,
                      v8_str(env->GetIsolate(), "new RegExp('/\\/\\\\');"))
      .ToLocalChecked();
  CHECK_EQ(3, compile_error_event_count);

  v8::Script::Compile(context, v8_str(env->GetIsolate(), "throw 1;"))
      .ToLocalChecked();
  CHECK_EQ(3, compile_error_event_count);
}

// Tests that break event is sent when event listener is reset.
TEST(BreakEventWhenEventListenerIsReset) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.context();
  const char* script = "function f() {};";

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   AfterCompileEventListener);
  v8::Script::Compile(context, v8_str(env->GetIsolate(), script))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   AfterCompileEventListener);
  v8::Debug::DebugBreak(env->GetIsolate());
  v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "f"))
          .ToLocalChecked());
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // Setting event listener to NULL should cause debugger unload.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());

  // Compilation cache should be disabled when debugger is active.
  CHECK_EQ(1, after_compile_event_count);
}


static int exception_event_count = 0;
static void ExceptionEventListener(const v8::Debug::EventDetails& details) {
  if (details.GetEvent() == v8::Exception) exception_event_count++;
}

// Tests that exception event is sent when event listener is reset.
TEST(ExceptionEventWhenEventListenerIsReset) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  v8::Local<v8::Context> context = env.context();
  // For this test, we want to break on uncaught exceptions:
  ChangeBreakOnException(false, true);

  exception_event_count = 0;
  const char* script = "function f() {throw new Error()};";

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   AfterCompileEventListener);
  v8::Script::Compile(context, v8_str(env->GetIsolate(), script))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), ExceptionEventListener);
  v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "f"))
          .ToLocalChecked());
  CHECK(f->Call(context, env->Global(), 0, NULL).IsEmpty());

  // Setting event listener to NULL should cause debugger unload.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());

  CHECK_EQ(1, exception_event_count);
}


// Tests after compile event is sent when there are some provisional
// breakpoints out of the scripts lines range.
TEST(ProvisionalBreakpointOnLineOutOfRange) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();
  const char* script = "function f() {};";
  const char* resource_name = "test_resource";

  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   AfterCompileEventListener);
  v8::Local<v8::Context> context = env.context();

  // Set a couple of provisional breakpoint on lines out of the script lines
  // range.
  int sbp1 = SetScriptBreakPointByNameFromJS(env->GetIsolate(), resource_name,
                                             3, -1 /* no column */);
  int sbp2 =
      SetScriptBreakPointByNameFromJS(env->GetIsolate(), resource_name, 5, 5);

  after_compile_event_count = 0;

  v8::ScriptOrigin origin(v8_str(env->GetIsolate(), resource_name),
                          v8::Integer::New(env->GetIsolate(), 10),
                          v8::Integer::New(env->GetIsolate(), 1));
  // Compile a script whose first line number is greater than the breakpoints'
  // lines.
  v8::Script::Compile(context, v8_str(env->GetIsolate(), script), &origin)
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();

  // If the script is compiled successfully there is exactly one after compile
  // event. In case of an exception in debugger code after compile event is not
  // sent.
  CHECK_EQ(1, after_compile_event_count);

  ClearBreakPointFromJS(env->GetIsolate(), sbp1);
  ClearBreakPointFromJS(env->GetIsolate(), sbp2);
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}

static void BreakEventListener(const v8::Debug::EventDetails& details) {
  if (details.GetEvent() == v8::Break) break_point_hit_count++;
}


// Test that if DebugBreak is forced it is ignored when code from
// debug-delay.js is executed.
TEST(NoDebugBreakInAfterCompileEventListener) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.context();

  // Register a debug event listener which sets the break flag and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), BreakEventListener);

  // Set the debug break flag.
  v8::Debug::DebugBreak(env->GetIsolate());

  // Create a function for testing stepping.
  const char* src = "function f() { eval('var x = 10;'); } ";
  v8::Local<v8::Function> f = CompileFunction(&env, src, "f");

  // There should be only one break event.
  CHECK_EQ(1, break_point_hit_count);

  // Set the debug break flag again.
  v8::Debug::DebugBreak(env->GetIsolate());
  f->Call(context, env->Global(), 0, NULL).ToLocalChecked();
  // There should be one more break event when the script is evaluated in 'f'.
  CHECK_EQ(2, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


// Test that the debug break flag works with function.apply.
TEST(DebugBreakFunctionApply) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.context();

  // Create a function for testing breaking in apply.
  v8::Local<v8::Function> foo = CompileFunction(
      &env,
      "function baz(x) { }"
      "function bar(x) { baz(); }"
      "function foo(){ bar.apply(this, [1]); }",
      "foo");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventBreakMax);

  // Set the debug break flag before calling the code using function.apply.
  v8::Debug::DebugBreak(env->GetIsolate());

  // Limit the number of debug breaks. This is a regression test for issue 493
  // where this test would enter an infinite loop.
  break_point_hit_count = 0;
  max_break_point_hit_count = 10000;  // 10000 => infinite loop.
  foo->Call(context, env->Global(), 0, NULL).ToLocalChecked();

  // When keeping the debug break several break will happen.
  CHECK_GT(break_point_hit_count, 1);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


v8::Local<v8::Context> debugee_context;
v8::Local<v8::Context> debugger_context;


// Property getter that checks that current and calling contexts
// are both the debugee contexts.
static void NamedGetterWithCallingContextCheck(
    v8::Local<v8::String> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  CHECK_EQ(0, strcmp(*v8::String::Utf8Value(name), "a"));
  v8::Local<v8::Context> current = info.GetIsolate()->GetCurrentContext();
  CHECK(current == debugee_context);
  CHECK(current != debugger_context);
  info.GetReturnValue().Set(1);
}


// Debug event listener that checks if the first argument of a function is
// an object with property 'a' == 1. If the property has custom accessor
// this handler will eventually invoke it.
static void DebugEventGetAtgumentPropertyValue(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  v8::Local<v8::Object> exec_state = event_details.GetExecutionState();
  if (event == v8::Break) {
    break_point_hit_count++;
    CHECK(debugger_context == CcTest::isolate()->GetCurrentContext());
    v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(CompileRun(
        "(function(exec_state) {\n"
        "    return (exec_state.frame(0).argumentValue(0).property('a').\n"
        "            value().value() == 1);\n"
        "})"));
    const int argc = 1;
    v8::Local<v8::Value> argv[argc] = {exec_state};
    v8::Local<v8::Value> result =
        func->Call(debugger_context, exec_state, argc, argv).ToLocalChecked();
    CHECK(result->IsTrue());
  }
}


TEST(CallingContextIsNotDebugContext) {
  v8::internal::Debug* debug = CcTest::i_isolate()->debug();
  // Create and enter a debugee context.
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  env.ExposeDebug();

  // Save handles to the debugger and debugee contexts to be used in
  // NamedGetterWithCallingContextCheck.
  debugee_context = env.context();
  debugger_context = v8::Utils::ToLocal(debug->debug_context());

  // Create object with 'a' property accessor.
  v8::Local<v8::ObjectTemplate> named = v8::ObjectTemplate::New(isolate);
  named->SetAccessor(v8_str(isolate, "a"), NamedGetterWithCallingContextCheck);
  CHECK(env->Global()
            ->Set(debugee_context, v8_str(isolate, "obj"),
                  named->NewInstance(debugee_context).ToLocalChecked())
            .FromJust());

  // Register the debug event listener
  v8::Debug::SetDebugEventListener(isolate, DebugEventGetAtgumentPropertyValue);

  // Create a function that invokes debugger.
  v8::Local<v8::Function> foo = CompileFunction(
      &env,
      "function bar(x) { debugger; }"
      "function foo(){ bar(obj); }",
      "foo");

  break_point_hit_count = 0;
  foo->Call(debugee_context, env->Global(), 0, NULL).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(isolate, nullptr);
  debugee_context = v8::Local<v8::Context>();
  debugger_context = v8::Local<v8::Context>();
  CheckDebuggerUnloaded(isolate);
}


static v8::Local<v8::Value> expected_callback_data;
static void DebugEventContextChecker(const v8::Debug::EventDetails& details) {
  CHECK(details.GetEventContext() == expected_context);
  CHECK(expected_callback_data->Equals(details.GetEventContext(),
                                       details.GetCallbackData())
            .FromJust());
}


// Check that event details contain context where debug event occured.
TEST(DebugEventContext) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  expected_context = v8::Context::New(isolate);
  expected_callback_data = v8::Int32::New(isolate, 2010);
  v8::Debug::SetDebugEventListener(isolate, DebugEventContextChecker,
                                   expected_callback_data);
  v8::Context::Scope context_scope(expected_context);
  v8::Script::Compile(expected_context,
                      v8_str(isolate, "(function(){debugger;})();"))
      .ToLocalChecked()
      ->Run(expected_context)
      .ToLocalChecked();
  expected_context.Clear();
  v8::Debug::SetDebugEventListener(isolate, nullptr);
  expected_context_data = v8::Local<v8::Value>();
  CheckDebuggerUnloaded(isolate);
}


static bool debug_event_break_deoptimize_done = false;

static void DebugEventBreakDeoptimize(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  v8::Local<v8::Object> exec_state = event_details.GetExecutionState();
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
  if (event == v8::Break) {
    if (!frame_function_name.IsEmpty()) {
      // Get the name of the function.
      const int argc = 2;
      v8::Local<v8::Value> argv[argc] = {
          exec_state, v8::Integer::New(CcTest::isolate(), 0)};
      v8::Local<v8::Value> result =
          frame_function_name->Call(context, exec_state, argc, argv)
              .ToLocalChecked();
      if (!result->IsUndefined()) {
        char fn[80];
        CHECK(result->IsString());
        v8::Local<v8::String> function_name(
            result->ToString(context).ToLocalChecked());
        function_name->WriteUtf8(fn);
        if (strcmp(fn, "bar") == 0) {
          i::Deoptimizer::DeoptimizeAll(CcTest::i_isolate());
          debug_event_break_deoptimize_done = true;
        }
      }
    }

    v8::Debug::DebugBreak(CcTest::isolate());
  }
}


// Test deoptimization when execution is broken using the debug break stack
// check interrupt.
TEST(DeoptimizeDuringDebugBreak) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();
  v8::Local<v8::Context> context = env.context();

  // Create a function for checking the function when hitting a break point.
  frame_function_name = CompileFunction(&env,
                                        frame_function_name_source,
                                        "frame_function_name");

  // Set a debug event listener which will keep interrupting execution until
  // debug break. When inside function bar it will deoptimize all functions.
  // This tests lazy deoptimization bailout for the stack check, as the first
  // time in function bar when using debug break and no break points will be at
  // the initial stack check.
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugEventBreakDeoptimize);

  // Compile and run function bar which will optimize it for some flag settings.
  v8::Local<v8::Function> f = CompileFunction(&env, "function bar(){}", "bar");
  f->Call(context, v8::Undefined(env->GetIsolate()), 0, NULL).ToLocalChecked();

  // Set debug break and call bar again.
  v8::Debug::DebugBreak(env->GetIsolate());
  f->Call(context, v8::Undefined(env->GetIsolate()), 0, NULL).ToLocalChecked();

  CHECK(debug_event_break_deoptimize_done);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
}


static void DebugEventBreakWithOptimizedStack(
    const v8::Debug::EventDetails& event_details) {
  v8::Isolate* isolate = event_details.GetEventContext()->GetIsolate();
  v8::DebugEvent event = event_details.GetEvent();
  v8::Local<v8::Object> exec_state = event_details.GetExecutionState();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (event == v8::Break) {
    if (!frame_function_name.IsEmpty()) {
      for (int i = 0; i < 2; i++) {
        const int argc = 2;
        v8::Local<v8::Value> argv[argc] = {exec_state,
                                           v8::Integer::New(isolate, i)};
        // Get the name of the function in frame i.
        v8::Local<v8::Value> result =
            frame_function_name->Call(context, exec_state, argc, argv)
                .ToLocalChecked();
        CHECK(result->IsString());
        v8::Local<v8::String> function_name(
            result->ToString(context).ToLocalChecked());
        CHECK(
            function_name->Equals(context, v8_str(isolate, "loop")).FromJust());
        // Get the name of the first argument in frame i.
        result = frame_argument_name->Call(context, exec_state, argc, argv)
                     .ToLocalChecked();
        CHECK(result->IsString());
        v8::Local<v8::String> argument_name(
            result->ToString(context).ToLocalChecked());
        CHECK(argument_name->Equals(context, v8_str(isolate, "count"))
                  .FromJust());
        // Get the value of the first argument in frame i. If the
        // function is optimized the value will be undefined, otherwise
        // the value will be '1 - i'.
        //
        // TODO(3141533): We should be able to get the real value for
        // optimized frames.
        result = frame_argument_value->Call(context, exec_state, argc, argv)
                     .ToLocalChecked();
        CHECK(result->IsUndefined() ||
              (result->Int32Value(context).FromJust() == 1 - i));
        // Get the name of the first local variable.
        result = frame_local_name->Call(context, exec_state, argc, argv)
                     .ToLocalChecked();
        CHECK(result->IsString());
        v8::Local<v8::String> local_name(
            result->ToString(context).ToLocalChecked());
        CHECK(local_name->Equals(context, v8_str(isolate, "local")).FromJust());
        // Get the value of the first local variable. If the function
        // is optimized the value will be undefined, otherwise it will
        // be 42.
        //
        // TODO(3141533): We should be able to get the real value for
        // optimized frames.
        result = frame_local_value->Call(context, exec_state, argc, argv)
                     .ToLocalChecked();
        CHECK(result->IsUndefined() ||
              (result->Int32Value(context).FromJust() == 42));
      }
    }
  }
}


static void ScheduleBreak(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Debug::SetDebugEventListener(args.GetIsolate(),
                                   DebugEventBreakWithOptimizedStack);
  v8::Debug::DebugBreak(args.GetIsolate());
}


TEST(DebugBreakStackInspection) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.context();

  frame_function_name =
      CompileFunction(&env, frame_function_name_source, "frame_function_name");
  frame_argument_name =
      CompileFunction(&env, frame_argument_name_source, "frame_argument_name");
  frame_argument_value = CompileFunction(&env,
                                         frame_argument_value_source,
                                         "frame_argument_value");
  frame_local_name =
      CompileFunction(&env, frame_local_name_source, "frame_local_name");
  frame_local_value =
      CompileFunction(&env, frame_local_value_source, "frame_local_value");

  v8::Local<v8::FunctionTemplate> schedule_break_template =
      v8::FunctionTemplate::New(env->GetIsolate(), ScheduleBreak);
  v8::Local<v8::Function> schedule_break =
      schedule_break_template->GetFunction(context).ToLocalChecked();
  CHECK(env->Global()
            ->Set(context, v8_str("scheduleBreak"), schedule_break)
            .FromJust());

  const char* src =
      "function loop(count) {"
      "  var local = 42;"
      "  if (count < 1) { scheduleBreak(); loop(count + 1); }"
      "}"
      "loop(0);";
  v8::Script::Compile(context, v8_str(env->GetIsolate(), src))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
}


// Test that setting the terminate execution flag during debug break processing.
static void TestDebugBreakInLoop(const char* loop_head,
                                 const char** loop_bodies,
                                 const char* loop_tail) {
  // Receive 10 breaks for each test and then terminate JavaScript execution.
  static const int kBreaksPerTest = 10;

  for (int i = 0; loop_bodies[i] != NULL; i++) {
    // Perform a lazy deoptimization after various numbers of breaks
    // have been hit.

    EmbeddedVector<char, 1024> buffer;
    SNPrintF(buffer, "function f() {%s%s%s}", loop_head, loop_bodies[i],
             loop_tail);

    i::PrintF("%s\n", buffer.start());

    for (int j = 0; j < 3; j++) {
      break_point_hit_count_deoptimize = j;
      if (j == 2) {
        break_point_hit_count_deoptimize = kBreaksPerTest;
      }

      break_point_hit_count = 0;
      max_break_point_hit_count = kBreaksPerTest;
      terminate_after_max_break_point_hit = true;

      // Function with infinite loop.
      CompileRun(buffer.start());

      // Set the debug break to enter the debugger as soon as possible.
      v8::Debug::DebugBreak(CcTest::isolate());

      // Call function with infinite loop.
      CompileRun("f();");
      CHECK_EQ(kBreaksPerTest, break_point_hit_count);

      CHECK(!CcTest::isolate()->IsExecutionTerminating());
    }
  }
}


static const char* loop_bodies_1[] = {"",
                                      "g()",
                                      "if (a == 0) { g() }",
                                      "if (a == 1) { g() }",
                                      "if (a == 0) { g() } else { h() }",
                                      "if (a == 0) { continue }",
                                      NULL};


static const char* loop_bodies_2[] = {
    "if (a == 1) { continue }",
    "switch (a) { case 1: g(); }",
    "switch (a) { case 1: continue; }",
    "switch (a) { case 1: g(); break; default: h() }",
    "switch (a) { case 1: continue; break; default: h() }",
    NULL};


void DebugBreakLoop(const char* loop_header, const char** loop_bodies,
                    const char* loop_footer) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which sets the break flag and counts.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventBreakMax);

  CompileRun(
      "var a = 1;\n"
      "function g() { }\n"
      "function h() { }");

  TestDebugBreakInLoop(loop_header, loop_bodies, loop_footer);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


TEST(DebugBreakInWhileTrue1) {
  DebugBreakLoop("while (true) {", loop_bodies_1, "}");
}


TEST(DebugBreakInWhileTrue2) {
  DebugBreakLoop("while (true) {", loop_bodies_2, "}");
}


TEST(DebugBreakInWhileCondition1) {
  DebugBreakLoop("while (a == 1) {", loop_bodies_1, "}");
}


TEST(DebugBreakInWhileCondition2) {
  DebugBreakLoop("while (a == 1) {", loop_bodies_2, "}");
}


TEST(DebugBreakInDoWhileTrue1) {
  DebugBreakLoop("do {", loop_bodies_1, "} while (true)");
}


TEST(DebugBreakInDoWhileTrue2) {
  DebugBreakLoop("do {", loop_bodies_2, "} while (true)");
}


TEST(DebugBreakInDoWhileCondition1) {
  DebugBreakLoop("do {", loop_bodies_1, "} while (a == 1)");
}


TEST(DebugBreakInDoWhileCondition2) {
  DebugBreakLoop("do {", loop_bodies_2, "} while (a == 1)");
}


TEST(DebugBreakInFor1) { DebugBreakLoop("for (;;) {", loop_bodies_1, "}"); }


TEST(DebugBreakInFor2) { DebugBreakLoop("for (;;) {", loop_bodies_2, "}"); }


TEST(DebugBreakInForCondition1) {
  DebugBreakLoop("for (;a == 1;) {", loop_bodies_1, "}");
}


TEST(DebugBreakInForCondition2) {
  DebugBreakLoop("for (;a == 1;) {", loop_bodies_2, "}");
}


v8::Local<v8::Script> inline_script;

static void DebugBreakInlineListener(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
  if (event != v8::Break) return;

  int expected_frame_count = 4;
  int expected_line_number[] = {1, 4, 7, 12};

  i::Handle<i::Object> compiled_script = v8::Utils::OpenHandle(*inline_script);
  i::Handle<i::Script> source_script = i::Handle<i::Script>(i::Script::cast(
      i::JSFunction::cast(*compiled_script)->shared()->script()));

  int break_id = CcTest::i_isolate()->debug()->break_id();
  char script[128];
  i::Vector<char> script_vector(script, sizeof(script));
  SNPrintF(script_vector, "%%GetFrameCount(%d)", break_id);
  v8::Local<v8::Value> result = CompileRun(script);

  int frame_count = result->Int32Value(context).FromJust();
  CHECK_EQ(expected_frame_count, frame_count);

  for (int i = 0; i < frame_count; i++) {
    // The 6. element in the returned array of GetFrameDetails contains the
    // source position of that frame.
    SNPrintF(script_vector, "%%GetFrameDetails(%d, %d)[6]", break_id, i);
    v8::Local<v8::Value> result = CompileRun(script);
    CHECK_EQ(expected_line_number[i],
             i::Script::GetLineNumber(source_script,
                                      result->Int32Value(context).FromJust()));
  }
  v8::Debug::SetDebugEventListener(CcTest::isolate(), nullptr);
  CcTest::isolate()->TerminateExecution();
}


TEST(DebugBreakInline) {
  i::FLAG_allow_natives_syntax = true;
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.context();
  const char* source =
      "function debug(b) {             \n"
      "  if (b) debugger;              \n"
      "}                               \n"
      "function f(b) {                 \n"
      "  debug(b)                      \n"
      "};                              \n"
      "function g(b) {                 \n"
      "  f(b);                         \n"
      "};                              \n"
      "g(false);                       \n"
      "g(false);                       \n"
      "%OptimizeFunctionOnNextCall(g); \n"
      "g(true);";
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugBreakInlineListener);
  inline_script =
      v8::Script::Compile(context, v8_str(env->GetIsolate(), source))
          .ToLocalChecked();
  inline_script->Run(context).ToLocalChecked();
}


static void DebugEventStepNext(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  if (event == v8::Break) {
    PrepareStep(StepNext);
  }
}


static void RunScriptInANewCFrame(const char* source) {
  v8::TryCatch try_catch(CcTest::isolate());
  CompileRun(source);
  CHECK(try_catch.HasCaught());
}


TEST(Regress131642) {
  // Bug description:
  // When doing StepNext through the first script, the debugger is not reset
  // after exiting through exception.  A flawed implementation enabling the
  // debugger to step into Array.prototype.forEach breaks inside the callback
  // for forEach in the second script under the assumption that we are in a
  // recursive call.  In an attempt to step out, we crawl the stack using the
  // recorded frame pointer from the first script and fail when not finding it
  // on the stack.
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugEventStepNext);

  // We step through the first script.  It exits through an exception.  We run
  // this inside a new frame to record a different FP than the second script
  // would expect.
  const char* script_1 = "debugger; throw new Error();";
  RunScriptInANewCFrame(script_1);

  // The second script uses forEach.
  const char* script_2 = "[0].forEach(function() { });";
  CompileRun(script_2);

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
}


// Import from test-heap.cc
namespace v8 {
namespace internal {

int CountNativeContexts();
}
}


static void NopListener(const v8::Debug::EventDetails& event_details) {
}


TEST(DebuggerCreatesContextIffActive) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  CHECK_EQ(1, v8::internal::CountNativeContexts());

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CompileRun("debugger;");
  CHECK_EQ(1, v8::internal::CountNativeContexts());

  v8::Debug::SetDebugEventListener(env->GetIsolate(), NopListener);
  CompileRun("debugger;");
  CHECK_EQ(2, v8::internal::CountNativeContexts());

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
}


TEST(LiveEditEnabled) {
  v8::internal::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Debug::SetLiveEditEnabled(env->GetIsolate(), true);
  CompileRun("%LiveEditCompareStrings('', '')");
}


TEST(LiveEditDisabled) {
  v8::internal::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Debug::SetLiveEditEnabled(env->GetIsolate(), false);
  CompileRun("%LiveEditCompareStrings('', '')");
}


TEST(PrecompiledFunction) {
  // Regression test for crbug.com/346207. If we have preparse data, parsing the
  // function in the presence of the debugger (and breakpoints) should still
  // succeed. The bug was that preparsing was done lazily and parsing was done
  // eagerly, so, the symbol streams didn't match.
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  env.ExposeDebug();
  v8::Debug::SetDebugEventListener(env->GetIsolate(), DebugBreakInlineListener);

  v8::Local<v8::Function> break_here =
      CompileFunction(&env, "function break_here(){}", "break_here");
  SetBreakPoint(break_here, 0);

  const char* source =
      "var a = b = c = 1;              \n"
      "function this_is_lazy() {       \n"
      // This symbol won't appear in the preparse data.
      "  var a;                        \n"
      "}                               \n"
      "function bar() {                \n"
      "  return \"bar\";               \n"
      "};                              \n"
      "a = b = c = 2;                  \n"
      "bar();                          \n";
  v8::Local<v8::Value> result = ParserCacheCompileRun(source);
  CHECK(result->IsString());
  v8::String::Utf8Value utf8(result);
  CHECK_EQ(0, strcmp("bar", *utf8));

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded(env->GetIsolate());
}


static void DebugBreakStackTraceListener(
    const v8::Debug::EventDetails& event_details) {
  v8::StackTrace::CurrentStackTrace(CcTest::isolate(), 10);
}


static void AddDebugBreak(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Debug::DebugBreak(args.GetIsolate());
}


TEST(DebugBreakStackTrace) {
  DebugLocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Debug::SetDebugEventListener(env->GetIsolate(),
                                   DebugBreakStackTraceListener);
  v8::Local<v8::Context> context = env.context();
  v8::Local<v8::FunctionTemplate> add_debug_break_template =
      v8::FunctionTemplate::New(env->GetIsolate(), AddDebugBreak);
  v8::Local<v8::Function> add_debug_break =
      add_debug_break_template->GetFunction(context).ToLocalChecked();
  CHECK(env->Global()
            ->Set(context, v8_str("add_debug_break"), add_debug_break)
            .FromJust());

  CompileRun("(function loop() {"
             "  for (var j = 0; j < 1000; j++) {"
             "    for (var i = 0; i < 1000; i++) {"
             "      if (i == 999) add_debug_break();"
             "    }"
             "  }"
             "})()");
}


v8::base::Semaphore terminate_requested_semaphore(0);
v8::base::Semaphore terminate_fired_semaphore(0);
bool terminate_already_fired = false;


static void DebugBreakTriggerTerminate(
    const v8::Debug::EventDetails& event_details) {
  if (event_details.GetEvent() != v8::Break || terminate_already_fired) return;
  terminate_requested_semaphore.Signal();
  // Wait for at most 2 seconds for the terminate request.
  CHECK(terminate_fired_semaphore.WaitFor(v8::base::TimeDelta::FromSeconds(2)));
  terminate_already_fired = true;
}


class TerminationThread : public v8::base::Thread {
 public:
  explicit TerminationThread(v8::Isolate* isolate)
      : Thread(Options("terminator")), isolate_(isolate) {}

  virtual void Run() {
    terminate_requested_semaphore.Wait();
    isolate_->TerminateExecution();
    terminate_fired_semaphore.Signal();
  }

 private:
  v8::Isolate* isolate_;
};


TEST(DebugBreakOffThreadTerminate) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Debug::SetDebugEventListener(isolate, DebugBreakTriggerTerminate);
  TerminationThread terminator(isolate);
  terminator.Start();
  v8::TryCatch try_catch(env->GetIsolate());
  v8::Debug::DebugBreak(isolate);
  CompileRun("while (true);");
  CHECK(try_catch.HasTerminated());
}


static void DebugEventExpectNoException(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  CHECK_NE(v8::Exception, event);
}


static void TryCatchWrappedThrowCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch(args.GetIsolate());
  CompileRun("throw 'rejection';");
  CHECK(try_catch.HasCaught());
}


TEST(DebugPromiseInterceptedByTryCatch) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Debug::SetDebugEventListener(isolate, &DebugEventExpectNoException);
  v8::Local<v8::Context> context = env.context();
  ChangeBreakOnException(false, true);

  v8::Local<v8::FunctionTemplate> fun =
      v8::FunctionTemplate::New(isolate, TryCatchWrappedThrowCallback);
  CHECK(env->Global()
            ->Set(context, v8_str("fun"),
                  fun->GetFunction(context).ToLocalChecked())
            .FromJust());

  CompileRun("var p = new Promise(function(res, rej) { fun(); res(); });");
  CompileRun(
      "var r;"
      "p.then(function() { r = 'resolved'; },"
      "       function() { r = 'rejected'; });");
  CHECK(CompileRun("r")->Equals(context, v8_str("resolved")).FromJust());
}


static int exception_event_counter = 0;


static void DebugEventCountException(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  if (event == v8::Exception) exception_event_counter++;
}


static void ThrowCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CompileRun("throw 'rejection';");
}


TEST(DebugPromiseRejectedByCallback) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Debug::SetDebugEventListener(isolate, &DebugEventCountException);
  v8::Local<v8::Context> context = env.context();
  ChangeBreakOnException(false, true);
  exception_event_counter = 0;

  v8::Local<v8::FunctionTemplate> fun =
      v8::FunctionTemplate::New(isolate, ThrowCallback);
  CHECK(env->Global()
            ->Set(context, v8_str("fun"),
                  fun->GetFunction(context).ToLocalChecked())
            .FromJust());

  CompileRun("var p = new Promise(function(res, rej) { fun(); res(); });");
  CompileRun(
      "var r;"
      "p.then(function() { r = 'resolved'; },"
      "       function(e) { r = 'rejected' + e; });");
  CHECK(
      CompileRun("r")->Equals(context, v8_str("rejectedrejection")).FromJust());
  CHECK_EQ(1, exception_event_counter);
}


static void DebugHarmonyScopingListener(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  if (event != v8::Break) return;

  int break_id = CcTest::i_isolate()->debug()->break_id();

  char script[128];
  i::Vector<char> script_vector(script, sizeof(script));
  SNPrintF(script_vector, "%%GetFrameCount(%d)", break_id);
  ExpectInt32(script, 1);

  SNPrintF(script_vector, "var frame = new FrameMirror(%d, 0);", break_id);
  CompileRun(script);
  ExpectInt32("frame.evaluate('x').value_", 1);
  ExpectInt32("frame.evaluate('y').value_", 2);

  CompileRun("var allScopes = frame.allScopes()");
  ExpectInt32("allScopes.length", 2);

  ExpectBoolean("allScopes[0].scopeType() === ScopeType.Script", true);

  ExpectInt32("allScopes[0].scopeObject().value_.x", 1);

  ExpectInt32("allScopes[0].scopeObject().value_.y", 2);

  CompileRun("allScopes[0].setVariableValue('x', 5);");
  CompileRun("allScopes[0].setVariableValue('y', 6);");
  ExpectInt32("frame.evaluate('x + y').value_", 11);
}


TEST(DebugBreakInLexicalScopes) {
  i::FLAG_allow_natives_syntax = true;

  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Debug::SetDebugEventListener(isolate, DebugHarmonyScopingListener);

  CompileRun(
      "'use strict';            \n"
      "let x = 1;               \n");
  ExpectInt32(
      "'use strict';            \n"
      "let y = 2;               \n"
      "debugger;                \n"
      "x * y",
      30);
  ExpectInt32(
      "x = 1; y = 2; \n"
      "debugger;"
      "x * y",
      30);
}

static int after_compile_handler_depth = 0;
static void HandleInterrupt(v8::Isolate* isolate, void* data) {
  CHECK_EQ(0, after_compile_handler_depth);
}

static void NoInterruptsOnDebugEvent(
    const v8::Debug::EventDetails& event_details) {
  if (event_details.GetEvent() != v8::AfterCompile) return;
  ++after_compile_handler_depth;
  // Do not allow nested AfterCompile events.
  CHECK(after_compile_handler_depth <= 1);
  v8::Isolate* isolate = event_details.GetEventContext()->GetIsolate();
  isolate->RequestInterrupt(&HandleInterrupt, nullptr);
  CompileRun("function foo() {}; foo();");
  --after_compile_handler_depth;
}

TEST(NoInterruptsInDebugListener) {
  DebugLocalContext env;
  v8::Debug::SetDebugEventListener(env->GetIsolate(), NoInterruptsOnDebugEvent);
  CompileRun("void(0);");
}

TEST(BreakLocationIterator) {
  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  v8::HandleScope scope(isolate);

  v8::Local<v8::Value> result = CompileRun(
      "function f() {\n"
      "  debugger;   \n"
      "  f();        \n"
      "  debugger;   \n"
      "}             \n"
      "f");
  Handle<i::Object> function_obj = v8::Utils::OpenHandle(*result);
  Handle<i::JSFunction> function = Handle<i::JSFunction>::cast(function_obj);
  Handle<i::SharedFunctionInfo> shared(function->shared());

  EnableDebugger(isolate);
  CHECK(i_isolate->debug()->EnsureDebugInfo(shared, function));

  Handle<i::DebugInfo> debug_info(shared->GetDebugInfo());
  Handle<i::AbstractCode> abstract_code(shared->abstract_code());

  {
    auto iterator = i::BreakIterator::GetIterator(debug_info, abstract_code,
                                                  i::ALL_BREAK_LOCATIONS);
    CHECK(iterator->GetBreakLocation().IsDebuggerStatement());
    CHECK_EQ(17, iterator->GetBreakLocation().position());
    iterator->Next();
    CHECK(iterator->GetBreakLocation().IsDebugBreakSlot());
    CHECK_EQ(32, iterator->GetBreakLocation().position());
    iterator->Next();
    CHECK(iterator->GetBreakLocation().IsCall());
    CHECK_EQ(32, iterator->GetBreakLocation().position());
    iterator->Next();
    CHECK(iterator->GetBreakLocation().IsDebuggerStatement());
    CHECK_EQ(47, iterator->GetBreakLocation().position());
    iterator->Next();
    CHECK(iterator->GetBreakLocation().IsReturn());
    CHECK_EQ(60, iterator->GetBreakLocation().position());
    iterator->Next();
    CHECK(iterator->Done());
  }

  {
    auto iterator = i::BreakIterator::GetIterator(debug_info, abstract_code,
                                                  i::CALLS_AND_RETURNS);
    CHECK(iterator->GetBreakLocation().IsCall());
    CHECK_EQ(32, iterator->GetBreakLocation().position());
    iterator->Next();
    CHECK(iterator->GetBreakLocation().IsReturn());
    CHECK_EQ(60, iterator->GetBreakLocation().position());
    iterator->Next();
    CHECK(iterator->Done());
  }

  DisableDebugger(isolate);
}

TEST(DisableTailCallElimination) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_harmony_tailcalls = true;
  // TODO(ishell, 4698): Investigate why TurboFan in --always-opt mode makes
  // stack[2].getFunctionName() return null.
  i::FLAG_turbo_inlining = false;

  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  CHECK(v8::Debug::IsTailCallEliminationEnabled(isolate));

  CompileRun(
      "'use strict';                                                         \n"
      "Error.prepareStackTrace = (error,stack) => {                          \n"
      "  error.strace = stack;                                               \n"
      "  return error.message + \"\\n    at \" + stack.join(\"\\n    at \"); \n"
      "}                                                                     \n"
      "                                                                      \n"
      "function getCaller() {                                                \n"
      "  var e = new Error();                                                \n"
      "  e.stack;  // prepare stack trace                                    \n"
      "  var stack = e.strace;                                               \n"
      "  %GlobalPrint('caller: ');                                           \n"
      "  %GlobalPrint(stack[2].getFunctionName());                           \n"
      "  %GlobalPrint('\\n');                                                \n"
      "  return stack[2].getFunctionName();                                  \n"
      "}                                                                     \n"
      "function f() {                                                        \n"
      "  var caller = getCaller();                                           \n"
      "  if (caller === 'g') return 1;                                       \n"
      "  if (caller === 'h') return 2;                                       \n"
      "  return 0;                                                           \n"
      "}                                                                     \n"
      "function g() {                                                        \n"
      "  return f();                                                         \n"
      "}                                                                     \n"
      "function h() {                                                        \n"
      "  var result = g();                                                   \n"
      "  return result;                                                      \n"
      "}                                                                     \n"
      "%NeverOptimizeFunction(getCaller);                                    \n"
      "%NeverOptimizeFunction(f);                                            \n"
      "%NeverOptimizeFunction(h);                                            \n"
      "");
  ExpectInt32("h();", 2);
  ExpectInt32("h(); %OptimizeFunctionOnNextCall(g); h();", 2);
  v8::Debug::SetTailCallEliminationEnabled(isolate, false);
  CHECK(!v8::Debug::IsTailCallEliminationEnabled(isolate));
  ExpectInt32("h();", 1);
  ExpectInt32("h(); %OptimizeFunctionOnNextCall(g); h();", 1);
  v8::Debug::SetTailCallEliminationEnabled(isolate, true);
  CHECK(v8::Debug::IsTailCallEliminationEnabled(isolate));
  ExpectInt32("h();", 2);
  ExpectInt32("h(); %OptimizeFunctionOnNextCall(g); h();", 2);
}

TEST(DebugStepNextTailCallEliminiation) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_harmony_tailcalls = true;
  // TODO(ishell, 4698): Investigate why TurboFan in --always-opt mode makes
  // stack[2].getFunctionName() return null.
  i::FLAG_turbo_inlining = false;

  DebugLocalContext env;
  env.ExposeDebug();
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  CHECK(v8::Debug::IsTailCallEliminationEnabled(isolate));

  const char* source =
      "'use strict';                                           \n"
      "var Debug = debug.Debug;                                \n"
      "var exception = null;                                   \n"
      "var breaks = 0;                                         \n"
      "var log = [];                                           \n"
      "function f(x) {                                         \n"
      "  if (x == 2) {                                         \n"
      "    debugger;             // Break a                    \n"
      "  }                                                     \n"
      "  if (x-- > 0) {          // Break b                    \n"
      "    return f(x);          // Break c                    \n"
      "  }                                                     \n"
      "}                         // Break e                    \n"
      "function listener(event, exec_state, event_data, data) {\n"
      "  if (event != Debug.DebugEvent.Break) return;          \n"
      "  try {                                                 \n"
      "    var line = exec_state.frame(0).sourceLineText();    \n"
      "    var col = exec_state.frame(0).sourceColumn();       \n"
      "    var match = line.match(/\\/\\/ Break (\\w)/);       \n"
      "    log.push(match[1] + col);                           \n"
      "    exec_state.prepareStep(Debug.StepAction.StepNext);  \n"
      "  } catch (e) {                                         \n"
      "    exception = e;                                      \n"
      "  };                                                    \n"
      "};                                                      \n"
      "Debug.setListener(listener);                            \n"
      "f(4);                                                   \n"
      "Debug.setListener(null);  // Break d                    \n";

  CompileRun(source);
  ExpectNull("exception");
  ExpectString("JSON.stringify(log)", "[\"a4\",\"b2\",\"c4\",\"c11\",\"d0\"]");

  v8::Debug::SetTailCallEliminationEnabled(isolate, false);
  CompileRun(
      "log = [];                            \n"
      "Debug.setListener(listener);         \n"
      "f(5);                                \n"
      "Debug.setListener(null);  // Break f \n");
  ExpectNull("exception");
  ExpectString("JSON.stringify(log)",
               "[\"a4\",\"b2\",\"c4\",\"e0\",\"e0\",\"e0\",\"e0\",\"f0\"]");
}

size_t current_action = 0;
StepAction actions[] = {StepNext, StepNext};
static void DebugStepOverFunctionWithCaughtExceptionListener(
    const v8::Debug::EventDetails& event_details) {
  v8::DebugEvent event = event_details.GetEvent();
  if (event != v8::Break) return;
  ++break_point_hit_count;
  if (current_action >= 2) return;
  PrepareStep(actions[current_action]);
}

TEST(DebugStepOverFunctionWithCaughtException) {
  i::FLAG_allow_natives_syntax = true;

  DebugLocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Debug::SetDebugEventListener(
      isolate, DebugStepOverFunctionWithCaughtExceptionListener);

  break_point_hit_count = 0;
  CompileRun(
      "function foo() {\n"
      "  try { throw new Error(); } catch (e) {}\n"
      "}\n"
      "debugger;\n"
      "foo();\n"
      "foo();\n");

  v8::Debug::SetDebugEventListener(env->GetIsolate(), nullptr);
  CHECK_EQ(break_point_hit_count, 4);
}

bool out_of_memory_callback_called = false;
void OutOfMemoryCallback(void* data) {
  out_of_memory_callback_called = true;
  reinterpret_cast<v8::Isolate*>(data)->IncreaseHeapLimitForDebugging();
}

UNINITIALIZED_TEST(DebugSetOutOfMemoryListener) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  create_params.constraints.set_max_old_space_size(10);
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  {
    v8::Isolate::Scope i_scope(isolate);
    v8::HandleScope scope(isolate);
    LocalContext context(isolate);
    v8::debug::SetOutOfMemoryCallback(isolate, OutOfMemoryCallback,
                                      reinterpret_cast<void*>(isolate));
    CHECK(!out_of_memory_callback_called);
    // The following allocation fails unless the out-of-memory callback
    // increases the heap limit.
    int length = 10 * i::MB / i::kPointerSize;
    i_isolate->factory()->NewFixedArray(length, i::TENURED);
    CHECK(out_of_memory_callback_called);
  }
  isolate->Dispose();
}
