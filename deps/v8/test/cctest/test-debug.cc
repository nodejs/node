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

#include "src/init/v8.h"

#include "src/api/api-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/debug/debug-interface.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frames.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/snapshot.h"
#include "src/utils/utils.h"
#include "test/cctest/cctest.h"

using ::v8::internal::Handle;
using ::v8::internal::StepNone;  // From StepAction enum
using ::v8::internal::StepIn;  // From StepAction enum
using ::v8::internal::StepNext;  // From StepAction enum
using ::v8::internal::StepOut;  // From StepAction enum

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
static v8::Local<v8::Function> CompileFunction(LocalContext* env,
                                               const char* source,
                                               const char* function_name) {
  return CompileFunction((*env)->GetIsolate(), source, function_name);
}

// Is there any debug info for the function?
static bool HasBreakInfo(v8::Local<v8::Function> fun) {
  Handle<v8::internal::JSFunction> f =
      Handle<v8::internal::JSFunction>::cast(v8::Utils::OpenHandle(*fun));
  Handle<v8::internal::SharedFunctionInfo> shared(f->shared(), f->GetIsolate());
  return shared->HasBreakInfo();
}

// Set a break point in a function with a position relative to function start,
// and return the associated break point number.
static i::Handle<i::BreakPoint> SetBreakPoint(v8::Local<v8::Function> fun,
                                              int position,
                                              const char* condition = nullptr) {
  i::Handle<i::JSFunction> function =
      i::Handle<i::JSFunction>::cast(v8::Utils::OpenHandle(*fun));
  position += function->shared().StartPosition();
  static int break_point_index = 0;
  i::Isolate* isolate = function->GetIsolate();
  i::Handle<i::String> condition_string =
      condition ? isolate->factory()->NewStringFromAsciiChecked(condition)
                : isolate->factory()->empty_string();
  i::Debug* debug = isolate->debug();
  i::Handle<i::BreakPoint> break_point =
      isolate->factory()->NewBreakPoint(++break_point_index, condition_string);

  debug->SetBreakpoint(handle(function->shared(), isolate), break_point,
                       &position);
  return break_point;
}


static void ClearBreakPoint(i::Handle<i::BreakPoint> break_point) {
  v8::internal::Isolate* isolate = CcTest::i_isolate();
  v8::internal::Debug* debug = isolate->debug();
  debug->ClearBreakPoint(break_point);
}


// Change break on exception.
static void ChangeBreakOnException(bool caught, bool uncaught) {
  v8::internal::Debug* debug = CcTest::i_isolate()->debug();
  debug->ChangeBreakOnException(v8::internal::BreakException, caught);
  debug->ChangeBreakOnException(v8::internal::BreakUncaughtException, uncaught);
}


// Prepare to step to next break location.
static void PrepareStep(i::StepAction step_action) {
  v8::internal::Debug* debug = CcTest::i_isolate()->debug();
  debug->PrepareStep(step_action);
}

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
void CheckDebuggerUnloaded() {
  // Check that the debugger context is cleared and that there is no debug
  // information stored for the debugger.
  CHECK(!CcTest::i_isolate()->debug()->debug_info_list_);

  // Collect garbage to ensure weak handles are cleared.
  CcTest::CollectAllGarbage();
  CcTest::CollectAllGarbage();

  // Iterate the heap and check that there are no debugger related objects left.
  HeapObjectIterator iterator(CcTest::heap());
  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    CHECK(!obj.IsDebugInfo());
  }
}


}  // namespace internal
}  // namespace v8


// Check that the debugger has been fully unloaded.
static void CheckDebuggerUnloaded() { v8::internal::CheckDebuggerUnloaded(); }

// --- D e b u g   E v e n t   H a n d l e r s
// ---
// --- The different tests uses a number of debug event handlers.
// ---

// Debug event handler which counts a number of events.
int break_point_hit_count = 0;
int break_point_hit_count_deoptimize = 0;
class DebugEventCounter : public v8::debug::DebugDelegate {
 public:
  void BreakProgramRequested(
      v8::Local<v8::Context>,
      const std::vector<v8::debug::BreakpointId>&) override {
    break_point_hit_count++;
    // Perform a full deoptimization when the specified number of
    // breaks have been hit.
    if (break_point_hit_count == break_point_hit_count_deoptimize) {
      i::Deoptimizer::DeoptimizeAll(CcTest::i_isolate());
    }
    if (step_action_ != StepNone) PrepareStep(step_action_);
  }

  void set_step_action(i::StepAction step_action) {
    step_action_ = step_action;
  }

 private:
  i::StepAction step_action_ = StepNone;
};

// Debug event handler which performs a garbage collection.
class DebugEventBreakPointCollectGarbage : public v8::debug::DebugDelegate {
 public:
  void BreakProgramRequested(
      v8::Local<v8::Context>,
      const std::vector<v8::debug::BreakpointId>&) override {
    // Perform a garbage collection when break point is hit and continue. Based
    // on the number of break points hit either scavenge or mark compact
    // collector is used.
    break_point_hit_count++;
    if (break_point_hit_count % 2 == 0) {
      // Scavenge.
      CcTest::CollectGarbage(v8::internal::NEW_SPACE);
    } else {
      // Mark sweep compact.
      CcTest::CollectAllGarbage();
    }
  }
};

// Debug event handler which re-issues a debug break and calls the garbage
// collector to have the heap verified.
class DebugEventBreak : public v8::debug::DebugDelegate {
 public:
  void BreakProgramRequested(
      v8::Local<v8::Context>,
      const std::vector<v8::debug::BreakpointId>&) override {
    // Count the number of breaks.
    break_point_hit_count++;

    // Run the garbage collector to enforce heap verification if option
    // --verify-heap is set.
    CcTest::CollectGarbage(v8::internal::NEW_SPACE);

    // Set the break flag again to come back here as soon as possible.
    v8::debug::SetBreakOnNextFunctionCall(CcTest::isolate());
  }
};

static void BreakRightNow(v8::Isolate* isolate, void*) {
  v8::debug::BreakRightNow(isolate);
}

// Debug event handler which re-issues a debug break until a limit has been
// reached.
int max_break_point_hit_count = 0;
bool terminate_after_max_break_point_hit = false;
class DebugEventBreakMax : public v8::debug::DebugDelegate {
 public:
  void BreakProgramRequested(
      v8::Local<v8::Context>,
      const std::vector<v8::debug::BreakpointId>&) override {
    v8::Isolate* v8_isolate = CcTest::isolate();
    v8::internal::Isolate* isolate = CcTest::i_isolate();
    if (break_point_hit_count < max_break_point_hit_count) {
      // Count the number of breaks.
      break_point_hit_count++;

      // Set the break flag again to come back here as soon as possible.
      v8_isolate->RequestInterrupt(BreakRightNow, nullptr);

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
};

// --- T h e   A c t u a l   T e s t s

// Test that the debug info in the VM is in sync with the functions being
// debugged.
TEST(DebugInfo) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  // Create a couple of functions for the test.
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){}", "foo");
  v8::Local<v8::Function> bar =
      CompileFunction(&env, "function bar(){}", "bar");
  // Initially no functions are debugged.
  CHECK_EQ(0, v8::internal::GetDebuggedFunctions()->length());
  CHECK(!HasBreakInfo(foo));
  CHECK(!HasBreakInfo(bar));
  EnableDebugger(env->GetIsolate());
  // One function (foo) is debugged.
  i::Handle<i::BreakPoint> bp1 = SetBreakPoint(foo, 0);
  CHECK_EQ(1, v8::internal::GetDebuggedFunctions()->length());
  CHECK(HasBreakInfo(foo));
  CHECK(!HasBreakInfo(bar));
  // Two functions are debugged.
  i::Handle<i::BreakPoint> bp2 = SetBreakPoint(bar, 0);
  CHECK_EQ(2, v8::internal::GetDebuggedFunctions()->length());
  CHECK(HasBreakInfo(foo));
  CHECK(HasBreakInfo(bar));
  // One function (bar) is debugged.
  ClearBreakPoint(bp1);
  CHECK_EQ(1, v8::internal::GetDebuggedFunctions()->length());
  CHECK(!HasBreakInfo(foo));
  CHECK(HasBreakInfo(bar));
  // No functions are debugged.
  ClearBreakPoint(bp2);
  DisableDebugger(env->GetIsolate());
  CHECK_EQ(0, v8::internal::GetDebuggedFunctions()->length());
  CHECK(!HasBreakInfo(foo));
  CHECK(!HasBreakInfo(bar));
}


// Test that a break point can be set at an IC store location.
TEST(BreakPointICStore) {
  break_point_hit_count = 0;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){bar=0;}", "foo");

  // Run without breakpoints.
  foo->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint
  i::Handle<i::BreakPoint> bp = SetBreakPoint(foo, 0);
  foo->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  foo->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

// Test that a break point can be set at an IC store location.
TEST(BreakPointCondition) {
  break_point_hit_count = 0;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  CompileRun("var a = false");
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo() { return 1 }", "foo");
  // Run without breakpoints.
  CompileRun("foo()");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint
  i::Handle<i::BreakPoint> bp = SetBreakPoint(foo, 0, "a == true");
  CompileRun("foo()");
  CHECK_EQ(0, break_point_hit_count);

  CompileRun("a = true");
  CompileRun("foo()");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("foo()");
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

// Test that a break point can be set at an IC load location.
TEST(BreakPointICLoad) {
  break_point_hit_count = 0;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  CompileRunChecked(env->GetIsolate(), "bar=1");
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){var x=bar;}", "foo");

  // Run without breakpoints.
  foo->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  i::Handle<i::BreakPoint> bp = SetBreakPoint(foo, 0);
  foo->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  foo->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


// Test that a break point can be set at an IC call location.
TEST(BreakPointICCall) {
  break_point_hit_count = 0;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  CompileRunChecked(env->GetIsolate(), "function bar(){}");
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){bar();}", "foo");

  // Run without breakpoints.
  foo->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint
  i::Handle<i::BreakPoint> bp = SetBreakPoint(foo, 0);
  foo->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);
  foo->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


// Test that a break point can be set at an IC call location and survive a GC.
TEST(BreakPointICCallWithGC) {
  break_point_hit_count = 0;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  DebugEventBreakPointCollectGarbage delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  CompileRunChecked(env->GetIsolate(), "function bar(){return 1;}");
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){return bar();}", "foo");
  v8::Local<v8::Context> context = env.local();

  // Run without breakpoints.
  CHECK_EQ(1, foo->Call(context, env->Global(), 0, nullptr)
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  i::Handle<i::BreakPoint> bp = SetBreakPoint(foo, 0);
  CHECK_EQ(1, foo->Call(context, env->Global(), 0, nullptr)
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(1, break_point_hit_count);
  CHECK_EQ(1, foo->Call(context, env->Global(), 0, nullptr)
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


// Test that a break point can be set at an IC call location and survive a GC.
TEST(BreakPointConstructCallWithGC) {
  break_point_hit_count = 0;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  DebugEventBreakPointCollectGarbage delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  CompileRunChecked(env->GetIsolate(), "function bar(){ this.x = 1;}");
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){return new bar(1).x;}", "foo");
  v8::Local<v8::Context> context = env.local();

  // Run without breakpoints.
  CHECK_EQ(1, foo->Call(context, env->Global(), 0, nullptr)
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  i::Handle<i::BreakPoint> bp = SetBreakPoint(foo, 0);
  CHECK_EQ(1, foo->Call(context, env->Global(), 0, nullptr)
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(1, break_point_hit_count);
  CHECK_EQ(1, foo->Call(context, env->Global(), 0, nullptr)
                  .ToLocalChecked()
                  ->Int32Value(context)
                  .FromJust());
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


TEST(BreakPointBuiltin) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  v8::Local<v8::Function> builtin;
  i::Handle<i::BreakPoint> bp;

  // === Test simple builtin ===
  break_point_hit_count = 0;
  builtin = CompileRun("String.prototype.repeat").As<v8::Function>();

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0, "this != 1");
  ExpectString("'b'.repeat(10)", "bbbbbbbbbb");
  CHECK_EQ(1, break_point_hit_count);

  ExpectString("'b'.repeat(10)", "bbbbbbbbbb");
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  ExpectString("'b'.repeat(10)", "bbbbbbbbbb");
  CHECK_EQ(2, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointApiIntrinsics) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  v8::Local<v8::Function> builtin;

  // === Test that using API-exposed functions won't trigger breakpoints ===
  {
    v8::Local<v8::Function> weakmap_get =
        CompileRun("WeakMap.prototype.get").As<v8::Function>();
    SetBreakPoint(weakmap_get, 0);
    v8::Local<v8::Function> weakmap_set =
        CompileRun("WeakMap.prototype.set").As<v8::Function>();
    SetBreakPoint(weakmap_set, 0);

    // Run with breakpoint.
    break_point_hit_count = 0;
    CompileRun("var w = new WeakMap(); w.set(w, 1); w.get(w);");
    CHECK_EQ(2, break_point_hit_count);

    break_point_hit_count = 0;
    v8::Local<v8::debug::WeakMap> weakmap =
        v8::debug::WeakMap::New(env->GetIsolate());
    CHECK(!weakmap->Set(env.local(), weakmap, v8_num(1)).IsEmpty());
    CHECK(!weakmap->Get(env.local(), weakmap).IsEmpty());
    CHECK_EQ(0, break_point_hit_count);
  }

  {
    v8::Local<v8::Function> object_to_string =
        CompileRun("Object.prototype.toString").As<v8::Function>();
    SetBreakPoint(object_to_string, 0);

    // Run with breakpoint.
    break_point_hit_count = 0;
    CompileRun("var o = {}; o.toString();");
    CHECK_EQ(1, break_point_hit_count);

    break_point_hit_count = 0;
    v8::Local<v8::Object> object = v8::Object::New(env->GetIsolate());
    CHECK(!object->ObjectProtoToString(env.local()).IsEmpty());
    CHECK_EQ(0, break_point_hit_count);
  }

  {
    v8::Local<v8::Function> map_set =
        CompileRun("Map.prototype.set").As<v8::Function>();
    v8::Local<v8::Function> map_get =
        CompileRun("Map.prototype.get").As<v8::Function>();
    v8::Local<v8::Function> map_has =
        CompileRun("Map.prototype.has").As<v8::Function>();
    v8::Local<v8::Function> map_delete =
        CompileRun("Map.prototype.delete").As<v8::Function>();
    SetBreakPoint(map_set, 0);
    SetBreakPoint(map_get, 0);
    SetBreakPoint(map_has, 0);
    SetBreakPoint(map_delete, 0);

    // Run with breakpoint.
    break_point_hit_count = 0;
    CompileRun(
        "var m = new Map(); m.set(m, 1); m.get(m); m.has(m); m.delete(m);");
    CHECK_EQ(4, break_point_hit_count);

    break_point_hit_count = 0;
    v8::Local<v8::Map> map = v8::Map::New(env->GetIsolate());
    CHECK(!map->Set(env.local(), map, v8_num(1)).IsEmpty());
    CHECK(!map->Get(env.local(), map).IsEmpty());
    CHECK(map->Has(env.local(), map).FromJust());
    CHECK(map->Delete(env.local(), map).FromJust());
    CHECK_EQ(0, break_point_hit_count);
  }

  {
    v8::Local<v8::Function> set_add =
        CompileRun("Set.prototype.add").As<v8::Function>();
    v8::Local<v8::Function> set_get =
        CompileRun("Set.prototype.has").As<v8::Function>();
    v8::Local<v8::Function> set_delete =
        CompileRun("Set.prototype.delete").As<v8::Function>();
    SetBreakPoint(set_add, 0);
    SetBreakPoint(set_get, 0);
    SetBreakPoint(set_delete, 0);

    // Run with breakpoint.
    break_point_hit_count = 0;
    CompileRun("var s = new Set(); s.add(s); s.has(s); s.delete(s);");
    CHECK_EQ(3, break_point_hit_count);

    break_point_hit_count = 0;
    v8::Local<v8::Set> set = v8::Set::New(env->GetIsolate());
    CHECK(!set->Add(env.local(), set).IsEmpty());
    CHECK(set->Has(env.local(), set).FromJust());
    CHECK(set->Delete(env.local(), set).FromJust());
    CHECK_EQ(0, break_point_hit_count);
  }

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointJSBuiltin) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  v8::Local<v8::Function> builtin;
  i::Handle<i::BreakPoint> bp;

  // === Test JS builtin ===
  break_point_hit_count = 0;
  builtin = CompileRun("Array.prototype.sort").As<v8::Function>();

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0);
  CompileRun("[1,2,3].sort()");
  CHECK_EQ(1, break_point_hit_count);

  CompileRun("[1,2,3].sort()");
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("[1,2,3].sort()");
  CHECK_EQ(2, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointBoundBuiltin) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  v8::Local<v8::Function> builtin;
  i::Handle<i::BreakPoint> bp;

  // === Test bound function from a builtin ===
  break_point_hit_count = 0;
  builtin = CompileRun(
                "var boundrepeat = String.prototype.repeat.bind('a');"
                "String.prototype.repeat")
                .As<v8::Function>();
  ExpectString("boundrepeat(10)", "aaaaaaaaaa");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0);
  ExpectString("boundrepeat(10)", "aaaaaaaaaa");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  ExpectString("boundrepeat(10)", "aaaaaaaaaa");
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointConstructorBuiltin) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  v8::Local<v8::Function> builtin;
  i::Handle<i::BreakPoint> bp;

  // === Test Promise constructor ===
  break_point_hit_count = 0;
  builtin = CompileRun("Promise").As<v8::Function>();
  ExpectString("(new Promise(()=>{})).toString()", "[object Promise]");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0, "this != 1");
  ExpectString("(new Promise(()=>{})).toString()", "[object Promise]");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  ExpectString("(new Promise(()=>{})).toString()", "[object Promise]");
  CHECK_EQ(1, break_point_hit_count);

  // === Test Object constructor ===
  break_point_hit_count = 0;
  builtin = CompileRun("Object").As<v8::Function>();
  CompileRun("new Object()");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0);
  CompileRun("new Object()");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("new Object()");
  CHECK_EQ(1, break_point_hit_count);

  // === Test Number constructor ===
  break_point_hit_count = 0;
  builtin = CompileRun("Number").As<v8::Function>();
  CompileRun("new Number()");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0);
  CompileRun("new Number()");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("new Number()");
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointInlinedBuiltin) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  v8::Local<v8::Function> builtin;
  i::Handle<i::BreakPoint> bp;

  // === Test inlined builtin ===
  break_point_hit_count = 0;
  builtin = CompileRun("Math.sin").As<v8::Function>();
  CompileRun("function test(x) { return 1 + Math.sin(x) }");
  CompileRun(
      "%PrepareFunctionForOptimization(test);"
      "test(0.5); test(0.6);"
      "%OptimizeFunctionOnNextCall(test); test(0.7);");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0, "this != 1");
  CompileRun("Math.sin(0.1);");
  CHECK_EQ(1, break_point_hit_count);
  CompileRun("test(0.2);");
  CHECK_EQ(2, break_point_hit_count);

  // Re-optimize.
  CompileRun(
      "%PrepareFunctionForOptimization(test);"
      "%OptimizeFunctionOnNextCall(test);");
  ExpectBoolean("test(0.3) < 2", true);
  CHECK_EQ(3, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("test(0.3);");
  CHECK_EQ(3, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointInlineBoundBuiltin) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  v8::Local<v8::Function> builtin;
  i::Handle<i::BreakPoint> bp;

  // === Test inlined bound builtin ===
  break_point_hit_count = 0;

  builtin = CompileRun(
                "var boundrepeat = String.prototype.repeat.bind('a');"
                "String.prototype.repeat")
                .As<v8::Function>();
  CompileRun("function test(x) { return 'a' + boundrepeat(x) }");
  CompileRun(
      "%PrepareFunctionForOptimization(test);"
      "test(4); test(5);"
      "%OptimizeFunctionOnNextCall(test); test(6);");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0, "this != 1");
  CompileRun("'a'.repeat(2);");
  CHECK_EQ(1, break_point_hit_count);
  CompileRun("test(7);");
  CHECK_EQ(2, break_point_hit_count);

  // Re-optimize.
  CompileRun(
      "%PrepareFunctionForOptimization(f);"
      "%OptimizeFunctionOnNextCall(test);");
  CompileRun("test(8);");
  CHECK_EQ(3, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("test(9);");
  CHECK_EQ(3, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointInlinedConstructorBuiltin) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_experimental_inline_promise_constructor = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  v8::Local<v8::Function> builtin;
  i::Handle<i::BreakPoint> bp;

  // === Test inlined constructor builtin (regular construct builtin) ===
  break_point_hit_count = 0;
  builtin = CompileRun("Promise").As<v8::Function>();
  CompileRun("function test(x) { return new Promise(()=>x); }");
  CompileRun(
      "%PrepareFunctionForOptimization(test);"
      "test(4); test(5);"
      "%OptimizeFunctionOnNextCall(test); test(6);");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0, "this != 1");
  CompileRun("new Promise(()=>{});");
  CHECK_EQ(1, break_point_hit_count);
  CompileRun("test(7);");
  CHECK_EQ(2, break_point_hit_count);

  // Re-optimize.
  CompileRun(
      "%PrepareFunctionForOptimization(f);"
      "%OptimizeFunctionOnNextCall(test);");
  CompileRun("test(8);");
  CHECK_EQ(3, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("test(9);");
  CHECK_EQ(3, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointBuiltinConcurrentOpt) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_block_concurrent_recompilation = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  v8::Local<v8::Function> builtin;
  i::Handle<i::BreakPoint> bp;

  // === Test concurrent optimization ===
  break_point_hit_count = 0;
  builtin = CompileRun("Math.sin").As<v8::Function>();
  CompileRun("function test(x) { return 1 + Math.sin(x) }");
  // Trigger concurrent compile job. It is suspended until unblock.
  CompileRun(
      "%PrepareFunctionForOptimization(test);"
      "test(0.5); test(0.6);"
      "%OptimizeFunctionOnNextCall(test, 'concurrent'); test(0.7);");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0);
  // Have the concurrent compile job finish now.
  CompileRun(
      "%UnblockConcurrentRecompilation();"
      "%GetOptimizationStatus(test, 'sync');");
  CompileRun("test(0.2);");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("test(0.3);");
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointBuiltinTFOperator) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  v8::Local<v8::Function> builtin;
  i::Handle<i::BreakPoint> bp;

  // === Test builtin represented as operator ===
  break_point_hit_count = 0;
  builtin = CompileRun("String.prototype.indexOf").As<v8::Function>();
  CompileRun("function test(x) { return 1 + 'foo'.indexOf(x) }");
  CompileRun(
      "%PrepareFunctionForOptimization(f);"
      "test('a'); test('b');"
      "%OptimizeFunctionOnNextCall(test); test('c');");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0);
  CompileRun("'bar'.indexOf('x');");
  CHECK_EQ(1, break_point_hit_count);
  CompileRun("test('d');");
  CHECK_EQ(2, break_point_hit_count);

  // Re-optimize.
  CompileRun(
      "%PrepareFunctionForOptimization(f);"
      "%OptimizeFunctionOnNextCall(test);");
  CompileRun("test('e');");
  CHECK_EQ(3, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("test('f');");
  CHECK_EQ(3, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointBuiltinNewContext) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  v8::Local<v8::Function> builtin;
  i::Handle<i::BreakPoint> bp;

// === Test builtin from a new context ===
// This does not work with no-snapshot build.
#ifdef V8_USE_SNAPSHOT
  break_point_hit_count = 0;
  builtin = CompileRun("String.prototype.repeat").As<v8::Function>();
  CompileRun("'a'.repeat(10)");
  CHECK_EQ(0, break_point_hit_count);
  // Set breakpoint.
  bp = SetBreakPoint(builtin, 0);

  {
    // Create and use new context after breakpoint has been set.
    v8::HandleScope handle_scope(env->GetIsolate());
    v8::Local<v8::Context> new_context = v8::Context::New(env->GetIsolate());
    v8::Context::Scope context_scope(new_context);

    // Run with breakpoint.
    CompileRun("'b'.repeat(10)");
    CHECK_EQ(1, break_point_hit_count);

    CompileRun("'b'.repeat(10)");
    CHECK_EQ(2, break_point_hit_count);

    // Run without breakpoints.
    ClearBreakPoint(bp);
    CompileRun("'b'.repeat(10)");
    CHECK_EQ(2, break_point_hit_count);
  }
#endif

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

void NoOpFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(v8_num(2));
}

TEST(BreakPointApiFunction) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  i::Handle<i::BreakPoint> bp;

  v8::Local<v8::FunctionTemplate> function_template =
      v8::FunctionTemplate::New(env->GetIsolate(), NoOpFunctionCallback);

  v8::Local<v8::Function> function =
      function_template->GetFunction(env.local()).ToLocalChecked();

  env->Global()->Set(env.local(), v8_str("f"), function).ToChecked();

  // === Test simple builtin ===
  break_point_hit_count = 0;

  // Run with breakpoint.
  bp = SetBreakPoint(function, 0, "this != 1");
  ExpectInt32("f()", 2);
  CHECK_EQ(1, break_point_hit_count);

  ExpectInt32("f()", 2);
  CHECK_EQ(2, break_point_hit_count);

  // Direct call through API does not trigger breakpoint.
  function->Call(env.local(), v8::Undefined(env->GetIsolate()), 0, nullptr)
      .ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  ExpectInt32("f()", 2);
  CHECK_EQ(2, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointApiConstructor) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  i::Handle<i::BreakPoint> bp;

  v8::Local<v8::FunctionTemplate> function_template =
      v8::FunctionTemplate::New(env->GetIsolate(), NoOpFunctionCallback);

  v8::Local<v8::Function> function =
      function_template->GetFunction(env.local()).ToLocalChecked();

  env->Global()->Set(env.local(), v8_str("f"), function).ToChecked();

  // === Test simple builtin ===
  break_point_hit_count = 0;

  // Run with breakpoint.
  bp = SetBreakPoint(function, 0, "this != 1");
  CompileRun("new f()");
  CHECK_EQ(1, break_point_hit_count);
  CompileRun("new f()");
  CHECK_EQ(2, break_point_hit_count);

  // Direct call through API does not trigger breakpoint.
  function->NewInstance(env.local()).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("new f()");
  CHECK_EQ(2, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

void GetWrapperCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(
      args[0]
          .As<v8::Object>()
          ->Get(args.GetIsolate()->GetCurrentContext(), args[1])
          .ToLocalChecked());
}

TEST(BreakPointApiGetter) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  i::Handle<i::BreakPoint> bp;

  v8::Local<v8::FunctionTemplate> function_template =
      v8::FunctionTemplate::New(env->GetIsolate(), NoOpFunctionCallback);
  v8::Local<v8::FunctionTemplate> get_template =
      v8::FunctionTemplate::New(env->GetIsolate(), GetWrapperCallback);

  v8::Local<v8::Function> function =
      function_template->GetFunction(env.local()).ToLocalChecked();
  v8::Local<v8::Function> get =
      get_template->GetFunction(env.local()).ToLocalChecked();

  env->Global()->Set(env.local(), v8_str("f"), function).ToChecked();
  env->Global()->Set(env.local(), v8_str("get_wrapper"), get).ToChecked();
  CompileRun(
      "var o = {};"
      "Object.defineProperty(o, 'f', { get: f, enumerable: true });");

  // === Test API builtin as getter ===
  break_point_hit_count = 0;

  // Run with breakpoint.
  bp = SetBreakPoint(function, 0);
  CompileRun("get_wrapper(o, 'f')");
  CHECK_EQ(0, break_point_hit_count);

  CompileRun("o.f");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("get_wrapper(o, 'f', 2)");
  CompileRun("o.f");
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

void SetWrapperCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args[0]
            .As<v8::Object>()
            ->Set(args.GetIsolate()->GetCurrentContext(), args[1], args[2])
            .FromJust());
}

TEST(BreakPointApiSetter) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  i::Handle<i::BreakPoint> bp;

  v8::Local<v8::FunctionTemplate> function_template =
      v8::FunctionTemplate::New(env->GetIsolate(), NoOpFunctionCallback);
  v8::Local<v8::FunctionTemplate> set_template =
      v8::FunctionTemplate::New(env->GetIsolate(), SetWrapperCallback);

  v8::Local<v8::Function> function =
      function_template->GetFunction(env.local()).ToLocalChecked();
  v8::Local<v8::Function> set =
      set_template->GetFunction(env.local()).ToLocalChecked();

  env->Global()->Set(env.local(), v8_str("f"), function).ToChecked();
  env->Global()->Set(env.local(), v8_str("set_wrapper"), set).ToChecked();

  CompileRun(
      "var o = {};"
      "Object.defineProperty(o, 'f', { set: f, enumerable: true });");

  // === Test API builtin as setter ===
  break_point_hit_count = 0;

  // Run with breakpoint.
  bp = SetBreakPoint(function, 0);

  CompileRun("o.f = 3");
  CHECK_EQ(1, break_point_hit_count);

  CompileRun("set_wrapper(o, 'f', 2)");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("o.f = 3");
  CHECK_EQ(1, break_point_hit_count);

  // === Test API builtin as setter, with condition ===
  break_point_hit_count = 0;

  // Run with breakpoint.
  bp = SetBreakPoint(function, 0, "arguments[0] == 3");
  CompileRun("set_wrapper(o, 'f', 2)");
  CHECK_EQ(0, break_point_hit_count);

  CompileRun("set_wrapper(o, 'f', 3)");
  CHECK_EQ(0, break_point_hit_count);

  CompileRun("o.f = 3");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("set_wrapper(o, 'f', 2)");
  CompileRun("o.f = 3");
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointApiAccessor) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  i::Handle<i::BreakPoint> bp;

  // Create 'foo' class, with a hidden property.
  v8::Local<v8::ObjectTemplate> obj_template =
      v8::ObjectTemplate::New(env->GetIsolate());
  v8::Local<v8::FunctionTemplate> accessor_template =
      v8::FunctionTemplate::New(env->GetIsolate(), NoOpFunctionCallback);
  obj_template->SetAccessorProperty(v8_str("f"), accessor_template,
                                    accessor_template);
  v8::Local<v8::Object> obj =
      obj_template->NewInstance(env.local()).ToLocalChecked();
  env->Global()->Set(env.local(), v8_str("o"), obj).ToChecked();

  v8::Local<v8::Function> function =
      CompileRun("Object.getOwnPropertyDescriptor(o, 'f').set")
          .As<v8::Function>();

  // === Test API accessor ===
  break_point_hit_count = 0;

  CompileRun("function get_loop() { for (var i = 0; i < 10; i++) o.f }");
  CompileRun("function set_loop() { for (var i = 0; i < 10; i++) o.f = 2 }");

  CompileRun("get_loop(); set_loop();");  // Initialize ICs.

  // Run with breakpoint.
  bp = SetBreakPoint(function, 0);

  CompileRun("o.f = 3");
  CHECK_EQ(1, break_point_hit_count);

  CompileRun("o.f");
  CHECK_EQ(2, break_point_hit_count);

  CompileRun("for (var i = 0; i < 10; i++) o.f");
  CHECK_EQ(12, break_point_hit_count);

  CompileRun("get_loop();");
  CHECK_EQ(22, break_point_hit_count);

  CompileRun("for (var i = 0; i < 10; i++) o.f = 2");
  CHECK_EQ(32, break_point_hit_count);

  CompileRun("set_loop();");
  CHECK_EQ(42, break_point_hit_count);

  // Test that the break point also works when we install the function
  // template on a new property (with a fresh AccessorPair instance).
  v8::Local<v8::ObjectTemplate> baz_template =
      v8::ObjectTemplate::New(env->GetIsolate());
  baz_template->SetAccessorProperty(v8_str("g"), accessor_template,
                                    accessor_template);
  v8::Local<v8::Object> baz =
      baz_template->NewInstance(env.local()).ToLocalChecked();
  env->Global()->Set(env.local(), v8_str("b"), baz).ToChecked();

  CompileRun("b.g = 4");
  CHECK_EQ(43, break_point_hit_count);

  CompileRun("b.g");
  CHECK_EQ(44, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("o.f = 3");
  CompileRun("o.f");
  CHECK_EQ(44, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointInlineApiFunction) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  i::Handle<i::BreakPoint> bp;

  v8::Local<v8::FunctionTemplate> function_template =
      v8::FunctionTemplate::New(env->GetIsolate(), NoOpFunctionCallback);

  v8::Local<v8::Function> function =
      function_template->GetFunction(env.local()).ToLocalChecked();

  env->Global()->Set(env.local(), v8_str("f"), function).ToChecked();
  CompileRun(
      "function g() { return 1 +  f(); };"
      "%PrepareFunctionForOptimization(g);");

  // === Test simple builtin ===
  break_point_hit_count = 0;

  // Run with breakpoint.
  bp = SetBreakPoint(function, 0);
  ExpectInt32("g()", 3);
  CHECK_EQ(1, break_point_hit_count);

  ExpectInt32("g()", 3);
  CHECK_EQ(2, break_point_hit_count);

  CompileRun("%OptimizeFunctionOnNextCall(g)");
  ExpectInt32("g()", 3);
  CHECK_EQ(3, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  ExpectInt32("g()", 3);
  CHECK_EQ(3, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

// Test that a break point can be set at a return store location.
TEST(BreakPointConditionBuiltin) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_block_concurrent_recompilation = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  v8::Local<v8::Function> builtin;
  i::Handle<i::BreakPoint> bp;

  // === Test global variable ===
  break_point_hit_count = 0;
  builtin = CompileRun("String.prototype.repeat").As<v8::Function>();
  CompileRun("var condition = false");
  CompileRun("'a'.repeat(10)");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0, "condition == true");
  CompileRun("'b'.repeat(10)");
  CHECK_EQ(0, break_point_hit_count);

  CompileRun("condition = true");
  CompileRun("'b'.repeat(10)");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("'b'.repeat(10)");
  CHECK_EQ(1, break_point_hit_count);

  // === Test arguments ===
  break_point_hit_count = 0;
  builtin = CompileRun("String.prototype.repeat").As<v8::Function>();
  CompileRun("function f(x) { return 'a'.repeat(x * 2); }");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0, "arguments[0] == 20");
  ExpectString("f(5)", "aaaaaaaaaa");
  CHECK_EQ(0, break_point_hit_count);

  ExpectString("f(10)", "aaaaaaaaaaaaaaaaaaaa");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  ExpectString("f(10)", "aaaaaaaaaaaaaaaaaaaa");
  CHECK_EQ(1, break_point_hit_count);

  // === Test adapted arguments ===
  break_point_hit_count = 0;
  builtin = CompileRun("String.prototype.repeat").As<v8::Function>();
  CompileRun("function f(x) { return 'a'.repeat(x * 2, x); }");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0,
                     "arguments[1] == 10 && arguments[2] == undefined");
  ExpectString("f(5)", "aaaaaaaaaa");
  CHECK_EQ(0, break_point_hit_count);

  ExpectString("f(10)", "aaaaaaaaaaaaaaaaaaaa");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  ExpectString("f(10)", "aaaaaaaaaaaaaaaaaaaa");
  CHECK_EQ(1, break_point_hit_count);

  // === Test var-arg builtins ===
  break_point_hit_count = 0;
  builtin = CompileRun("String.fromCharCode").As<v8::Function>();
  CompileRun("function f() { return String.fromCharCode(1, 2, 3); }");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0, "arguments.length == 3 && arguments[1] == 2");
  CompileRun("f(1, 2, 3)");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("f(1, 2, 3)");
  CHECK_EQ(1, break_point_hit_count);

  // === Test rest arguments ===
  break_point_hit_count = 0;
  builtin = CompileRun("String.fromCharCode").As<v8::Function>();
  CompileRun("function f(...args) { return String.fromCharCode(...args); }");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0, "arguments.length == 3 && arguments[1] == 2");
  CompileRun("f(1, 2, 3)");
  CHECK_EQ(1, break_point_hit_count);

  ClearBreakPoint(bp);
  CompileRun("f(1, 3, 3)");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("f(1, 2, 3)");
  CHECK_EQ(1, break_point_hit_count);

  // === Test receiver ===
  break_point_hit_count = 0;
  builtin = CompileRun("String.prototype.repeat").As<v8::Function>();
  CompileRun("function f(x) { return x.repeat(10); }");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  bp = SetBreakPoint(builtin, 0, "this == 'a'");
  ExpectString("f('b')", "bbbbbbbbbb");
  CHECK_EQ(0, break_point_hit_count);

  ExpectString("f('a')", "aaaaaaaaaa");
  CHECK_EQ(1, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  ExpectString("f('a')", "aaaaaaaaaa");
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(BreakPointInlining) {
  i::FLAG_allow_natives_syntax = true;
  break_point_hit_count = 0;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  break_point_hit_count = 0;
  v8::Local<v8::Function> inlinee =
      CompileRun("function f(x) { return x*2; } f").As<v8::Function>();
  CompileRun("function test(x) { return 1 + f(x) }");
  CompileRun(
      "%PrepareFunctionForOptimization(test);"
      "test(0.5); test(0.6);"
      "%OptimizeFunctionOnNextCall(test); test(0.7);");
  CHECK_EQ(0, break_point_hit_count);

  // Run with breakpoint.
  i::Handle<i::BreakPoint> bp = SetBreakPoint(inlinee, 0);
  CompileRun("f(0.1);");
  CHECK_EQ(1, break_point_hit_count);
  CompileRun("test(0.2);");
  CHECK_EQ(2, break_point_hit_count);

  // Re-optimize.
  CompileRun(
      "%PrepareFunctionForOptimization(test);"
      "%OptimizeFunctionOnNextCall(test);");
  CompileRun("test(0.3);");
  CHECK_EQ(3, break_point_hit_count);

  // Run without breakpoints.
  ClearBreakPoint(bp);
  CompileRun("test(0.3);");
  CHECK_EQ(3, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

static void CallWithBreakPoints(v8::Local<v8::Context> context,
                                v8::Local<v8::Object> recv,
                                v8::Local<v8::Function> f,
                                int break_point_count, int call_count) {
  break_point_hit_count = 0;
  for (int i = 0; i < call_count; i++) {
    f->Call(context, recv, 0, nullptr).ToLocalChecked();
    CHECK_EQ((i + 1) * break_point_count, break_point_hit_count);
  }
}


// Test GC during break point processing.
TEST(GCDuringBreakPointProcessing) {
  break_point_hit_count = 0;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.local();

  DebugEventBreakPointCollectGarbage delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
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

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


// Call the function three times with different garbage collections in between
// and make sure that the break point survives.
static void CallAndGC(v8::Local<v8::Context> context,
                      v8::Local<v8::Object> recv, v8::Local<v8::Function> f) {
  break_point_hit_count = 0;

  for (int i = 0; i < 3; i++) {
    // Call function.
    f->Call(context, recv, 0, nullptr).ToLocalChecked();
    CHECK_EQ(1 + i * 3, break_point_hit_count);

    // Scavenge and call function.
    CcTest::CollectGarbage(v8::internal::NEW_SPACE);
    f->Call(context, recv, 0, nullptr).ToLocalChecked();
    CHECK_EQ(2 + i * 3, break_point_hit_count);

    // Mark sweep (and perhaps compact) and call function.
    CcTest::CollectAllGarbage();
    f->Call(context, recv, 0, nullptr).ToLocalChecked();
    CHECK_EQ(3 + i * 3, break_point_hit_count);
  }
}


// Test that a break point can be set at a return store location.
TEST(BreakPointSurviveGC) {
  break_point_hit_count = 0;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.local();

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
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

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

// Test that the debugger statement causes a break.
TEST(DebuggerStatement) {
  break_point_hit_count = 0;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  v8::Local<v8::Context> context = env.local();
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
  bar->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);

  // Run function with two debugger statement
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(3, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


// Test setting a breakpoint on the debugger statement.
TEST(DebuggerStatementBreakpoint) {
    break_point_hit_count = 0;
    LocalContext env;
    v8::HandleScope scope(env->GetIsolate());
    v8::Local<v8::Context> context = env.local();
    DebugEventCounter delegate;
    v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
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
    foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
    CHECK_EQ(1, break_point_hit_count);

    i::Handle<i::BreakPoint> bp = SetBreakPoint(foo, 0);

    // Set breakpoint does not duplicate hits
    foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
    CHECK_EQ(2, break_point_hit_count);

    ClearBreakPoint(bp);
    v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
    CheckDebuggerUnloaded();
}


// Test that the conditional breakpoints work event if code generation from
// strings is prohibited in the debugee context.
TEST(ConditionalBreakpointWithCodeGenerationDisallowed) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  v8::Local<v8::Context> context = env.local();
  v8::Local<v8::Function> foo = CompileFunction(&env,
    "function foo(x) {\n"
    "  var s = 'String value2';\n"
    "  return s + x;\n"
    "}",
    "foo");

  // Set conditional breakpoint with condition 'true'.
  SetBreakPoint(foo, 4, "true");

  break_point_hit_count = 0;
  env->AllowCodeGenerationFromStrings(false);
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


// Simple test of the stepping mechanism using only store ICs.
TEST(DebugStepLinear) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Create a function for testing stepping.
  v8::Local<v8::Function> foo = CompileFunction(&env,
                                                "function foo(){a=1;b=1;c=1;}",
                                                "foo");

  // Run foo to allow it to get optimized.
  CompileRun("a=0; b=0; c=0; foo();");

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  SetBreakPoint(foo, 3);

  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Context> context = env.local();
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(4, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();

  // Register a debug event listener which just counts.
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  SetBreakPoint(foo, 3);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // Without stepping only active break points are hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


// Test of the stepping mechanism for keyed load in a loop.
TEST(DebugStepKeyedLoadLoop) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

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

  v8::Local<v8::Context> context = env.local();
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
  run_step.set_step_action(StepNext);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), kArgc, args).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(44, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


// Test of the stepping mechanism for keyed store in a loop.
TEST(DebugStepKeyedStoreLoop) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

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

  v8::Local<v8::Context> context = env.local();
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
  run_step.set_step_action(StepNext);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), kArgc, args).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(44, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


// Test of the stepping mechanism for named load in a loop.
TEST(DebugStepNamedLoadLoop) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
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
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // Set up break point and step through the function.
  SetBreakPoint(foo, 4);
  run_step.set_step_action(StepNext);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(65, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


static void DoDebugStepNamedStoreLoop(int expected) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  // Create a function for testing stepping of named store.
  v8::Local<v8::Context> context = env.local();
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
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // Set up break point and step through the function.
  SetBreakPoint(foo, 3);
  run_step.set_step_action(StepNext);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // With stepping all expected break locations are hit.
  CHECK_EQ(expected, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


// Test of the stepping mechanism for named load in a loop.
TEST(DebugStepNamedStoreLoop) { DoDebugStepNamedStoreLoop(34); }

// Test the stepping mechanism with different ICs.
TEST(DebugStepLinearMixedICs) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
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

  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(10, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();

  // Register a debug event listener which just counts.
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  SetBreakPoint(foo, 0);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // Without stepping only active break points are hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


TEST(DebugStepDeclarations) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
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
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(5, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


TEST(DebugStepLocals) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
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
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(5, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


TEST(DebugStepIf) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
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
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_true[argc] = {v8::True(isolate)};
  foo->Call(context, env->Global(), argc, argv_true).ToLocalChecked();
  CHECK_EQ(4, break_point_hit_count);

  // Stepping through the false part.
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_false[argc] = {v8::False(isolate)};
  foo->Call(context, env->Global(), argc, argv_false).ToLocalChecked();
  CHECK_EQ(5, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


TEST(DebugStepSwitch) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
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
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_1[argc] = {v8::Number::New(isolate, 1)};
  foo->Call(context, env->Global(), argc, argv_1).ToLocalChecked();
  CHECK_EQ(6, break_point_hit_count);

  // Another case.
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_2[argc] = {v8::Number::New(isolate, 2)};
  foo->Call(context, env->Global(), argc, argv_2).ToLocalChecked();
  CHECK_EQ(5, break_point_hit_count);

  // Last case.
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_3[argc] = {v8::Number::New(isolate, 3)};
  foo->Call(context, env->Global(), argc, argv_3).ToLocalChecked();
  CHECK_EQ(7, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


TEST(DebugStepWhile) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
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
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_0[argc] = {v8::Number::New(isolate, 0)};
  foo->Call(context, env->Global(), argc, argv_0).ToLocalChecked();
  CHECK_EQ(3, break_point_hit_count);

  // Looping 10 times.
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_10[argc] = {v8::Number::New(isolate, 10)};
  foo->Call(context, env->Global(), argc, argv_10).ToLocalChecked();
  CHECK_EQ(23, break_point_hit_count);

  // Looping 100 times.
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_100[argc] = {v8::Number::New(isolate, 100)};
  foo->Call(context, env->Global(), argc, argv_100).ToLocalChecked();
  CHECK_EQ(203, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


TEST(DebugStepDoWhile) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
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
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_0[argc] = {v8::Number::New(isolate, 0)};
  foo->Call(context, env->Global(), argc, argv_0).ToLocalChecked();
  CHECK_EQ(4, break_point_hit_count);

  // Looping 10 times.
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_10[argc] = {v8::Number::New(isolate, 10)};
  foo->Call(context, env->Global(), argc, argv_10).ToLocalChecked();
  CHECK_EQ(22, break_point_hit_count);

  // Looping 100 times.
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_100[argc] = {v8::Number::New(isolate, 100)};
  foo->Call(context, env->Global(), argc, argv_100).ToLocalChecked();
  CHECK_EQ(202, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


TEST(DebugStepFor) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
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
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_0[argc] = {v8::Number::New(isolate, 0)};
  foo->Call(context, env->Global(), argc, argv_0).ToLocalChecked();
  CHECK_EQ(4, break_point_hit_count);

  // Looping 10 times.
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_10[argc] = {v8::Number::New(isolate, 10)};
  foo->Call(context, env->Global(), argc, argv_10).ToLocalChecked();
  CHECK_EQ(34, break_point_hit_count);

  // Looping 100 times.
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_100[argc] = {v8::Number::New(isolate, 100)};
  foo->Call(context, env->Global(), argc, argv_100).ToLocalChecked();
  CHECK_EQ(304, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


TEST(DebugStepForContinue) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
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
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_10[argc] = {v8::Number::New(isolate, 10)};
  result = foo->Call(context, env->Global(), argc, argv_10).ToLocalChecked();
  CHECK_EQ(5, result->Int32Value(context).FromJust());
  CHECK_EQ(62, break_point_hit_count);

  // Looping 100 times.
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_100[argc] = {v8::Number::New(isolate, 100)};
  result = foo->Call(context, env->Global(), argc, argv_100).ToLocalChecked();
  CHECK_EQ(50, result->Int32Value(context).FromJust());
  CHECK_EQ(557, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


TEST(DebugStepForBreak) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
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
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_10[argc] = {v8::Number::New(isolate, 10)};
  result = foo->Call(context, env->Global(), argc, argv_10).ToLocalChecked();
  CHECK_EQ(9, result->Int32Value(context).FromJust());
  CHECK_EQ(64, break_point_hit_count);

  // Looping 100 times.
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  v8::Local<v8::Value> argv_100[argc] = {v8::Number::New(isolate, 100)};
  result = foo->Call(context, env->Global(), argc, argv_100).ToLocalChecked();
  CHECK_EQ(99, result->Int32Value(context).FromJust());
  CHECK_EQ(604, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


TEST(DebugStepForIn) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
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

  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
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

  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(10, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


TEST(DebugStepWith) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
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

  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(4, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


TEST(DebugConditional) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
  // Create a function for testing stepping. Run it to allow it to get
  // optimized.
  const char* src =
      "function foo(x) { "
      "  return x ? 1 : 2;"
      "}"
      "foo()";
  v8::Local<v8::Function> foo = CompileFunction(&env, src, "foo");
  SetBreakPoint(foo, 0);  // "var a;"

  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  const int argc = 1;
  v8::Local<v8::Value> argv_true[argc] = {v8::True(isolate)};
  foo->Call(context, env->Global(), argc, argv_true).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

// Test that step in does not step into native functions.
TEST(DebugStepNatives) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Create a function for testing stepping.
  v8::Local<v8::Function> foo =
      CompileFunction(&env, "function foo(){debugger;Math.sin(1);}", "foo");

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(3, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();

  // Register a debug event listener which just counts.
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // Without stepping only active break points are hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

// Test that step in works with function.apply.
TEST(DebugStepFunctionApply) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Create a function for testing stepping.
  v8::Local<v8::Function> foo =
      CompileFunction(&env,
                      "function bar(x, y, z) { if (x == 1) { a = y; b = z; } }"
                      "function foo(){ debugger; bar.apply(this, [1,2,3]); }",
                      "foo");

  // Register a debug event listener which steps and counts.
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);

  v8::Local<v8::Context> context = env.local();
  run_step.set_step_action(StepIn);
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // With stepping all break locations are hit.
  CHECK_EQ(7, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();

  // Register a debug event listener which just counts.
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // Without stepping only the debugger statement is hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


// Test that step in works with function.call.
TEST(DebugStepFunctionCall) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::Context> context = env.local();
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
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);
  run_step.set_step_action(StepIn);

  // Check stepping where the if condition in bar is false.
  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(6, break_point_hit_count);

  // Check stepping where the if condition in bar is true.
  break_point_hit_count = 0;
  const int argc = 1;
  v8::Local<v8::Value> argv[argc] = {v8::True(isolate)};
  foo->Call(context, env->Global(), argc, argv).ToLocalChecked();
  CHECK_EQ(8, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();

  // Register a debug event listener which just counts.
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // Without stepping only the debugger statement is hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(isolate, nullptr);
  CheckDebuggerUnloaded();
}


// Test that step in works with Function.call.apply.
TEST(DebugStepFunctionCallApply) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::Context> context = env.local();
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
  DebugEventCounter run_step;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &run_step);
  run_step.set_step_action(StepIn);

  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(6, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();

  // Register a debug event listener which just counts.
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  break_point_hit_count = 0;
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // Without stepping only the debugger statement is hit.
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(isolate, nullptr);
  CheckDebuggerUnloaded();
}


// Tests that breakpoint will be hit if it's set in script.
TEST(PauseInScript) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(env->GetIsolate());

  // Register a debug event listener which counts.
  DebugEventCounter event_counter;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &event_counter);

  v8::Local<v8::Context> context = env.local();
  // Create a script that returns a function.
  const char* src = "(function (evt) {})";
  const char* script_name = "StepInHandlerTest";

  v8::ScriptOrigin origin(v8_str(env->GetIsolate(), script_name),
                          v8::Integer::New(env->GetIsolate(), 0));
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, v8_str(env->GetIsolate(), src), &origin)
          .ToLocalChecked();

  // Set breakpoint in the script.
  i::Handle<i::Script> i_script(
      i::Script::cast(v8::Utils::OpenHandle(*script)->shared().script()),
      isolate);
  i::Handle<i::String> condition = isolate->factory()->empty_string();
  int position = 0;
  int id;
  isolate->debug()->SetBreakPointForScript(i_script, condition, &position, &id);
  break_point_hit_count = 0;

  v8::Local<v8::Value> r = script->Run(context).ToLocalChecked();

  CHECK(r->IsFunction());
  CHECK_EQ(1, break_point_hit_count);

  // Get rid of the debug delegate.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

int message_callback_count = 0;

TEST(DebugBreak) {
  i::FLAG_stress_compaction = false;
#ifdef VERIFY_HEAP
  i::FLAG_verify_heap = true;
#endif
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which sets the break flag and counts.
  DebugEventBreak delegate;
  v8::debug::SetDebugDelegate(isolate, &delegate);

  v8::Local<v8::Context> context = env.local();
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
  f0->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  f1->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  f2->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  f3->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // Set the debug break flag.
  v8::debug::SetBreakOnNextFunctionCall(env->GetIsolate());

  // Call all functions with different argument count.
  break_point_hit_count = 0;
  for (unsigned int i = 0; i < arraysize(argv); i++) {
    f0->Call(context, env->Global(), i, argv).ToLocalChecked();
    f1->Call(context, env->Global(), i, argv).ToLocalChecked();
    f2->Call(context, env->Global(), i, argv).ToLocalChecked();
    f3->Call(context, env->Global(), i, argv).ToLocalChecked();
  }

  // One break for each function called.
  CHECK_EQ(4 * arraysize(argv), break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

class DebugScopingListener : public v8::debug::DebugDelegate {
 public:
  void BreakProgramRequested(
      v8::Local<v8::Context>,
      const std::vector<v8::debug::BreakpointId>&) override {
    auto stack_traces =
        v8::debug::StackTraceIterator::Create(CcTest::isolate());
    v8::debug::Location location = stack_traces->GetSourceLocation();
    CHECK_EQ(26, location.GetColumnNumber());
    CHECK_EQ(0, location.GetLineNumber());

    auto scopes = stack_traces->GetScopeIterator();
    CHECK_EQ(v8::debug::ScopeIterator::ScopeTypeWith, scopes->GetType());
    CHECK_EQ(20, scopes->GetStartLocation().GetColumnNumber());
    CHECK_EQ(31, scopes->GetEndLocation().GetColumnNumber());

    scopes->Advance();
    CHECK_EQ(v8::debug::ScopeIterator::ScopeTypeLocal, scopes->GetType());
    CHECK_EQ(0, scopes->GetStartLocation().GetColumnNumber());
    CHECK_EQ(68, scopes->GetEndLocation().GetColumnNumber());

    scopes->Advance();
    CHECK_EQ(v8::debug::ScopeIterator::ScopeTypeGlobal, scopes->GetType());

    scopes->Advance();
    CHECK(scopes->Done());
  }
};

TEST(DebugBreakInWrappedScript) {
  i::FLAG_stress_compaction = false;
#ifdef VERIFY_HEAP
  i::FLAG_verify_heap = true;
#endif
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Register a debug event listener which sets the break flag and counts.
  DebugScopingListener delegate;
  v8::debug::SetDebugDelegate(isolate, &delegate);

  static const char* source =
      //   0         1         2         3         4         5         6 7
      "try { with({o : []}){ o[0](); } } catch (e) { return e.toString(); }";
  static const char* expect = "TypeError: o[0] is not a function";

  // For this test, we want to break on uncaught exceptions:
  ChangeBreakOnException(true, true);

  {
    v8::ScriptCompiler::Source script_source(v8_str(source));
    v8::Local<v8::Function> fun =
        v8::ScriptCompiler::CompileFunctionInContext(
            env.local(), &script_source, 0, nullptr, 0, nullptr)
            .ToLocalChecked();
    v8::Local<v8::Value> result =
        fun->Call(env.local(), env->Global(), 0, nullptr).ToLocalChecked();
    CHECK(result->IsString());
    CHECK(v8::Local<v8::String>::Cast(result)
              ->Equals(env.local(), v8_str(expect))
              .FromJust());
  }

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

static void EmptyHandler(const v8::FunctionCallbackInfo<v8::Value>& args) {}

TEST(DebugScopeIteratorWithFunctionTemplate) {
  LocalContext env;
  v8::HandleScope handle_scope(env->GetIsolate());
  v8::Isolate* isolate = env->GetIsolate();
  EnableDebugger(isolate);
  v8::Local<v8::Function> func =
      v8::Function::New(env.local(), EmptyHandler).ToLocalChecked();
  std::unique_ptr<v8::debug::ScopeIterator> iterator =
      v8::debug::ScopeIterator::CreateForFunction(isolate, func);
  CHECK(iterator->Done());
  DisableDebugger(isolate);
}

TEST(DebugBreakWithoutJS) {
  i::FLAG_stress_compaction = false;
#ifdef VERIFY_HEAP
  i::FLAG_verify_heap = true;
#endif
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = env.local();

  // Register a debug event listener which sets the break flag and counts.
  DebugEventBreak delegate;
  v8::debug::SetDebugDelegate(isolate, &delegate);

  // Set the debug break flag.
  v8::debug::SetBreakOnNextFunctionCall(env->GetIsolate());

  v8::Local<v8::String> json = v8_str("[1]");
  v8::Local<v8::Value> parsed = v8::JSON::Parse(context, json).ToLocalChecked();
  CHECK(v8::JSON::Stringify(context, parsed)
            .ToLocalChecked()
            ->Equals(context, json)
            .FromJust());
  CHECK_EQ(0, break_point_hit_count);
  CompileRun("");
  CHECK_EQ(1, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

// Test to ensure that JavaScript code keeps running while the debug break
// through the stack limit flag is set but breaks are disabled.
TEST(DisableBreak) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which sets the break flag and counts.
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  v8::Local<v8::Context> context = env.local();
  // Create a function for testing stepping.
  const char* src = "function f() {g()};function g(){i=0; while(i<10){i++}}";
  v8::Local<v8::Function> f = CompileFunction(&env, src, "f");

  // Set, test and cancel debug break.
  v8::debug::SetBreakOnNextFunctionCall(env->GetIsolate());
  v8::debug::ClearBreakOnNextFunctionCall(env->GetIsolate());

  // Set the debug break flag.
  v8::debug::SetBreakOnNextFunctionCall(env->GetIsolate());

  // Call all functions with different argument count.
  break_point_hit_count = 0;
  f->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(1, break_point_hit_count);

  {
    v8::debug::SetBreakOnNextFunctionCall(env->GetIsolate());
    i::Isolate* isolate = reinterpret_cast<i::Isolate*>(env->GetIsolate());
    v8::internal::DisableBreak disable_break(isolate->debug());
    f->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
    CHECK_EQ(1, break_point_hit_count);
  }

  f->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  CHECK_EQ(2, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

TEST(DisableDebuggerStatement) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which sets the break flag and counts.
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  CompileRun("debugger;");
  CHECK_EQ(1, break_point_hit_count);

  // Check that we ignore debugger statement when breakpoints aren't active.
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(env->GetIsolate());
  isolate->debug()->set_break_points_active(false);
  CompileRun("debugger;");
  CHECK_EQ(1, break_point_hit_count);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
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
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(isolate, &delegate);

  // Set the debug break flag.
  v8::debug::SetBreakOnNextFunctionCall(isolate);
  break_point_hit_count = 0;
  {
    // Create a context with an extension to make sure that some JavaScript
    // code is executed during bootstrapping.
    v8::RegisterExtension(v8::base::make_unique<v8::Extension>(
        "simpletest", kSimpleExtensionSource));
    const char* extension_names[] = { "simpletest" };
    v8::ExtensionConfiguration extensions(1, extension_names);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate, &extensions);
  }
  // Check that no DebugBreak events occurred during the context creation.
  CHECK_EQ(0, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(isolate, nullptr);
  CheckDebuggerUnloaded();
}

TEST(SetDebugEventListenerOnUninitializedVM) {
  v8::HandleScope scope(CcTest::isolate());
  EnableDebugger(CcTest::isolate());
}

// Test that clearing the debug event listener actually clears all break points
// and related information.
TEST(DebuggerUnload) {
  LocalContext env;
  v8::HandleScope handle_scope(env->GetIsolate());
  // Check debugger is unloaded before it is used.
  CheckDebuggerUnloaded();

  // Set a debug event listener.
  break_point_hit_count = 0;
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  v8::Local<v8::Context> context = env.local();
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
    foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
    CHECK_EQ(2, break_point_hit_count);
    bar->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
    CHECK_EQ(4, break_point_hit_count);
  }

  // Remove the debug event listener without clearing breakpoints. Do this
  // outside a handle scope.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

int event_listener_hit_count = 0;

// Test for issue http://code.google.com/p/v8/issues/detail?id=289.
// Make sure that DebugGetLoadedScripts doesn't return scripts
// with disposed external source.
class EmptyExternalStringResource : public v8::String::ExternalStringResource {
 public:
  EmptyExternalStringResource() { empty_[0] = 0; }
  ~EmptyExternalStringResource() override = default;
  size_t length() const override { return empty_.length(); }
  const uint16_t* data() const override { return empty_.begin(); }

 private:
  ::v8::internal::EmbeddedVector<uint16_t, 1> empty_;
};

TEST(DebugScriptLineEndsAreAscending) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Compile a test script.
  v8::Local<v8::String> script = v8_str(isolate,
                                        "function f() {\n"
                                        "  debugger;\n"
                                        "}\n");

  v8::ScriptOrigin origin1 = v8::ScriptOrigin(v8_str(isolate, "name"));
  v8::Local<v8::Script> script1 =
      v8::Script::Compile(env.local(), script, &origin1).ToLocalChecked();
  USE(script1);

  Handle<v8::internal::FixedArray> instances;
  {
    v8::internal::Debug* debug = CcTest::i_isolate()->debug();
    instances = debug->GetLoadedScripts();
  }

  CHECK_GT(instances->length(), 0);
  for (int i = 0; i < instances->length(); i++) {
    Handle<v8::internal::Script> script = Handle<v8::internal::Script>(
        v8::internal::Script::cast(instances->get(i)), CcTest::i_isolate());

    v8::internal::Script::InitLineEnds(script);
    v8::internal::FixedArray ends =
        v8::internal::FixedArray::cast(script->line_ends());
    CHECK_GT(ends.length(), 0);

    int prev_end = -1;
    for (int j = 0; j < ends.length(); j++) {
      const int curr_end = v8::internal::Smi::ToInt(ends.get(j));
      CHECK_GT(curr_end, prev_end);
      prev_end = curr_end;
    }
  }
}

static v8::Local<v8::Context> expected_context;
static v8::Local<v8::Value> expected_context_data;

class ContextCheckEventListener : public v8::debug::DebugDelegate {
 public:
  void BreakProgramRequested(v8::Local<v8::Context> paused_context,
                             const std::vector<v8::debug::BreakpointId>&
                                 inspector_break_points_hit) override {
    CheckContext();
  }
  void ScriptCompiled(v8::Local<v8::debug::Script> script, bool is_live_edited,
                      bool has_compile_error) override {
    CheckContext();
  }
  void ExceptionThrown(v8::Local<v8::Context> paused_context,
                       v8::Local<v8::Value> exception,
                       v8::Local<v8::Value> promise, bool is_uncaught,
                       v8::debug::ExceptionType) override {
    CheckContext();
  }
  bool IsFunctionBlackboxed(v8::Local<v8::debug::Script> script,
                            const v8::debug::Location& start,
                            const v8::debug::Location& end) override {
    CheckContext();
    return false;
  }

 private:
  void CheckContext() {
    v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
    CHECK(context == expected_context);
    CHECK(context->GetEmbedderData(0)->StrictEquals(expected_context_data));
    event_listener_hit_count++;
  }
};

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
  context_1 =
      v8::Context::New(isolate, nullptr, global_template, global_object);
  context_2 =
      v8::Context::New(isolate, nullptr, global_template, global_object);

  ContextCheckEventListener delegate;
  v8::debug::SetDebugDelegate(isolate, &delegate);

  // Default data value is undefined.
  CHECK_EQ(0, context_1->GetNumberOfEmbedderDataFields());
  CHECK_EQ(0, context_2->GetNumberOfEmbedderDataFields());

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
    f->Call(context_1, context_1->Global(), 0, nullptr).ToLocalChecked();
  }

  // Enter and run function in the second context.
  {
    v8::Context::Scope context_scope(context_2);
    expected_context = context_2;
    expected_context_data = data_2;
    v8::Local<v8::Function> f = CompileFunction(isolate, source, "f");
    f->Call(context_2, context_2->Global(), 0, nullptr).ToLocalChecked();
  }

  // Two times compile event and two times break event.
  CHECK_GT(event_listener_hit_count, 3);

  v8::debug::SetDebugDelegate(isolate, nullptr);
  CheckDebuggerUnloaded();
}

// Test which creates a context and sets embedder data on it. Checks that this
// data is set correctly and that when the debug event listener is called for
// break event in an eval statement the expected context is the one returned by
// Message.GetEventContext.
TEST(EvalContextData) {
  v8::HandleScope scope(CcTest::isolate());

  v8::Local<v8::Context> context_1;
  v8::Local<v8::ObjectTemplate> global_template =
      v8::Local<v8::ObjectTemplate>();
  context_1 = v8::Context::New(CcTest::isolate(), nullptr, global_template);

  ContextCheckEventListener delegate;
  v8::debug::SetDebugDelegate(CcTest::isolate(), &delegate);

  // Contexts initially do not have embedder data fields.
  CHECK_EQ(0, context_1->GetNumberOfEmbedderDataFields());

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
    f->Call(context_1, context_1->Global(), 0, nullptr).ToLocalChecked();
  }

  v8::debug::SetDebugDelegate(CcTest::isolate(), nullptr);

  // One time compile event and one time break event.
  CHECK_GT(event_listener_hit_count, 2);
  CheckDebuggerUnloaded();
}

// Debug event listener which counts script compiled events.
class ScriptCompiledDelegate : public v8::debug::DebugDelegate {
 public:
  void ScriptCompiled(v8::Local<v8::debug::Script>, bool,
                      bool has_compile_error) override {
    if (!has_compile_error) {
      after_compile_event_count++;
    } else {
      compile_error_event_count++;
    }
  }

  int after_compile_event_count = 0;
  int compile_error_event_count = 0;
};

// Tests that after compile event is sent as many times as there are scripts
// compiled.
TEST(AfterCompileEventWhenEventListenerIsReset) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.local();
  const char* script = "var a=1";

  ScriptCompiledDelegate delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  v8::Script::Compile(context, v8_str(env->GetIsolate(), script))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);

  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  v8::debug::SetBreakOnNextFunctionCall(env->GetIsolate());
  v8::Script::Compile(context, v8_str(env->GetIsolate(), script))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();

  // Setting listener to nullptr should cause debugger unload.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();

  // Compilation cache should be disabled when debugger is active.
  CHECK_EQ(2, delegate.after_compile_event_count);
}

// Tests that syntax error event is sent as many times as there are scripts
// with syntax error compiled.
TEST(SyntaxErrorEventOnSyntaxException) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // For this test, we want to break on uncaught exceptions:
  ChangeBreakOnException(false, true);

  ScriptCompiledDelegate delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  v8::Local<v8::Context> context = env.local();

  // Check initial state.
  CHECK_EQ(0, delegate.compile_error_event_count);

  // Throws SyntaxError: Unexpected end of input
  CHECK(
      v8::Script::Compile(context, v8_str(env->GetIsolate(), "+++")).IsEmpty());
  CHECK_EQ(1, delegate.compile_error_event_count);

  CHECK(v8::Script::Compile(context, v8_str(env->GetIsolate(), "/sel\\/: \\"))
            .IsEmpty());
  CHECK_EQ(2, delegate.compile_error_event_count);

  v8::Local<v8::Script> script =
      v8::Script::Compile(context,
                          v8_str(env->GetIsolate(), "JSON.parse('1234:')"))
          .ToLocalChecked();
  CHECK_EQ(2, delegate.compile_error_event_count);
  CHECK(script->Run(context).IsEmpty());
  CHECK_EQ(3, delegate.compile_error_event_count);

  v8::Script::Compile(context,
                      v8_str(env->GetIsolate(), "new RegExp('/\\/\\\\');"))
      .ToLocalChecked();
  CHECK_EQ(3, delegate.compile_error_event_count);

  v8::Script::Compile(context, v8_str(env->GetIsolate(), "throw 1;"))
      .ToLocalChecked();
  CHECK_EQ(3, delegate.compile_error_event_count);
}

class ExceptionEventCounter : public v8::debug::DebugDelegate {
 public:
  void ExceptionThrown(v8::Local<v8::Context> paused_context,
                       v8::Local<v8::Value> exception,
                       v8::Local<v8::Value> promise, bool is_uncaught,
                       v8::debug::ExceptionType) override {
    exception_event_count++;
  }
  int exception_event_count = 0;
};

TEST(NoBreakOnStackOverflow) {
  i::FLAG_stack_size = 100;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  ChangeBreakOnException(true, true);

  ExceptionEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  CHECK_EQ(0, delegate.exception_event_count);

  CompileRun(
      "function f() { return f(); }"
      "try { f() } catch {}");

  CHECK_EQ(0, delegate.exception_event_count);
}

// Tests that break event is sent when event listener is reset.
TEST(BreakEventWhenEventListenerIsReset) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.local();
  const char* script = "function f() {};";

  ScriptCompiledDelegate delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  v8::Script::Compile(context, v8_str(env->GetIsolate(), script))
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);

  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  v8::debug::SetBreakOnNextFunctionCall(env->GetIsolate());
  v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "f"))
          .ToLocalChecked());
  f->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // Setting event listener to nullptr should cause debugger unload.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();

  // Compilation cache should be disabled when debugger is active.
  CHECK_EQ(1, delegate.after_compile_event_count);
}

// Tests that script is reported as compiled when bound to context.
TEST(AfterCompileEventOnBindToContext) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  ScriptCompiledDelegate delegate;
  v8::debug::SetDebugDelegate(isolate, &delegate);

  const char* source = "var a=1";
  v8::ScriptCompiler::Source script_source(
      v8::String::NewFromUtf8(isolate, source, v8::NewStringType::kNormal)
          .ToLocalChecked());

  v8::Local<v8::UnboundScript> unbound =
      v8::ScriptCompiler::CompileUnboundScript(isolate, &script_source)
          .ToLocalChecked();
  CHECK_EQ(delegate.after_compile_event_count, 0);

  unbound->BindToCurrentContext();
  CHECK_EQ(delegate.after_compile_event_count, 1);
  v8::debug::SetDebugDelegate(isolate, nullptr);
}


// Test that if DebugBreak is forced it is ignored when code from
// debug-delay.js is executed.
TEST(NoDebugBreakInAfterCompileEventListener) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.local();

  // Register a debug event listener which sets the break flag and counts.
  DebugEventCounter delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  // Set the debug break flag.
  v8::debug::SetBreakOnNextFunctionCall(env->GetIsolate());

  // Create a function for testing stepping.
  const char* src = "function f() { eval('var x = 10;'); } ";
  v8::Local<v8::Function> f = CompileFunction(&env, src, "f");

  // There should be only one break event.
  CHECK_EQ(1, break_point_hit_count);

  // Set the debug break flag again.
  v8::debug::SetBreakOnNextFunctionCall(env->GetIsolate());
  f->Call(context, env->Global(), 0, nullptr).ToLocalChecked();
  // There should be one more break event when the script is evaluated in 'f'.
  CHECK_EQ(2, break_point_hit_count);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}


// Test that the debug break flag works with function.apply.
TEST(DebugBreakFunctionApply) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.local();

  // Create a function for testing breaking in apply.
  v8::Local<v8::Function> foo = CompileFunction(
      &env,
      "function baz(x) { }"
      "function bar(x) { baz(); }"
      "function foo(){ bar.apply(this, [1]); }",
      "foo");

  // Register a debug event listener which steps and counts.
  DebugEventBreakMax delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  // Set the debug break flag before calling the code using function.apply.
  v8::debug::SetBreakOnNextFunctionCall(env->GetIsolate());

  // Limit the number of debug breaks. This is a regression test for issue 493
  // where this test would enter an infinite loop.
  break_point_hit_count = 0;
  max_break_point_hit_count = 10000;  // 10000 => infinite loop.
  foo->Call(context, env->Global(), 0, nullptr).ToLocalChecked();

  // When keeping the debug break several break will happen.
  CHECK_GT(break_point_hit_count, 1);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
}

// Test that setting the terminate execution flag during debug break processing.
static void TestDebugBreakInLoop(const char* loop_head,
                                 const char** loop_bodies,
                                 const char* loop_tail) {
  // Receive 10 breaks for each test and then terminate JavaScript execution.
  static const int kBreaksPerTest = 10;

  for (int i = 0; loop_bodies[i] != nullptr; i++) {
    // Perform a lazy deoptimization after various numbers of breaks
    // have been hit.

    i::EmbeddedVector<char, 1024> buffer;
    SNPrintF(buffer, "function f() {%s%s%s}", loop_head, loop_bodies[i],
             loop_tail);

    i::PrintF("%s\n", buffer.begin());

    for (int j = 0; j < 3; j++) {
      break_point_hit_count_deoptimize = j;
      if (j == 2) {
        break_point_hit_count_deoptimize = kBreaksPerTest;
      }

      break_point_hit_count = 0;
      max_break_point_hit_count = kBreaksPerTest;
      terminate_after_max_break_point_hit = true;

      // Function with infinite loop.
      CompileRun(buffer.begin());

      // Set the debug break to enter the debugger as soon as possible.
      v8::debug::SetBreakOnNextFunctionCall(CcTest::isolate());

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
                                      nullptr};

static const char* loop_bodies_2[] = {
    "if (a == 1) { continue }",
    "switch (a) { case 1: g(); }",
    "switch (a) { case 1: continue; }",
    "switch (a) { case 1: g(); break; default: h() }",
    "switch (a) { case 1: continue; break; default: h() }",
    nullptr};

void DebugBreakLoop(const char* loop_header, const char** loop_bodies,
                    const char* loop_footer) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Register a debug event listener which sets the break flag and counts.
  DebugEventBreakMax delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  CompileRun(
      "var a = 1;\n"
      "function g() { }\n"
      "function h() { }");

  TestDebugBreakInLoop(loop_header, loop_bodies, loop_footer);

  // Get rid of the debug event listener.
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CheckDebuggerUnloaded();
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

class DebugBreakInlineListener : public v8::debug::DebugDelegate {
 public:
  void BreakProgramRequested(v8::Local<v8::Context> paused_context,
                             const std::vector<v8::debug::BreakpointId>&
                                 inspector_break_points_hit) override {
    int expected_frame_count = 4;
    int expected_line_number[] = {1, 4, 7, 13};

    int frame_count = 0;
    auto iterator = v8::debug::StackTraceIterator::Create(CcTest::isolate());
    for (; !iterator->Done(); iterator->Advance(), ++frame_count) {
      v8::debug::Location loc = iterator->GetSourceLocation();
      CHECK_EQ(expected_line_number[frame_count], loc.GetLineNumber());
    }
    CHECK_EQ(frame_count, expected_frame_count);
  }
};

TEST(DebugBreakInline) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  v8::Local<v8::Context> context = env.local();
  const char* source =
      "function debug(b) {                 \n"
      "  if (b) debugger;                  \n"
      "}                                   \n"
      "function f(b) {                     \n"
      "  debug(b)                          \n"
      "};                                  \n"
      "function g(b) {                     \n"
      "  f(b);                             \n"
      "};                                  \n"
      "%PrepareFunctionForOptimization(g); \n"
      "g(false);                           \n"
      "g(false);                           \n"
      "%OptimizeFunctionOnNextCall(g);     \n"
      "g(true);";
  DebugBreakInlineListener delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  v8::Local<v8::Script> inline_script =
      v8::Script::Compile(context, v8_str(env->GetIsolate(), source))
          .ToLocalChecked();
  inline_script->Run(context).ToLocalChecked();
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
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
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  DebugEventCounter delegate;
  delegate.set_step_action(StepNext);
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);

  // We step through the first script.  It exits through an exception.  We run
  // this inside a new frame to record a different FP than the second script
  // would expect.
  const char* script_1 = "debugger; throw new Error();";
  RunScriptInANewCFrame(script_1);

  // The second script uses forEach.
  const char* script_2 = "[0].forEach(function() { });";
  CompileRun(script_2);

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
}

class DebugBreakStackTraceListener : public v8::debug::DebugDelegate {
 public:
  void BreakProgramRequested(v8::Local<v8::Context> paused_context,
                             const std::vector<v8::debug::BreakpointId>&
                                 inspector_break_points_hit) override {
    v8::StackTrace::CurrentStackTrace(CcTest::isolate(), 10);
  }
};

static void AddDebugBreak(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::debug::SetBreakOnNextFunctionCall(args.GetIsolate());
}

TEST(DebugBreakStackTrace) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  DebugBreakStackTraceListener delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  v8::Local<v8::Context> context = env.local();
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

class DebugBreakTriggerTerminate : public v8::debug::DebugDelegate {
 public:
  void BreakProgramRequested(v8::Local<v8::Context> paused_context,
                             const std::vector<v8::debug::BreakpointId>&
                                 inspector_break_points_hit) override {
    if (terminate_already_fired_) return;
    terminate_requested_semaphore.Signal();
    // Wait for at most 2 seconds for the terminate request.
    CHECK(
        terminate_fired_semaphore.WaitFor(v8::base::TimeDelta::FromSeconds(2)));
    terminate_already_fired_ = true;
  }

 private:
  bool terminate_already_fired_ = false;
};

class TerminationThread : public v8::base::Thread {
 public:
  explicit TerminationThread(v8::Isolate* isolate)
      : Thread(Options("terminator")), isolate_(isolate) {}

  void Run() override {
    terminate_requested_semaphore.Wait();
    isolate_->TerminateExecution();
    terminate_fired_semaphore.Signal();
  }

 private:
  v8::Isolate* isolate_;
};


TEST(DebugBreakOffThreadTerminate) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  DebugBreakTriggerTerminate delegate;
  v8::debug::SetDebugDelegate(isolate, &delegate);
  TerminationThread terminator(isolate);
  CHECK(terminator.Start());
  v8::TryCatch try_catch(env->GetIsolate());
  env->GetIsolate()->RequestInterrupt(BreakRightNow, nullptr);
  CompileRun("while (true);");
  CHECK(try_catch.HasTerminated());
}

class ArchiveRestoreThread : public v8::base::Thread,
                             public v8::debug::DebugDelegate {
 public:
  ArchiveRestoreThread(v8::Isolate* isolate, int spawn_count)
      : Thread(Options("ArchiveRestoreThread")),
        isolate_(isolate),
        debug_(reinterpret_cast<i::Isolate*>(isolate_)->debug()),
        spawn_count_(spawn_count),
        break_count_(0) {}

  void Run() override {
    v8::Locker locker(isolate_);
    isolate_->Enter();

    v8::HandleScope scope(isolate_);
    v8::Local<v8::Context> context = v8::Context::New(isolate_);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Function> test = CompileFunction(isolate_,
                                                   "function test(n) {\n"
                                                   "  debugger;\n"
                                                   "  return n + 1;\n"
                                                   "}\n",
                                                   "test");

    debug_->SetDebugDelegate(this);
    v8::internal::DisableBreak enable_break(debug_, false);

    v8::Local<v8::Value> args[1] = {v8::Integer::New(isolate_, spawn_count_)};

    int result = test->Call(context, context->Global(), 1, args)
                     .ToLocalChecked()
                     ->Int32Value(context)
                     .FromJust();

    // Verify that test(spawn_count_) returned spawn_count_ + 1.
    CHECK_EQ(spawn_count_ + 1, result);

    isolate_->Exit();
  }

  void BreakProgramRequested(
      v8::Local<v8::Context> context,
      const std::vector<v8::debug::BreakpointId>&) override {
    auto stack_traces = v8::debug::StackTraceIterator::Create(isolate_);
    if (!stack_traces->Done()) {
      v8::debug::Location location = stack_traces->GetSourceLocation();

      i::PrintF("ArchiveRestoreThread #%d hit breakpoint at line %d\n",
                spawn_count_, location.GetLineNumber());

      switch (location.GetLineNumber()) {
        case 1:  // debugger;
          CHECK_EQ(break_count_, 0);

          // Attempt to stop on the next line after the first debugger
          // statement. If debug->{Archive,Restore}Debug() improperly reset
          // thread-local debug information, the debugger will fail to stop
          // before the test function returns.
          debug_->PrepareStep(StepNext);

          // Spawning threads while handling the current breakpoint verifies
          // that the parent thread correctly archived and restored the
          // state necessary to stop on the next line. If not, then control
          // will simply continue past the `return n + 1` statement.
          MaybeSpawnChildThread();

          break;

        case 2:  // return n + 1;
          CHECK_EQ(break_count_, 1);
          break;

        default:
          CHECK(false);
      }
    }

    ++break_count_;
  }

  void MaybeSpawnChildThread() {
    if (spawn_count_ > 1) {
      v8::Unlocker unlocker(isolate_);

      // Spawn a thread that spawns a thread that spawns a thread (and so
      // on) so that the ThreadManager is forced to archive and restore
      // the current thread.
      ArchiveRestoreThread child(isolate_, spawn_count_ - 1);
      CHECK(child.Start());
      child.Join();

      // The child thread sets itself as the debug delegate, so we need to
      // usurp it after the child finishes, or else future breakpoints
      // will be delegated to a destroyed ArchiveRestoreThread object.
      debug_->SetDebugDelegate(this);

      // This is the most important check in this test, since
      // child.GetBreakCount() will return 1 if the debugger fails to stop
      // on the `return n + 1` line after the grandchild thread returns.
      CHECK_EQ(child.GetBreakCount(), 2);
    }
  }

  int GetBreakCount() { return break_count_; }

 private:
  v8::Isolate* isolate_;
  v8::internal::Debug* debug_;
  const int spawn_count_;
  int break_count_;
};

TEST(DebugArchiveRestore) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  ArchiveRestoreThread thread(isolate, 5);
  // Instead of calling thread.Start() and thread.Join() here, we call
  // thread.Run() directly, to make sure we exercise archive/restore
  // logic on the *current* thread as well as other threads.
  thread.Run();
  CHECK_EQ(thread.GetBreakCount(), 2);

  isolate->Dispose();
}

class DebugEventExpectNoException : public v8::debug::DebugDelegate {
 public:
  void ExceptionThrown(v8::Local<v8::Context> paused_context,
                       v8::Local<v8::Value> exception,
                       v8::Local<v8::Value> promise, bool is_uncaught,
                       v8::debug::ExceptionType) override {
    CHECK(false);
  }
};

static void TryCatchWrappedThrowCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch(args.GetIsolate());
  CompileRun("throw 'rejection';");
  CHECK(try_catch.HasCaught());
}

TEST(DebugPromiseInterceptedByTryCatch) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  DebugEventExpectNoException delegate;
  v8::debug::SetDebugDelegate(isolate, &delegate);
  v8::Local<v8::Context> context = env.local();
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

class NoInterruptsOnDebugEvent : public v8::debug::DebugDelegate {
 public:
  void ScriptCompiled(v8::Local<v8::debug::Script> script, bool is_live_edited,
                      bool has_compile_error) override {
    ++after_compile_handler_depth_;
    // Do not allow nested AfterCompile events.
    CHECK_LE(after_compile_handler_depth_, 1);
    v8::Isolate* isolate = CcTest::isolate();
    v8::Isolate::AllowJavascriptExecutionScope allow_script(isolate);
    isolate->RequestInterrupt(&HandleInterrupt, this);
    CompileRun("function foo() {}; foo();");
    --after_compile_handler_depth_;
  }

 private:
  static void HandleInterrupt(v8::Isolate* isolate, void* data) {
    NoInterruptsOnDebugEvent* d = static_cast<NoInterruptsOnDebugEvent*>(data);
    CHECK_EQ(0, d->after_compile_handler_depth_);
  }

  int after_compile_handler_depth_ = 0;
};

TEST(NoInterruptsInDebugListener) {
  LocalContext env;
  v8::HandleScope handle_scope(env->GetIsolate());
  NoInterruptsOnDebugEvent delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  CompileRun("void(0);");
}

TEST(BreakLocationIterator) {
  LocalContext env;
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
  Handle<i::SharedFunctionInfo> shared(function->shared(), i_isolate);

  EnableDebugger(isolate);
  CHECK(i_isolate->debug()->EnsureBreakInfo(shared));
  i_isolate->debug()->PrepareFunctionForDebugExecution(shared);

  Handle<i::DebugInfo> debug_info(shared->GetDebugInfo(), i_isolate);

  {
    i::BreakIterator iterator(debug_info);
    CHECK(iterator.GetBreakLocation().IsDebuggerStatement());
    CHECK_EQ(17, iterator.GetBreakLocation().position());
    iterator.Next();
    CHECK(iterator.GetBreakLocation().IsDebugBreakSlot());
    CHECK_EQ(32, iterator.GetBreakLocation().position());
    iterator.Next();
    CHECK(iterator.GetBreakLocation().IsCall());
    CHECK_EQ(32, iterator.GetBreakLocation().position());
    iterator.Next();
    CHECK(iterator.GetBreakLocation().IsDebuggerStatement());
    CHECK_EQ(47, iterator.GetBreakLocation().position());
    iterator.Next();
    CHECK(iterator.GetBreakLocation().IsReturn());
    CHECK_EQ(60, iterator.GetBreakLocation().position());
    iterator.Next();
    CHECK(iterator.Done());
  }

  DisableDebugger(isolate);
}

class DebugStepOverFunctionWithCaughtExceptionListener
    : public v8::debug::DebugDelegate {
 public:
  void BreakProgramRequested(v8::Local<v8::Context> paused_context,
                             const std::vector<v8::debug::BreakpointId>&
                                 inspector_break_points_hit) override {
    ++break_point_hit_count;
    if (break_point_hit_count >= 3) return;
    PrepareStep(StepNext);
  }
  int break_point_hit_count = 0;
};

TEST(DebugStepOverFunctionWithCaughtException) {
  i::FLAG_allow_natives_syntax = true;

  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  DebugStepOverFunctionWithCaughtExceptionListener delegate;
  v8::debug::SetDebugDelegate(isolate, &delegate);

  CompileRun(
      "function foo() {\n"
      "  try { throw new Error(); } catch (e) {}\n"
      "}\n"
      "debugger;\n"
      "foo();\n"
      "foo();\n");

  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
  CHECK_EQ(3, delegate.break_point_hit_count);
}

bool near_heap_limit_callback_called = false;
size_t NearHeapLimitCallback(void* data, size_t current_heap_limit,
                             size_t initial_heap_limit) {
  near_heap_limit_callback_called = true;
  return initial_heap_limit + 10u * i::MB;
}

UNINITIALIZED_TEST(DebugSetOutOfMemoryListener) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  create_params.constraints.set_max_old_generation_size_in_bytes(10 * i::MB);
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  {
    v8::Isolate::Scope i_scope(isolate);
    v8::HandleScope scope(isolate);
    LocalContext context(isolate);
    isolate->AddNearHeapLimitCallback(NearHeapLimitCallback, nullptr);
    CHECK(!near_heap_limit_callback_called);
    // The following allocation fails unless the out-of-memory callback
    // increases the heap limit.
    int length = 10 * i::MB / i::kTaggedSize;
    i_isolate->factory()->NewFixedArray(length, i::AllocationType::kOld);
    CHECK(near_heap_limit_callback_called);
    isolate->RemoveNearHeapLimitCallback(NearHeapLimitCallback, 0);
  }
  isolate->Dispose();
}

TEST(DebugCoverage) {
  i::FLAG_always_opt = false;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::debug::Coverage::SelectMode(isolate,
                                  v8::debug::CoverageMode::kPreciseCount);
  v8::Local<v8::String> source = v8_str(
      "function f() {\n"
      "}\n"
      "f();\n"
      "f();");
  CompileRun(source);
  v8::debug::Coverage coverage = v8::debug::Coverage::CollectPrecise(isolate);
  CHECK_EQ(1u, coverage.ScriptCount());
  v8::debug::Coverage::ScriptData script_data = coverage.GetScriptData(0);
  v8::Local<v8::debug::Script> script = script_data.GetScript();
  CHECK(script->Source()
            .ToLocalChecked()
            ->Equals(env.local(), source)
            .FromMaybe(false));

  CHECK_EQ(2u, script_data.FunctionCount());
  v8::debug::Coverage::FunctionData function_data =
      script_data.GetFunctionData(0);
  v8::debug::Location start =
      script->GetSourceLocation(function_data.StartOffset());
  v8::debug::Location end =
      script->GetSourceLocation(function_data.EndOffset());
  CHECK_EQ(0, start.GetLineNumber());
  CHECK_EQ(0, start.GetColumnNumber());
  CHECK_EQ(3, end.GetLineNumber());
  CHECK_EQ(4, end.GetColumnNumber());
  CHECK_EQ(1, function_data.Count());

  function_data = script_data.GetFunctionData(1);
  start = script->GetSourceLocation(function_data.StartOffset());
  end = script->GetSourceLocation(function_data.EndOffset());
  CHECK_EQ(0, start.GetLineNumber());
  CHECK_EQ(0, start.GetColumnNumber());
  CHECK_EQ(1, end.GetLineNumber());
  CHECK_EQ(1, end.GetColumnNumber());
  CHECK_EQ(2, function_data.Count());
}

namespace {
v8::debug::Coverage::ScriptData GetScriptDataAndDeleteCoverage(
    v8::Isolate* isolate) {
  v8::debug::Coverage coverage = v8::debug::Coverage::CollectPrecise(isolate);
  CHECK_EQ(1u, coverage.ScriptCount());
  return coverage.GetScriptData(0);
}
}  // namespace

TEST(DebugCoverageWithCoverageOutOfScope) {
  i::FLAG_always_opt = false;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::debug::Coverage::SelectMode(isolate,
                                  v8::debug::CoverageMode::kPreciseCount);
  v8::Local<v8::String> source = v8_str(
      "function f() {\n"
      "}\n"
      "f();\n"
      "f();");
  CompileRun(source);
  v8::debug::Coverage::ScriptData script_data =
      GetScriptDataAndDeleteCoverage(isolate);
  v8::Local<v8::debug::Script> script = script_data.GetScript();
  CHECK(script->Source()
            .ToLocalChecked()
            ->Equals(env.local(), source)
            .FromMaybe(false));

  CHECK_EQ(2u, script_data.FunctionCount());
  v8::debug::Coverage::FunctionData function_data =
      script_data.GetFunctionData(0);

  CHECK_EQ(0, function_data.StartOffset());
  CHECK_EQ(26, function_data.EndOffset());

  v8::debug::Location start =
      script->GetSourceLocation(function_data.StartOffset());
  v8::debug::Location end =
      script->GetSourceLocation(function_data.EndOffset());
  CHECK_EQ(0, start.GetLineNumber());
  CHECK_EQ(0, start.GetColumnNumber());
  CHECK_EQ(3, end.GetLineNumber());
  CHECK_EQ(4, end.GetColumnNumber());
  CHECK_EQ(1, function_data.Count());

  function_data = script_data.GetFunctionData(1);
  start = script->GetSourceLocation(function_data.StartOffset());
  end = script->GetSourceLocation(function_data.EndOffset());

  CHECK_EQ(0, function_data.StartOffset());
  CHECK_EQ(16, function_data.EndOffset());

  CHECK_EQ(0, start.GetLineNumber());
  CHECK_EQ(0, start.GetColumnNumber());
  CHECK_EQ(1, end.GetLineNumber());
  CHECK_EQ(1, end.GetColumnNumber());
  CHECK_EQ(2, function_data.Count());
}

namespace {
v8::debug::Coverage::FunctionData GetFunctionDataAndDeleteCoverage(
    v8::Isolate* isolate) {
  v8::debug::Coverage coverage = v8::debug::Coverage::CollectPrecise(isolate);
  CHECK_EQ(1u, coverage.ScriptCount());

  v8::debug::Coverage::ScriptData script_data = coverage.GetScriptData(0);

  CHECK_EQ(2u, script_data.FunctionCount());
  v8::debug::Coverage::FunctionData function_data =
      script_data.GetFunctionData(0);
  CHECK_EQ(1, function_data.Count());
  CHECK_EQ(0, function_data.StartOffset());
  CHECK_EQ(26, function_data.EndOffset());
  return function_data;
}
}  // namespace

TEST(DebugCoverageWithScriptDataOutOfScope) {
  i::FLAG_always_opt = false;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::debug::Coverage::SelectMode(isolate,
                                  v8::debug::CoverageMode::kPreciseCount);
  v8::Local<v8::String> source = v8_str(
      "function f() {\n"
      "}\n"
      "f();\n"
      "f();");
  CompileRun(source);

  v8::debug::Coverage::FunctionData function_data =
      GetFunctionDataAndDeleteCoverage(isolate);
  CHECK_EQ(1, function_data.Count());
  CHECK_EQ(0, function_data.StartOffset());
  CHECK_EQ(26, function_data.EndOffset());
}

TEST(BuiltinsExceptionPrediction) {
  v8::Isolate* isolate = CcTest::isolate();
  i::Isolate* iisolate = CcTest::i_isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::New(isolate);

  i::Builtins* builtins = iisolate->builtins();
  bool fail = false;
  for (int i = 0; i < i::Builtins::builtin_count; i++) {
    i::Code builtin = builtins->builtin(i);
    if (builtin.kind() != i::Code::BUILTIN) continue;
    auto prediction = builtin.GetBuiltinCatchPrediction();
    USE(prediction);
  }
  CHECK(!fail);
}

TEST(DebugGetPossibleBreakpointsReturnLocations) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::String> source = v8_str(
      "function fib(x) {\n"
      "  if (x < 0) return;\n"
      "  if (x === 0) return 1;\n"
      "  if (x === 1) return fib(0);\n"
      "  return x > 2 ? fib(x - 1) + fib(x - 2) : fib(1) + fib(0);\n"
      "}");
  CompileRun(source);
  v8::PersistentValueVector<v8::debug::Script> scripts(isolate);
  v8::debug::GetLoadedScripts(isolate, scripts);
  CHECK_EQ(scripts.Size(), 1);
  std::vector<v8::debug::BreakLocation> locations;
  CHECK(scripts.Get(0)->GetPossibleBreakpoints(
      v8::debug::Location(0, 17), v8::debug::Location(), true, &locations));
  int returns_count = 0;
  for (size_t i = 0; i < locations.size(); ++i) {
    if (locations[i].type() == v8::debug::kReturnBreakLocation) {
      ++returns_count;
    }
  }
  // With Ignition we generate one return location per return statement,
  // each has line = 5, column = 0 as statement position.
  CHECK_EQ(returns_count, 4);
}

TEST(DebugEvaluateNoSideEffect) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  EnableDebugger(env->GetIsolate());
  i::Isolate* isolate = CcTest::i_isolate();
  std::vector<i::Handle<i::JSFunction>> all_functions;
  {
    i::HeapObjectIterator iterator(isolate->heap());
    for (i::HeapObject obj = iterator.Next(); !obj.is_null();
         obj = iterator.Next()) {
      if (!obj.IsJSFunction()) continue;
      i::JSFunction fun = i::JSFunction::cast(obj);
      all_functions.emplace_back(fun, isolate);
    }
  }

  // Perform side effect check on all built-in functions. The side effect check
  // itself contains additional sanity checks.
  for (i::Handle<i::JSFunction> fun : all_functions) {
    bool failed = false;
    isolate->debug()->StartSideEffectCheckMode();
    failed = !isolate->debug()->PerformSideEffectCheck(
        fun, v8::Utils::OpenHandle(*env->Global()));
    isolate->debug()->StopSideEffectCheckMode();
    if (failed) isolate->clear_pending_exception();
  }
  DisableDebugger(env->GetIsolate());
}

namespace {
i::MaybeHandle<i::Script> FindScript(
    i::Isolate* isolate, const std::vector<i::Handle<i::Script>>& scripts,
    const char* name) {
  Handle<i::String> i_name =
      isolate->factory()->NewStringFromAsciiChecked(name);
  for (const auto& script : scripts) {
    if (!script->name().IsString()) continue;
    if (i_name->Equals(i::String::cast(script->name()))) return script;
  }
  return i::MaybeHandle<i::Script>();
}
}  // anonymous namespace

UNINITIALIZED_TEST(LoadedAtStartupScripts) {
  i::FLAG_expose_gc = true;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  {
    v8::Isolate::Scope i_scope(isolate);
    v8::HandleScope scope(isolate);
    LocalContext context(isolate);

    std::vector<i::Handle<i::Script>> scripts;
    CompileWithOrigin(v8_str("function foo(){}"), v8_str("normal.js"),
                      v8_bool(false));
    std::unordered_map<int, int> count_by_type;
    {
      i::DisallowHeapAllocation no_gc;
      i::Script::Iterator iterator(i_isolate);
      for (i::Script script = iterator.Next(); !script.is_null();
           script = iterator.Next()) {
        if (script.type() == i::Script::TYPE_NATIVE &&
            script.name().IsUndefined(i_isolate)) {
          continue;
        }
        ++count_by_type[script.type()];
        scripts.emplace_back(script, i_isolate);
      }
    }
    CHECK_EQ(count_by_type[i::Script::TYPE_NATIVE], 0);
    CHECK_EQ(count_by_type[i::Script::TYPE_EXTENSION], 2);
    CHECK_EQ(count_by_type[i::Script::TYPE_NORMAL], 1);
    CHECK_EQ(count_by_type[i::Script::TYPE_WASM], 0);
    CHECK_EQ(count_by_type[i::Script::TYPE_INSPECTOR], 0);

    i::Handle<i::Script> gc_script =
        FindScript(i_isolate, scripts, "v8/gc").ToHandleChecked();
    CHECK_EQ(gc_script->type(), i::Script::TYPE_EXTENSION);

    i::Handle<i::Script> normal_script =
        FindScript(i_isolate, scripts, "normal.js").ToHandleChecked();
    CHECK_EQ(normal_script->type(), i::Script::TYPE_NORMAL);
  }
  isolate->Dispose();
}

TEST(SourceInfo) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  const char* source =
      "//\n"
      "function a() { b(); };\n"
      "function    b() {\n"
      "  c(true);\n"
      "};\n"
      "  function c(x) {\n"
      "    if (x) {\n"
      "      return 1;\n"
      "    } else {\n"
      "      return 1;\n"
      "    }\n"
      "  };\n"
      "function d(x) {\n"
      "  x = 1 ;\n"
      "  x = 2 ;\n"
      "  x = 3 ;\n"
      "  x = 4 ;\n"
      "  x = 5 ;\n"
      "  x = 6 ;\n"
      "  x = 7 ;\n"
      "  x = 8 ;\n"
      "  x = 9 ;\n"
      "  x = 10;\n"
      "  x = 11;\n"
      "  x = 12;\n"
      "  x = 13;\n"
      "  x = 14;\n"
      "  x = 15;\n"
      "}\n";
  v8::Local<v8::Script> v8_script =
      v8::Script::Compile(env.local(), v8_str(source)).ToLocalChecked();
  i::Handle<i::Script> i_script(
      i::Script::cast(v8::Utils::OpenHandle(*v8_script)->shared().script()),
      CcTest::i_isolate());
  v8::Local<v8::debug::Script> script =
      v8::ToApiHandle<v8::debug::Script>(i_script);

  // Test that when running through source positions the position, line and
  // column progresses as expected.
  v8::debug::Location prev_location = script->GetSourceLocation(0);
  CHECK_EQ(prev_location.GetLineNumber(), 0);
  CHECK_EQ(prev_location.GetColumnNumber(), 0);
  for (int offset = 1; offset < 100; ++offset) {
    v8::debug::Location location = script->GetSourceLocation(offset);
    if (prev_location.GetLineNumber() == location.GetLineNumber()) {
      CHECK_EQ(location.GetColumnNumber(), prev_location.GetColumnNumber() + 1);
    } else {
      CHECK_EQ(location.GetLineNumber(), prev_location.GetLineNumber() + 1);
      CHECK_EQ(location.GetColumnNumber(), 0);
    }
    prev_location = location;
  }

  // Every line of d() is the same length.  Verify we can loop through all
  // positions and find the right line # for each.
  // The position of the first line of d(), i.e. "x = 1 ;".
  const int start_line_d = 13;
  const int start_code_d =
      static_cast<int>(strstr(source, "  x = 1 ;") - source);
  const int num_lines_d = 15;
  const int line_length_d = 10;
  int p = start_code_d;
  for (int line = 0; line < num_lines_d; ++line) {
    for (int column = 0; column < line_length_d; ++column) {
      v8::debug::Location location = script->GetSourceLocation(p);
      CHECK_EQ(location.GetLineNumber(), start_line_d + line);
      CHECK_EQ(location.GetColumnNumber(), column);
      ++p;
    }
  }

  // Test first positon.
  CHECK_EQ(script->GetSourceLocation(0).GetLineNumber(), 0);
  CHECK_EQ(script->GetSourceLocation(0).GetColumnNumber(), 0);

  // Test second positon.
  CHECK_EQ(script->GetSourceLocation(1).GetLineNumber(), 0);
  CHECK_EQ(script->GetSourceLocation(1).GetColumnNumber(), 1);

  // Test first positin in function a().
  const int start_a =
      static_cast<int>(strstr(source, "function a") - source) + 10;
  CHECK_EQ(script->GetSourceLocation(start_a).GetLineNumber(), 1);
  CHECK_EQ(script->GetSourceLocation(start_a).GetColumnNumber(), 10);

  // Test first positin in function b().
  const int start_b =
      static_cast<int>(strstr(source, "function    b") - source) + 13;
  CHECK_EQ(script->GetSourceLocation(start_b).GetLineNumber(), 2);
  CHECK_EQ(script->GetSourceLocation(start_b).GetColumnNumber(), 13);

  // Test first positin in function c().
  const int start_c =
      static_cast<int>(strstr(source, "function c") - source) + 10;
  CHECK_EQ(script->GetSourceLocation(start_c).GetLineNumber(), 5);
  CHECK_EQ(script->GetSourceLocation(start_c).GetColumnNumber(), 12);

  // Test first positin in function d().
  const int start_d =
      static_cast<int>(strstr(source, "function d") - source) + 10;
  CHECK_EQ(script->GetSourceLocation(start_d).GetLineNumber(), 12);
  CHECK_EQ(script->GetSourceLocation(start_d).GetColumnNumber(), 10);

  // Test offsets.
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(1, 10)), start_a);
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(2, 13)), start_b);
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(3, 0)), start_b + 5);
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(3, 2)), start_b + 7);
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(4, 0)), start_b + 16);
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(5, 12)), start_c);
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(6, 0)), start_c + 6);
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(7, 0)), start_c + 19);
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(8, 0)), start_c + 35);
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(9, 0)), start_c + 48);
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(10, 0)), start_c + 64);
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(11, 0)), start_c + 70);
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(12, 10)), start_d);
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(13, 0)), start_d + 6);
  for (int i = 1; i <= num_lines_d; ++i) {
    CHECK_EQ(script->GetSourceOffset(v8::debug::Location(start_line_d + i, 0)),
             6 + (i * line_length_d) + start_d);
  }
  CHECK_EQ(script->GetSourceOffset(v8::debug::Location(start_line_d + 17, 0)),
           start_d + 158);

  // Make sure invalid inputs work properly.
  const int last_position = static_cast<int>(strlen(source)) - 1;
  CHECK_EQ(script->GetSourceLocation(-1).GetLineNumber(), 0);
  CHECK_EQ(script->GetSourceLocation(last_position + 2).GetLineNumber(),
           i::kNoSourcePosition);

  // Test last position.
  CHECK_EQ(script->GetSourceLocation(last_position).GetLineNumber(), 28);
  CHECK_EQ(script->GetSourceLocation(last_position).GetColumnNumber(), 1);
  CHECK_EQ(script->GetSourceLocation(last_position + 1).GetLineNumber(), 29);
  CHECK_EQ(script->GetSourceLocation(last_position + 1).GetColumnNumber(), 0);
}

namespace {
class SetBreakpointOnScriptCompiled : public v8::debug::DebugDelegate {
 public:
  void ScriptCompiled(v8::Local<v8::debug::Script> script, bool is_live_edited,
                      bool has_compile_error) override {
    v8::Local<v8::String> name;
    if (!script->SourceURL().ToLocal(&name)) return;
    v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
    if (!name->Equals(context, v8_str("test")).FromJust()) return;
    CHECK(!has_compile_error);
    v8::debug::Location loc(1, 2);
    CHECK(script->SetBreakpoint(v8_str(""), &loc, &id_));
    CHECK_EQ(loc.GetLineNumber(), 1);
    CHECK_EQ(loc.GetColumnNumber(), 10);
  }

  void BreakProgramRequested(v8::Local<v8::Context> paused_context,
                             const std::vector<v8::debug::BreakpointId>&
                                 inspector_break_points_hit) override {
    ++break_count_;
    CHECK_EQ(inspector_break_points_hit[0], id_);
  }

  int break_count() const { return break_count_; }

 private:
  int break_count_ = 0;
  v8::debug::BreakpointId id_;
};
}  // anonymous namespace

TEST(Regress517592) {
  LocalContext env;
  v8::HandleScope handle_scope(env->GetIsolate());
  SetBreakpointOnScriptCompiled delegate;
  v8::debug::SetDebugDelegate(env->GetIsolate(), &delegate);
  CompileRun(
      v8_str("eval('var foo = function foo() {\\n' +\n"
             "'  var a = 1;\\n' +\n"
             "'}\\n' +\n"
             "'//@ sourceURL=test')"));
  CHECK_EQ(delegate.break_count(), 0);
  CompileRun(v8_str("foo()"));
  CHECK_EQ(delegate.break_count(), 1);
  v8::debug::SetDebugDelegate(env->GetIsolate(), nullptr);
}

TEST(GetPrivateFields) {
  LocalContext env;
  v8::Isolate* v8_isolate = CcTest::isolate();
  v8::internal::Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(v8_isolate);
  v8::Local<v8::Context> context = env.local();
  v8::Local<v8::String> source = v8_str(
      "var X = class {\n"
      "  #foo = 1;\n"
      "  #bar = function() {};\n"
      "}\n"
      "var x = new X()");
  CompileRun(source);
  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "x"))
          .ToLocalChecked());
  v8::Local<v8::Array> private_names =
      v8::debug::GetPrivateFields(context, object).ToLocalChecked();

  for (int i = 0; i < 4; i = i + 2) {
    Handle<v8::internal::JSReceiver> private_name =
        v8::Utils::OpenHandle(*private_names->Get(context, i)
                                   .ToLocalChecked()
                                   ->ToObject(context)
                                   .ToLocalChecked());
    Handle<v8::internal::JSPrimitiveWrapper> private_value =
        Handle<v8::internal::JSPrimitiveWrapper>::cast(private_name);
    Handle<v8::internal::Symbol> priv_symbol(
        v8::internal::Symbol::cast(private_value->value()), isolate);
    CHECK(priv_symbol->is_private_name());
  }

  source = v8_str(
      "var Y = class {\n"
      "  #baz = 2;\n"
      "}\n"
      "var X = class extends Y{\n"
      "  #foo = 1;\n"
      "  #bar = function() {};\n"
      "}\n"
      "var x = new X()");
  CompileRun(source);
  object = v8::Local<v8::Object>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "x"))
          .ToLocalChecked());
  private_names = v8::debug::GetPrivateFields(context, object).ToLocalChecked();

  for (int i = 0; i < 6; i = i + 2) {
    Handle<v8::internal::JSReceiver> private_name =
        v8::Utils::OpenHandle(*private_names->Get(context, i)
                                   .ToLocalChecked()
                                   ->ToObject(context)
                                   .ToLocalChecked());
    Handle<v8::internal::JSPrimitiveWrapper> private_value =
        Handle<v8::internal::JSPrimitiveWrapper>::cast(private_name);
    Handle<v8::internal::Symbol> priv_symbol(
        v8::internal::Symbol::cast(private_value->value()), isolate);
    CHECK(priv_symbol->is_private_name());
  }

  source = v8_str(
      "var Y = class {\n"
      "  constructor() {"
      "    return new Proxy({}, {});"
      "  }"
      "}\n"
      "var X = class extends Y{\n"
      "  #foo = 1;\n"
      "  #bar = function() {};\n"
      "}\n"
      "var x = new X()");
  CompileRun(source);
  object = v8::Local<v8::Object>::Cast(
      env->Global()
          ->Get(context, v8_str(env->GetIsolate(), "x"))
          .ToLocalChecked());
  private_names = v8::debug::GetPrivateFields(context, object).ToLocalChecked();

  for (int i = 0; i < 4; i = i + 2) {
    Handle<v8::internal::JSReceiver> private_name =
        v8::Utils::OpenHandle(*private_names->Get(context, i)
                                   .ToLocalChecked()
                                   ->ToObject(context)
                                   .ToLocalChecked());
    Handle<v8::internal::JSPrimitiveWrapper> private_value =
        Handle<v8::internal::JSPrimitiveWrapper>::cast(private_name);
    Handle<v8::internal::Symbol> priv_symbol(
        v8::internal::Symbol::cast(private_value->value()), isolate);
    CHECK(priv_symbol->is_private_name());
  }
}
