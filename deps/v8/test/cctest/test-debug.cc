// Copyright 2007-2008 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "api.h"
#include "compilation-cache.h"
#include "debug.h"
#include "platform.h"
#include "stub-cache.h"
#include "cctest.h"


using ::v8::internal::EmbeddedVector;
using ::v8::internal::Object;
using ::v8::internal::OS;
using ::v8::internal::Handle;
using ::v8::internal::Heap;
using ::v8::internal::JSGlobalProxy;
using ::v8::internal::Code;
using ::v8::internal::Debug;
using ::v8::internal::Debugger;
using ::v8::internal::CommandMessage;
using ::v8::internal::CommandMessageQueue;
using ::v8::internal::StepAction;
using ::v8::internal::StepIn;  // From StepAction enum
using ::v8::internal::StepNext;  // From StepAction enum
using ::v8::internal::StepOut;  // From StepAction enum
using ::v8::internal::Vector;


// Size of temp buffer for formatting small strings.
#define SMALL_STRING_BUFFER_SIZE 80

// --- A d d i t i o n a l   C h e c k   H e l p e r s


// Helper function used by the CHECK_EQ function when given Address
// arguments.  Should not be called directly.
static inline void CheckEqualsHelper(const char* file, int line,
                                     const char* expected_source,
                                     ::v8::internal::Address expected,
                                     const char* value_source,
                                     ::v8::internal::Address value) {
  if (expected != value) {
    V8_Fatal(file, line, "CHECK_EQ(%s, %s) failed\n#   "
                         "Expected: %i\n#   Found: %i",
             expected_source, value_source, expected, value);
  }
}


// Helper function used by the CHECK_NE function when given Address
// arguments.  Should not be called directly.
static inline void CheckNonEqualsHelper(const char* file, int line,
                                        const char* unexpected_source,
                                        ::v8::internal::Address unexpected,
                                        const char* value_source,
                                        ::v8::internal::Address value) {
  if (unexpected == value) {
    V8_Fatal(file, line, "CHECK_NE(%s, %s) failed\n#   Value: %i",
             unexpected_source, value_source, value);
  }
}


// Helper function used by the CHECK function when given code
// arguments.  Should not be called directly.
static inline void CheckEqualsHelper(const char* file, int line,
                                     const char* expected_source,
                                     const Code* expected,
                                     const char* value_source,
                                     const Code* value) {
  if (expected != value) {
    V8_Fatal(file, line, "CHECK_EQ(%s, %s) failed\n#   "
                         "Expected: %p\n#   Found: %p",
             expected_source, value_source, expected, value);
  }
}


static inline void CheckNonEqualsHelper(const char* file, int line,
                                        const char* expected_source,
                                        const Code* expected,
                                        const char* value_source,
                                        const Code* value) {
  if (expected == value) {
    V8_Fatal(file, line, "CHECK_NE(%s, %s) failed\n#   Value: %p",
             expected_source, value_source, value);
  }
}


// --- H e l p e r   C l a s s e s


// Helper class for creating a V8 enviromnent for running tests
class DebugLocalContext {
 public:
  inline DebugLocalContext(
      v8::ExtensionConfiguration* extensions = 0,
      v8::Handle<v8::ObjectTemplate> global_template =
          v8::Handle<v8::ObjectTemplate>(),
      v8::Handle<v8::Value> global_object = v8::Handle<v8::Value>())
      : context_(v8::Context::New(extensions, global_template, global_object)) {
    context_->Enter();
  }
  inline ~DebugLocalContext() {
    context_->Exit();
    context_.Dispose();
  }
  inline v8::Context* operator->() { return *context_; }
  inline v8::Context* operator*() { return *context_; }
  inline bool IsReady() { return !context_.IsEmpty(); }
  void ExposeDebug() {
    // Expose the debug context global object in the global object for testing.
    Debug::Load();
    Debug::debug_context()->set_security_token(
        v8::Utils::OpenHandle(*context_)->security_token());

    Handle<JSGlobalProxy> global(Handle<JSGlobalProxy>::cast(
        v8::Utils::OpenHandle(*context_->Global())));
    Handle<v8::internal::String> debug_string =
        v8::internal::Factory::LookupAsciiSymbol("debug");
    SetProperty(global, debug_string,
        Handle<Object>(Debug::debug_context()->global_proxy()), DONT_ENUM);
  }
 private:
  v8::Persistent<v8::Context> context_;
};


// --- H e l p e r   F u n c t i o n s


// Compile and run the supplied source and return the fequested function.
static v8::Local<v8::Function> CompileFunction(DebugLocalContext* env,
                                               const char* source,
                                               const char* function_name) {
  v8::Script::Compile(v8::String::New(source))->Run();
  return v8::Local<v8::Function>::Cast(
      (*env)->Global()->Get(v8::String::New(function_name)));
}


// Compile and run the supplied source and return the requested function.
static v8::Local<v8::Function> CompileFunction(const char* source,
                                               const char* function_name) {
  v8::Script::Compile(v8::String::New(source))->Run();
  return v8::Local<v8::Function>::Cast(
    v8::Context::GetCurrent()->Global()->Get(v8::String::New(function_name)));
}


// Helper function that compiles and runs the source.
static v8::Local<v8::Value> CompileRun(const char* source) {
  return v8::Script::Compile(v8::String::New(source))->Run();
}


// Is there any debug info for the function?
static bool HasDebugInfo(v8::Handle<v8::Function> fun) {
  Handle<v8::internal::JSFunction> f = v8::Utils::OpenHandle(*fun);
  Handle<v8::internal::SharedFunctionInfo> shared(f->shared());
  return Debug::HasDebugInfo(shared);
}


// Set a break point in a function and return the associated break point
// number.
static int SetBreakPoint(Handle<v8::internal::JSFunction> fun, int position) {
  static int break_point = 0;
  Handle<v8::internal::SharedFunctionInfo> shared(fun->shared());
  Debug::SetBreakPoint(
      shared, position,
      Handle<Object>(v8::internal::Smi::FromInt(++break_point)));
  return break_point;
}


// Set a break point in a function and return the associated break point
// number.
static int SetBreakPoint(v8::Handle<v8::Function> fun, int position) {
  return SetBreakPoint(v8::Utils::OpenHandle(*fun), position);
}


// Set a break point in a function using the Debug object and return the
// associated break point number.
static int SetBreakPointFromJS(const char* function_name,
                               int line, int position) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  OS::SNPrintF(buffer,
               "debug.Debug.setBreakPoint(%s,%d,%d)",
               function_name, line, position);
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  v8::Handle<v8::String> str = v8::String::New(buffer.start());
  return v8::Script::Compile(str)->Run()->Int32Value();
}


// Set a break point in a script identified by id using the global Debug object.
static int SetScriptBreakPointByIdFromJS(int script_id, int line, int column) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  if (column >= 0) {
    // Column specified set script break point on precise location.
    OS::SNPrintF(buffer,
                 "debug.Debug.setScriptBreakPointById(%d,%d,%d)",
                 script_id, line, column);
  } else {
    // Column not specified set script break point on line.
    OS::SNPrintF(buffer,
                 "debug.Debug.setScriptBreakPointById(%d,%d)",
                 script_id, line);
  }
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  {
    v8::TryCatch try_catch;
    v8::Handle<v8::String> str = v8::String::New(buffer.start());
    v8::Handle<v8::Value> value = v8::Script::Compile(str)->Run();
    CHECK(!try_catch.HasCaught());
    return value->Int32Value();
  }
}


// Set a break point in a script identified by name using the global Debug
// object.
static int SetScriptBreakPointByNameFromJS(const char* script_name,
                                           int line, int column) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  if (column >= 0) {
    // Column specified set script break point on precise location.
    OS::SNPrintF(buffer,
                 "debug.Debug.setScriptBreakPointByName(\"%s\",%d,%d)",
                 script_name, line, column);
  } else {
    // Column not specified set script break point on line.
    OS::SNPrintF(buffer,
                 "debug.Debug.setScriptBreakPointByName(\"%s\",%d)",
                 script_name, line);
  }
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  {
    v8::TryCatch try_catch;
    v8::Handle<v8::String> str = v8::String::New(buffer.start());
    v8::Handle<v8::Value> value = v8::Script::Compile(str)->Run();
    CHECK(!try_catch.HasCaught());
    return value->Int32Value();
  }
}


// Clear a break point.
static void ClearBreakPoint(int break_point) {
  Debug::ClearBreakPoint(
      Handle<Object>(v8::internal::Smi::FromInt(break_point)));
}


// Clear a break point using the global Debug object.
static void ClearBreakPointFromJS(int break_point_number) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  OS::SNPrintF(buffer,
               "debug.Debug.clearBreakPoint(%d)",
               break_point_number);
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  v8::Script::Compile(v8::String::New(buffer.start()))->Run();
}


static void EnableScriptBreakPointFromJS(int break_point_number) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  OS::SNPrintF(buffer,
               "debug.Debug.enableScriptBreakPoint(%d)",
               break_point_number);
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  v8::Script::Compile(v8::String::New(buffer.start()))->Run();
}


static void DisableScriptBreakPointFromJS(int break_point_number) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  OS::SNPrintF(buffer,
               "debug.Debug.disableScriptBreakPoint(%d)",
               break_point_number);
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  v8::Script::Compile(v8::String::New(buffer.start()))->Run();
}


static void ChangeScriptBreakPointConditionFromJS(int break_point_number,
                                                  const char* condition) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  OS::SNPrintF(buffer,
               "debug.Debug.changeScriptBreakPointCondition(%d, \"%s\")",
               break_point_number, condition);
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  v8::Script::Compile(v8::String::New(buffer.start()))->Run();
}


static void ChangeScriptBreakPointIgnoreCountFromJS(int break_point_number,
                                                    int ignoreCount) {
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
  OS::SNPrintF(buffer,
               "debug.Debug.changeScriptBreakPointIgnoreCount(%d, %d)",
               break_point_number, ignoreCount);
  buffer[SMALL_STRING_BUFFER_SIZE - 1] = '\0';
  v8::Script::Compile(v8::String::New(buffer.start()))->Run();
}


// Change break on exception.
static void ChangeBreakOnException(bool caught, bool uncaught) {
  Debug::ChangeBreakOnException(v8::internal::BreakException, caught);
  Debug::ChangeBreakOnException(v8::internal::BreakUncaughtException, uncaught);
}


// Change break on exception using the global Debug object.
static void ChangeBreakOnExceptionFromJS(bool caught, bool uncaught) {
  if (caught) {
    v8::Script::Compile(
        v8::String::New("debug.Debug.setBreakOnException()"))->Run();
  } else {
    v8::Script::Compile(
        v8::String::New("debug.Debug.clearBreakOnException()"))->Run();
  }
  if (uncaught) {
    v8::Script::Compile(
        v8::String::New("debug.Debug.setBreakOnUncaughtException()"))->Run();
  } else {
    v8::Script::Compile(
        v8::String::New("debug.Debug.clearBreakOnUncaughtException()"))->Run();
  }
}


// Prepare to step to next break location.
static void PrepareStep(StepAction step_action) {
  Debug::PrepareStep(step_action, 1);
}


// This function is in namespace v8::internal to be friend with class
// v8::internal::Debug.
namespace v8 {
namespace internal {

// Collect the currently debugged functions.
Handle<FixedArray> GetDebuggedFunctions() {
  v8::internal::DebugInfoListNode* node = Debug::debug_info_list_;

  // Find the number of debugged functions.
  int count = 0;
  while (node) {
    count++;
    node = node->next();
  }

  // Allocate array for the debugged functions
  Handle<FixedArray> debugged_functions =
      v8::internal::Factory::NewFixedArray(count);

  // Run through the debug info objects and collect all functions.
  count = 0;
  while (node) {
    debugged_functions->set(count++, *node->debug_info());
    node = node->next();
  }

  return debugged_functions;
}


static Handle<Code> ComputeCallDebugBreak(int argc) {
  CALL_HEAP_FUNCTION(v8::internal::StubCache::ComputeCallDebugBreak(argc),
                     Code);
}


// Check that the debugger has been fully unloaded.
void CheckDebuggerUnloaded(bool check_functions) {
  // Check that the debugger context is cleared and that there is no debug
  // information stored for the debugger.
  CHECK(Debug::debug_context().is_null());
  CHECK_EQ(NULL, Debug::debug_info_list_);

  // Collect garbage to ensure weak handles are cleared.
  Heap::CollectAllGarbage(false);
  Heap::CollectAllGarbage(false);

  // Iterate the head and check that there are no debugger related objects left.
  HeapIterator iterator;
  while (iterator.has_next()) {
    HeapObject* obj = iterator.next();
    CHECK(obj != NULL);
    CHECK(!obj->IsDebugInfo());
    CHECK(!obj->IsBreakPointInfo());

    // If deep check of functions is requested check that no debug break code
    // is left in all functions.
    if (check_functions) {
      if (obj->IsJSFunction()) {
        JSFunction* fun = JSFunction::cast(obj);
        for (RelocIterator it(fun->shared()->code()); !it.done(); it.next()) {
          RelocInfo::Mode rmode = it.rinfo()->rmode();
          if (RelocInfo::IsCodeTarget(rmode)) {
            CHECK(!Debug::IsDebugBreak(it.rinfo()->target_address()));
          } else if (RelocInfo::IsJSReturn(rmode)) {
            CHECK(!Debug::IsDebugBreakAtReturn(it.rinfo()));
          }
        }
      }
    }
  }
}


} }  // namespace v8::internal


// Check that the debugger has been fully unloaded.
static void CheckDebuggerUnloaded(bool check_functions = false) {
  v8::internal::CheckDebuggerUnloaded(check_functions);
}


// Inherit from BreakLocationIterator to get access to protected parts for
// testing.
class TestBreakLocationIterator: public v8::internal::BreakLocationIterator {
 public:
  explicit TestBreakLocationIterator(Handle<v8::internal::DebugInfo> debug_info)
    : BreakLocationIterator(debug_info, v8::internal::SOURCE_BREAK_LOCATIONS) {}
  v8::internal::RelocIterator* it() { return reloc_iterator_; }
  v8::internal::RelocIterator* it_original() {
    return reloc_iterator_original_;
  }
};


// Compile a function, set a break point and check that the call at the break
// location in the code is the expected debug_break function.
void CheckDebugBreakFunction(DebugLocalContext* env,
                             const char* source, const char* name,
                             int position, v8::internal::RelocInfo::Mode mode,
                             Code* debug_break) {
  // Create function and set the break point.
  Handle<v8::internal::JSFunction> fun = v8::Utils::OpenHandle(
      *CompileFunction(env, source, name));
  int bp = SetBreakPoint(fun, position);

  // Check that the debug break function is as expected.
  Handle<v8::internal::SharedFunctionInfo> shared(fun->shared());
  CHECK(Debug::HasDebugInfo(shared));
  TestBreakLocationIterator it1(Debug::GetDebugInfo(shared));
  it1.FindBreakLocationFromPosition(position);
  CHECK_EQ(mode, it1.it()->rinfo()->rmode());
  if (mode != v8::internal::RelocInfo::JS_RETURN) {
    CHECK_EQ(debug_break,
        Code::GetCodeFromTargetAddress(it1.it()->rinfo()->target_address()));
  } else {
    CHECK(Debug::IsDebugBreakAtReturn(it1.it()->rinfo()));
  }

  // Clear the break point and check that the debug break function is no longer
  // there
  ClearBreakPoint(bp);
  CHECK(!Debug::HasDebugInfo(shared));
  CHECK(Debug::EnsureDebugInfo(shared));
  TestBreakLocationIterator it2(Debug::GetDebugInfo(shared));
  it2.FindBreakLocationFromPosition(position);
  CHECK_EQ(mode, it2.it()->rinfo()->rmode());
  if (mode == v8::internal::RelocInfo::JS_RETURN) {
    CHECK(!Debug::IsDebugBreakAtReturn(it2.it()->rinfo()));
  }
}


// --- D e b u g   E v e n t   H a n d l e r s
// ---
// --- The different tests uses a number of debug event handlers.
// ---


// Source for The JavaScript function which picks out the function name of the
// top frame.
const char* frame_function_name_source =
    "function frame_function_name(exec_state) {"
    "  return exec_state.frame(0).func().name();"
    "}";
v8::Local<v8::Function> frame_function_name;


// Source for The JavaScript function which picks out the source line for the
// top frame.
const char* frame_source_line_source =
    "function frame_source_line(exec_state) {"
    "  return exec_state.frame(0).sourceLine();"
    "}";
v8::Local<v8::Function> frame_source_line;


// Source for The JavaScript function which picks out the source column for the
// top frame.
const char* frame_source_column_source =
    "function frame_source_column(exec_state) {"
    "  return exec_state.frame(0).sourceColumn();"
    "}";
v8::Local<v8::Function> frame_source_column;


// Source for The JavaScript function which picks out the script name for the
// top frame.
const char* frame_script_name_source =
    "function frame_script_name(exec_state) {"
    "  return exec_state.frame(0).func().script().name();"
    "}";
v8::Local<v8::Function> frame_script_name;


// Source for The JavaScript function which picks out the script data for the
// top frame.
const char* frame_script_data_source =
    "function frame_script_data(exec_state) {"
    "  return exec_state.frame(0).func().script().data();"
    "}";
v8::Local<v8::Function> frame_script_data;


// Source for The JavaScript function which returns the number of frames.
static const char* frame_count_source =
    "function frame_count(exec_state) {"
    "  return exec_state.frameCount();"
    "}";
v8::Handle<v8::Function> frame_count;


// Global variable to store the last function hit - used by some tests.
char last_function_hit[80];

// Global variable to store the name and data for last script hit - used by some
// tests.
char last_script_name_hit[80];
char last_script_data_hit[80];

// Global variables to store the last source position - used by some tests.
int last_source_line = -1;
int last_source_column = -1;

// Debug event handler which counts the break points which have been hit.
int break_point_hit_count = 0;
static void DebugEventBreakPointHitCount(v8::DebugEvent event,
                                         v8::Handle<v8::Object> exec_state,
                                         v8::Handle<v8::Object> event_data,
                                         v8::Handle<v8::Value> data) {
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(v8::internal::Debug::break_id(), 0);

  // Count the number of breaks.
  if (event == v8::Break) {
    break_point_hit_count++;
    if (!frame_function_name.IsEmpty()) {
      // Get the name of the function.
      const int argc = 1;
      v8::Handle<v8::Value> argv[argc] = { exec_state };
      v8::Handle<v8::Value> result = frame_function_name->Call(exec_state,
                                                               argc, argv);
      if (result->IsUndefined()) {
        last_function_hit[0] = '\0';
      } else {
        CHECK(result->IsString());
        v8::Handle<v8::String> function_name(result->ToString());
        function_name->WriteAscii(last_function_hit);
      }
    }

    if (!frame_source_line.IsEmpty()) {
      // Get the source line.
      const int argc = 1;
      v8::Handle<v8::Value> argv[argc] = { exec_state };
      v8::Handle<v8::Value> result = frame_source_line->Call(exec_state,
                                                             argc, argv);
      CHECK(result->IsNumber());
      last_source_line = result->Int32Value();
    }

    if (!frame_source_column.IsEmpty()) {
      // Get the source column.
      const int argc = 1;
      v8::Handle<v8::Value> argv[argc] = { exec_state };
      v8::Handle<v8::Value> result = frame_source_column->Call(exec_state,
                                                               argc, argv);
      CHECK(result->IsNumber());
      last_source_column = result->Int32Value();
    }

    if (!frame_script_name.IsEmpty()) {
      // Get the script name of the function script.
      const int argc = 1;
      v8::Handle<v8::Value> argv[argc] = { exec_state };
      v8::Handle<v8::Value> result = frame_script_name->Call(exec_state,
                                                             argc, argv);
      if (result->IsUndefined()) {
        last_script_name_hit[0] = '\0';
      } else {
        CHECK(result->IsString());
        v8::Handle<v8::String> script_name(result->ToString());
        script_name->WriteAscii(last_script_name_hit);
      }
    }

    if (!frame_script_data.IsEmpty()) {
      // Get the script data of the function script.
      const int argc = 1;
      v8::Handle<v8::Value> argv[argc] = { exec_state };
      v8::Handle<v8::Value> result = frame_script_data->Call(exec_state,
                                                             argc, argv);
      if (result->IsUndefined()) {
        last_script_data_hit[0] = '\0';
      } else {
        result = result->ToString();
        CHECK(result->IsString());
        v8::Handle<v8::String> script_data(result->ToString());
        script_data->WriteAscii(last_script_data_hit);
      }
    }
  }
}


// Debug event handler which counts a number of events and collects the stack
// height if there is a function compiled for that.
int exception_hit_count = 0;
int uncaught_exception_hit_count = 0;
int last_js_stack_height = -1;

static void DebugEventCounterClear() {
  break_point_hit_count = 0;
  exception_hit_count = 0;
  uncaught_exception_hit_count = 0;
}

static void DebugEventCounter(v8::DebugEvent event,
                              v8::Handle<v8::Object> exec_state,
                              v8::Handle<v8::Object> event_data,
                              v8::Handle<v8::Value> data) {
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(v8::internal::Debug::break_id(), 0);

  // Count the number of breaks.
  if (event == v8::Break) {
    break_point_hit_count++;
  } else if (event == v8::Exception) {
    exception_hit_count++;

    // Check whether the exception was uncaught.
    v8::Local<v8::String> fun_name = v8::String::New("uncaught");
    v8::Local<v8::Function> fun =
        v8::Function::Cast(*event_data->Get(fun_name));
    v8::Local<v8::Value> result = *fun->Call(event_data, 0, NULL);
    if (result->IsTrue()) {
      uncaught_exception_hit_count++;
    }
  }

  // Collect the JavsScript stack height if the function frame_count is
  // compiled.
  if (!frame_count.IsEmpty()) {
    static const int kArgc = 1;
    v8::Handle<v8::Value> argv[kArgc] = { exec_state };
    // Using exec_state as receiver is just to have a receiver.
    v8::Handle<v8::Value> result =  frame_count->Call(exec_state, kArgc, argv);
    last_js_stack_height = result->Int32Value();
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
  v8::Handle<v8::Value> expected;  // The expected result.
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
static void DebugEventEvaluate(v8::DebugEvent event,
                               v8::Handle<v8::Object> exec_state,
                               v8::Handle<v8::Object> event_data,
                               v8::Handle<v8::Value> data) {
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(v8::internal::Debug::break_id(), 0);

  if (event == v8::Break) {
    for (int i = 0; checks[i].expr != NULL; i++) {
      const int argc = 3;
      v8::Handle<v8::Value> argv[argc] = { exec_state,
                                           v8::String::New(checks[i].expr),
                                           checks[i].expected };
      v8::Handle<v8::Value> result =
          evaluate_check_function->Call(exec_state, argc, argv);
      if (!result->IsTrue()) {
        v8::String::AsciiValue ascii(checks[i].expected->ToString());
        V8_Fatal(__FILE__, __LINE__, "%s != %s", checks[i].expr, *ascii);
      }
    }
  }
}


// This debug event listener removes a breakpoint in a function
int debug_event_remove_break_point = 0;
static void DebugEventRemoveBreakPoint(v8::DebugEvent event,
                                       v8::Handle<v8::Object> exec_state,
                                       v8::Handle<v8::Object> event_data,
                                       v8::Handle<v8::Value> data) {
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(v8::internal::Debug::break_id(), 0);

  if (event == v8::Break) {
    break_point_hit_count++;
    v8::Handle<v8::Function> fun = v8::Handle<v8::Function>::Cast(data);
    ClearBreakPoint(debug_event_remove_break_point);
  }
}


// Debug event handler which counts break points hit and performs a step
// afterwards.
StepAction step_action = StepIn;  // Step action to perform when stepping.
static void DebugEventStep(v8::DebugEvent event,
                           v8::Handle<v8::Object> exec_state,
                           v8::Handle<v8::Object> event_data,
                           v8::Handle<v8::Value> data) {
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(v8::internal::Debug::break_id(), 0);

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
static void DebugEventStepSequence(v8::DebugEvent event,
                                   v8::Handle<v8::Object> exec_state,
                                   v8::Handle<v8::Object> event_data,
                                   v8::Handle<v8::Value> data) {
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(v8::internal::Debug::break_id(), 0);

  if (event == v8::Break || event == v8::Exception) {
    // Check that the current function is the expected.
    CHECK(break_point_hit_count <
          static_cast<int>(strlen(expected_step_sequence)));
    const int argc = 1;
    v8::Handle<v8::Value> argv[argc] = { exec_state };
    v8::Handle<v8::Value> result = frame_function_name->Call(exec_state,
                                                             argc, argv);
    CHECK(result->IsString());
    v8::String::AsciiValue function_name(result->ToString());
    CHECK_EQ(1, strlen(*function_name));
    CHECK_EQ((*function_name)[0],
              expected_step_sequence[break_point_hit_count]);

    // Perform step.
    break_point_hit_count++;
    PrepareStep(step_action);
  }
}


// Debug event handler which performs a garbage collection.
static void DebugEventBreakPointCollectGarbage(
    v8::DebugEvent event,
    v8::Handle<v8::Object> exec_state,
    v8::Handle<v8::Object> event_data,
    v8::Handle<v8::Value> data) {
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(v8::internal::Debug::break_id(), 0);

  // Perform a garbage collection when break point is hit and continue. Based
  // on the number of break points hit either scavenge or mark compact
  // collector is used.
  if (event == v8::Break) {
    break_point_hit_count++;
    if (break_point_hit_count % 2 == 0) {
      // Scavenge.
      Heap::CollectGarbage(0, v8::internal::NEW_SPACE);
    } else {
      // Mark sweep (and perhaps compact).
      Heap::CollectAllGarbage(false);
    }
  }
}


// Debug event handler which re-issues a debug break and calls the garbage
// collector to have the heap verified.
static void DebugEventBreak(v8::DebugEvent event,
                            v8::Handle<v8::Object> exec_state,
                            v8::Handle<v8::Object> event_data,
                            v8::Handle<v8::Value> data) {
  // When hitting a debug event listener there must be a break set.
  CHECK_NE(v8::internal::Debug::break_id(), 0);

  if (event == v8::Break) {
    // Count the number of breaks.
    break_point_hit_count++;

    // Run the garbage collector to enforce heap verification if option
    // --verify-heap is set.
    Heap::CollectGarbage(0, v8::internal::NEW_SPACE);

    // Set the break flag again to come back here as soon as possible.
    v8::Debug::DebugBreak();
  }
}


// --- M e s s a g e   C a l l b a c k


// Message callback which counts the number of messages.
int message_callback_count = 0;

static void MessageCallbackCountClear() {
  message_callback_count = 0;
}

static void MessageCallbackCount(v8::Handle<v8::Message> message,
                                 v8::Handle<v8::Value> data) {
  message_callback_count++;
}


// --- T h e   A c t u a l   T e s t s


// Test that the debug break function is the expected one for different kinds
// of break locations.
TEST(DebugStub) {
  using ::v8::internal::Builtins;
  v8::HandleScope scope;
  DebugLocalContext env;

  CheckDebugBreakFunction(&env,
                          "function f1(){}", "f1",
                          0,
                          v8::internal::RelocInfo::JS_RETURN,
                          NULL);
  CheckDebugBreakFunction(&env,
                          "function f2(){x=1;}", "f2",
                          0,
                          v8::internal::RelocInfo::CODE_TARGET,
                          Builtins::builtin(Builtins::StoreIC_DebugBreak));
  CheckDebugBreakFunction(&env,
                          "function f3(){var a=x;}", "f3",
                          0,
                          v8::internal::RelocInfo::CODE_TARGET_CONTEXT,
                          Builtins::builtin(Builtins::LoadIC_DebugBreak));

// TODO(1240753): Make the test architecture independent or split
// parts of the debugger into architecture dependent files. This
// part currently disabled as it is not portable between IA32/ARM.
// Currently on ICs for keyed store/load on ARM.
#if !defined (__arm__) && !defined(__thumb__)
  CheckDebugBreakFunction(
      &env,
      "function f4(){var index='propertyName'; var a={}; a[index] = 'x';}",
      "f4",
      0,
      v8::internal::RelocInfo::CODE_TARGET,
      Builtins::builtin(Builtins::KeyedStoreIC_DebugBreak));
  CheckDebugBreakFunction(
      &env,
      "function f5(){var index='propertyName'; var a={}; return a[index];}",
      "f5",
      0,
      v8::internal::RelocInfo::CODE_TARGET,
      Builtins::builtin(Builtins::KeyedLoadIC_DebugBreak));
#endif

  // Check the debug break code stubs for call ICs with different number of
  // parameters.
  Handle<Code> debug_break_0 = v8::internal::ComputeCallDebugBreak(0);
  Handle<Code> debug_break_1 = v8::internal::ComputeCallDebugBreak(1);
  Handle<Code> debug_break_4 = v8::internal::ComputeCallDebugBreak(4);

  CheckDebugBreakFunction(&env,
                          "function f4_0(){x();}", "f4_0",
                          0,
                          v8::internal::RelocInfo::CODE_TARGET_CONTEXT,
                          *debug_break_0);

  CheckDebugBreakFunction(&env,
                          "function f4_1(){x(1);}", "f4_1",
                          0,
                          v8::internal::RelocInfo::CODE_TARGET_CONTEXT,
                          *debug_break_1);

  CheckDebugBreakFunction(&env,
                          "function f4_4(){x(1,2,3,4);}", "f4_4",
                          0,
                          v8::internal::RelocInfo::CODE_TARGET_CONTEXT,
                          *debug_break_4);
}


// Test that the debug info in the VM is in sync with the functions being
// debugged.
TEST(DebugInfo) {
  v8::HandleScope scope;
  DebugLocalContext env;
  // Create a couple of functions for the test.
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){}", "foo");
  v8::Local<v8::Function> bar =
      CompileFunction(&env, "function bar(){}", "bar");
  // Initially no functions are debugged.
  CHECK_EQ(0, v8::internal::GetDebuggedFunctions()->length());
  CHECK(!HasDebugInfo(foo));
  CHECK(!HasDebugInfo(bar));
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
  CHECK_EQ(0, v8::internal::GetDebuggedFunctions()->length());
  CHECK(!HasDebugInfo(foo));
  CHECK(!HasDebugInfo(bar));
}


// Test that a break point can be set at an IC store location.
TEST(BreakPointICStore) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;

  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());
  v8::Script::Compile(v8::String::New("function foo(){bar=0;}"))->Run();
  v8::Local<v8::Function> foo =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("foo")));

  // Run without breakpoints.
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint
  int bp = SetBreakPoint(foo, 0);
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test that a break point can be set at an IC load location.
TEST(BreakPointICLoad) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;
  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());
  v8::Script::Compile(v8::String::New("bar=1"))->Run();
  v8::Script::Compile(v8::String::New("function foo(){var x=bar;}"))->Run();
  v8::Local<v8::Function> foo =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("foo")));

  // Run without breakpoints.
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint
  int bp = SetBreakPoint(foo, 0);
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test that a break point can be set at an IC call location.
TEST(BreakPointICCall) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;
  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());
  v8::Script::Compile(v8::String::New("function bar(){}"))->Run();
  v8::Script::Compile(v8::String::New("function foo(){bar();}"))->Run();
  v8::Local<v8::Function> foo =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("foo")));

  // Run without breakpoints.
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint
  int bp = SetBreakPoint(foo, 0);
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test that a break point can be set at a return store location.
TEST(BreakPointReturn) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;

  // Create a functions for checking the source line and column when hitting
  // a break point.
  frame_source_line = CompileFunction(&env,
                                      frame_source_line_source,
                                      "frame_source_line");
  frame_source_column = CompileFunction(&env,
                                        frame_source_column_source,
                                        "frame_source_column");


  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());
  v8::Script::Compile(v8::String::New("function foo(){}"))->Run();
  v8::Local<v8::Function> foo =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("foo")));

  // Run without breakpoints.
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint
  int bp = SetBreakPoint(foo, 0);
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);
  CHECK_EQ(0, last_source_line);
  CHECK_EQ(16, last_source_column);
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);
  CHECK_EQ(0, last_source_line);
  CHECK_EQ(16, last_source_column);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


static void CallWithBreakPoints(v8::Local<v8::Object> recv,
                                v8::Local<v8::Function> f,
                                int break_point_count,
                                int call_count) {
  break_point_hit_count = 0;
  for (int i = 0; i < call_count; i++) {
    f->Call(recv, 0, NULL);
    CHECK_EQ((i + 1) * break_point_count, break_point_hit_count);
  }
}

// Test GC during break point processing.
TEST(GCDuringBreakPointProcessing) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;

  v8::Debug::SetDebugEventListener(DebugEventBreakPointCollectGarbage,
                                   v8::Undefined());
  v8::Local<v8::Function> foo;

  // Test IC store break point with garbage collection.
  foo = CompileFunction(&env, "function foo(){bar=0;}", "foo");
  SetBreakPoint(foo, 0);
  CallWithBreakPoints(env->Global(), foo, 1, 10);

  // Test IC load break point with garbage collection.
  foo = CompileFunction(&env, "bar=1;function foo(){var x=bar;}", "foo");
  SetBreakPoint(foo, 0);
  CallWithBreakPoints(env->Global(), foo, 1, 10);

  // Test IC call break point with garbage collection.
  foo = CompileFunction(&env, "function bar(){};function foo(){bar();}", "foo");
  SetBreakPoint(foo, 0);
  CallWithBreakPoints(env->Global(), foo, 1, 10);

  // Test return break point with garbage collection.
  foo = CompileFunction(&env, "function foo(){}", "foo");
  SetBreakPoint(foo, 0);
  CallWithBreakPoints(env->Global(), foo, 1, 25);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Call the function three times with different garbage collections in between
// and make sure that the break point survives.
static void CallAndGC(v8::Local<v8::Object> recv, v8::Local<v8::Function> f) {
  break_point_hit_count = 0;

  for (int i = 0; i < 3; i++) {
    // Call function.
    f->Call(recv, 0, NULL);
    CHECK_EQ(1 + i * 3, break_point_hit_count);

    // Scavenge and call function.
    Heap::CollectGarbage(0, v8::internal::NEW_SPACE);
    f->Call(recv, 0, NULL);
    CHECK_EQ(2 + i * 3, break_point_hit_count);

    // Mark sweep (and perhaps compact) and call function.
    Heap::CollectAllGarbage(false);
    f->Call(recv, 0, NULL);
    CHECK_EQ(3 + i * 3, break_point_hit_count);
  }
}


// Test that a break point can be set at a return store location.
TEST(BreakPointSurviveGC) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;

  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());
  v8::Local<v8::Function> foo;

  // Test IC store break point with garbage collection.
  foo = CompileFunction(&env, "function foo(){bar=0;}", "foo");
  SetBreakPoint(foo, 0);
  CallAndGC(env->Global(), foo);

  // Test IC load break point with garbage collection.
  foo = CompileFunction(&env, "bar=1;function foo(){var x=bar;}", "foo");
  SetBreakPoint(foo, 0);
  CallAndGC(env->Global(), foo);

  // Test IC call break point with garbage collection.
  foo = CompileFunction(&env, "function bar(){};function foo(){bar();}", "foo");
  SetBreakPoint(foo, 0);
  CallAndGC(env->Global(), foo);

  // Test return break point with garbage collection.
  foo = CompileFunction(&env, "function foo(){}", "foo");
  SetBreakPoint(foo, 0);
  CallAndGC(env->Global(), foo);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test that break points can be set using the global Debug object.
TEST(BreakPointThroughJavaScript) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());
  v8::Script::Compile(v8::String::New("function bar(){}"))->Run();
  v8::Script::Compile(v8::String::New("function foo(){bar();bar();}"))->Run();
  //                                               012345678901234567890
  //                                                         1         2
  // Break points are set at position 3 and 9
  v8::Local<v8::Script> foo = v8::Script::Compile(v8::String::New("foo()"));

  // Run without breakpoints.
  foo->Run();
  CHECK_EQ(0, break_point_hit_count);

  // Run with one breakpoint
  int bp1 = SetBreakPointFromJS("foo", 0, 3);
  foo->Run();
  CHECK_EQ(1, break_point_hit_count);
  foo->Run();
  CHECK_EQ(2, break_point_hit_count);

  // Run with two breakpoints
  int bp2 = SetBreakPointFromJS("foo", 0, 9);
  foo->Run();
  CHECK_EQ(4, break_point_hit_count);
  foo->Run();
  CHECK_EQ(6, break_point_hit_count);

  // Run with one breakpoint
  ClearBreakPointFromJS(bp2);
  foo->Run();
  CHECK_EQ(7, break_point_hit_count);
  foo->Run();
  CHECK_EQ(8, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPointFromJS(bp1);
  foo->Run();
  CHECK_EQ(8, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();

  // Make sure that the break point numbers are consecutive.
  CHECK_EQ(1, bp1);
  CHECK_EQ(2, bp2);
}


// Test that break points on scripts identified by name can be set using the
// global Debug object.
TEST(ScriptBreakPointByNameThroughJavaScript) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());

  v8::Local<v8::String> script = v8::String::New(
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
  v8::ScriptOrigin origin =
      v8::ScriptOrigin(v8::String::New("test"));
  v8::Script::Compile(script, &origin)->Run();
  v8::Local<v8::Function> f =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));
  v8::Local<v8::Function> g =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("g")));

  // Call f and g without break points.
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  // Call f and g with break point on line 12.
  int sbp1 = SetScriptBreakPointByNameFromJS("test", 12, 0);
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);

  // Remove the break point again.
  break_point_hit_count = 0;
  ClearBreakPointFromJS(sbp1);
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  // Call f and g with break point on line 2.
  int sbp2 = SetScriptBreakPointByNameFromJS("test", 2, 0);
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  // Call f and g with break point on line 2, 4, 12, 14 and 15.
  int sbp3 = SetScriptBreakPointByNameFromJS("test", 4, 0);
  int sbp4 = SetScriptBreakPointByNameFromJS("test", 12, 0);
  int sbp5 = SetScriptBreakPointByNameFromJS("test", 14, 0);
  int sbp6 = SetScriptBreakPointByNameFromJS("test", 15, 0);
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(7, break_point_hit_count);

  // Remove all the break points again.
  break_point_hit_count = 0;
  ClearBreakPointFromJS(sbp2);
  ClearBreakPointFromJS(sbp3);
  ClearBreakPointFromJS(sbp4);
  ClearBreakPointFromJS(sbp5);
  ClearBreakPointFromJS(sbp6);
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();

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
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());

  v8::Local<v8::String> source = v8::String::New(
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
  v8::ScriptOrigin origin =
      v8::ScriptOrigin(v8::String::New("test"));
  v8::Local<v8::Script> script = v8::Script::Compile(source, &origin);
  script->Run();
  v8::Local<v8::Function> f =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));
  v8::Local<v8::Function> g =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("g")));

  // Get the script id knowing that internally it is a 32 integer.
  uint32_t script_id = script->Id()->Uint32Value();

  // Call f and g without break points.
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  // Call f and g with break point on line 12.
  int sbp1 = SetScriptBreakPointByIdFromJS(script_id, 12, 0);
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);

  // Remove the break point again.
  break_point_hit_count = 0;
  ClearBreakPointFromJS(sbp1);
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  // Call f and g with break point on line 2.
  int sbp2 = SetScriptBreakPointByIdFromJS(script_id, 2, 0);
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  // Call f and g with break point on line 2, 4, 12, 14 and 15.
  int sbp3 = SetScriptBreakPointByIdFromJS(script_id, 4, 0);
  int sbp4 = SetScriptBreakPointByIdFromJS(script_id, 12, 0);
  int sbp5 = SetScriptBreakPointByIdFromJS(script_id, 14, 0);
  int sbp6 = SetScriptBreakPointByIdFromJS(script_id, 15, 0);
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(7, break_point_hit_count);

  // Remove all the break points again.
  break_point_hit_count = 0;
  ClearBreakPointFromJS(sbp2);
  ClearBreakPointFromJS(sbp3);
  ClearBreakPointFromJS(sbp4);
  ClearBreakPointFromJS(sbp5);
  ClearBreakPointFromJS(sbp6);
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();

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
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());

  v8::Local<v8::String> script = v8::String::New(
    "function f() {\n"
    "  a = 0;  // line 1\n"
    "};");

  // Compile the script and get function f.
  v8::ScriptOrigin origin =
      v8::ScriptOrigin(v8::String::New("test"));
  v8::Script::Compile(script, &origin)->Run();
  v8::Local<v8::Function> f =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));

  // Set script break point on line 1 (in function f).
  int sbp = SetScriptBreakPointByNameFromJS("test", 1, 0);

  // Call f while enabeling and disabling the script break point.
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);

  DisableScriptBreakPointFromJS(sbp);
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);

  EnableScriptBreakPointFromJS(sbp);
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  DisableScriptBreakPointFromJS(sbp);
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  // Reload the script and get f again checking that the disabeling survives.
  v8::Script::Compile(script, &origin)->Run();
  f = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  EnableScriptBreakPointFromJS(sbp);
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(3, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test conditional script break points.
TEST(ConditionalScriptBreakPoint) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());

  v8::Local<v8::String> script = v8::String::New(
    "count = 0;\n"
    "function f() {\n"
    "  g(count++);  // line 2\n"
    "};\n"
    "function g(x) {\n"
    "  var a=x;  // line 5\n"
    "};");

  // Compile the script and get function f.
  v8::ScriptOrigin origin =
      v8::ScriptOrigin(v8::String::New("test"));
  v8::Script::Compile(script, &origin)->Run();
  v8::Local<v8::Function> f =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));

  // Set script break point on line 5 (in function g).
  int sbp1 = SetScriptBreakPointByNameFromJS("test", 5, 0);

  // Call f with different conditions on the script break point.
  break_point_hit_count = 0;
  ChangeScriptBreakPointConditionFromJS(sbp1, "false");
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  ChangeScriptBreakPointConditionFromJS(sbp1, "true");
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);

  ChangeScriptBreakPointConditionFromJS(sbp1, "a % 2 == 0");
  break_point_hit_count = 0;
  for (int i = 0; i < 10; i++) {
    f->Call(env->Global(), 0, NULL);
  }
  CHECK_EQ(5, break_point_hit_count);

  // Reload the script and get f again checking that the condition survives.
  v8::Script::Compile(script, &origin)->Run();
  f = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));

  break_point_hit_count = 0;
  for (int i = 0; i < 10; i++) {
    f->Call(env->Global(), 0, NULL);
  }
  CHECK_EQ(5, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test ignore count on script break points.
TEST(ScriptBreakPointIgnoreCount) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());

  v8::Local<v8::String> script = v8::String::New(
    "function f() {\n"
    "  a = 0;  // line 1\n"
    "};");

  // Compile the script and get function f.
  v8::ScriptOrigin origin =
      v8::ScriptOrigin(v8::String::New("test"));
  v8::Script::Compile(script, &origin)->Run();
  v8::Local<v8::Function> f =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));

  // Set script break point on line 1 (in function f).
  int sbp = SetScriptBreakPointByNameFromJS("test", 1, 0);

  // Call f with different ignores on the script break point.
  break_point_hit_count = 0;
  ChangeScriptBreakPointIgnoreCountFromJS(sbp, 1);
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);

  ChangeScriptBreakPointIgnoreCountFromJS(sbp, 5);
  break_point_hit_count = 0;
  for (int i = 0; i < 10; i++) {
    f->Call(env->Global(), 0, NULL);
  }
  CHECK_EQ(5, break_point_hit_count);

  // Reload the script and get f again checking that the ignore survives.
  v8::Script::Compile(script, &origin)->Run();
  f = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));

  break_point_hit_count = 0;
  for (int i = 0; i < 10; i++) {
    f->Call(env->Global(), 0, NULL);
  }
  CHECK_EQ(5, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test that script break points survive when a script is reloaded.
TEST(ScriptBreakPointReload) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());

  v8::Local<v8::Function> f;
  v8::Local<v8::String> script = v8::String::New(
    "function f() {\n"
    "  function h() {\n"
    "    a = 0;  // line 2\n"
    "  }\n"
    "  b = 1;  // line 4\n"
    "  return h();\n"
    "}");

  v8::ScriptOrigin origin_1 = v8::ScriptOrigin(v8::String::New("1"));
  v8::ScriptOrigin origin_2 = v8::ScriptOrigin(v8::String::New("2"));

  // Set a script break point before the script is loaded.
  SetScriptBreakPointByNameFromJS("1", 2, 0);

  // Compile the script and get the function.
  v8::Script::Compile(script, &origin_1)->Run();
  f = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));

  // Call f and check that the script break point is active.
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);

  // Compile the script again with a different script data and get the
  // function.
  v8::Script::Compile(script, &origin_2)->Run();
  f = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));

  // Call f and check that no break points are set.
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  // Compile the script again and get the function.
  v8::Script::Compile(script, &origin_1)->Run();
  f = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));

  // Call f and check that the script break point is active.
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test when several scripts has the same script data
TEST(ScriptBreakPointMultiple) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());

  v8::Local<v8::Function> f;
  v8::Local<v8::String> script_f = v8::String::New(
    "function f() {\n"
    "  a = 0;  // line 1\n"
    "}");

  v8::Local<v8::Function> g;
  v8::Local<v8::String> script_g = v8::String::New(
    "function g() {\n"
    "  b = 0;  // line 1\n"
    "}");

  v8::ScriptOrigin origin =
      v8::ScriptOrigin(v8::String::New("test"));

  // Set a script break point before the scripts are loaded.
  int sbp = SetScriptBreakPointByNameFromJS("test", 1, 0);

  // Compile the scripts with same script data and get the functions.
  v8::Script::Compile(script_f, &origin)->Run();
  f = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));
  v8::Script::Compile(script_g, &origin)->Run();
  g = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("g")));

  // Call f and g and check that the script break point is active.
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  // Clear the script break point.
  ClearBreakPointFromJS(sbp);

  // Call f and g and check that the script break point is no longer active.
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  // Set script break point with the scripts loaded.
  sbp = SetScriptBreakPointByNameFromJS("test", 1, 0);

  // Call f and g and check that the script break point is active.
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test the script origin which has both name and line offset.
TEST(ScriptBreakPointLineOffset) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());

  v8::Local<v8::Function> f;
  v8::Local<v8::String> script = v8::String::New(
    "function f() {\n"
    "  a = 0;  // line 8 as this script has line offset 7\n"
    "  b = 0;  // line 9 as this script has line offset 7\n"
    "}");

  // Create script origin both name and line offset.
  v8::ScriptOrigin origin(v8::String::New("test.html"),
                          v8::Integer::New(7));

  // Set two script break points before the script is loaded.
  int sbp1 = SetScriptBreakPointByNameFromJS("test.html", 8, 0);
  int sbp2 = SetScriptBreakPointByNameFromJS("test.html", 9, 0);

  // Compile the script and get the function.
  v8::Script::Compile(script, &origin)->Run();
  f = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));

  // Call f and check that the script break point is active.
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  // Clear the script break points.
  ClearBreakPointFromJS(sbp1);
  ClearBreakPointFromJS(sbp2);

  // Call f and check that no script break points are active.
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  // Set a script break point with the script loaded.
  sbp1 = SetScriptBreakPointByNameFromJS("test.html", 9, 0);

  // Call f and check that the script break point is active.
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test script break points set on lines.
TEST(ScriptBreakPointLine) {
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  // Create a function for checking the function when hitting a break point.
  frame_function_name = CompileFunction(&env,
                                        frame_function_name_source,
                                        "frame_function_name");

  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());

  v8::Local<v8::Function> f;
  v8::Local<v8::Function> g;
  v8::Local<v8::String> script = v8::String::New(
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

  // Set a couple script break point before the script is loaded.
  int sbp1 = SetScriptBreakPointByNameFromJS("test.html", 0, -1);
  int sbp2 = SetScriptBreakPointByNameFromJS("test.html", 1, -1);
  int sbp3 = SetScriptBreakPointByNameFromJS("test.html", 5, -1);

  // Compile the script and get the function.
  break_point_hit_count = 0;
  v8::ScriptOrigin origin(v8::String::New("test.html"), v8::Integer::New(0));
  v8::Script::Compile(script, &origin)->Run();
  f = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));
  g = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("g")));

  // Chesk that a break point was hit when the script was run.
  CHECK_EQ(1, break_point_hit_count);
  CHECK_EQ(0, strlen(last_function_hit));

  // Call f and check that the script break point.
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);
  CHECK_EQ("f", last_function_hit);

  // Call g and check that the script break point.
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(3, break_point_hit_count);
  CHECK_EQ("g", last_function_hit);

  // Clear the script break point on g and set one on h.
  ClearBreakPointFromJS(sbp3);
  int sbp4 = SetScriptBreakPointByNameFromJS("test.html", 6, -1);

  // Call g and check that the script break point in h is hit.
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(4, break_point_hit_count);
  CHECK_EQ("h", last_function_hit);

  // Clear break points in f and h. Set a new one in the script between
  // functions f and g and test that there is no break points in f and g any
  // more.
  ClearBreakPointFromJS(sbp2);
  ClearBreakPointFromJS(sbp4);
  int sbp5 = SetScriptBreakPointByNameFromJS("test.html", 4, -1);
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  // Reload the script which should hit two break points.
  break_point_hit_count = 0;
  v8::Script::Compile(script, &origin)->Run();
  CHECK_EQ(2, break_point_hit_count);
  CHECK_EQ(0, strlen(last_function_hit));

  // Set a break point in the code after the last function decleration.
  int sbp6 = SetScriptBreakPointByNameFromJS("test.html", 12, -1);

  // Reload the script which should hit three break points.
  break_point_hit_count = 0;
  v8::Script::Compile(script, &origin)->Run();
  CHECK_EQ(3, break_point_hit_count);
  CHECK_EQ(0, strlen(last_function_hit));

  // Clear the last break points, and reload the script which should not hit any
  // break points.
  ClearBreakPointFromJS(sbp1);
  ClearBreakPointFromJS(sbp5);
  ClearBreakPointFromJS(sbp6);
  break_point_hit_count = 0;
  v8::Script::Compile(script, &origin)->Run();
  CHECK_EQ(0, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test that it is possible to remove the last break point for a function
// inside the break handling of that break point.
TEST(RemoveBreakPointInBreak) {
  v8::HandleScope scope;
  DebugLocalContext env;

  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){a=1;}", "foo");
  debug_event_remove_break_point = SetBreakPoint(foo, 0);

  // Register the debug event listener pasing the function
  v8::Debug::SetDebugEventListener(DebugEventRemoveBreakPoint, foo);

  break_point_hit_count = 0;
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);

  break_point_hit_count = 0;
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test that the debugger statement causes a break.
TEST(DebuggerStatement) {
  break_point_hit_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;
  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());
  v8::Script::Compile(v8::String::New("function bar(){debugger}"))->Run();
  v8::Script::Compile(v8::String::New(
      "function foo(){debugger;debugger;}"))->Run();
  v8::Local<v8::Function> foo =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("foo")));
  v8::Local<v8::Function> bar =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("bar")));

  // Run function with debugger statement
  bar->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);

  // Run function with two debugger statement
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(3, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Thest that the evaluation of expressions when a break point is hit generates
// the correct results.
TEST(DebugEvaluate) {
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  // Create a function for checking the evaluation when hitting a break point.
  evaluate_check_function = CompileFunction(&env,
                                            evaluate_check_source,
                                            "evaluate_check");
  // Register the debug event listener
  v8::Debug::SetDebugEventListener(DebugEventEvaluate);

  // Different expected vaules of x and a when in a break point (u = undefined,
  // d = Hello, world!).
  struct EvaluateCheck checks_uu[] = {
    {"x", v8::Undefined()},
    {"a", v8::Undefined()},
    {NULL, v8::Handle<v8::Value>()}
  };
  struct EvaluateCheck checks_hu[] = {
    {"x", v8::String::New("Hello, world!")},
    {"a", v8::Undefined()},
    {NULL, v8::Handle<v8::Value>()}
  };
  struct EvaluateCheck checks_hh[] = {
    {"x", v8::String::New("Hello, world!")},
    {"a", v8::String::New("Hello, world!")},
    {NULL, v8::Handle<v8::Value>()}
  };

  // Simple test function. The "y=0" is in the function foo to provide a break
  // location. For "y=0" the "y" is at position 15 in the barbar function
  // therefore setting breakpoint at position 15 will break at "y=0" and
  // setting it higher will break after.
  v8::Local<v8::Function> foo = CompileFunction(&env,
    "function foo(x) {"
    "  var a;"
    "  y=0; /* To ensure break location.*/"
    "  a=x;"
    "}",
    "foo");
  const int foo_break_position = 15;

  // Arguments with one parameter "Hello, world!"
  v8::Handle<v8::Value> argv_foo[1] = { v8::String::New("Hello, world!") };

  // Call foo with breakpoint set before a=x and undefined as parameter.
  int bp = SetBreakPoint(foo, foo_break_position);
  checks = checks_uu;
  foo->Call(env->Global(), 0, NULL);

  // Call foo with breakpoint set before a=x and parameter "Hello, world!".
  checks = checks_hu;
  foo->Call(env->Global(), 1, argv_foo);

  // Call foo with breakpoint set after a=x and parameter "Hello, world!".
  ClearBreakPoint(bp);
  SetBreakPoint(foo, foo_break_position + 1);
  checks = checks_hh;
  foo->Call(env->Global(), 1, argv_foo);

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
  v8::Handle<v8::Value> argv_bar_1[2] = {
    v8::Undefined(),
    v8::Number::New(barbar_break_position)
  };
  bar->Call(env->Global(), 2, argv_bar_1);

  // Call bar setting breakpoint before a=x in barbar and parameter
  // "Hello, world!".
  checks = checks_hu;
  v8::Handle<v8::Value> argv_bar_2[2] = {
    v8::String::New("Hello, world!"),
    v8::Number::New(barbar_break_position)
  };
  bar->Call(env->Global(), 2, argv_bar_2);

  // Call bar setting breakpoint after a=x in barbar and parameter
  // "Hello, world!".
  checks = checks_hh;
  v8::Handle<v8::Value> argv_bar_3[2] = {
    v8::String::New("Hello, world!"),
    v8::Number::New(barbar_break_position + 1)
  };
  bar->Call(env->Global(), 2, argv_bar_3);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Simple test of the stepping mechanism using only store ICs.
TEST(DebugStepLinear) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Create a function for testing stepping.
  v8::Local<v8::Function> foo = CompileFunction(&env,
                                                "function foo(){a=1;b=1;c=1;}",
                                                "foo");
  SetBreakPoint(foo, 3);

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(DebugEventStep);

  step_action = StepIn;
  break_point_hit_count = 0;
  foo->Call(env->Global(), 0, NULL);

  // With stepping all break locations are hit.
  CHECK_EQ(4, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();

  // Register a debug event listener which just counts.
  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount);

  SetBreakPoint(foo, 3);
  break_point_hit_count = 0;
  foo->Call(env->Global(), 0, NULL);

  // Without stepping only active break points are hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test of the stepping mechanism for keyed load in a loop.
TEST(DebugStepKeyedLoadLoop) {
  v8::HandleScope scope;
  DebugLocalContext env;

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
      "}\n",
      "foo");

  // Create array [0,1,2,3,4,5,6,7,8,9]
  v8::Local<v8::Array> a = v8::Array::New(10);
  for (int i = 0; i < 10; i++) {
    a->Set(v8::Number::New(i), v8::Number::New(i));
  }

  // Call function without any break points to ensure inlining is in place.
  const int kArgc = 1;
  v8::Handle<v8::Value> args[kArgc] = { a };
  foo->Call(env->Global(), kArgc, args);

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(DebugEventStep);

  // Setup break point and step through the function.
  SetBreakPoint(foo, 3);
  step_action = StepNext;
  break_point_hit_count = 0;
  foo->Call(env->Global(), kArgc, args);

  // With stepping all break locations are hit.
  CHECK_EQ(22, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test of the stepping mechanism for keyed store in a loop.
TEST(DebugStepKeyedStoreLoop) {
  v8::HandleScope scope;
  DebugLocalContext env;

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
      "}\n",
      "foo");

  // Create array [0,1,2,3,4,5,6,7,8,9]
  v8::Local<v8::Array> a = v8::Array::New(10);
  for (int i = 0; i < 10; i++) {
    a->Set(v8::Number::New(i), v8::Number::New(i));
  }

  // Call function without any break points to ensure inlining is in place.
  const int kArgc = 1;
  v8::Handle<v8::Value> args[kArgc] = { a };
  foo->Call(env->Global(), kArgc, args);

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(DebugEventStep);

  // Setup break point and step through the function.
  SetBreakPoint(foo, 3);
  step_action = StepNext;
  break_point_hit_count = 0;
  foo->Call(env->Global(), kArgc, args);

  // With stepping all break locations are hit.
  CHECK_EQ(22, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test the stepping mechanism with different ICs.
TEST(DebugStepLinearMixedICs) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Create a function for testing stepping.
  v8::Local<v8::Function> foo = CompileFunction(&env,
      "function bar() {};"
      "function foo() {"
      "  var x;"
      "  var index='name';"
      "  var y = {};"
      "  a=1;b=2;x=a;y[index]=3;x=y[index];bar();}", "foo");
  SetBreakPoint(foo, 0);

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(DebugEventStep);

  step_action = StepIn;
  break_point_hit_count = 0;
  foo->Call(env->Global(), 0, NULL);

  // With stepping all break locations are hit.
  CHECK_EQ(8, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();

  // Register a debug event listener which just counts.
  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount);

  SetBreakPoint(foo, 0);
  break_point_hit_count = 0;
  foo->Call(env->Global(), 0, NULL);

  // Without stepping only active break points are hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


TEST(DebugStepIf) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(DebugEventStep);

  // Create a function for testing stepping.
  const int argc = 1;
  const char* src = "function foo(x) { "
                    "  a = 1;"
                    "  if (x) {"
                    "    b = 1;"
                    "  } else {"
                    "    c = 1;"
                    "    d = 1;"
                    "  }"
                    "}";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");
  SetBreakPoint(foo, 0);

  // Stepping through the true part.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Handle<v8::Value> argv_true[argc] = { v8::True() };
  foo->Call(env->Global(), argc, argv_true);
  CHECK_EQ(3, break_point_hit_count);

  // Stepping through the false part.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Handle<v8::Value> argv_false[argc] = { v8::False() };
  foo->Call(env->Global(), argc, argv_false);
  CHECK_EQ(4, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


TEST(DebugStepSwitch) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(DebugEventStep);

  // Create a function for testing stepping.
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
                    "      break;"
                    "  }"
                    "}";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");
  SetBreakPoint(foo, 0);

  // One case with fall-through.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Handle<v8::Value> argv_1[argc] = { v8::Number::New(1) };
  foo->Call(env->Global(), argc, argv_1);
  CHECK_EQ(4, break_point_hit_count);

  // Another case.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Handle<v8::Value> argv_2[argc] = { v8::Number::New(2) };
  foo->Call(env->Global(), argc, argv_2);
  CHECK_EQ(3, break_point_hit_count);

  // Last case.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Handle<v8::Value> argv_3[argc] = { v8::Number::New(3) };
  foo->Call(env->Global(), argc, argv_3);
  CHECK_EQ(4, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


TEST(DebugStepFor) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(DebugEventStep);

  // Create a function for testing stepping.
  const int argc = 1;
  const char* src = "function foo(x) { "
                    "  a = 1;"
                    "  for (i = 0; i < x; i++) {"
                    "    b = 1;"
                    "  }"
                    "}";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");
  SetBreakPoint(foo, 8);  // "a = 1;"

  // Looping 10 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Handle<v8::Value> argv_10[argc] = { v8::Number::New(10) };
  foo->Call(env->Global(), argc, argv_10);
  CHECK_EQ(23, break_point_hit_count);

  // Looping 100 times.
  step_action = StepIn;
  break_point_hit_count = 0;
  v8::Handle<v8::Value> argv_100[argc] = { v8::Number::New(100) };
  foo->Call(env->Global(), argc, argv_100);
  CHECK_EQ(203, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


TEST(StepInOutSimple) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Create a function for checking the function when hitting a break point.
  frame_function_name = CompileFunction(&env,
                                        frame_function_name_source,
                                        "frame_function_name");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(DebugEventStepSequence);

  // Create functions for testing stepping.
  const char* src = "function a() {b();c();}; "
                    "function b() {c();}; "
                    "function c() {}; ";
  v8::Local<v8::Function> a = CompileFunction(&env, src, "a");
  SetBreakPoint(a, 0);

  // Step through invocation of a with step in.
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "abcbaca";
  a->Call(env->Global(), 0, NULL);
  CHECK_EQ(strlen(expected_step_sequence), break_point_hit_count);

  // Step through invocation of a with step next.
  step_action = StepNext;
  break_point_hit_count = 0;
  expected_step_sequence = "aaa";
  a->Call(env->Global(), 0, NULL);
  CHECK_EQ(strlen(expected_step_sequence), break_point_hit_count);

  // Step through invocation of a with step out.
  step_action = StepOut;
  break_point_hit_count = 0;
  expected_step_sequence = "a";
  a->Call(env->Global(), 0, NULL);
  CHECK_EQ(strlen(expected_step_sequence), break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


TEST(StepInOutTree) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Create a function for checking the function when hitting a break point.
  frame_function_name = CompileFunction(&env,
                                        frame_function_name_source,
                                        "frame_function_name");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(DebugEventStepSequence);

  // Create functions for testing stepping.
  const char* src = "function a() {b(c(d()),d());c(d());d()}; "
                    "function b(x,y) {c();}; "
                    "function c(x) {}; "
                    "function d() {}; ";
  v8::Local<v8::Function> a = CompileFunction(&env, src, "a");
  SetBreakPoint(a, 0);

  // Step through invocation of a with step in.
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "adacadabcbadacada";
  a->Call(env->Global(), 0, NULL);
  CHECK_EQ(strlen(expected_step_sequence), break_point_hit_count);

  // Step through invocation of a with step next.
  step_action = StepNext;
  break_point_hit_count = 0;
  expected_step_sequence = "aaaa";
  a->Call(env->Global(), 0, NULL);
  CHECK_EQ(strlen(expected_step_sequence), break_point_hit_count);

  // Step through invocation of a with step out.
  step_action = StepOut;
  break_point_hit_count = 0;
  expected_step_sequence = "a";
  a->Call(env->Global(), 0, NULL);
  CHECK_EQ(strlen(expected_step_sequence), break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded(true);
}


TEST(StepInOutBranch) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Create a function for checking the function when hitting a break point.
  frame_function_name = CompileFunction(&env,
                                        frame_function_name_source,
                                        "frame_function_name");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(DebugEventStepSequence);

  // Create functions for testing stepping.
  const char* src = "function a() {b(false);c();}; "
                    "function b(x) {if(x){c();};}; "
                    "function c() {}; ";
  v8::Local<v8::Function> a = CompileFunction(&env, src, "a");
  SetBreakPoint(a, 0);

  // Step through invocation of a.
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "abaca";
  a->Call(env->Global(), 0, NULL);
  CHECK_EQ(strlen(expected_step_sequence), break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test that step in does not step into native functions.
TEST(DebugStepNatives) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Create a function for testing stepping.
  v8::Local<v8::Function> foo = CompileFunction(
      &env,
      "function foo(){debugger;Math.sin(1);}",
      "foo");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(DebugEventStep);

  step_action = StepIn;
  break_point_hit_count = 0;
  foo->Call(env->Global(), 0, NULL);

  // With stepping all break locations are hit.
  CHECK_EQ(3, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();

  // Register a debug event listener which just counts.
  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount);

  break_point_hit_count = 0;
  foo->Call(env->Global(), 0, NULL);

  // Without stepping only active break points are hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test that step in works with function.apply.
TEST(DebugStepFunctionApply) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Create a function for testing stepping.
  v8::Local<v8::Function> foo = CompileFunction(
      &env,
      "function bar(x, y, z) { if (x == 1) { a = y; b = z; } }"
      "function foo(){ debugger; bar.apply(this, [1,2,3]); }",
      "foo");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(DebugEventStep);

  step_action = StepIn;
  break_point_hit_count = 0;
  foo->Call(env->Global(), 0, NULL);

  // With stepping all break locations are hit.
  CHECK_EQ(6, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();

  // Register a debug event listener which just counts.
  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount);

  break_point_hit_count = 0;
  foo->Call(env->Global(), 0, NULL);

  // Without stepping only the debugger statement is hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test that step in works with function.call.
TEST(DebugStepFunctionCall) {
  v8::HandleScope scope;
  DebugLocalContext env;

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
  v8::Debug::SetDebugEventListener(DebugEventStep);
  step_action = StepIn;

  // Check stepping where the if condition in bar is false.
  break_point_hit_count = 0;
  foo->Call(env->Global(), 0, NULL);
  CHECK_EQ(4, break_point_hit_count);

  // Check stepping where the if condition in bar is true.
  break_point_hit_count = 0;
  const int argc = 1;
  v8::Handle<v8::Value> argv[argc] = { v8::True() };
  foo->Call(env->Global(), argc, argv);
  CHECK_EQ(6, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();

  // Register a debug event listener which just counts.
  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount);

  break_point_hit_count = 0;
  foo->Call(env->Global(), 0, NULL);

  // Without stepping only the debugger statement is hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test break on exceptions. For each exception break combination the number
// of debug event exception callbacks and message callbacks are collected. The
// number of debug event exception callbacks are used to check that the
// debugger is called correctly and the number of message callbacks is used to
// check that uncaught exceptions are still returned even if there is a break
// for them.
TEST(BreakOnException) {
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  v8::internal::Top::TraceException(false);

  // Create functions for testing break on exception.
  v8::Local<v8::Function> throws =
      CompileFunction(&env, "function throws(){throw 1;}", "throws");
  v8::Local<v8::Function> caught =
      CompileFunction(&env,
                      "function caught(){try {throws();} catch(e) {};}",
                      "caught");
  v8::Local<v8::Function> notCaught =
      CompileFunction(&env, "function notCaught(){throws();}", "notCaught");

  v8::V8::AddMessageListener(MessageCallbackCount);
  v8::Debug::SetDebugEventListener(DebugEventCounter);

  // Initial state should be break on uncaught exception.
  DebugEventCounterClear();
  MessageCallbackCountClear();
  caught->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, exception_hit_count);
  CHECK_EQ(0, uncaught_exception_hit_count);
  CHECK_EQ(0, message_callback_count);
  notCaught->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, exception_hit_count);
  CHECK_EQ(1, uncaught_exception_hit_count);
  CHECK_EQ(1, message_callback_count);

  // No break on exception
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnException(false, false);
  caught->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, exception_hit_count);
  CHECK_EQ(0, uncaught_exception_hit_count);
  CHECK_EQ(0, message_callback_count);
  notCaught->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, exception_hit_count);
  CHECK_EQ(0, uncaught_exception_hit_count);
  CHECK_EQ(1, message_callback_count);

  // Break on uncaught exception
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnException(false, true);
  caught->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, exception_hit_count);
  CHECK_EQ(0, uncaught_exception_hit_count);
  CHECK_EQ(0, message_callback_count);
  notCaught->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, exception_hit_count);
  CHECK_EQ(1, uncaught_exception_hit_count);
  CHECK_EQ(1, message_callback_count);

  // Break on exception and uncaught exception
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnException(true, true);
  caught->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, exception_hit_count);
  CHECK_EQ(0, uncaught_exception_hit_count);
  CHECK_EQ(0, message_callback_count);
  notCaught->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, exception_hit_count);
  CHECK_EQ(1, uncaught_exception_hit_count);
  CHECK_EQ(1, message_callback_count);

  // Break on exception
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnException(true, false);
  caught->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, exception_hit_count);
  CHECK_EQ(0, uncaught_exception_hit_count);
  CHECK_EQ(0, message_callback_count);
  notCaught->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, exception_hit_count);
  CHECK_EQ(1, uncaught_exception_hit_count);
  CHECK_EQ(1, message_callback_count);

  // No break on exception using JavaScript
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnExceptionFromJS(false, false);
  caught->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, exception_hit_count);
  CHECK_EQ(0, uncaught_exception_hit_count);
  CHECK_EQ(0, message_callback_count);
  notCaught->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, exception_hit_count);
  CHECK_EQ(0, uncaught_exception_hit_count);
  CHECK_EQ(1, message_callback_count);

  // Break on uncaught exception using JavaScript
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnExceptionFromJS(false, true);
  caught->Call(env->Global(), 0, NULL);
  CHECK_EQ(0, exception_hit_count);
  CHECK_EQ(0, uncaught_exception_hit_count);
  CHECK_EQ(0, message_callback_count);
  notCaught->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, exception_hit_count);
  CHECK_EQ(1, uncaught_exception_hit_count);
  CHECK_EQ(1, message_callback_count);

  // Break on exception and uncaught exception using JavaScript
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnExceptionFromJS(true, true);
  caught->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, exception_hit_count);
  CHECK_EQ(0, message_callback_count);
  CHECK_EQ(0, uncaught_exception_hit_count);
  notCaught->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, exception_hit_count);
  CHECK_EQ(1, uncaught_exception_hit_count);
  CHECK_EQ(1, message_callback_count);

  // Break on exception using JavaScript
  DebugEventCounterClear();
  MessageCallbackCountClear();
  ChangeBreakOnExceptionFromJS(true, false);
  caught->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, exception_hit_count);
  CHECK_EQ(0, uncaught_exception_hit_count);
  CHECK_EQ(0, message_callback_count);
  notCaught->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, exception_hit_count);
  CHECK_EQ(1, uncaught_exception_hit_count);
  CHECK_EQ(1, message_callback_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
  v8::V8::RemoveMessageListeners(MessageCallbackCount);
}


// Test break on exception from compiler errors. When compiling using
// v8::Script::Compile there is no JavaScript stack whereas when compiling using
// eval there are JavaScript frames.
TEST(BreakOnCompileException) {
  v8::HandleScope scope;
  DebugLocalContext env;

  v8::internal::Top::TraceException(false);

  // Create a function for checking the function when hitting a break point.
  frame_count = CompileFunction(&env, frame_count_source, "frame_count");

  v8::V8::AddMessageListener(MessageCallbackCount);
  v8::Debug::SetDebugEventListener(DebugEventCounter);

  DebugEventCounterClear();
  MessageCallbackCountClear();

  // Check initial state.
  CHECK_EQ(0, exception_hit_count);
  CHECK_EQ(0, uncaught_exception_hit_count);
  CHECK_EQ(0, message_callback_count);
  CHECK_EQ(-1, last_js_stack_height);

  // Throws SyntaxError: Unexpected end of input
  v8::Script::Compile(v8::String::New("+++"));
  CHECK_EQ(1, exception_hit_count);
  CHECK_EQ(1, uncaught_exception_hit_count);
  CHECK_EQ(1, message_callback_count);
  CHECK_EQ(0, last_js_stack_height);  // No JavaScript stack.

  // Throws SyntaxError: Unexpected identifier
  v8::Script::Compile(v8::String::New("x x"));
  CHECK_EQ(2, exception_hit_count);
  CHECK_EQ(2, uncaught_exception_hit_count);
  CHECK_EQ(2, message_callback_count);
  CHECK_EQ(0, last_js_stack_height);  // No JavaScript stack.

  // Throws SyntaxError: Unexpected end of input
  v8::Script::Compile(v8::String::New("eval('+++')"))->Run();
  CHECK_EQ(3, exception_hit_count);
  CHECK_EQ(3, uncaught_exception_hit_count);
  CHECK_EQ(3, message_callback_count);
  CHECK_EQ(1, last_js_stack_height);

  // Throws SyntaxError: Unexpected identifier
  v8::Script::Compile(v8::String::New("eval('x x')"))->Run();
  CHECK_EQ(4, exception_hit_count);
  CHECK_EQ(4, uncaught_exception_hit_count);
  CHECK_EQ(4, message_callback_count);
  CHECK_EQ(1, last_js_stack_height);
}


TEST(StepWithException) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Create a function for checking the function when hitting a break point.
  frame_function_name = CompileFunction(&env,
                                        frame_function_name_source,
                                        "frame_function_name");

  // Register a debug event listener which steps and counts.
  v8::Debug::SetDebugEventListener(DebugEventStepSequence);

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
  v8::Local<v8::Function> a = CompileFunction(&env, src, "a");
  SetBreakPoint(a, 0);
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "aa";
  a->Call(env->Global(), 0, NULL);
  CHECK_EQ(strlen(expected_step_sequence), break_point_hit_count);

  // Step through invocation of b + c.
  v8::Local<v8::Function> b = CompileFunction(&env, src, "b");
  SetBreakPoint(b, 0);
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "bcc";
  b->Call(env->Global(), 0, NULL);
  CHECK_EQ(strlen(expected_step_sequence), break_point_hit_count);

  // Step through invocation of d + e.
  v8::Local<v8::Function> d = CompileFunction(&env, src, "d");
  SetBreakPoint(d, 0);
  ChangeBreakOnException(false, true);
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "dded";
  d->Call(env->Global(), 0, NULL);
  CHECK_EQ(strlen(expected_step_sequence), break_point_hit_count);

  // Step through invocation of d + e now with break on caught exceptions.
  ChangeBreakOnException(true, true);
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "ddeed";
  d->Call(env->Global(), 0, NULL);
  CHECK_EQ(strlen(expected_step_sequence), break_point_hit_count);

  // Step through invocation of f + g + h.
  v8::Local<v8::Function> f = CompileFunction(&env, src, "f");
  SetBreakPoint(f, 0);
  ChangeBreakOnException(false, true);
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "ffghf";
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(strlen(expected_step_sequence), break_point_hit_count);

  // Step through invocation of f + g + h now with break on caught exceptions.
  ChangeBreakOnException(true, true);
  step_action = StepIn;
  break_point_hit_count = 0;
  expected_step_sequence = "ffghhf";
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(strlen(expected_step_sequence), break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


TEST(DebugBreak) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // This test should be run with option --verify-heap. As --verify-heap is
  // only available in debug mode only check for it in that case.
#ifdef DEBUG
  CHECK(v8::internal::FLAG_verify_heap);
#endif

  // Register a debug event listener which sets the break flag and counts.
  v8::Debug::SetDebugEventListener(DebugEventBreak);

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
  v8::Handle<v8::Value> argv[] = { v8::Number::New(1),
                                   v8::Number::New(1),
                                   v8::Number::New(1),
                                   v8::Number::New(1) };

  // Call all functions to make sure that they are compiled.
  f0->Call(env->Global(), 0, NULL);
  f1->Call(env->Global(), 0, NULL);
  f2->Call(env->Global(), 0, NULL);
  f3->Call(env->Global(), 0, NULL);

  // Set the debug break flag.
  v8::Debug::DebugBreak();

  // Call all functions with different argument count.
  break_point_hit_count = 0;
  for (unsigned int i = 0; i < ARRAY_SIZE(argv); i++) {
    f0->Call(env->Global(), i, argv);
    f1->Call(env->Global(), i, argv);
    f2->Call(env->Global(), i, argv);
    f3->Call(env->Global(), i, argv);
  }

  // One break for each function called.
  CHECK_EQ(4 * ARRAY_SIZE(argv), break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Test to ensure that JavaScript code keeps running while the debug break
// through the stack limit flag is set but breaks are disabled.
TEST(DisableBreak) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Register a debug event listener which sets the break flag and counts.
  v8::Debug::SetDebugEventListener(DebugEventCounter);

  // Create a function for testing stepping.
  const char* src = "function f() {g()};function g(){i=0; while(i<10){i++}}";
  v8::Local<v8::Function> f = CompileFunction(&env, src, "f");

  // Set the debug break flag.
  v8::Debug::DebugBreak();

  // Call all functions with different argument count.
  break_point_hit_count = 0;
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);

  {
    v8::Debug::DebugBreak();
    v8::internal::DisableBreak disable_break(true);
    f->Call(env->Global(), 0, NULL);
    CHECK_EQ(1, break_point_hit_count);
  }

  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


static v8::Handle<v8::Array> NamedEnum(const v8::AccessorInfo&) {
  v8::Handle<v8::Array> result = v8::Array::New(3);
  result->Set(v8::Integer::New(0), v8::String::New("a"));
  result->Set(v8::Integer::New(1), v8::String::New("b"));
  result->Set(v8::Integer::New(2), v8::String::New("c"));
  return result;
}


static v8::Handle<v8::Array> IndexedEnum(const v8::AccessorInfo&) {
  v8::Handle<v8::Array> result = v8::Array::New(2);
  result->Set(v8::Integer::New(0), v8::Number::New(1));
  result->Set(v8::Integer::New(1), v8::Number::New(10));
  return result;
}


static v8::Handle<v8::Value> NamedGetter(v8::Local<v8::String> name,
                                         const v8::AccessorInfo& info) {
  v8::String::AsciiValue n(name);
  if (strcmp(*n, "a") == 0) {
    return v8::String::New("AA");
  } else if (strcmp(*n, "b") == 0) {
    return v8::String::New("BB");
  } else if (strcmp(*n, "c") == 0) {
    return v8::String::New("CC");
  } else {
    return v8::Undefined();
  }

  return name;
}


static v8::Handle<v8::Value> IndexedGetter(uint32_t index,
                                           const v8::AccessorInfo& info) {
  return v8::Number::New(index + 1);
}


TEST(InterceptorPropertyMirror) {
  // Create a V8 environment with debug access.
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  // Create object with named interceptor.
  v8::Handle<v8::ObjectTemplate> named = v8::ObjectTemplate::New();
  named->SetNamedPropertyHandler(NamedGetter, NULL, NULL, NULL, NamedEnum);
  env->Global()->Set(v8::String::New("intercepted_named"),
                     named->NewInstance());

  // Create object with indexed interceptor.
  v8::Handle<v8::ObjectTemplate> indexed = v8::ObjectTemplate::New();
  indexed->SetIndexedPropertyHandler(IndexedGetter,
                                     NULL,
                                     NULL,
                                     NULL,
                                     IndexedEnum);
  env->Global()->Set(v8::String::New("intercepted_indexed"),
                     indexed->NewInstance());

  // Create object with both named and indexed interceptor.
  v8::Handle<v8::ObjectTemplate> both = v8::ObjectTemplate::New();
  both->SetNamedPropertyHandler(NamedGetter, NULL, NULL, NULL, NamedEnum);
  both->SetIndexedPropertyHandler(IndexedGetter, NULL, NULL, NULL, IndexedEnum);
  env->Global()->Set(v8::String::New("intercepted_both"), both->NewInstance());

  // Get mirrors for the three objects with interceptor.
  CompileRun(
      "named_mirror = debug.MakeMirror(intercepted_named);"
      "indexed_mirror = debug.MakeMirror(intercepted_indexed);"
      "both_mirror = debug.MakeMirror(intercepted_both)");
  CHECK(CompileRun(
       "named_mirror instanceof debug.ObjectMirror")->BooleanValue());
  CHECK(CompileRun(
        "indexed_mirror instanceof debug.ObjectMirror")->BooleanValue());
  CHECK(CompileRun(
        "both_mirror instanceof debug.ObjectMirror")->BooleanValue());

  // Get the property names from the interceptors
  CompileRun(
      "named_names = named_mirror.propertyNames();"
      "indexed_names = indexed_mirror.propertyNames();"
      "both_names = both_mirror.propertyNames()");
  CHECK_EQ(3, CompileRun("named_names.length")->Int32Value());
  CHECK_EQ(2, CompileRun("indexed_names.length")->Int32Value());
  CHECK_EQ(5, CompileRun("both_names.length")->Int32Value());

  // Check the expected number of properties.
  const char* source;
  source = "named_mirror.properties().length";
  CHECK_EQ(3, CompileRun(source)->Int32Value());

  source = "indexed_mirror.properties().length";
  CHECK_EQ(2, CompileRun(source)->Int32Value());

  source = "both_mirror.properties().length";
  CHECK_EQ(5, CompileRun(source)->Int32Value());

  // 1 is PropertyKind.Named;
  source = "both_mirror.properties(1).length";
  CHECK_EQ(3, CompileRun(source)->Int32Value());

  // 2 is PropertyKind.Indexed;
  source = "both_mirror.properties(2).length";
  CHECK_EQ(2, CompileRun(source)->Int32Value());

  // 3 is PropertyKind.Named  | PropertyKind.Indexed;
  source = "both_mirror.properties(3).length";
  CHECK_EQ(5, CompileRun(source)->Int32Value());

  // Get the interceptor properties for the object with only named interceptor.
  CompileRun("named_values = named_mirror.properties()");

  // Check that the properties are interceptor properties.
  for (int i = 0; i < 3; i++) {
    EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
    OS::SNPrintF(buffer,
                 "named_values[%d] instanceof debug.PropertyMirror", i);
    CHECK(CompileRun(buffer.start())->BooleanValue());

    // 4 is PropertyType.Interceptor
    OS::SNPrintF(buffer, "named_values[%d].propertyType()", i);
    CHECK_EQ(4, CompileRun(buffer.start())->Int32Value());

    OS::SNPrintF(buffer, "named_values[%d].isNative()", i);
    CHECK(CompileRun(buffer.start())->BooleanValue());
  }

  // Get the interceptor properties for the object with only indexed
  // interceptor.
  CompileRun("indexed_values = indexed_mirror.properties()");

  // Check that the properties are interceptor properties.
  for (int i = 0; i < 2; i++) {
    EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
    OS::SNPrintF(buffer,
                 "indexed_values[%d] instanceof debug.PropertyMirror", i);
    CHECK(CompileRun(buffer.start())->BooleanValue());
  }

  // Get the interceptor properties for the object with both types of
  // interceptors.
  CompileRun("both_values = both_mirror.properties()");

  // Check that the properties are interceptor properties.
  for (int i = 0; i < 5; i++) {
    EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> buffer;
    OS::SNPrintF(buffer, "both_values[%d] instanceof debug.PropertyMirror", i);
    CHECK(CompileRun(buffer.start())->BooleanValue());
  }

  // Check the property names.
  source = "both_values[0].name() == 'a'";
  CHECK(CompileRun(source)->BooleanValue());

  source = "both_values[1].name() == 'b'";
  CHECK(CompileRun(source)->BooleanValue());

  source = "both_values[2].name() == 'c'";
  CHECK(CompileRun(source)->BooleanValue());

  source = "both_values[3].name() == 1";
  CHECK(CompileRun(source)->BooleanValue());

  source = "both_values[4].name() == 10";
  CHECK(CompileRun(source)->BooleanValue());
}


TEST(HiddenPrototypePropertyMirror) {
  // Create a V8 environment with debug access.
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  v8::Handle<v8::FunctionTemplate> t0 = v8::FunctionTemplate::New();
  t0->InstanceTemplate()->Set(v8::String::New("x"), v8::Number::New(0));
  v8::Handle<v8::FunctionTemplate> t1 = v8::FunctionTemplate::New();
  t1->SetHiddenPrototype(true);
  t1->InstanceTemplate()->Set(v8::String::New("y"), v8::Number::New(1));
  v8::Handle<v8::FunctionTemplate> t2 = v8::FunctionTemplate::New();
  t2->SetHiddenPrototype(true);
  t2->InstanceTemplate()->Set(v8::String::New("z"), v8::Number::New(2));
  v8::Handle<v8::FunctionTemplate> t3 = v8::FunctionTemplate::New();
  t3->InstanceTemplate()->Set(v8::String::New("u"), v8::Number::New(3));

  // Create object and set them on the global object.
  v8::Handle<v8::Object> o0 = t0->GetFunction()->NewInstance();
  env->Global()->Set(v8::String::New("o0"), o0);
  v8::Handle<v8::Object> o1 = t1->GetFunction()->NewInstance();
  env->Global()->Set(v8::String::New("o1"), o1);
  v8::Handle<v8::Object> o2 = t2->GetFunction()->NewInstance();
  env->Global()->Set(v8::String::New("o2"), o2);
  v8::Handle<v8::Object> o3 = t3->GetFunction()->NewInstance();
  env->Global()->Set(v8::String::New("o3"), o3);

  // Get mirrors for the four objects.
  CompileRun(
      "o0_mirror = debug.MakeMirror(o0);"
      "o1_mirror = debug.MakeMirror(o1);"
      "o2_mirror = debug.MakeMirror(o2);"
      "o3_mirror = debug.MakeMirror(o3)");
  CHECK(CompileRun("o0_mirror instanceof debug.ObjectMirror")->BooleanValue());
  CHECK(CompileRun("o1_mirror instanceof debug.ObjectMirror")->BooleanValue());
  CHECK(CompileRun("o2_mirror instanceof debug.ObjectMirror")->BooleanValue());
  CHECK(CompileRun("o3_mirror instanceof debug.ObjectMirror")->BooleanValue());

  // Check that each object has one property.
  CHECK_EQ(1, CompileRun(
              "o0_mirror.propertyNames().length")->Int32Value());
  CHECK_EQ(1, CompileRun(
              "o1_mirror.propertyNames().length")->Int32Value());
  CHECK_EQ(1, CompileRun(
              "o2_mirror.propertyNames().length")->Int32Value());
  CHECK_EQ(1, CompileRun(
              "o3_mirror.propertyNames().length")->Int32Value());

  // Set o1 as prototype for o0. o1 has the hidden prototype flag so all
  // properties on o1 should be seen on o0.
  o0->Set(v8::String::New("__proto__"), o1);
  CHECK_EQ(2, CompileRun(
              "o0_mirror.propertyNames().length")->Int32Value());
  CHECK_EQ(0, CompileRun(
              "o0_mirror.property('x').value().value()")->Int32Value());
  CHECK_EQ(1, CompileRun(
              "o0_mirror.property('y').value().value()")->Int32Value());

  // Set o2 as prototype for o0 (it will end up after o1 as o1 has the hidden
  // prototype flag. o2 also has the hidden prototype flag so all properties
  // on o2 should be seen on o0 as well as properties on o1.
  o0->Set(v8::String::New("__proto__"), o2);
  CHECK_EQ(3, CompileRun(
              "o0_mirror.propertyNames().length")->Int32Value());
  CHECK_EQ(0, CompileRun(
              "o0_mirror.property('x').value().value()")->Int32Value());
  CHECK_EQ(1, CompileRun(
              "o0_mirror.property('y').value().value()")->Int32Value());
  CHECK_EQ(2, CompileRun(
              "o0_mirror.property('z').value().value()")->Int32Value());

  // Set o3 as prototype for o0 (it will end up after o1 and o2 as both o1 and
  // o2 has the hidden prototype flag. o3 does not have the hidden prototype
  // flag so properties on o3 should not be seen on o0 whereas the properties
  // from o1 and o2 should still be seen on o0.
  // Final prototype chain: o0 -> o1 -> o2 -> o3
  // Hidden prototypes:           ^^    ^^
  o0->Set(v8::String::New("__proto__"), o3);
  CHECK_EQ(3, CompileRun(
              "o0_mirror.propertyNames().length")->Int32Value());
  CHECK_EQ(1, CompileRun(
              "o3_mirror.propertyNames().length")->Int32Value());
  CHECK_EQ(0, CompileRun(
              "o0_mirror.property('x').value().value()")->Int32Value());
  CHECK_EQ(1, CompileRun(
              "o0_mirror.property('y').value().value()")->Int32Value());
  CHECK_EQ(2, CompileRun(
              "o0_mirror.property('z').value().value()")->Int32Value());
  CHECK(CompileRun("o0_mirror.property('u').isUndefined()")->BooleanValue());

  // The prototype (__proto__) for o0 should be o3 as o1 and o2 are hidden.
  CHECK(CompileRun("o0_mirror.protoObject() == o3_mirror")->BooleanValue());
}


static v8::Handle<v8::Value> ProtperyXNativeGetter(
    v8::Local<v8::String> property, const v8::AccessorInfo& info) {
  return v8::Integer::New(10);
}


TEST(NativeGetterPropertyMirror) {
  // Create a V8 environment with debug access.
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  v8::Handle<v8::String> name = v8::String::New("x");
  // Create object with named accessor.
  v8::Handle<v8::ObjectTemplate> named = v8::ObjectTemplate::New();
  named->SetAccessor(name, &ProtperyXNativeGetter, NULL,
      v8::Handle<v8::Value>(), v8::DEFAULT, v8::None);

  // Create object with named property getter.
  env->Global()->Set(v8::String::New("instance"), named->NewInstance());
  CHECK_EQ(10, CompileRun("instance.x")->Int32Value());

  // Get mirror for the object with property getter.
  CompileRun("instance_mirror = debug.MakeMirror(instance);");
  CHECK(CompileRun(
      "instance_mirror instanceof debug.ObjectMirror")->BooleanValue());

  CompileRun("named_names = instance_mirror.propertyNames();");
  CHECK_EQ(1, CompileRun("named_names.length")->Int32Value());
  CHECK(CompileRun("named_names[0] == 'x'")->BooleanValue());
  CHECK(CompileRun(
      "instance_mirror.property('x').value().isNumber()")->BooleanValue());
  CHECK(CompileRun(
      "instance_mirror.property('x').value().value() == 10")->BooleanValue());
}


static v8::Handle<v8::Value> ProtperyXNativeGetterThrowingError(
    v8::Local<v8::String> property, const v8::AccessorInfo& info) {
  return CompileRun("throw new Error('Error message');");
}


TEST(NativeGetterThrowingErrorPropertyMirror) {
  // Create a V8 environment with debug access.
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  v8::Handle<v8::String> name = v8::String::New("x");
  // Create object with named accessor.
  v8::Handle<v8::ObjectTemplate> named = v8::ObjectTemplate::New();
  named->SetAccessor(name, &ProtperyXNativeGetterThrowingError, NULL,
      v8::Handle<v8::Value>(), v8::DEFAULT, v8::None);

  // Create object with named property getter.
  env->Global()->Set(v8::String::New("instance"), named->NewInstance());

  // Get mirror for the object with property getter.
  CompileRun("instance_mirror = debug.MakeMirror(instance);");
  CHECK(CompileRun(
      "instance_mirror instanceof debug.ObjectMirror")->BooleanValue());
  CompileRun("named_names = instance_mirror.propertyNames();");
  CHECK_EQ(1, CompileRun("named_names.length")->Int32Value());
  CHECK(CompileRun("named_names[0] == 'x'")->BooleanValue());
  CHECK(CompileRun(
      "instance_mirror.property('x').value().isError()")->BooleanValue());

  // Check that the message is that passed to the Error constructor.
  CHECK(CompileRun(
      "instance_mirror.property('x').value().message() == 'Error message'")->
          BooleanValue());
}



// Multithreaded tests of JSON debugger protocol

// Support classes

// Copies a C string to a 16-bit string.  Does not check for buffer overflow.
// Does not use the V8 engine to convert strings, so it can be used
// in any thread.  Returns the length of the string.
int AsciiToUtf16(const char* input_buffer, uint16_t* output_buffer) {
  int i;
  for (i = 0; input_buffer[i] != '\0'; ++i) {
    // ASCII does not use chars > 127, but be careful anyway.
    output_buffer[i] = static_cast<unsigned char>(input_buffer[i]);
  }
  output_buffer[i] = 0;
  return i;
}

// Copies a 16-bit string to a C string by dropping the high byte of
// each character.  Does not check for buffer overflow.
// Can be used in any thread.  Requires string length as an input.
int Utf16ToAscii(const uint16_t* input_buffer, int length,
                 char* output_buffer) {
  for (int i = 0; i < length; ++i) {
    output_buffer[i] = static_cast<char>(input_buffer[i]);
  }
  output_buffer[length] = '\0';
  return length;
}

// Provides synchronization between k threads, where k is an input to the
// constructor.  The Wait() call blocks a thread until it is called for the
// k'th time, then all calls return.  Each ThreadBarrier object can only
// be used once.
class ThreadBarrier {
 public:
  explicit ThreadBarrier(int num_threads);
  ~ThreadBarrier();
  void Wait();
 private:
  int num_threads_;
  int num_blocked_;
  v8::internal::Mutex* lock_;
  v8::internal::Semaphore* sem_;
  bool invalid_;
};

ThreadBarrier::ThreadBarrier(int num_threads)
    : num_threads_(num_threads), num_blocked_(0) {
  lock_ = OS::CreateMutex();
  sem_ = OS::CreateSemaphore(0);
  invalid_ = false;  // A barrier may only be used once.  Then it is invalid.
}

// Do not call, due to race condition with Wait().
// Could be resolved with Pthread condition variables.
ThreadBarrier::~ThreadBarrier() {
  lock_->Lock();
  delete lock_;
  delete sem_;
}

void ThreadBarrier::Wait() {
  lock_->Lock();
  CHECK(!invalid_);
  if (num_blocked_ == num_threads_ - 1) {
    // Signal and unblock all waiting threads.
    for (int i = 0; i < num_threads_ - 1; ++i) {
      sem_->Signal();
    }
    invalid_ = true;
    printf("BARRIER\n\n");
    fflush(stdout);
    lock_->Unlock();
  } else {  // Wait for the semaphore.
    ++num_blocked_;
    lock_->Unlock();  // Potential race condition with destructor because
    sem_->Wait();  // these two lines are not atomic.
  }
}

// A set containing enough barriers and semaphores for any of the tests.
class Barriers {
 public:
  Barriers();
  void Initialize();
  ThreadBarrier barrier_1;
  ThreadBarrier barrier_2;
  ThreadBarrier barrier_3;
  ThreadBarrier barrier_4;
  ThreadBarrier barrier_5;
  v8::internal::Semaphore* semaphore_1;
  v8::internal::Semaphore* semaphore_2;
};

Barriers::Barriers() : barrier_1(2), barrier_2(2),
    barrier_3(2), barrier_4(2), barrier_5(2) {}

void Barriers::Initialize() {
  semaphore_1 = OS::CreateSemaphore(0);
  semaphore_2 = OS::CreateSemaphore(0);
}


// We match parts of the message to decide if it is a break message.
bool IsBreakEventMessage(char *message) {
  const char* type_event = "\"type\":\"event\"";
  const char* event_break = "\"event\":\"break\"";
  // Does the message contain both type:event and event:break?
  return strstr(message, type_event) != NULL &&
         strstr(message, event_break) != NULL;
}


// We match parts of the message to decide if it is a exception message.
bool IsExceptionEventMessage(char *message) {
  const char* type_event = "\"type\":\"event\"";
  const char* event_exception = "\"event\":\"exception\"";
  // Does the message contain both type:event and event:exception?
  return strstr(message, type_event) != NULL &&
      strstr(message, event_exception) != NULL;
}


// We match the message wether it is an evaluate response message.
bool IsEvaluateResponseMessage(char* message) {
  const char* type_response = "\"type\":\"response\"";
  const char* command_evaluate = "\"command\":\"evaluate\"";
  // Does the message contain both type:response and command:evaluate?
  return strstr(message, type_response) != NULL &&
         strstr(message, command_evaluate) != NULL;
}


// We match parts of the message to get evaluate result int value.
int GetEvaluateIntResult(char *message) {
  const char* value = "\"value\":";
  char* pos = strstr(message, value);
  if (pos == NULL) {
    return -1;
  }
  int res = -1;
  res = atoi(pos + strlen(value));
  return res;
}


// We match parts of the message to get hit breakpoint id.
int GetBreakpointIdFromBreakEventMessage(char *message) {
  const char* breakpoints = "\"breakpoints\":[";
  char* pos = strstr(message, breakpoints);
  if (pos == NULL) {
    return -1;
  }
  int res = -1;
  res = atoi(pos + strlen(breakpoints));
  return res;
}


/* Test MessageQueues */
/* Tests the message queues that hold debugger commands and
 * response messages to the debugger.  Fills queues and makes
 * them grow.
 */
Barriers message_queue_barriers;

// This is the debugger thread, that executes no v8 calls except
// placing JSON debugger commands in the queue.
class MessageQueueDebuggerThread : public v8::internal::Thread {
 public:
  void Run();
};

static void MessageHandler(const uint16_t* message, int length,
                           v8::Debug::ClientData* client_data) {
  static char print_buffer[1000];
  Utf16ToAscii(message, length, print_buffer);
  if (IsBreakEventMessage(print_buffer)) {
    // Lets test script wait until break occurs to send commands.
    // Signals when a break is reported.
    message_queue_barriers.semaphore_2->Signal();
  }

  // Allow message handler to block on a semaphore, to test queueing of
  // messages while blocked.
  message_queue_barriers.semaphore_1->Wait();
}

void MessageQueueDebuggerThread::Run() {
  const int kBufferSize = 1000;
  uint16_t buffer_1[kBufferSize];
  uint16_t buffer_2[kBufferSize];
  const char* command_1 =
      "{\"seq\":117,"
       "\"type\":\"request\","
       "\"command\":\"evaluate\","
       "\"arguments\":{\"expression\":\"1+2\"}}";
  const char* command_2 =
    "{\"seq\":118,"
     "\"type\":\"request\","
     "\"command\":\"evaluate\","
     "\"arguments\":{\"expression\":\"1+a\"}}";
  const char* command_3 =
    "{\"seq\":119,"
     "\"type\":\"request\","
     "\"command\":\"evaluate\","
     "\"arguments\":{\"expression\":\"c.d * b\"}}";
  const char* command_continue =
    "{\"seq\":106,"
     "\"type\":\"request\","
     "\"command\":\"continue\"}";
  const char* command_single_step =
    "{\"seq\":107,"
     "\"type\":\"request\","
     "\"command\":\"continue\","
     "\"arguments\":{\"stepaction\":\"next\"}}";

  /* Interleaved sequence of actions by the two threads:*/
  // Main thread compiles and runs source_1
  message_queue_barriers.semaphore_1->Signal();
  message_queue_barriers.barrier_1.Wait();
  // Post 6 commands, filling the command queue and making it expand.
  // These calls return immediately, but the commands stay on the queue
  // until the execution of source_2.
  // Note: AsciiToUtf16 executes before SendCommand, so command is copied
  // to buffer before buffer is sent to SendCommand.
  v8::Debug::SendCommand(buffer_1, AsciiToUtf16(command_1, buffer_1));
  v8::Debug::SendCommand(buffer_2, AsciiToUtf16(command_2, buffer_2));
  v8::Debug::SendCommand(buffer_2, AsciiToUtf16(command_3, buffer_2));
  v8::Debug::SendCommand(buffer_2, AsciiToUtf16(command_3, buffer_2));
  v8::Debug::SendCommand(buffer_2, AsciiToUtf16(command_3, buffer_2));
  message_queue_barriers.barrier_2.Wait();
  // Main thread compiles and runs source_2.
  // Queued commands are executed at the start of compilation of source_2(
  // beforeCompile event).
  // Free the message handler to process all the messages from the queue. 7
  // messages are expected: 2 afterCompile events and 5 responses.
  // All the commands added so far will fail to execute as long as call stack
  // is empty on beforeCompile event.
  for (int i = 0; i < 6 ; ++i) {
    message_queue_barriers.semaphore_1->Signal();
  }
  message_queue_barriers.barrier_3.Wait();
  // Main thread compiles and runs source_3.
  // Don't stop in the afterCompile handler.
  message_queue_barriers.semaphore_1->Signal();
  // source_3 includes a debugger statement, which causes a break event.
  // Wait on break event from hitting "debugger" statement
  message_queue_barriers.semaphore_2->Wait();
  // These should execute after the "debugger" statement in source_2
  v8::Debug::SendCommand(buffer_1, AsciiToUtf16(command_1, buffer_1));
  v8::Debug::SendCommand(buffer_2, AsciiToUtf16(command_2, buffer_2));
  v8::Debug::SendCommand(buffer_2, AsciiToUtf16(command_3, buffer_2));
  v8::Debug::SendCommand(buffer_2, AsciiToUtf16(command_single_step, buffer_2));
  // Run after 2 break events, 4 responses.
  for (int i = 0; i < 6 ; ++i) {
    message_queue_barriers.semaphore_1->Signal();
  }
  // Wait on break event after a single step executes.
  message_queue_barriers.semaphore_2->Wait();
  v8::Debug::SendCommand(buffer_1, AsciiToUtf16(command_2, buffer_1));
  v8::Debug::SendCommand(buffer_2, AsciiToUtf16(command_continue, buffer_2));
  // Run after 2 responses.
  for (int i = 0; i < 2 ; ++i) {
    message_queue_barriers.semaphore_1->Signal();
  }
  // Main thread continues running source_3 to end, waits for this thread.
}

MessageQueueDebuggerThread message_queue_debugger_thread;

// This thread runs the v8 engine.
TEST(MessageQueues) {
  // Create a V8 environment
  v8::HandleScope scope;
  DebugLocalContext env;
  message_queue_barriers.Initialize();
  v8::Debug::SetMessageHandler(MessageHandler);
  message_queue_debugger_thread.Start();

  const char* source_1 = "a = 3; b = 4; c = new Object(); c.d = 5;";
  const char* source_2 = "e = 17;";
  const char* source_3 = "a = 4; debugger; a = 5; a = 6; a = 7;";

  // See MessageQueueDebuggerThread::Run for interleaved sequence of
  // API calls and events in the two threads.
  CompileRun(source_1);
  message_queue_barriers.barrier_1.Wait();
  message_queue_barriers.barrier_2.Wait();
  CompileRun(source_2);
  message_queue_barriers.barrier_3.Wait();
  CompileRun(source_3);
  message_queue_debugger_thread.Join();
  fflush(stdout);
}


class TestClientData : public v8::Debug::ClientData {
 public:
  TestClientData() {
    constructor_call_counter++;
  }
  virtual ~TestClientData() {
    destructor_call_counter++;
  }

  static void ResetCounters() {
    constructor_call_counter = 0;
    destructor_call_counter = 0;
  }

  static int constructor_call_counter;
  static int destructor_call_counter;
};

int TestClientData::constructor_call_counter = 0;
int TestClientData::destructor_call_counter = 0;


// Tests that MessageQueue doesn't destroy client data when expands and
// does destroy when it dies.
TEST(MessageQueueExpandAndDestroy) {
  TestClientData::ResetCounters();
  { // Create a scope for the queue.
    CommandMessageQueue queue(1);
    queue.Put(CommandMessage::New(Vector<uint16_t>::empty(),
                                  new TestClientData()));
    queue.Put(CommandMessage::New(Vector<uint16_t>::empty(),
                                  new TestClientData()));
    queue.Put(CommandMessage::New(Vector<uint16_t>::empty(),
                                  new TestClientData()));
    CHECK_EQ(0, TestClientData::destructor_call_counter);
    queue.Get().Dispose();
    CHECK_EQ(1, TestClientData::destructor_call_counter);
    queue.Put(CommandMessage::New(Vector<uint16_t>::empty(),
                                  new TestClientData()));
    queue.Put(CommandMessage::New(Vector<uint16_t>::empty(),
                                  new TestClientData()));
    queue.Put(CommandMessage::New(Vector<uint16_t>::empty(),
                                  new TestClientData()));
    queue.Put(CommandMessage::New(Vector<uint16_t>::empty(),
                                  new TestClientData()));
    queue.Put(CommandMessage::New(Vector<uint16_t>::empty(),
                                  new TestClientData()));
    CHECK_EQ(1, TestClientData::destructor_call_counter);
    queue.Get().Dispose();
    CHECK_EQ(2, TestClientData::destructor_call_counter);
  }
  // All the client data should be destroyed when the queue is destroyed.
  CHECK_EQ(TestClientData::destructor_call_counter,
           TestClientData::destructor_call_counter);
}


static int handled_client_data_instances_count = 0;
static void MessageHandlerCountingClientData(
    const v8::Debug::Message& message) {
  if (message.GetClientData() != NULL) {
    handled_client_data_instances_count++;
  }
}


// Tests that all client data passed to the debugger are sent to the handler.
TEST(SendClientDataToHandler) {
  // Create a V8 environment
  v8::HandleScope scope;
  DebugLocalContext env;
  TestClientData::ResetCounters();
  handled_client_data_instances_count = 0;
  v8::Debug::SetMessageHandler2(MessageHandlerCountingClientData);
  const char* source_1 = "a = 3; b = 4; c = new Object(); c.d = 5;";
  const int kBufferSize = 1000;
  uint16_t buffer[kBufferSize];
  const char* command_1 =
      "{\"seq\":117,"
       "\"type\":\"request\","
       "\"command\":\"evaluate\","
       "\"arguments\":{\"expression\":\"1+2\"}}";
  const char* command_2 =
    "{\"seq\":118,"
     "\"type\":\"request\","
     "\"command\":\"evaluate\","
     "\"arguments\":{\"expression\":\"1+a\"}}";
  const char* command_continue =
    "{\"seq\":106,"
     "\"type\":\"request\","
     "\"command\":\"continue\"}";

  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_1, buffer),
                         new TestClientData());
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_2, buffer), NULL);
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_2, buffer),
                         new TestClientData());
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_2, buffer),
                         new TestClientData());
  // All the messages will be processed on beforeCompile event.
  CompileRun(source_1);
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_continue, buffer));
  CHECK_EQ(3, TestClientData::constructor_call_counter);
  CHECK_EQ(TestClientData::constructor_call_counter,
           handled_client_data_instances_count);
  CHECK_EQ(TestClientData::constructor_call_counter,
           TestClientData::destructor_call_counter);
}


/* Test ThreadedDebugging */
/* This test interrupts a running infinite loop that is
 * occupying the v8 thread by a break command from the
 * debugger thread.  It then changes the value of a
 * global object, to make the loop terminate.
 */

Barriers threaded_debugging_barriers;

class V8Thread : public v8::internal::Thread {
 public:
  void Run();
};

class DebuggerThread : public v8::internal::Thread {
 public:
  void Run();
};


static v8::Handle<v8::Value> ThreadedAtBarrier1(const v8::Arguments& args) {
  threaded_debugging_barriers.barrier_1.Wait();
  return v8::Undefined();
}


static void ThreadedMessageHandler(const v8::Debug::Message& message) {
  static char print_buffer[1000];
  v8::String::Value json(message.GetJSON());
  Utf16ToAscii(*json, json.length(), print_buffer);
  if (IsBreakEventMessage(print_buffer)) {
    threaded_debugging_barriers.barrier_2.Wait();
  }
}


void V8Thread::Run() {
  const char* source =
      "flag = true;\n"
      "function bar( new_value ) {\n"
      "  flag = new_value;\n"
      "  return \"Return from bar(\" + new_value + \")\";\n"
      "}\n"
      "\n"
      "function foo() {\n"
      "  var x = 1;\n"
      "  while ( flag == true ) {\n"
      "    if ( x == 1 ) {\n"
      "      ThreadedAtBarrier1();\n"
      "    }\n"
      "    x = x + 1;\n"
      "  }\n"
      "}\n"
      "\n"
      "foo();\n";

  v8::HandleScope scope;
  DebugLocalContext env;
  v8::Debug::SetMessageHandler2(&ThreadedMessageHandler);
  v8::Handle<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();
  global_template->Set(v8::String::New("ThreadedAtBarrier1"),
                       v8::FunctionTemplate::New(ThreadedAtBarrier1));
  v8::Handle<v8::Context> context = v8::Context::New(NULL, global_template);
  v8::Context::Scope context_scope(context);

  CompileRun(source);
}

void DebuggerThread::Run() {
  const int kBufSize = 1000;
  uint16_t buffer[kBufSize];

  const char* command_1 = "{\"seq\":102,"
      "\"type\":\"request\","
      "\"command\":\"evaluate\","
      "\"arguments\":{\"expression\":\"bar(false)\"}}";
  const char* command_2 = "{\"seq\":103,"
      "\"type\":\"request\","
      "\"command\":\"continue\"}";

  threaded_debugging_barriers.barrier_1.Wait();
  v8::Debug::DebugBreak();
  threaded_debugging_barriers.barrier_2.Wait();
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_1, buffer));
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_2, buffer));
}

DebuggerThread debugger_thread;
V8Thread v8_thread;

TEST(ThreadedDebugging) {
  // Create a V8 environment
  threaded_debugging_barriers.Initialize();

  v8_thread.Start();
  debugger_thread.Start();

  v8_thread.Join();
  debugger_thread.Join();
}

/* Test RecursiveBreakpoints */
/* In this test, the debugger evaluates a function with a breakpoint, after
 * hitting a breakpoint in another function.  We do this with both values
 * of the flag enabling recursive breakpoints, and verify that the second
 * breakpoint is hit when enabled, and missed when disabled.
 */

class BreakpointsV8Thread : public v8::internal::Thread {
 public:
  void Run();
};

class BreakpointsDebuggerThread : public v8::internal::Thread {
 public:
  void Run();
};


Barriers* breakpoints_barriers;
int break_event_breakpoint_id;
int evaluate_int_result;

static void BreakpointsMessageHandler(const v8::Debug::Message& message) {
  static char print_buffer[1000];
  v8::String::Value json(message.GetJSON());
  Utf16ToAscii(*json, json.length(), print_buffer);

  if (IsBreakEventMessage(print_buffer)) {
    break_event_breakpoint_id =
        GetBreakpointIdFromBreakEventMessage(print_buffer);
    breakpoints_barriers->semaphore_1->Signal();
  } else if (IsEvaluateResponseMessage(print_buffer)) {
    evaluate_int_result = GetEvaluateIntResult(print_buffer);
    breakpoints_barriers->semaphore_1->Signal();
  }
}


void BreakpointsV8Thread::Run() {
  const char* source_1 = "var y_global = 3;\n"
    "function cat( new_value ) {\n"
    "  var x = new_value;\n"
    "  y_global = y_global + 4;\n"
    "  x = 3 * x + 1;\n"
    "  y_global = y_global + 5;\n"
    "  return x;\n"
    "}\n"
    "\n"
    "function dog() {\n"
    "  var x = 1;\n"
    "  x = y_global;"
    "  var z = 3;"
    "  x += 100;\n"
    "  return x;\n"
    "}\n"
    "\n";
  const char* source_2 = "cat(17);\n"
    "cat(19);\n";

  v8::HandleScope scope;
  DebugLocalContext env;
  v8::Debug::SetMessageHandler2(&BreakpointsMessageHandler);

  CompileRun(source_1);
  breakpoints_barriers->barrier_1.Wait();
  breakpoints_barriers->barrier_2.Wait();
  CompileRun(source_2);
}


void BreakpointsDebuggerThread::Run() {
  const int kBufSize = 1000;
  uint16_t buffer[kBufSize];

  const char* command_1 = "{\"seq\":101,"
      "\"type\":\"request\","
      "\"command\":\"setbreakpoint\","
      "\"arguments\":{\"type\":\"function\",\"target\":\"cat\",\"line\":3}}";
  const char* command_2 = "{\"seq\":102,"
      "\"type\":\"request\","
      "\"command\":\"setbreakpoint\","
      "\"arguments\":{\"type\":\"function\",\"target\":\"dog\",\"line\":3}}";
  const char* command_3 = "{\"seq\":103,"
      "\"type\":\"request\","
      "\"command\":\"evaluate\","
      "\"arguments\":{\"expression\":\"dog()\",\"disable_break\":false}}";
  const char* command_4 = "{\"seq\":104,"
      "\"type\":\"request\","
      "\"command\":\"evaluate\","
      "\"arguments\":{\"expression\":\"x + 1\",\"disable_break\":true}}";
  const char* command_5 = "{\"seq\":105,"
      "\"type\":\"request\","
      "\"command\":\"continue\"}";
  const char* command_6 = "{\"seq\":106,"
      "\"type\":\"request\","
      "\"command\":\"continue\"}";
  const char* command_7 = "{\"seq\":107,"
     "\"type\":\"request\","
     "\"command\":\"evaluate\","
     "\"arguments\":{\"expression\":\"dog()\",\"disable_break\":true}}";
  const char* command_8 = "{\"seq\":108,"
      "\"type\":\"request\","
      "\"command\":\"continue\"}";


  // v8 thread initializes, runs source_1
  breakpoints_barriers->barrier_1.Wait();
  // 1:Set breakpoint in cat() (will get id 1).
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_1, buffer));
  // 2:Set breakpoint in dog() (will get id 2).
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_2, buffer));
  breakpoints_barriers->barrier_2.Wait();
  // V8 thread starts compiling source_2.
  // Automatic break happens, to run queued commands
  // breakpoints_barriers->semaphore_1->Wait();
  // Commands 1 through 3 run, thread continues.
  // v8 thread runs source_2 to breakpoint in cat().
  // message callback receives break event.
  breakpoints_barriers->semaphore_1->Wait();
  // Must have hit breakpoint #1.
  CHECK_EQ(1, break_event_breakpoint_id);
  // 4:Evaluate dog() (which has a breakpoint).
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_3, buffer));
  // V8 thread hits breakpoint in dog().
  breakpoints_barriers->semaphore_1->Wait();  // wait for break event
  // Must have hit breakpoint #2.
  CHECK_EQ(2, break_event_breakpoint_id);
  // 5:Evaluate (x + 1).
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_4, buffer));
  // Evaluate (x + 1) finishes.
  breakpoints_barriers->semaphore_1->Wait();
  // Must have result 108.
  CHECK_EQ(108, evaluate_int_result);
  // 6:Continue evaluation of dog().
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_5, buffer));
  // Evaluate dog() finishes.
  breakpoints_barriers->semaphore_1->Wait();
  // Must have result 107.
  CHECK_EQ(107, evaluate_int_result);
  // 7:Continue evaluation of source_2, finish cat(17), hit breakpoint
  // in cat(19).
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_6, buffer));
  // Message callback gets break event.
  breakpoints_barriers->semaphore_1->Wait();  // wait for break event
  // Must have hit breakpoint #1.
  CHECK_EQ(1, break_event_breakpoint_id);
  // 8: Evaluate dog() with breaks disabled.
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_7, buffer));
  // Evaluate dog() finishes.
  breakpoints_barriers->semaphore_1->Wait();
  // Must have result 116.
  CHECK_EQ(116, evaluate_int_result);
  // 9: Continue evaluation of source2, reach end.
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_8, buffer));
}

BreakpointsDebuggerThread breakpoints_debugger_thread;
BreakpointsV8Thread breakpoints_v8_thread;

TEST(RecursiveBreakpoints) {
  i::FLAG_debugger_auto_break = true;

  // Create a V8 environment
  Barriers stack_allocated_breakpoints_barriers;
  stack_allocated_breakpoints_barriers.Initialize();
  breakpoints_barriers = &stack_allocated_breakpoints_barriers;

  breakpoints_v8_thread.Start();
  breakpoints_debugger_thread.Start();

  breakpoints_v8_thread.Join();
  breakpoints_debugger_thread.Join();
}


static void DummyDebugEventListener(v8::DebugEvent event,
                                    v8::Handle<v8::Object> exec_state,
                                    v8::Handle<v8::Object> event_data,
                                    v8::Handle<v8::Value> data) {
}


TEST(SetDebugEventListenerOnUninitializedVM) {
  v8::Debug::SetDebugEventListener(DummyDebugEventListener);
}


static void DummyMessageHandler(const v8::Debug::Message& message) {
}


TEST(SetMessageHandlerOnUninitializedVM) {
  v8::Debug::SetMessageHandler2(DummyMessageHandler);
}


TEST(DebugBreakOnUninitializedVM) {
  v8::Debug::DebugBreak();
}


TEST(SendCommandToUninitializedVM) {
  const char* dummy_command = "{}";
  uint16_t dummy_buffer[80];
  int dummy_length = AsciiToUtf16(dummy_command, dummy_buffer);
  v8::Debug::SendCommand(dummy_buffer, dummy_length);
}


// Source for a JavaScript function which returns the data parameter of a
// function called in the context of the debugger. If no data parameter is
// passed it throws an exception.
static const char* debugger_call_with_data_source =
    "function debugger_call_with_data(exec_state, data) {"
    "  if (data) return data;"
    "  throw 'No data!'"
    "}";
v8::Handle<v8::Function> debugger_call_with_data;


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
v8::Handle<v8::Function> debugger_call_with_closure;

// Function to retrieve the number of JavaScript frames by calling a JavaScript
// in the debugger.
static v8::Handle<v8::Value> CheckFrameCount(const v8::Arguments& args) {
  CHECK(v8::Debug::Call(frame_count)->IsNumber());
  CHECK_EQ(args[0]->Int32Value(),
           v8::Debug::Call(frame_count)->Int32Value());
  return v8::Undefined();
}


// Function to retrieve the source line of the top JavaScript frame by calling a
// JavaScript function in the debugger.
static v8::Handle<v8::Value> CheckSourceLine(const v8::Arguments& args) {
  CHECK(v8::Debug::Call(frame_source_line)->IsNumber());
  CHECK_EQ(args[0]->Int32Value(),
           v8::Debug::Call(frame_source_line)->Int32Value());
  return v8::Undefined();
}


// Function to test passing an additional parameter to a JavaScript function
// called in the debugger. It also tests that functions called in the debugger
// can throw exceptions.
static v8::Handle<v8::Value> CheckDataParameter(const v8::Arguments& args) {
  v8::Handle<v8::String> data = v8::String::New("Test");
  CHECK(v8::Debug::Call(debugger_call_with_data, data)->IsString());

  CHECK(v8::Debug::Call(debugger_call_with_data).IsEmpty());
  CHECK(v8::Debug::Call(debugger_call_with_data).IsEmpty());

  v8::TryCatch catcher;
  v8::Debug::Call(debugger_call_with_data);
  CHECK(catcher.HasCaught());
  CHECK(catcher.Exception()->IsString());

  return v8::Undefined();
}


// Function to test using a JavaScript with closure in the debugger.
static v8::Handle<v8::Value> CheckClosure(const v8::Arguments& args) {
  CHECK(v8::Debug::Call(debugger_call_with_closure)->IsNumber());
  CHECK_EQ(3, v8::Debug::Call(debugger_call_with_closure)->Int32Value());
  return v8::Undefined();
}


// Test functions called through the debugger.
TEST(CallFunctionInDebugger) {
  // Create and enter a context with the functions CheckFrameCount,
  // CheckSourceLine and CheckDataParameter installed.
  v8::HandleScope scope;
  v8::Handle<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();
  global_template->Set(v8::String::New("CheckFrameCount"),
                       v8::FunctionTemplate::New(CheckFrameCount));
  global_template->Set(v8::String::New("CheckSourceLine"),
                       v8::FunctionTemplate::New(CheckSourceLine));
  global_template->Set(v8::String::New("CheckDataParameter"),
                       v8::FunctionTemplate::New(CheckDataParameter));
  global_template->Set(v8::String::New("CheckClosure"),
                       v8::FunctionTemplate::New(CheckClosure));
  v8::Handle<v8::Context> context = v8::Context::New(NULL, global_template);
  v8::Context::Scope context_scope(context);

  // Compile a function for checking the number of JavaScript frames.
  v8::Script::Compile(v8::String::New(frame_count_source))->Run();
  frame_count = v8::Local<v8::Function>::Cast(
      context->Global()->Get(v8::String::New("frame_count")));

  // Compile a function for returning the source line for the top frame.
  v8::Script::Compile(v8::String::New(frame_source_line_source))->Run();
  frame_source_line = v8::Local<v8::Function>::Cast(
      context->Global()->Get(v8::String::New("frame_source_line")));

  // Compile a function returning the data parameter.
  v8::Script::Compile(v8::String::New(debugger_call_with_data_source))->Run();
  debugger_call_with_data = v8::Local<v8::Function>::Cast(
      context->Global()->Get(v8::String::New("debugger_call_with_data")));

  // Compile a function capturing closure.
  debugger_call_with_closure = v8::Local<v8::Function>::Cast(
      v8::Script::Compile(
          v8::String::New(debugger_call_with_closure_source))->Run());

  // Calling a function through the debugger returns undefined if there are no
  // JavaScript frames.
  CHECK(v8::Debug::Call(frame_count)->IsUndefined());
  CHECK(v8::Debug::Call(frame_source_line)->IsUndefined());
  CHECK(v8::Debug::Call(debugger_call_with_data)->IsUndefined());

  // Test that the number of frames can be retrieved.
  v8::Script::Compile(v8::String::New("CheckFrameCount(1)"))->Run();
  v8::Script::Compile(v8::String::New("function f() {"
                                      "  CheckFrameCount(2);"
                                      "}; f()"))->Run();

  // Test that the source line can be retrieved.
  v8::Script::Compile(v8::String::New("CheckSourceLine(0)"))->Run();
  v8::Script::Compile(v8::String::New("function f() {\n"
                                      "  CheckSourceLine(1)\n"
                                      "  CheckSourceLine(2)\n"
                                      "  CheckSourceLine(3)\n"
                                      "}; f()"))->Run();

  // Test that a parameter can be passed to a function called in the debugger.
  v8::Script::Compile(v8::String::New("CheckDataParameter()"))->Run();

  // Test that a function with closure can be run in the debugger.
  v8::Script::Compile(v8::String::New("CheckClosure()"))->Run();


  // Test that the source line is correct when there is a line offset.
  v8::ScriptOrigin origin(v8::String::New("test"),
                          v8::Integer::New(7));
  v8::Script::Compile(v8::String::New("CheckSourceLine(7)"), &origin)->Run();
  v8::Script::Compile(v8::String::New("function f() {\n"
                                      "  CheckSourceLine(8)\n"
                                      "  CheckSourceLine(9)\n"
                                      "  CheckSourceLine(10)\n"
                                      "}; f()"), &origin)->Run();
}


// Debugger message handler which counts the number of breaks.
static void SendContinueCommand();
static void MessageHandlerBreakPointHitCount(
    const v8::Debug::Message& message) {
  if (message.IsEvent() && message.GetEvent() == v8::Break) {
    // Count the number of breaks.
    break_point_hit_count++;

    SendContinueCommand();
  }
}


// Test that clearing the debug event listener actually clears all break points
// and related information.
TEST(DebuggerUnload) {
  DebugLocalContext env;

  // Check debugger is unloaded before it is used.
  CheckDebuggerUnloaded();

  // Set a debug event listener.
  break_point_hit_count = 0;
  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());
  {
    v8::HandleScope scope;
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
    foo->Call(env->Global(), 0, NULL);
    CHECK_EQ(2, break_point_hit_count);
    bar->Call(env->Global(), 0, NULL);
    CHECK_EQ(4, break_point_hit_count);
  }

  // Remove the debug event listener without clearing breakpoints. Do this
  // outside a handle scope.
  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded(true);

  // Now set a debug message handler.
  break_point_hit_count = 0;
  v8::Debug::SetMessageHandler2(MessageHandlerBreakPointHitCount);
  {
    v8::HandleScope scope;

    // Get the test functions again.
    v8::Local<v8::Function> foo =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("foo")));
    v8::Local<v8::Function> bar =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("foo")));

    foo->Call(env->Global(), 0, NULL);
    CHECK_EQ(0, break_point_hit_count);

    // Set break points and run again.
    SetBreakPoint(foo, 0);
    SetBreakPoint(foo, 4);
    foo->Call(env->Global(), 0, NULL);
    CHECK_EQ(2, break_point_hit_count);
  }

  // Remove the debug message handler without clearing breakpoints. Do this
  // outside a handle scope.
  v8::Debug::SetMessageHandler2(NULL);
  CheckDebuggerUnloaded(true);
}


// Sends continue command to the debugger.
static void SendContinueCommand() {
  const int kBufferSize = 1000;
  uint16_t buffer[kBufferSize];
  const char* command_continue =
    "{\"seq\":0,"
     "\"type\":\"request\","
     "\"command\":\"continue\"}";

  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_continue, buffer));
}


// Debugger message handler which counts the number of times it is called.
static int message_handler_hit_count = 0;
static void MessageHandlerHitCount(const v8::Debug::Message& message) {
  message_handler_hit_count++;

  static char print_buffer[1000];
  v8::String::Value json(message.GetJSON());
  Utf16ToAscii(*json, json.length(), print_buffer);
  if (IsExceptionEventMessage(print_buffer)) {
    // Send a continue command for exception events.
    SendContinueCommand();
  }
}


// Test clearing the debug message handler.
TEST(DebuggerClearMessageHandler) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Check debugger is unloaded before it is used.
  CheckDebuggerUnloaded();

  // Set a debug message handler.
  v8::Debug::SetMessageHandler2(MessageHandlerHitCount);

  // Run code to throw a unhandled exception. This should end up in the message
  // handler.
  CompileRun("throw 1");

  // The message handler should be called.
  CHECK_GT(message_handler_hit_count, 0);

  // Clear debug message handler.
  message_handler_hit_count = 0;
  v8::Debug::SetMessageHandler(NULL);

  // Run code to throw a unhandled exception. This should end up in the message
  // handler.
  CompileRun("throw 1");

  // The message handler should not be called more.
  CHECK_EQ(0, message_handler_hit_count);

  CheckDebuggerUnloaded(true);
}


// Debugger message handler which clears the message handler while active.
static void MessageHandlerClearingMessageHandler(
    const v8::Debug::Message& message) {
  message_handler_hit_count++;

  // Clear debug message handler.
  v8::Debug::SetMessageHandler(NULL);
}


// Test clearing the debug message handler while processing a debug event.
TEST(DebuggerClearMessageHandlerWhileActive) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Check debugger is unloaded before it is used.
  CheckDebuggerUnloaded();

  // Set a debug message handler.
  v8::Debug::SetMessageHandler2(MessageHandlerClearingMessageHandler);

  // Run code to throw a unhandled exception. This should end up in the message
  // handler.
  CompileRun("throw 1");

  // The message handler should be called.
  CHECK_EQ(1, message_handler_hit_count);

  CheckDebuggerUnloaded(true);
}


/* Test DebuggerHostDispatch */
/* In this test, the debugger waits for a command on a breakpoint
 * and is dispatching host commands while in the infinite loop.
 */

class HostDispatchV8Thread : public v8::internal::Thread {
 public:
  void Run();
};

class HostDispatchDebuggerThread : public v8::internal::Thread {
 public:
  void Run();
};

Barriers* host_dispatch_barriers;

static void HostDispatchMessageHandler(const v8::Debug::Message& message) {
  static char print_buffer[1000];
  v8::String::Value json(message.GetJSON());
  Utf16ToAscii(*json, json.length(), print_buffer);
}


static void HostDispatchDispatchHandler() {
  host_dispatch_barriers->semaphore_1->Signal();
}


void HostDispatchV8Thread::Run() {
  const char* source_1 = "var y_global = 3;\n"
    "function cat( new_value ) {\n"
    "  var x = new_value;\n"
    "  y_global = 4;\n"
    "  x = 3 * x + 1;\n"
    "  y_global = 5;\n"
    "  return x;\n"
    "}\n"
    "\n";
  const char* source_2 = "cat(17);\n";

  v8::HandleScope scope;
  DebugLocalContext env;

  // Setup message and host dispatch handlers.
  v8::Debug::SetMessageHandler2(HostDispatchMessageHandler);
  v8::Debug::SetHostDispatchHandler(HostDispatchDispatchHandler, 10 /* ms */);

  CompileRun(source_1);
  host_dispatch_barriers->barrier_1.Wait();
  host_dispatch_barriers->barrier_2.Wait();
  CompileRun(source_2);
}


void HostDispatchDebuggerThread::Run() {
  const int kBufSize = 1000;
  uint16_t buffer[kBufSize];

  const char* command_1 = "{\"seq\":101,"
      "\"type\":\"request\","
      "\"command\":\"setbreakpoint\","
      "\"arguments\":{\"type\":\"function\",\"target\":\"cat\",\"line\":3}}";
  const char* command_2 = "{\"seq\":102,"
      "\"type\":\"request\","
      "\"command\":\"continue\"}";

  // v8 thread initializes, runs source_1
  host_dispatch_barriers->barrier_1.Wait();
  // 1: Set breakpoint in cat().
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_1, buffer));

  host_dispatch_barriers->barrier_2.Wait();
  // v8 thread starts compiling source_2.
  // Break happens, to run queued commands and host dispatches.
  // Wait for host dispatch to be processed.
  host_dispatch_barriers->semaphore_1->Wait();
  // 2: Continue evaluation
  v8::Debug::SendCommand(buffer, AsciiToUtf16(command_2, buffer));
}

HostDispatchDebuggerThread host_dispatch_debugger_thread;
HostDispatchV8Thread host_dispatch_v8_thread;


TEST(DebuggerHostDispatch) {
  i::FLAG_debugger_auto_break = true;

  // Create a V8 environment
  Barriers stack_allocated_host_dispatch_barriers;
  stack_allocated_host_dispatch_barriers.Initialize();
  host_dispatch_barriers = &stack_allocated_host_dispatch_barriers;

  host_dispatch_v8_thread.Start();
  host_dispatch_debugger_thread.Start();

  host_dispatch_v8_thread.Join();
  host_dispatch_debugger_thread.Join();
}


TEST(DebuggerAgent) {
  // Make sure these ports is not used by other tests to allow tests to run in
  // parallel.
  const int kPort1 = 5858;
  const int kPort2 = 5857;
  const int kPort3 = 5856;

  // Make a string with the port2 number.
  const int kPortBufferLen = 6;
  char port2_str[kPortBufferLen];
  OS::SNPrintF(i::Vector<char>(port2_str, kPortBufferLen), "%d", kPort2);

  bool ok;

  // Initialize the socket library.
  i::Socket::Setup();

  // Test starting and stopping the agent without any client connection.
  i::Debugger::StartAgent("test", kPort1);
  i::Debugger::StopAgent();

  // Test starting the agent, connecting a client and shutting down the agent
  // with the client connected.
  ok = i::Debugger::StartAgent("test", kPort2);
  CHECK(ok);
  i::Debugger::WaitForAgent();
  i::Socket* client = i::OS::CreateSocket();
  ok = client->Connect("localhost", port2_str);
  CHECK(ok);
  i::Debugger::StopAgent();
  delete client;

  // Test starting and stopping the agent with the required port already
  // occoupied.
  i::Socket* server = i::OS::CreateSocket();
  server->Bind(kPort3);

  i::Debugger::StartAgent("test", kPort3);
  i::Debugger::StopAgent();

  delete server;
}


class DebuggerAgentProtocolServerThread : public i::Thread {
 public:
  explicit DebuggerAgentProtocolServerThread(int port)
      : port_(port), server_(NULL), client_(NULL),
        listening_(OS::CreateSemaphore(0)) {
  }
  ~DebuggerAgentProtocolServerThread() {
    // Close both sockets.
    delete client_;
    delete server_;
    delete listening_;
  }

  void Run();
  void WaitForListening() { listening_->Wait(); }
  char* body() { return *body_; }

 private:
  int port_;
  i::SmartPointer<char> body_;
  i::Socket* server_;  // Server socket used for bind/accept.
  i::Socket* client_;  // Single client connection used by the test.
  i::Semaphore* listening_;  // Signalled when the server is in listen mode.
};


void DebuggerAgentProtocolServerThread::Run() {
  bool ok;

  // Create the server socket and bind it to the requested port.
  server_ = i::OS::CreateSocket();
  CHECK(server_ != NULL);
  ok = server_->Bind(port_);
  CHECK(ok);

  // Listen for new connections.
  ok = server_->Listen(1);
  CHECK(ok);
  listening_->Signal();

  // Accept a connection.
  client_ = server_->Accept();
  CHECK(client_ != NULL);

  // Receive a debugger agent protocol message.
  i::DebuggerAgentUtil::ReceiveMessage(client_);
}


TEST(DebuggerAgentProtocolOverflowHeader) {
  // Make sure this port is not used by other tests to allow tests to run in
  // parallel.
  const int kPort = 5860;
  static const char* kLocalhost = "localhost";

  // Make a string with the port number.
  const int kPortBufferLen = 6;
  char port_str[kPortBufferLen];
  OS::SNPrintF(i::Vector<char>(port_str, kPortBufferLen), "%d", kPort);

  // Initialize the socket library.
  i::Socket::Setup();

  // Create a socket server to receive a debugger agent message.
  DebuggerAgentProtocolServerThread* server =
      new DebuggerAgentProtocolServerThread(kPort);
  server->Start();
  server->WaitForListening();

  // Connect.
  i::Socket* client = i::OS::CreateSocket();
  CHECK(client != NULL);
  bool ok = client->Connect(kLocalhost, port_str);
  CHECK(ok);

  // Send headers which overflow the receive buffer.
  static const int kBufferSize = 1000;
  char buffer[kBufferSize];

  // Long key and short value: XXXX....XXXX:0\r\n.
  for (int i = 0; i < kBufferSize - 4; i++) {
    buffer[i] = 'X';
  }
  buffer[kBufferSize - 4] = ':';
  buffer[kBufferSize - 3] = '0';
  buffer[kBufferSize - 2] = '\r';
  buffer[kBufferSize - 1] = '\n';
  client->Send(buffer, kBufferSize);

  // Short key and long value: X:XXXX....XXXX\r\n.
  buffer[0] = 'X';
  buffer[1] = ':';
  for (int i = 2; i < kBufferSize - 2; i++) {
    buffer[i] = 'X';
  }
  buffer[kBufferSize - 2] = '\r';
  buffer[kBufferSize - 1] = '\n';
  client->Send(buffer, kBufferSize);

  // Add empty body to request.
  const char* content_length_zero_header = "Content-Length:0\r\n";
  client->Send(content_length_zero_header, strlen(content_length_zero_header));
  client->Send("\r\n", 2);

  // Wait until data is received.
  server->Join();

  // Check for empty body.
  CHECK(server->body() == NULL);

  // Close the client before the server to avoid TIME_WAIT issues.
  client->Shutdown();
  delete client;
  delete server;
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


TEST(DebugGetLoadedScripts) {
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  EmptyExternalStringResource source_ext_str;
  v8::Local<v8::String> source = v8::String::NewExternal(&source_ext_str);
  v8::Handle<v8::Script> evil_script = v8::Script::Compile(source);
  Handle<i::ExternalTwoByteString> i_source(
      i::ExternalTwoByteString::cast(*v8::Utils::OpenHandle(*source)));
  // This situation can happen if source was an external string disposed
  // by its owner.
  i_source->set_resource(0);

  bool allow_natives_syntax = i::FLAG_allow_natives_syntax;
  i::FLAG_allow_natives_syntax = true;
  CompileRun(
      "var scripts = %DebugGetLoadedScripts();"
      "var count = scripts.length;"
      "for (var i = 0; i < count; ++i) {"
      "  scripts[i].line_ends;"
      "}");
  // Must not crash while accessing line_ends.
  i::FLAG_allow_natives_syntax = allow_natives_syntax;

  // Some scripts are retrieved - at least the number of native scripts.
  CHECK_GT((*env)->Global()->Get(v8::String::New("count"))->Int32Value(), 8);
}


// Test script break points set on lines.
TEST(ScriptNameAndData) {
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();

  // Create functions for retrieving script name and data for the function on
  // the top frame when hitting a break point.
  frame_script_name = CompileFunction(&env,
                                      frame_script_name_source,
                                      "frame_script_name");
  frame_script_data = CompileFunction(&env,
                                      frame_script_data_source,
                                      "frame_script_data");

  v8::Debug::SetDebugEventListener(DebugEventBreakPointHitCount,
                                   v8::Undefined());

  // Test function source.
  v8::Local<v8::String> script = v8::String::New(
    "function f() {\n"
    "  debugger;\n"
    "}\n");

  v8::ScriptOrigin origin1 = v8::ScriptOrigin(v8::String::New("name"));
  v8::Handle<v8::Script> script1 = v8::Script::Compile(script, &origin1);
  script1->SetData(v8::String::New("data"));
  script1->Run();
  v8::Local<v8::Function> f;
  f = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));

  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(1, break_point_hit_count);
  CHECK_EQ("name", last_script_name_hit);
  CHECK_EQ("data", last_script_data_hit);

  // Compile the same script again without setting data. As the compilation
  // cache is disabled when debugging expect the data to be missing.
  v8::Script::Compile(script, &origin1)->Run();
  f = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, break_point_hit_count);
  CHECK_EQ("name", last_script_name_hit);
  CHECK_EQ("", last_script_data_hit);  // Undefined results in empty string.

  v8::Local<v8::String> data_obj_source = v8::String::New(
    "({ a: 'abc',\n"
    "  b: 123,\n"
    "  toString: function() { return this.a + ' ' + this.b; }\n"
    "})\n");
  v8::Local<v8::Value> data_obj = v8::Script::Compile(data_obj_source)->Run();
  v8::ScriptOrigin origin2 = v8::ScriptOrigin(v8::String::New("new name"));
  v8::Handle<v8::Script> script2 = v8::Script::Compile(script, &origin2);
  script2->Run();
  script2->SetData(data_obj);
  f = v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(3, break_point_hit_count);
  CHECK_EQ("new name", last_script_name_hit);
  CHECK_EQ("abc 123", last_script_data_hit);
}


static v8::Persistent<v8::Context> expected_context;
static v8::Handle<v8::Value> expected_context_data;


// Check that the expected context is the one generating the debug event.
static void ContextCheckMessageHandler(const v8::Debug::Message& message) {
  CHECK(message.GetEventContext() == expected_context);
  CHECK(message.GetEventContext()->GetData()->StrictEquals(
      expected_context_data));
  message_handler_hit_count++;

  static char print_buffer[1000];
  v8::String::Value json(message.GetJSON());
  Utf16ToAscii(*json, json.length(), print_buffer);

  // Send a continue command for break events.
  if (IsBreakEventMessage(print_buffer)) {
    SendContinueCommand();
  }
}


// Test which creates two contexts and sets different embedder data on each.
// Checks that this data is set correctly and that when the debug message
// handler is called the expected context is the one active.
TEST(ContextData) {
  v8::HandleScope scope;

  v8::Debug::SetMessageHandler2(ContextCheckMessageHandler);

  // Create two contexts.
  v8::Persistent<v8::Context> context_1;
  v8::Persistent<v8::Context> context_2;
  v8::Handle<v8::ObjectTemplate> global_template =
      v8::Handle<v8::ObjectTemplate>();
  v8::Handle<v8::Value> global_object = v8::Handle<v8::Value>();
  context_1 = v8::Context::New(NULL, global_template, global_object);
  context_2 = v8::Context::New(NULL, global_template, global_object);

  // Default data value is undefined.
  CHECK(context_1->GetData()->IsUndefined());
  CHECK(context_2->GetData()->IsUndefined());

  // Set and check different data values.
  v8::Handle<v8::Value> data_1 = v8::Number::New(1);
  v8::Handle<v8::Value> data_2 = v8::String::New("2");
  context_1->SetData(data_1);
  context_2->SetData(data_2);
  CHECK(context_1->GetData()->StrictEquals(data_1));
  CHECK(context_2->GetData()->StrictEquals(data_2));

  // Simple test function which causes a break.
  const char* source = "function f() { debugger; }";

  // Enter and run function in the first context.
  {
    v8::Context::Scope context_scope(context_1);
    expected_context = context_1;
    expected_context_data = data_1;
    v8::Local<v8::Function> f = CompileFunction(source, "f");
    f->Call(context_1->Global(), 0, NULL);
  }


  // Enter and run function in the second context.
  {
    v8::Context::Scope context_scope(context_2);
    expected_context = context_2;
    expected_context_data = data_2;
    v8::Local<v8::Function> f = CompileFunction(source, "f");
    f->Call(context_2->Global(), 0, NULL);
  }

  // Two times compile event and two times break event.
  CHECK_GT(message_handler_hit_count, 4);

  v8::Debug::SetMessageHandler2(NULL);
  CheckDebuggerUnloaded();
}


// Debug message handler which issues a debug break when it hits a break event.
static int message_handler_break_hit_count = 0;
static void DebugBreakMessageHandler(const v8::Debug::Message& message) {
  // Schedule a debug break for break events.
  if (message.IsEvent() && message.GetEvent() == v8::Break) {
    message_handler_break_hit_count++;
    if (message_handler_break_hit_count == 1) {
      v8::Debug::DebugBreak();
    }
  }

  // Issue a continue command if this event will not cause the VM to start
  // running.
  if (!message.WillStartRunning()) {
    SendContinueCommand();
  }
}


// Test that a debug break can be scheduled while in a message handler.
TEST(DebugBreakInMessageHandler) {
  v8::HandleScope scope;
  DebugLocalContext env;

  v8::Debug::SetMessageHandler2(DebugBreakMessageHandler);

  // Test functions.
  const char* script = "function f() { debugger; g(); } function g() { }";
  CompileRun(script);
  v8::Local<v8::Function> f =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));
  v8::Local<v8::Function> g =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("g")));

  // Call f then g. The debugger statement in f will casue a break which will
  // cause another break.
  f->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, message_handler_break_hit_count);
  // Calling g will not cause any additional breaks.
  g->Call(env->Global(), 0, NULL);
  CHECK_EQ(2, message_handler_break_hit_count);
}


#ifdef V8_NATIVE_REGEXP
// Debug event handler which gets the function on the top frame and schedules a
// break a number of times.
static void DebugEventDebugBreak(
    v8::DebugEvent event,
    v8::Handle<v8::Object> exec_state,
    v8::Handle<v8::Object> event_data,
    v8::Handle<v8::Value> data) {

  if (event == v8::Break) {
    break_point_hit_count++;

    // Get the name of the top frame function.
    if (!frame_function_name.IsEmpty()) {
      // Get the name of the function.
      const int argc = 1;
      v8::Handle<v8::Value> argv[argc] = { exec_state };
      v8::Handle<v8::Value> result = frame_function_name->Call(exec_state,
                                                               argc, argv);
      if (result->IsUndefined()) {
        last_function_hit[0] = '\0';
      } else {
        CHECK(result->IsString());
        v8::Handle<v8::String> function_name(result->ToString());
        function_name->WriteAscii(last_function_hit);
      }
    }

    // Keep forcing breaks.
    if (break_point_hit_count < 20) {
      v8::Debug::DebugBreak();
    }
  }
}


TEST(RegExpDebugBreak) {
  // This test only applies to native regexps.
  v8::HandleScope scope;
  DebugLocalContext env;

  // Create a function for checking the function when hitting a break point.
  frame_function_name = CompileFunction(&env,
                                        frame_function_name_source,
                                        "frame_function_name");

  // Test RegExp which matches white spaces and comments at the begining of a
  // source line.
  const char* script =
    "var sourceLineBeginningSkip = /^(?:[ \\v\\h]*(?:\\/\\*.*?\\*\\/)*)*/;\n"
    "function f(s) { return s.match(sourceLineBeginningSkip)[0].length; }";

  v8::Local<v8::Function> f = CompileFunction(script, "f");
  const int argc = 1;
  v8::Handle<v8::Value> argv[argc] = { v8::String::New("  /* xxx */ a=0;") };
  v8::Local<v8::Value> result = f->Call(env->Global(), argc, argv);
  CHECK_EQ(12, result->Int32Value());

  v8::Debug::SetDebugEventListener(DebugEventDebugBreak);
  v8::Debug::DebugBreak();
  result = f->Call(env->Global(), argc, argv);

  // Check that there was only one break event. Matching RegExp should not
  // cause Break events.
  CHECK_EQ(1, break_point_hit_count);
  CHECK_EQ("f", last_function_hit);
}
#endif  // V8_NATIVE_REGEXP


// Common part of EvalContextData and NestedBreakEventContextData tests.
static void ExecuteScriptForContextCheck() {
  // Create a context.
  v8::Persistent<v8::Context> context_1;
  v8::Handle<v8::ObjectTemplate> global_template =
      v8::Handle<v8::ObjectTemplate>();
  v8::Handle<v8::Value> global_object = v8::Handle<v8::Value>();
  context_1 = v8::Context::New(NULL, global_template, global_object);

  // Default data value is undefined.
  CHECK(context_1->GetData()->IsUndefined());

  // Set and check a data value.
  v8::Handle<v8::Value> data_1 = v8::Number::New(1);
  context_1->SetData(data_1);
  CHECK(context_1->GetData()->StrictEquals(data_1));

  // Simple test function with eval that causes a break.
  const char* source = "function f() { eval('debugger;'); }";

  // Enter and run function in the context.
  {
    v8::Context::Scope context_scope(context_1);
    expected_context = context_1;
    expected_context_data = data_1;
    v8::Local<v8::Function> f = CompileFunction(source, "f");
    f->Call(context_1->Global(), 0, NULL);
  }
}


// Test which creates a context and sets embedder data on it. Checks that this
// data is set correctly and that when the debug message handler is called for
// break event in an eval statement the expected context is the one returned by
// Message.GetEventContext.
TEST(EvalContextData) {
  v8::HandleScope scope;
  v8::Debug::SetMessageHandler2(ContextCheckMessageHandler);

  ExecuteScriptForContextCheck();

  // One time compile event and one time break event.
  CHECK_GT(message_handler_hit_count, 2);
  v8::Debug::SetMessageHandler2(NULL);
  CheckDebuggerUnloaded();
}


static bool sent_eval = false;
static int break_count = 0;
static int continue_command_send_count = 0;
// Check that the expected context is the one generating the debug event
// including the case of nested break event.
static void DebugEvalContextCheckMessageHandler(
    const v8::Debug::Message& message) {
  CHECK(message.GetEventContext() == expected_context);
  CHECK(message.GetEventContext()->GetData()->StrictEquals(
      expected_context_data));
  message_handler_hit_count++;

  static char print_buffer[1000];
  v8::String::Value json(message.GetJSON());
  Utf16ToAscii(*json, json.length(), print_buffer);

  if (IsBreakEventMessage(print_buffer)) {
    break_count++;
    if (!sent_eval) {
      sent_eval = true;

      const int kBufferSize = 1000;
      uint16_t buffer[kBufferSize];
      const char* eval_command =
        "{\"seq\":0,"
         "\"type\":\"request\","
         "\"command\":\"evaluate\","
         "arguments:{\"expression\":\"debugger;\","
         "\"global\":true,\"disable_break\":false}}";

      // Send evaluate command.
      v8::Debug::SendCommand(buffer, AsciiToUtf16(eval_command, buffer));
      return;
    } else {
      // It's a break event caused by the evaluation request above.
      SendContinueCommand();
      continue_command_send_count++;
    }
  } else if (IsEvaluateResponseMessage(print_buffer) &&
      continue_command_send_count < 2) {
    // Response to the evaluation request. We're still on the breakpoint so
    // send continue.
    SendContinueCommand();
    continue_command_send_count++;
  }
}


// Tests that context returned for break event is correct when the event occurs
// in 'evaluate' debugger request.
TEST(NestedBreakEventContextData) {
  v8::HandleScope scope;
  break_count = 0;
  message_handler_hit_count = 0;
  v8::Debug::SetMessageHandler2(DebugEvalContextCheckMessageHandler);

  ExecuteScriptForContextCheck();

  // One time compile event and two times break event.
  CHECK_GT(message_handler_hit_count, 3);

  // One break from the source and another from the evaluate request.
  CHECK_EQ(break_count, 2);
  v8::Debug::SetMessageHandler2(NULL);
  CheckDebuggerUnloaded();
}


// Debug event listener which counts the script collected events.
int script_collected_count = 0;
static void DebugEventScriptCollectedEvent(v8::DebugEvent event,
                                           v8::Handle<v8::Object> exec_state,
                                           v8::Handle<v8::Object> event_data,
                                           v8::Handle<v8::Value> data) {
  // Count the number of breaks.
  if (event == v8::ScriptCollected) {
    script_collected_count++;
  }
}


// Test that scripts collected are reported through the debug event listener.
TEST(ScriptCollectedEvent) {
  break_point_hit_count = 0;
  script_collected_count = 0;
  v8::HandleScope scope;
  DebugLocalContext env;

  // Request the loaded scripts to initialize the debugger script cache.
  Debug::GetLoadedScripts();

  // Do garbage collection to ensure that only the script in this test will be
  // collected afterwards.
  Heap::CollectAllGarbage(false);

  script_collected_count = 0;
  v8::Debug::SetDebugEventListener(DebugEventScriptCollectedEvent,
                                   v8::Undefined());
  {
    v8::Script::Compile(v8::String::New("eval('a=1')"))->Run();
    v8::Script::Compile(v8::String::New("eval('a=2')"))->Run();
  }

  // Do garbage collection to collect the script above which is no longer
  // referenced.
  Heap::CollectAllGarbage(false);

  CHECK_EQ(2, script_collected_count);

  v8::Debug::SetDebugEventListener(NULL);
  CheckDebuggerUnloaded();
}


// Debug event listener which counts the script collected events.
int script_collected_message_count = 0;
static void ScriptCollectedMessageHandler(const v8::Debug::Message& message) {
  // Count the number of scripts collected.
  if (message.IsEvent() && message.GetEvent() == v8::ScriptCollected) {
    script_collected_message_count++;
    v8::Handle<v8::Context> context = message.GetEventContext();
    CHECK(context.IsEmpty());
  }
}


// Test that GetEventContext doesn't fail and return empty handle for
// ScriptCollected events.
TEST(ScriptCollectedEventContext) {
  script_collected_message_count = 0;
  v8::HandleScope scope;

  { // Scope for the DebugLocalContext.
    DebugLocalContext env;

    // Request the loaded scripts to initialize the debugger script cache.
    Debug::GetLoadedScripts();

    // Do garbage collection to ensure that only the script in this test will be
    // collected afterwards.
    Heap::CollectAllGarbage(false);

    v8::Debug::SetMessageHandler2(ScriptCollectedMessageHandler);
    {
      v8::Script::Compile(v8::String::New("eval('a=1')"))->Run();
      v8::Script::Compile(v8::String::New("eval('a=2')"))->Run();
    }
  }

  // Do garbage collection to collect the script above which is no longer
  // referenced.
  Heap::CollectAllGarbage(false);

  CHECK_EQ(2, script_collected_message_count);

  v8::Debug::SetMessageHandler2(NULL);
}


// Debug event listener which counts the after compile events.
int after_compile_message_count = 0;
static void AfterCompileMessageHandler(const v8::Debug::Message& message) {
  // Count the number of scripts collected.
  if (message.IsEvent()) {
    if (message.GetEvent() == v8::AfterCompile) {
      after_compile_message_count++;
    } else if (message.GetEvent() == v8::Break) {
      SendContinueCommand();
    }
  }
}


// Tests that after compile event is sent as many times as there are scripts
// compiled.
TEST(AfterCompileMessageWhenMessageHandlerIsReset) {
  v8::HandleScope scope;
  DebugLocalContext env;
  after_compile_message_count = 0;
  const char* script = "var a=1";

  v8::Debug::SetMessageHandler2(AfterCompileMessageHandler);
  v8::Script::Compile(v8::String::New(script))->Run();
  v8::Debug::SetMessageHandler2(NULL);

  v8::Debug::SetMessageHandler2(AfterCompileMessageHandler);
  v8::Debug::DebugBreak();
  v8::Script::Compile(v8::String::New(script))->Run();

  // Setting listener to NULL should cause debugger unload.
  v8::Debug::SetMessageHandler2(NULL);
  CheckDebuggerUnloaded();

  // Compilation cache should be disabled when debugger is active.
  CHECK_EQ(2, after_compile_message_count);
}


// Tests that break event is sent when message handler is reset.
TEST(BreakMessageWhenMessageHandlerIsReset) {
  v8::HandleScope scope;
  DebugLocalContext env;
  after_compile_message_count = 0;
  const char* script = "function f() {};";

  v8::Debug::SetMessageHandler2(AfterCompileMessageHandler);
  v8::Script::Compile(v8::String::New(script))->Run();
  v8::Debug::SetMessageHandler2(NULL);

  v8::Debug::SetMessageHandler2(AfterCompileMessageHandler);
  v8::Debug::DebugBreak();
  v8::Local<v8::Function> f =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));
  f->Call(env->Global(), 0, NULL);

  // Setting message handler to NULL should cause debugger unload.
  v8::Debug::SetMessageHandler2(NULL);
  CheckDebuggerUnloaded();

  // Compilation cache should be disabled when debugger is active.
  CHECK_EQ(1, after_compile_message_count);
}


static int exception_event_count = 0;
static void ExceptionMessageHandler(const v8::Debug::Message& message) {
  if (message.IsEvent() && message.GetEvent() == v8::Exception) {
    exception_event_count++;
    SendContinueCommand();
  }
}


// Tests that exception event is sent when message handler is reset.
TEST(ExceptionMessageWhenMessageHandlerIsReset) {
  v8::HandleScope scope;
  DebugLocalContext env;
  exception_event_count = 0;
  const char* script = "function f() {throw new Error()};";

  v8::Debug::SetMessageHandler2(AfterCompileMessageHandler);
  v8::Script::Compile(v8::String::New(script))->Run();
  v8::Debug::SetMessageHandler2(NULL);

  v8::Debug::SetMessageHandler2(ExceptionMessageHandler);
  v8::Local<v8::Function> f =
      v8::Local<v8::Function>::Cast(env->Global()->Get(v8::String::New("f")));
  f->Call(env->Global(), 0, NULL);

  // Setting message handler to NULL should cause debugger unload.
  v8::Debug::SetMessageHandler2(NULL);
  CheckDebuggerUnloaded();

  CHECK_EQ(1, exception_event_count);
}


// Tests after compile event is sent when there are some provisional
// breakpoints out of the scripts lines range.
TEST(ProvisionalBreakpointOnLineOutOfRange) {
  v8::HandleScope scope;
  DebugLocalContext env;
  env.ExposeDebug();
  const char* script = "function f() {};";
  const char* resource_name = "test_resource";

  // Set a couple of provisional breakpoint on lines out of the script lines
  // range.
  int sbp1 = SetScriptBreakPointByNameFromJS(resource_name, 3,
                                             -1 /* no column */);
  int sbp2 = SetScriptBreakPointByNameFromJS(resource_name, 5, 5);

  after_compile_message_count = 0;
  v8::Debug::SetMessageHandler2(AfterCompileMessageHandler);

  v8::ScriptOrigin origin(
      v8::String::New(resource_name),
      v8::Integer::New(10),
      v8::Integer::New(1));
  // Compile a script whose first line number is greater than the breakpoints'
  // lines.
  v8::Script::Compile(v8::String::New(script), &origin)->Run();

  // If the script is compiled successfully there is exactly one after compile
  // event. In case of an exception in debugger code after compile event is not
  // sent.
  CHECK_EQ(1, after_compile_message_count);

  ClearBreakPointFromJS(sbp1);
  ClearBreakPointFromJS(sbp2);
  v8::Debug::SetMessageHandler2(NULL);
}


static void BreakMessageHandler(const v8::Debug::Message& message) {
  if (message.IsEvent() && message.GetEvent() == v8::Break) {
    // Count the number of breaks.
    break_point_hit_count++;

    v8::HandleScope scope;
    v8::Handle<v8::String> json = message.GetJSON();

    SendContinueCommand();
  } else if (message.IsEvent() && message.GetEvent() == v8::AfterCompile) {
    v8::HandleScope scope;

    bool is_debug_break = i::StackGuard::IsDebugBreak();
    // Force DebugBreak flag while serializer is working.
    i::StackGuard::DebugBreak();

    // Force serialization to trigger some internal JS execution.
    v8::Handle<v8::String> json = message.GetJSON();

    // Restore previous state.
    if (is_debug_break) {
      i::StackGuard::DebugBreak();
    } else {
      i::StackGuard::Continue(i::DEBUGBREAK);
    }
  }
}


// Test that if DebugBreak is forced it is ignored when code from
// debug-delay.js is executed.
TEST(NoDebugBreakInAfterCompileMessageHandler) {
  v8::HandleScope scope;
  DebugLocalContext env;

  // Register a debug event listener which sets the break flag and counts.
  v8::Debug::SetMessageHandler2(BreakMessageHandler);

  // Set the debug break flag.
  v8::Debug::DebugBreak();

  // Create a function for testing stepping.
  const char* src = "function f() { eval('var x = 10;'); } ";
  v8::Local<v8::Function> f = CompileFunction(&env, src, "f");

  // There should be only one break event.
  CHECK_EQ(1, break_point_hit_count);

  // Set the debug break flag again.
  v8::Debug::DebugBreak();
  f->Call(env->Global(), 0, NULL);
  // There should be one more break event when the script is evaluated in 'f'.
  CHECK_EQ(2, break_point_hit_count);

  // Get rid of the debug message handler.
  v8::Debug::SetMessageHandler2(NULL);
  CheckDebuggerUnloaded();
}


TEST(GetMirror) {
  v8::HandleScope scope;
  DebugLocalContext env;
  v8::Handle<v8::Value> obj = v8::Debug::GetMirror(v8::String::New("hodja"));
  v8::Handle<v8::Function> run_test = v8::Handle<v8::Function>::Cast(
      v8::Script::New(
          v8::String::New(
              "function runTest(mirror) {"
              "  return mirror.isString() && (mirror.length() == 5);"
              "}"
              ""
              "runTest;"))->Run());
  v8::Handle<v8::Value> result = run_test->Call(env->Global(), 1, &obj);
  CHECK(result->IsTrue());
}
