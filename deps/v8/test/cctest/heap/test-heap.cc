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

#include <utility>

#include "include/v8-function.h"
#include "src/api/api-inl.h"
#include "src/base/strings.h"
#include "src/base/strong-alias.h"
#include "src/builtins/builtins-inl.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/compiler.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/script-details.h"
#include "src/common/globals.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/flags/flags.h"
#include "src/handles/global-handles-inl.h"
#include "src/heap/combined-heap.h"
#include "src/heap/factory.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/heap-verifier.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/large-page-inl.h"
#include "src/heap/large-spaces.h"
#include "src/heap/local-heap-inl.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/marking-barrier.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/memory-reducer.h"
#include "src/heap/mutable-page.h"
#include "src/heap/remembered-set-inl.h"
#include "src/heap/safepoint.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/allocation-site.h"
#include "src/objects/call-site-info-inl.h"
#include "src/objects/elements.h"
#include "src/objects/field-type.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/managed-inl.h"
#include "src/objects/object-conversions-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/objects/slots.h"
#include "src/objects/transitions.h"
#include "src/regexp/regexp.h"
#include "src/snapshot/snapshot.h"
#include "src/tracing/tracing-category-observer.h"
#include "src/utils/ostreams.h"
#include "test/cctest/cctest.h"
#include "test/cctest/feedback-vector-helper.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"
#include "test/cctest/test-transitions.h"
#include "test/common/noop-bytecode-verifier.h"

namespace v8 {
namespace internal {
namespace heap {

// We only start allocation-site tracking with the second instantiation.
static const int kPretenureCreationCount =
    PretenuringHandler::GetMinMementoCountForTesting() + 1;







static void OptimizeEmptyFunction(const char* name) {
  HandleScope scope(CcTest::i_isolate());
  base::EmbeddedVector<char, 256> source;
  base::SNPrintF(source,
                 "function %s() { return 0; }"
                 "%%PrepareFunctionForOptimization(%s);"
                 "%s(); %s();"
                 "%%OptimizeFunctionOnNextCall(%s);"
                 "%s();",
                 name, name, name, name, name, name);
  CompileRun(source.begin());
}

// Count the number of native contexts in the weak list of native contexts.
int CountNativeContexts() {
  int count = 0;
  Tagged<Object> object = CcTest::heap()->native_contexts_list();
  while (!IsUndefined(object)) {
    count++;
    object = Cast<Context>(object)->next_context_link();
  }
  return count;
}

TEST(TestInternalWeakLists) {
  v8_flags.allow_natives_syntax = true;

  // Some flags turn Scavenge collections into Mark-sweep collections
  // and hence are incompatible with this test case.
  if (v8_flags.gc_global || v8_flags.stress_compaction ||
      v8_flags.stress_incremental_marking || v8_flags.single_generation ||
      v8_flags.stress_concurrent_allocation) {
    return;
  }
  v8_flags.retain_maps_for_n_gc = 0;

  static const int kNumTestContexts = 10;

  ManualGCScope manual_gc_scope;
  v8::Isolate* v8_isolate = CcTest::isolate();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = CcTest::heap();
  HandleScope scope(isolate);
  v8::Global<v8::Context> ctx[kNumTestContexts];
  if (!isolate->use_optimizer()) return;

  CHECK_EQ(0, CountNativeContexts());

  // Create a number of global contests which gets linked together.
  for (int i = 0; i < kNumTestContexts; i++) {
    // Create a handle scope so no contexts or function objects get stuck in the
    // outer handle scope.
    HandleScope new_scope(isolate);

    ctx[i].Reset(v8_isolate, v8::Context::New(v8_isolate));

    // Collect garbage that might have been created by one of the
    // installed extensions.
    isolate->compilation_cache()->Clear();
    {
      // In this test, we need to invoke GC without stack, otherwise some
      // objects may not be reclaimed because of conservative stack scanning.
      DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
      heap::InvokeMajorGC(heap);
    }

    CHECK_EQ(i + 1, CountNativeContexts());

    ctx[i].Get(v8_isolate)->Enter();

    OptimizeEmptyFunction("f1");
    OptimizeEmptyFunction("f2");
    OptimizeEmptyFunction("f3");
    OptimizeEmptyFunction("f4");
    OptimizeEmptyFunction("f5");

    // Remove function f1, and
    CompileRun("f1=null");

    // Scavenge treats these references as strong.
    for (int j = 0; j < 10; j++) {
      DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
      heap::InvokeMinorGC(heap);
    }

    // Mark compact handles the weak references.
    isolate->compilation_cache()->Clear();
    {
      DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
      heap::InvokeMajorGC(heap);
    }

    // Get rid of f3 and f5 in the same way.
    CompileRun("f3=null");
    {
      DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
      for (int j = 0; j < 10; j++) {
        heap::InvokeMinorGC(heap);
      }
      heap::InvokeMajorGC(heap);
    }
    CompileRun("f5=null");
    {
      DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
      for (int j = 0; j < 10; j++) {
        heap::InvokeMinorGC(heap);
      }
      heap::InvokeMajorGC(heap);
    }
    ctx[i].Get(v8_isolate)->Exit();
  }

  // Force compilation cache cleanup.
  heap->NotifyContextDisposed(true);
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    heap::InvokeMajorGC(heap);
  }

  // Dispose the native contexts one by one.
  for (int i = 0; i < kNumTestContexts; i++) {
    ctx[i].Reset();

    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);

    // Scavenge treats these references as strong.
    for (int j = 0; j < 10; j++) {
      heap::InvokeMinorGC(heap);
      CHECK_EQ(kNumTestContexts - i, CountNativeContexts());
    }
    // Mark-compact handles the weak references.
    heap::InvokeMajorGC(heap);

    CHECK_EQ(kNumTestContexts - i - 1, CountNativeContexts());
  }

  CHECK_EQ(0, CountNativeContexts());
}

static int forced_gc_counter = 0;

void MockUseCounterCallback(v8::Isolate* isolate,
                            v8::Isolate::UseCounterFeature feature) {
  isolate->GetCurrentContext();
  if (feature == v8::Isolate::kForcedGC) {
    forced_gc_counter++;
  }
}

TEST(CountForcedGC) {
  v8_flags.expose_gc = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  isolate->SetUseCounterCallback(MockUseCounterCallback);

  forced_gc_counter = 0;
  const char* source = "gc();";
  CompileRun(source);
  CHECK_GT(forced_gc_counter, 0);
}

#ifdef OBJECT_PRINT
TEST(PrintSharedFunctionInfo) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();
  const char* source =
      "f = function() { return 987654321; }\n"
      "g = function() { return 123456789; }\n";
  CompileRun(source);
  i::DirectHandle<JSFunction> g = i::Cast<JSFunction>(
      v8::Utils::OpenDirectHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(ctx, v8_str("g")).ToLocalChecked())));

  StdoutStream os;
  Print(g->shared(), os);
  os << std::endl;
}
#endif  // OBJECT_PRINT

TEST(IncrementalMarkingPreservesMonomorphicCallIC) {
  if (!v8_flags.use_ic) return;
  if (!v8_flags.incremental_marking) return;
  v8_flags.allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> fun1, fun2;
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();
  {
    CompileRun("function fun() {};");
    fun1 = CcTest::global()->Get(ctx, v8_str("fun")).ToLocalChecked();
  }

  {
    CompileRun("function fun() {};");
    fun2 = CcTest::global()->Get(ctx, v8_str("fun")).ToLocalChecked();
  }

  // Prepare function f that contains type feedback for the two closures.
  CHECK(CcTest::global()->Set(ctx, v8_str("fun1"), fun1).FromJust());
  CHECK(CcTest::global()->Set(ctx, v8_str("fun2"), fun2).FromJust());
  CompileRun(
      "function f(a, b) { a(); b(); } %EnsureFeedbackVectorForFunction(f); "
      "f(fun1, fun2);");

  DirectHandle<JSFunction> f = Cast<JSFunction>(
      v8::Utils::OpenDirectHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(ctx, v8_str("f")).ToLocalChecked())));

  Handle<FeedbackVector> feedback_vector(f->feedback_vector(),
                                         CcTest::i_isolate());
  FeedbackVectorHelper feedback_helper(feedback_vector);

  int expected_slots = 2;
  CHECK_EQ(expected_slots, feedback_helper.slot_count());
  int slot1 = 0;
  int slot2 = 1;
  CHECK(feedback_vector->Get(feedback_helper.slot(slot1)).IsWeak());
  CHECK(feedback_vector->Get(feedback_helper.slot(slot2)).IsWeak());

  heap::SimulateIncrementalMarking(CcTest::heap());
  heap::InvokeMajorGC(CcTest::heap());

  CHECK(feedback_vector->Get(feedback_helper.slot(slot1)).IsWeak());
  CHECK(feedback_vector->Get(feedback_helper.slot(slot2)).IsWeak());
}

static void CheckVectorIC(DirectHandle<JSFunction> f, int slot_index,
                          InlineCacheState desired_state) {
  Handle<FeedbackVector> vector =
      Handle<FeedbackVector>(f->feedback_vector(), CcTest::i_isolate());
  FeedbackVectorHelper helper(vector);
  FeedbackSlot slot = helper.slot(slot_index);
  FeedbackNexus nexus(CcTest::i_isolate(), vector, slot);
  CHECK(nexus.ic_state() == desired_state);
}

TEST(IncrementalMarkingPreservesMonomorphicIC) {
  if (!v8_flags.use_ic) return;
  if (!v8_flags.incremental_marking) return;
  v8_flags.allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();
  // Prepare function f that contains a monomorphic IC for object
  // originating from the same native context.
  CompileRun(
      "function fun() { this.x = 1; }; var obj = new fun();"
      "%EnsureFeedbackVectorForFunction(f);"
      "function f(o) { return o.x; } f(obj); f(obj);");
  DirectHandle<JSFunction> f = Cast<JSFunction>(
      v8::Utils::OpenDirectHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(ctx, v8_str("f")).ToLocalChecked())));

  CheckVectorIC(f, 0, InlineCacheState::MONOMORPHIC);

  heap::SimulateIncrementalMarking(CcTest::heap());
  heap::InvokeMajorGC(CcTest::heap());

  CheckVectorIC(f, 0, InlineCacheState::MONOMORPHIC);
}

TEST(IncrementalMarkingPreservesPolymorphicIC) {
  if (!v8_flags.use_ic) return;
  if (!v8_flags.incremental_marking) return;
  v8_flags.allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> obj1, obj2;
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 1; }; var obj = new fun();");
    obj1 = env->Global()->Get(env.local(), v8_str("obj")).ToLocalChecked();
  }

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 2; }; var obj = new fun();");
    obj2 = env->Global()->Get(env.local(), v8_str("obj")).ToLocalChecked();
  }

  // Prepare function f that contains a polymorphic IC for objects
  // originating from two different native contexts.
  CHECK(CcTest::global()->Set(ctx, v8_str("obj1"), obj1).FromJust());
  CHECK(CcTest::global()->Set(ctx, v8_str("obj2"), obj2).FromJust());
  CompileRun(
      "function f(o) { return o.x; }; "
      "%EnsureFeedbackVectorForFunction(f);"
      "f(obj1); f(obj1); f(obj2);");
  DirectHandle<JSFunction> f = Cast<JSFunction>(
      v8::Utils::OpenDirectHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(ctx, v8_str("f")).ToLocalChecked())));

  CheckVectorIC(f, 0, InlineCacheState::POLYMORPHIC);

  // Fire context dispose notification.
  heap::SimulateIncrementalMarking(CcTest::heap());
  heap::InvokeMajorGC(CcTest::heap());

  CheckVectorIC(f, 0, InlineCacheState::POLYMORPHIC);
}

TEST(ContextDisposeDoesntClearPolymorphicIC) {
  if (!v8_flags.use_ic) return;
  if (!v8_flags.incremental_marking) return;
  v8_flags.allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> obj1, obj2;
  v8::Local<v8::Context> ctx = CcTest::isolate()->GetCurrentContext();

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 1; }; var obj = new fun();");
    obj1 = env->Global()->Get(env.local(), v8_str("obj")).ToLocalChecked();
  }

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 2; }; var obj = new fun();");
    obj2 = env->Global()->Get(env.local(), v8_str("obj")).ToLocalChecked();
  }

  // Prepare function f that contains a polymorphic IC for objects
  // originating from two different native contexts.
  CHECK(CcTest::global()->Set(ctx, v8_str("obj1"), obj1).FromJust());
  CHECK(CcTest::global()->Set(ctx, v8_str("obj2"), obj2).FromJust());
  CompileRun(
      "function f(o) { return o.x; }; "
      "%EnsureFeedbackVectorForFunction(f);"
      "f(obj1); f(obj1); f(obj2);");
  DirectHandle<JSFunction> f = Cast<JSFunction>(
      v8::Utils::OpenDirectHandle(*v8::Local<v8::Function>::Cast(
          CcTest::global()->Get(ctx, v8_str("f")).ToLocalChecked())));

  CheckVectorIC(f, 0, InlineCacheState::POLYMORPHIC);

  // Fire context dispose notification.
  CcTest::isolate()->ContextDisposedNotification(
      v8::ContextDependants::kSomeDependants);
  heap::SimulateIncrementalMarking(CcTest::heap());
  heap::InvokeMajorGC(CcTest::heap());

  CheckVectorIC(f, 0, InlineCacheState::POLYMORPHIC);
}

class SourceResource : public v8::String::ExternalOneByteStringResource {
 public:
  explicit SourceResource(const char* data)
    : data_(data), length_(strlen(data)) { }

  void Dispose() override {
    i::DeleteArray(data_);
    data_ = nullptr;
  }

  const char* data() const override { return data_; }

  size_t length() const override { return length_; }

  bool IsDisposed() { return data_ == nullptr; }

 private:
  const char* data_;
  size_t length_;
};

void ReleaseStackTraceDataTest(v8::Isolate* isolate, const char* source,
                               const char* accessor) {
  // Test that the data retained by the Error.stack accessor is released
  // after the first time the accessor is fired.  We use external string
  // to check whether the data is being released since the external string
  // resource's callback is fired when the external string is GC'ed.
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Heap* heap = i_isolate->heap();
  v8::HandleScope scope(isolate);

  SourceResource* resource = new SourceResource(i::StrDup(source));
  {
    v8::HandleScope new_scope(isolate);
    v8::Local<v8::Context> ctx = isolate->GetCurrentContext();
    v8::Local<v8::String> source_string =
        v8::String::NewExternalOneByte(isolate, resource).ToLocalChecked();
    heap::InvokeMemoryReducingMajorGCs(heap);
    v8::Script::Compile(ctx, source_string)
        .ToLocalChecked()
        ->Run(ctx)
        .ToLocalChecked();
    CHECK(!resource->IsDisposed());
  }
  CHECK(!resource->IsDisposed());

  CompileRun(accessor);

  {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    heap::InvokeMemoryReducingMajorGCs(heap);
  }

  // External source has been released.
  CHECK(resource->IsDisposed());
  delete resource;
}

UNINITIALIZED_TEST(ReleaseStackTraceData) {
#ifndef V8_LITE_MODE
  // ICs retain objects.
  v8_flags.use_ic = false;
#endif  // V8_LITE_MODE
  v8_flags.concurrent_recompilation = false;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();
    static const char* source1 =
        "var error = null;            "
        /* Normal Error */
        "try {                        "
        "  throw new Error();         "
        "} catch (e) {                "
        "  error = e;                 "
        "}                            ";
    static const char* source2 =
        "var error = null;            "
        /* Stack overflow */
        "try {                        "
        "  (function f() { f(); })(); "
        "} catch (e) {                "
        "  error = e;                 "
        "}                            ";
    static const char* getter = "error.stack";
    static const char* setter = "error.stack = 0";

    ReleaseStackTraceDataTest(isolate, source1, setter);
    ReleaseStackTraceDataTest(isolate, source2, setter);
    ReleaseStackTraceDataTest(isolate, source1, getter);
    ReleaseStackTraceDataTest(isolate, source2, getter);
  }
  isolate->Dispose();
}


static int AllocationSitesCount(Heap* heap) {
  int count = 0;
  for (Tagged<Object> site = heap->allocation_sites_list();
       IsAllocationSite(site);) {
    Tagged<AllocationSiteWithWeakNext> cur =
        Cast<AllocationSiteWithWeakNext>(site);
    CHECK(cur->HasWeakNext());
    site = cur->weak_next();
    count++;
  }
  return count;
}

static int SlimAllocationSiteCount(Heap* heap) {
  int count = 0;
  for (Tagged<Object> weak_list = heap->allocation_sites_list();
       IsAllocationSite(weak_list);) {
    Tagged<AllocationSiteWithWeakNext> weak_cur =
        Cast<AllocationSiteWithWeakNext>(weak_list);
    for (Tagged<Object> site = weak_cur->nested_site();
         IsAllocationSite(site);) {
      Tagged<AllocationSite> cur = Cast<AllocationSite>(site);
      CHECK(!cur->HasWeakNext());
      site = cur->nested_site();
      count++;
    }
    weak_list = weak_cur->weak_next();
  }
  return count;
}

void CheckNumberOfAllocations(Heap* heap, const char* source,
                              int expected_full_alloc,
                              int expected_slim_alloc) {
  int prev_fat_alloc_count = AllocationSitesCount(heap);
  int prev_slim_alloc_count = SlimAllocationSiteCount(heap);

  CompileRun(source);

  int fat_alloc_sites = AllocationSitesCount(heap) - prev_fat_alloc_count;
  int slim_alloc_sites = SlimAllocationSiteCount(heap) - prev_slim_alloc_count;

  CHECK_EQ(expected_full_alloc, fat_alloc_sites);
  CHECK_EQ(expected_slim_alloc, slim_alloc_sites);
}

TEST(AllocationSiteCreation) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  i::v8_flags.allow_natives_syntax = true;

  // Array literals.
  CheckNumberOfAllocations(heap,
                           "function f1() {"
                           "  return []; "
                           "};"
                           "%EnsureFeedbackVectorForFunction(f1); f1();",
                           1, 0);
  CheckNumberOfAllocations(heap,
                           "function f2() {"
                           "  return [1, 2];"
                           "};"
                           "%EnsureFeedbackVectorForFunction(f2); f2();",
                           1, 0);
  CheckNumberOfAllocations(heap,
                           "function f3() {"
                           "  return [[1], [2]];"
                           "};"
                           "%EnsureFeedbackVectorForFunction(f3); f3();",
                           1, 2);
  CheckNumberOfAllocations(heap,
                           "function f4() { "
                           "return [0, [1, 1.1, 1.2, "
                           "], 1.5, [2.1, 2.2], 3];"
                           "};"
                           "%EnsureFeedbackVectorForFunction(f4); f4();",
                           1, 2);

  // Object literals have lazy AllocationSites
  CheckNumberOfAllocations(heap,
                           "function f5() {"
                           " return {};"
                           "};"
                           "%EnsureFeedbackVectorForFunction(f5); f5();",
                           0, 0);

  // No AllocationSites are created for the empty object literal.
  for (int i = 0; i < 5; i++) {
    CheckNumberOfAllocations(heap, "f5(); ", 0, 0);
  }

  CheckNumberOfAllocations(heap,
                           "function f6() {"
                           "  return {a:1};"
                           "};"
                           "%EnsureFeedbackVectorForFunction(f6); f6();",
                           0, 0);

  CheckNumberOfAllocations(heap, "f6(); ", 1, 0);

  CheckNumberOfAllocations(heap,
                           "function f7() {"
                           "  return {a:1, b:2};"
                           "};"
                           "%EnsureFeedbackVectorForFunction(f7); f7(); ",
                           0, 0);
  CheckNumberOfAllocations(heap, "f7(); ", 1, 0);

  // No Allocation sites are created for object subliterals
  CheckNumberOfAllocations(heap,
                           "function f8() {"
                           "return {a:{}, b:{ a:2, c:{ d:{f:{}}} } }; "
                           "};"
                           "%EnsureFeedbackVectorForFunction(f8); f8();",
                           0, 0);
  CheckNumberOfAllocations(heap, "f8(); ", 1, 0);

  // We currently eagerly create allocation sites if there are sub-arrays.
  // Allocation sites are created only for array subliterals
  CheckNumberOfAllocations(heap,
                           "function f9() {"
                           "return {a:[1, 2, 3], b:{ a:2, c:{ d:{f:[]} } }}; "
                           "};"
                           "%EnsureFeedbackVectorForFunction(f9); f9(); ",
                           1, 2);

  // No new AllocationSites created on the second invocation.
  CheckNumberOfAllocations(heap, "f9(); ", 0, 0);
}

TEST(CellsInOptimizedCodeAreWeak) {
  v8_flags.allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_optimizer()) return;
  HandleScope outer_scope(heap->isolate());
  IndirectHandle<Code> code;
  {
    LocalContext context;
    HandleScope scope(heap->isolate());

    CompileRun(
        "bar = (function() {"
        "  function bar() {"
        "    return foo(1);"
        "  };"
        "  %PrepareFunctionForOptimization(bar);"
        "  var foo = function(x) { with (x) { return 1 + x; } };"
        "  %NeverOptimizeFunction(foo);"
        "  bar(foo);"
        "  bar(foo);"
        "  bar(foo);"
        "  %OptimizeFunctionOnNextCall(bar);"
        "  bar(foo);"
        "  return bar;})();");

    DirectHandle<JSFunction> bar = Cast<JSFunction>(v8::Utils::OpenDirectHandle(
        *v8::Local<v8::Function>::Cast(CcTest::global()
                                           ->Get(context.local(), v8_str("bar"))
                                           .ToLocalChecked())));
    code = handle(bar->code(isolate), isolate);
    code = scope.CloseAndEscape(code);
  }

  // Now make sure that a gc should get rid of the function
  for (int i = 0; i < 4; i++) {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    heap::InvokeMajorGC(heap);
  }

  CHECK(code->marked_for_deoptimization());
  CHECK(code->embedded_objects_cleared());
}

TEST(ObjectsInOptimizedCodeAreWeak) {
  v8_flags.allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_optimizer()) return;
  HandleScope outer_scope(heap->isolate());
  IndirectHandle<Code> code;
  {
    LocalContext context;
    HandleScope scope(heap->isolate());

    CompileRun(
        "function bar() {"
        "  return foo(1);"
        "};"
        "%PrepareFunctionForOptimization(bar);"
        "function foo(x) { with (x) { return 1 + x; } };"
        "%NeverOptimizeFunction(foo);"
        "bar();"
        "bar();"
        "bar();"
        "%OptimizeFunctionOnNextCall(bar);"
        "bar();");

    DirectHandle<JSFunction> bar = Cast<JSFunction>(v8::Utils::OpenDirectHandle(
        *v8::Local<v8::Function>::Cast(CcTest::global()
                                           ->Get(context.local(), v8_str("bar"))
                                           .ToLocalChecked())));
    code = handle(bar->code(isolate), isolate);
    code = scope.CloseAndEscape(code);
  }

  // Now make sure that a gc should get rid of the function
  for (int i = 0; i < 4; i++) {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    heap::InvokeMajorGC(heap);
  }

  CHECK(code->marked_for_deoptimization());
  CHECK(code->embedded_objects_cleared());
}

TEST(NewSpaceObjectsInOptimizedCode) {
  if (v8_flags.single_generation) return;
  v8_flags.allow_natives_syntax = true;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_optimizer()) return;
  HandleScope outer_scope(isolate);
  IndirectHandle<Code> code;
  {
    LocalContext context;
    HandleScope scope(isolate);

    CompileRun(
        "var foo;"
        "var bar;"
        "(function() {"
        "  function foo_func(x) { with (x) { return 1 + x; } };"
        "  %NeverOptimizeFunction(foo_func);"
        "  function bar_func() {"
        "    return foo(1);"
        "  };"
        "  %PrepareFunctionForOptimization(bar_func);"
        "  bar = bar_func;"
        "  foo = foo_func;"
        "  bar_func();"
        "  bar_func();"
        "  bar_func();"
        "  %OptimizeFunctionOnNextCall(bar_func);"
        "  bar_func();"
        "})();");

    DirectHandle<JSFunction> bar = Cast<JSFunction>(v8::Utils::OpenDirectHandle(
        *v8::Local<v8::Function>::Cast(CcTest::global()
                                           ->Get(context.local(), v8_str("bar"))
                                           .ToLocalChecked())));

    DirectHandle<JSFunction> foo = Cast<JSFunction>(v8::Utils::OpenDirectHandle(
        *v8::Local<v8::Function>::Cast(CcTest::global()
                                           ->Get(context.local(), v8_str("foo"))
                                           .ToLocalChecked())));

    CHECK(HeapLayout::InYoungGeneration(*foo));
    heap::InvokeMajorGC(heap);
    CHECK(!HeapLayout::InYoungGeneration(*foo));
#ifdef VERIFY_HEAP
    HeapVerifier::VerifyHeap(CcTest::heap());
#endif
    CHECK(!bar->code(isolate)->marked_for_deoptimization());
    code = handle(bar->code(isolate), isolate);
    code = scope.CloseAndEscape(code);
  }

  // Now make sure that a gc should get rid of the function
  for (int i = 0; i < 4; i++) {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    heap::InvokeMajorGC(heap);
  }

  CHECK(code->marked_for_deoptimization());
  CHECK(code->embedded_objects_cleared());
}

TEST(ObjectsInEagerlyDeoptimizedCodeAreWeak) {
  v8_flags.allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_optimizer()) return;
  HandleScope outer_scope(heap->isolate());
  IndirectHandle<Code> code;
  {
    LocalContext context;
    HandleScope scope(heap->isolate());

    CompileRun(
        "function bar() {"
        "  return foo(1);"
        "};"
        "function foo(x) { with (x) { return 1 + x; } };"
        "%NeverOptimizeFunction(foo);"
        "%PrepareFunctionForOptimization(bar);"
        "bar();"
        "bar();"
        "bar();"
        "%OptimizeFunctionOnNextCall(bar);"
        "bar();");

    DirectHandle<JSFunction> bar = Cast<JSFunction>(v8::Utils::OpenDirectHandle(
        *v8::Local<v8::Function>::Cast(CcTest::global()
                                           ->Get(context.local(), v8_str("bar"))
                                           .ToLocalChecked())));
    code = handle(bar->code(isolate), isolate);
    CompileRun("%DeoptimizeFunction(bar);");
    CHECK(code->marked_for_deoptimization());
    CHECK(!(*code).SafeEquals(bar->code(isolate)));
    code = scope.CloseAndEscape(code);
  }

  // Now make sure that a gc should get rid of the function
  for (int i = 0; i < 4; i++) {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    heap::InvokeMajorGC(heap);
  }

  CHECK(code->marked_for_deoptimization());
  CHECK(code->embedded_objects_cleared());
}

static DirectHandle<InstructionStream> DummyOptimizedCode(Isolate* isolate) {
  uint8_t buffer[i::Assembler::kDefaultBufferSize];
  MacroAssembler masm(isolate, v8::internal::CodeObjectRequired{true},
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));
  CodeDesc desc;
#if V8_TARGET_ARCH_ARM64
  UseScratchRegisterScope temps(&masm);
  Register tmp = temps.AcquireX();
  masm.Mov(tmp, Operand(isolate->factory()->undefined_value()));
  masm.Push(tmp, tmp);
#else
  masm.Push(isolate->factory()->undefined_value());
  masm.Push(isolate->factory()->undefined_value());
#endif
  masm.Drop(2);
  masm.GetCode(isolate, &desc);
  DirectHandle<InstructionStream> code(
      Factory::CodeBuilder(isolate, desc, CodeKind::TURBOFAN_JS)
          .set_self_reference(masm.CodeObject())
          .set_empty_source_position_table()
          .set_deoptimization_data(DeoptimizationData::Empty(isolate))
          .Build()
          ->instruction_stream(),
      isolate);
  CHECK(IsInstructionStream(*code));
  return code;
}


DirectHandle<JSFunction> GetFunctionByName(Isolate* isolate, const char* name) {
  DirectHandle<String> str = isolate->factory()->InternalizeUtf8String(name);
  DirectHandle<Object> obj =
      Object::GetProperty(isolate, isolate->global_object(), str)
          .ToHandleChecked();
  return Cast<JSFunction>(obj);
}

void CheckIC(DirectHandle<JSFunction> function, int slot_index,
             InlineCacheState state) {
  Tagged<FeedbackVector> vector = function->feedback_vector();
  FeedbackSlot slot(slot_index);
  FeedbackNexus nexus(CcTest::i_isolate(), vector, slot);
  CHECK_EQ(nexus.ic_state(), state);
}

TEST(MonomorphicStaysMonomorphicAfterGC) {
  if (!v8_flags.use_ic) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  v8_flags.allow_natives_syntax = true;
  CompileRun(
      "function loadIC(obj) {"
      "  return obj.name;"
      "}"
      "%EnsureFeedbackVectorForFunction(loadIC);"
      "function testIC() {"
      "  var proto = {'name' : 'weak'};"
      "  var obj = Object.create(proto);"
      "  loadIC(obj);"
      "  loadIC(obj);"
      "  loadIC(obj);"
      "  return proto;"
      "};");
  DirectHandle<JSFunction> loadIC = GetFunctionByName(isolate, "loadIC");
  {
    v8::HandleScope new_scope(CcTest::isolate());
    CompileRun("(testIC())");
  }
  heap::InvokeMajorGC(CcTest::heap());
  CheckIC(loadIC, 0, InlineCacheState::MONOMORPHIC);
  {
    v8::HandleScope new_scope(CcTest::isolate());
    CompileRun("(testIC())");
  }
  CheckIC(loadIC, 0, InlineCacheState::MONOMORPHIC);
}

TEST(PolymorphicStaysPolymorphicAfterGC) {
  if (!v8_flags.use_ic) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  v8_flags.allow_natives_syntax = true;
  CompileRun(
      "function loadIC(obj) {"
      "  return obj.name;"
      "}"
      "%EnsureFeedbackVectorForFunction(loadIC);"
      "function testIC() {"
      "  var proto = {'name' : 'weak'};"
      "  var obj = Object.create(proto);"
      "  loadIC(obj);"
      "  loadIC(obj);"
      "  loadIC(obj);"
      "  var poly = Object.create(proto);"
      "  poly.x = true;"
      "  loadIC(poly);"
      "  return proto;"
      "};");
  DirectHandle<JSFunction> loadIC = GetFunctionByName(isolate, "loadIC");
  {
    v8::HandleScope new_scope(CcTest::isolate());
    CompileRun("(testIC())");
  }
  heap::InvokeMajorGC(CcTest::heap());
  CheckIC(loadIC, 0, InlineCacheState::POLYMORPHIC);
  {
    v8::HandleScope new_scope(CcTest::isolate());
    CompileRun("(testIC())");
  }
  CheckIC(loadIC, 0, InlineCacheState::POLYMORPHIC);
}

#ifdef DEBUG
TEST(AddInstructionChangesNewSpacePromotion) {
  v8_flags.allow_natives_syntax = true;
  v8_flags.expose_gc = true;
  v8_flags.stress_compaction = true;
  FlagList::EnforceFlagImplications();
  HeapAllocator::SetAllocationGcInterval(1000);
  CcTest::InitializeVM();
  if (!v8_flags.allocation_site_pretenuring) return;
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  LocalContext env;
  CompileRun(
      "function add(a, b) {"
      "  return a + b;"
      "}"
      "add(1, 2);"
      "add(\"a\", \"b\");"
      "var oldSpaceObject;"
      "gc();"
      "function crash(x) {"
      "  var object = {a: null, b: null};"
      "  var result = add(1.5, x | 0);"
      "  object.a = result;"
      "  oldSpaceObject = object;"
      "  return object;"
      "}"
      "%PrepareFunctionForOptimization(crash);"
      "crash(1);"
      "crash(1);"
      "%OptimizeFunctionOnNextCall(crash);"
      "crash(1);");

  v8::Local<v8::Object> global = CcTest::global();
  v8::Local<v8::Function> g = v8::Local<v8::Function>::Cast(
      global->Get(env.local(), v8_str("crash")).ToLocalChecked());
  v8::Local<v8::Value> info1[] = {v8_num(1)};
  heap->DisableInlineAllocation();
  heap->set_allocation_timeout(1);
  g->Call(env.local(), global, 1, info1).ToLocalChecked();
  heap::InvokeMajorGC(heap);
}

void OnFatalErrorExpectOOM(const char* location, const char* message) {
  // Exit with 0 if the location matches our expectation.
  exit(strcmp(location, "CALL_AND_RETRY_LAST"));
}

TEST(CEntryStubOOM) {
  v8_flags.allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CcTest::isolate()->SetFatalErrorHandler(OnFatalErrorExpectOOM);

  v8::Local<v8::Value> result = CompileRun(
      "%SetAllocationTimeout(1, 1);"
      "var a = [];"
      "a.__proto__ = [];"
      "a.unshift(1)");

  CHECK(result->IsNumber());
}

#endif  // DEBUG

static void InterruptCallback357137(v8::Isolate* isolate, void* data) { }

static void RequestInterrupt(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  CcTest::isolate()->RequestInterrupt(&InterruptCallback357137, nullptr);
}

HEAP_TEST(Regress538257) {
  ManualGCScope manual_gc_scope;
  heap::ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);
  v8::Isolate::CreateParams create_params;
  // Set heap limits.
  create_params.constraints.set_max_young_generation_size_in_bytes(3 * MB);
#ifdef DEBUG
  create_params.constraints.set_max_old_generation_size_in_bytes(20 * MB);
#else
  create_params.constraints.set_max_old_generation_size_in_bytes(6 * MB);
#endif
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  isolate->Enter();
  {
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
    Heap* heap = i_isolate->heap();
    HandleScope handle_scope(i_isolate);
    PagedSpace* old_space = heap->old_space();
    const int kMaxObjects = 10000;
    const int kFixedArrayLen = 512;
    Handle<FixedArray> objects[kMaxObjects];
    for (int i = 0; (i < kMaxObjects) &&
                    heap->CanExpandOldGeneration(old_space->AreaSize());
         i++) {
      objects[i] = i_isolate->factory()->NewFixedArray(kFixedArrayLen,
                                                       AllocationType::kOld);
      heap::ForceEvacuationCandidate(NormalPage::FromHeapObject(*objects[i]));
    }
    heap::SimulateFullSpace(old_space);
    heap::InvokeMajorGC(heap);
    // If we get this far, we've successfully aborted compaction. Any further
    // allocations might trigger OOM.
  }
  isolate->Exit();
  isolate->Dispose();
}

TEST(Regress357137) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope hscope(isolate);
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->Set(isolate, "interrupt",
              v8::FunctionTemplate::New(isolate, RequestInterrupt));
  v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
  CHECK(!context.IsEmpty());
  v8::Context::Scope cscope(context);

  v8::Local<v8::Value> result = CompileRun(
      "var locals = '';"
      "for (var i = 0; i < 512; i++) locals += 'var v' + i + '= 42;';"
      "eval('function f() {' + locals + 'return function() { return v0; }; }');"
      "interrupt();"  // This triggers a fake stack overflow in f.
      "f()()");
  CHECK_EQ(42.0, result->ToNumber(context).ToLocalChecked()->Value());
}

TEST(Regress507979) {
  const int kFixedArrayLen = 10;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handle_scope(isolate);

  DirectHandle<FixedArray> o1 =
      isolate->factory()->NewFixedArray(kFixedArrayLen);
  DirectHandle<FixedArray> o2 =
      isolate->factory()->NewFixedArray(kFixedArrayLen);
  CHECK(InCorrectGeneration(*o1));
  CHECK(InCorrectGeneration(*o2));

  HeapObjectIterator it(isolate->heap(),
                        i::HeapObjectIterator::kFilterUnreachable);

  // Replace parts of an object placed before a live object with a filler. This
  // way the filler object shares the mark bits with the following live object.
  o1->RightTrim(isolate, kFixedArrayLen - 1);

  for (Tagged<HeapObject> obj = it.Next(); !obj.is_null(); obj = it.Next()) {
    // Let's not optimize the loop away.
    CHECK_NE(obj.address(), kNullAddress);
  }
}

TEST(Regress388880) {
  if (!v8_flags.incremental_marking) return;
  v8_flags.stress_incremental_marking = false;
  v8_flags.expose_gc = true;
  v8_flags.stress_concurrent_allocation = false;  // For SimulateFullSpace.
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  DirectHandle<Map> map1 = Map::Create(isolate, 1);
  Handle<String> name = factory->NewStringFromStaticChars("foo");
  name = factory->InternalizeString(name);
  DirectHandle<Map> map2 =
      Map::CopyWithField(isolate, map1, name, FieldType::Any(isolate), NONE,
                         PropertyConstness::kMutable, Representation::Tagged(),
                         OMIT_TRANSITION)
          .ToHandleChecked();

  size_t desired_offset = NormalPage::kPageSize - map1->instance_size();

  // Allocate padding objects in old pointer space so, that object allocated
  // afterwards would end at the end of the page.
  heap::SimulateFullSpace(heap->old_space());
  size_t padding_size =
      desired_offset - MemoryChunkLayout::ObjectStartOffsetInDataPage();
  heap::CreatePadding(heap, static_cast<int>(padding_size),
                      AllocationType::kOld);

  DirectHandle<JSObject> o =
      factory->NewJSObjectFromMap(map1, AllocationType::kOld);
  o->set_raw_properties_or_hash(*factory->empty_fixed_array());

  // Ensure that the object allocated where we need it.
  NormalPage* page = NormalPage::FromHeapObject(*o);
  CHECK_EQ(desired_offset, page->Offset(o->address()));

  // Now we have an object right at the end of the page.

  // Enable incremental marking to trigger actions in Heap::AdjustLiveBytes()
  // that would cause crash.
  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  CcTest::heap()->StartIncrementalMarking(i::GCFlag::kNoFlags,
                                          i::GarbageCollectionReason::kTesting);
  CHECK(marking->IsMarking());

  // Now everything is set up for crashing in JSObject::MigrateFastToFast()
  // when it calls heap->AdjustLiveBytes(...).
  JSObject::MigrateToMap(isolate, o, map2);
}

TEST(Regress3631) {
  if (!v8_flags.incremental_marking) return;
  v8_flags.expose_gc = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  v8::Local<v8::Value> result = CompileRun(
      "var weak_map = new WeakMap();"
      "var future_keys = [];"
      "for (var i = 0; i < 50; i++) {"
      "  var key = {'k' : i + 0.1};"
      "  weak_map.set(key, 1);"
      "  future_keys.push({'x' : i + 0.2});"
      "}"
      "weak_map");
  if (marking->IsStopped()) {
    CcTest::heap()->StartIncrementalMarking(
        i::GCFlag::kNoFlags, i::GarbageCollectionReason::kTesting);
  }
  // Incrementally mark the backing store.
  DirectHandle<JSReceiver> obj =
      v8::Utils::OpenDirectHandle(*v8::Local<v8::Object>::Cast(result));
  DirectHandle<JSWeakCollection> weak_map(Cast<JSWeakCollection>(*obj),
                                          isolate);
  SimulateIncrementalMarking(heap);
  // Stash the backing store in a handle.
  DirectHandle<Object> save(weak_map->table(), isolate);
  // The following line will update the backing store.
  CompileRun(
      "for (var i = 0; i < 50; i++) {"
      "  weak_map.set(future_keys[i], i);"
      "}");
  heap::InvokeMajorGC(heap);
}

TEST(Regress442710) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  HandleScope sc(isolate);
  DirectHandle<JSGlobalObject> global(
      CcTest::i_isolate()->context()->global_object(), isolate);
  DirectHandle<JSArray> array = factory->NewJSArray(2);

  DirectHandle<String> name = factory->InternalizeUtf8String("testArray");
  Object::SetProperty(isolate, global, name, array).Check();
  CompileRun("testArray[0] = 1; testArray[1] = 2; testArray.shift();");
  heap::InvokeMajorGC(CcTest::heap());
}

HEAP_TEST(NumberStringCacheSize) {
  // Test that the number-string cache has not been resized in the snapshot.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  if (!isolate->snapshot_available()) return;
  Heap* heap = isolate->heap();
  CHECK_EQ(SmiStringCache::kInitialSize, heap->smi_string_cache()->capacity());
  CHECK_EQ(DoubleStringCache::kInitialSize,
           heap->double_string_cache()->capacity());
}

TEST(Regress3877) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  CompileRun("function cls() { this.x = 10; }");
  IndirectHandle<WeakFixedArray> weak_prototype_holder =
      factory->NewWeakFixedArray(1);
  {
    HandleScope inner_scope(isolate);
    v8::Local<v8::Value> result = CompileRun("cls.prototype");
    DirectHandle<JSReceiver> proto =
        v8::Utils::OpenDirectHandle(*v8::Local<v8::Object>::Cast(result));
    weak_prototype_holder->set(0, MakeWeak(*proto));
  }
  CHECK(!weak_prototype_holder->get(0).IsCleared());
  CompileRun(
      "var a = { };"
      "a.x = new cls();"
      "cls.prototype = null;");
  for (int i = 0; i < 4; i++) {
    heap::InvokeMajorGC(heap);
  }
  // The map of a.x keeps prototype alive
  CHECK(!weak_prototype_holder->get(0).IsCleared());
  // Detach the map (by promoting it to a prototype).
  CompileRun("var b = {}; b.__proto__ = a.x");
  // Change the map of a.x and make the previous map garbage collectable.
  CompileRun("a.x.__proto__ = {};");

  for (int i = 0; i < 4; i++) {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    heap::InvokeMajorGC(heap);
  }
  CHECK(weak_prototype_holder->get(0).IsCleared());
}

Handle<WeakFixedArray> AddRetainedMap(Isolate* isolate,
                                      DirectHandle<NativeContext> context) {
  HandleScope inner_scope(isolate);
  DirectHandle<Map> map = Map::Create(isolate, 1);
  v8::Local<v8::Value> result =
      CompileRun("(function () { return {x : 10}; })();");
  DirectHandle<JSReceiver> proto =
      v8::Utils::OpenDirectHandle(*v8::Local<v8::Object>::Cast(result));
  Map::SetPrototype(isolate, map, proto);
  GlobalHandleVector<Map> maps(isolate->heap());
  maps.Push(*map);
  isolate->heap()->AddRetainedMaps(context, std::move(maps));
  Handle<WeakFixedArray> array = isolate->factory()->NewWeakFixedArray(1);
  array->set(0, MakeWeak(*map));
  return inner_scope.CloseAndEscape(array);
}

void CheckMapRetainingFor(int n) {
  v8_flags.retain_maps_for_n_gc = n;
  v8::Isolate* isolate = CcTest::isolate();
  Isolate* i_isolate = CcTest::i_isolate();
  Heap* heap = i_isolate->heap();

  IndirectHandle<NativeContext> native_context;
  // This global is used to visit the object's constructor alive when starting
  // incremental marking. The native context keeps the constructor alive. The
  // constructor needs to be alive to retain the map.
  v8::Global<v8::Context> global_ctxt;

  {
    v8::Local<v8::Context> ctx = v8::Context::New(isolate);
    IndirectHandle<Context> context = Utils::OpenIndirectHandle(*ctx);
    CHECK(IsNativeContext(*context));
    native_context = Cast<NativeContext>(context);
    global_ctxt.Reset(isolate, ctx);
    ctx->Enter();
  }

  IndirectHandle<WeakFixedArray> array_with_map =
      AddRetainedMap(i_isolate, native_context);
  CHECK(array_with_map->get(0).IsWeak());
  for (int i = 0; i < n; i++) {
    heap::SimulateIncrementalMarking(heap);
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    heap::InvokeMajorGC(heap);
  }
  CHECK(array_with_map->get(0).IsWeak());
  {
    heap::SimulateIncrementalMarking(heap);
    // In this test, we need to invoke GC without stack, otherwise some objects
    // may not be reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    heap::InvokeMajorGC(heap);
  }
  CHECK(array_with_map->get(0).IsCleared());

  global_ctxt.Get(isolate)->Exit();
}

TEST(MapRetaining) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CheckMapRetainingFor(v8_flags.retain_maps_for_n_gc);
  CheckMapRetainingFor(0);
  CheckMapRetainingFor(1);
  CheckMapRetainingFor(7);
}

TEST(RetainedMapsCleanup) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  v8::Local<v8::Context> ctx = v8::Context::New(CcTest::isolate());
  DirectHandle<Context> context = Utils::OpenDirectHandle(*ctx);
  CHECK(IsNativeContext(*context));
  DirectHandle<NativeContext> native_context = Cast<NativeContext>(context);

  ctx->Enter();
  DirectHandle<WeakFixedArray> array_with_map =
      AddRetainedMap(isolate, native_context);
  CHECK(array_with_map->get(0).IsWeak());
  heap->NotifyContextDisposed(true);
  heap::InvokeMajorGC(heap);
  ctx->Exit();

  CHECK_EQ(ReadOnlyRoots(heap).empty_weak_array_list(),
           native_context->retained_maps());
}

TEST(PreprocessStackTrace) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::TryCatch try_catch(CcTest::isolate());
  CompileRun("throw new Error();");
  CHECK(try_catch.HasCaught());
  Isolate* isolate = CcTest::i_isolate();
  DirectHandle<JSAny> exception =
      Cast<JSAny>(v8::Utils::OpenHandle(*try_catch.Exception()));
  DirectHandle<Name> key = isolate->factory()->error_stack_symbol();
  DirectHandle<JSAny> stack_trace = Cast<JSAny>(
      Object::GetProperty(isolate, exception, key).ToHandleChecked());
  DirectHandle<Object> code =
      Object::GetElement(isolate, stack_trace, 3).ToHandleChecked();
  CHECK(IsInstructionStream(*code));

  heap::InvokeMemoryReducingMajorGCs(CcTest::heap());

  DirectHandle<Object> pos =
      Object::GetElement(isolate, stack_trace, 3).ToHandleChecked();
  CHECK(IsSmi(*pos));

  DirectHandle<FixedArray> frame_array = Cast<FixedArray>(stack_trace);
  const uint32_t array_length = frame_array->length().value();
  for (uint32_t i = 0; i < array_length; i++) {
    DirectHandle<Object> element =
        Object::GetElement(isolate, stack_trace, i).ToHandleChecked();
    CHECK(!IsInstructionStream(*element));
  }
}

void AllocateInSpace(Isolate* isolate, size_t bytes, AllocationSpace space) {
  CHECK_LE(OFFSET_OF_DATA_START(FixedArray), bytes);
  CHECK(IsAligned(bytes, kTaggedSize));
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  AlwaysAllocateScopeForTesting always_allocate(isolate->heap());
  int elements = static_cast<int>((bytes - OFFSET_OF_DATA_START(FixedArray)) /
                                  kTaggedSize);
  DirectHandle<FixedArray> array = factory->NewFixedArray(
      elements,
      space == NEW_SPACE ? AllocationType::kYoung : AllocationType::kOld);
  CHECK((space == NEW_SPACE) == HeapLayout::InYoungGeneration(*array));
  CHECK_EQ(bytes, static_cast<size_t>(array->Size()));
}

TEST(NewSpaceAllocationCounter) {
  if (v8_flags.single_generation) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  heap->FreeMainThreadLinearAllocationAreas();
  size_t counter1 = heap->NewSpaceAllocationCounter();
  heap::EmptyNewSpaceUsingGC(heap);  // Ensure new space is empty.
  const size_t kSize = 1024;
  AllocateInSpace(isolate, kSize, NEW_SPACE);
  heap->FreeMainThreadLinearAllocationAreas();
  size_t counter2 = heap->NewSpaceAllocationCounter();
  CHECK_EQ(kSize, counter2 - counter1);
  heap::InvokeMinorGC(heap);
  size_t counter3 = heap->NewSpaceAllocationCounter();
  CHECK_EQ(0U, counter3 - counter2);
  // Test counter overflow.
  heap->FreeMainThreadLinearAllocationAreas();
  size_t max_counter = static_cast<size_t>(-1);
  heap->SetNewSpaceAllocationCounterForTesting(max_counter - 10 * kSize);
  size_t start = heap->NewSpaceAllocationCounter();
  for (int i = 0; i < 20; i++) {
    AllocateInSpace(isolate, kSize, NEW_SPACE);
    heap->FreeMainThreadLinearAllocationAreas();
    size_t counter = heap->NewSpaceAllocationCounter();
    CHECK_EQ(kSize, counter - start);
    start = counter;
  }
}

TEST(OldSpaceAllocationCounter) {
  // Using the string forwarding table can free allocations during sweeping, due
  // to ThinString trimming, thus failing this test.
  // The flag (and handling of the forwarding table/ThinString transitions in
  // young gen) is only temporary so we just skip this test for now.
  if (v8_flags.always_use_string_forwarding_table) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  // Disable LAB, such that calculations with SizeOfObjects() and object size
  // are correct.
  heap->DisableInlineAllocation();
  heap::EmptyNewSpaceUsingGC(heap);
  size_t counter1 = heap->OldGenerationAllocationCounter();
  const size_t kSize = 1024;
  AllocateInSpace(isolate, kSize, OLD_SPACE);
  size_t counter2 = heap->OldGenerationAllocationCounter();
  // TODO(ulan): replace all CHECK_LE with CHECK_EQ after v8:4148 is fixed.
  CHECK_LE(kSize, counter2 - counter1);
  heap::InvokeMinorGC(heap);
  size_t counter3 = heap->OldGenerationAllocationCounter();
  CHECK_EQ(0u, counter3 - counter2);
  AllocateInSpace(isolate, kSize, OLD_SPACE);
  heap::InvokeMajorGC(heap);
  size_t counter4 = heap->OldGenerationAllocationCounter();
  CHECK_LE(kSize, counter4 - counter3);
  // Test counter overflow.
  size_t max_counter = static_cast<size_t>(-1);
  heap->set_old_generation_allocation_counter_at_last_gc(max_counter -
                                                         10 * kSize);
  size_t start = heap->OldGenerationAllocationCounter();
  for (int i = 0; i < 20; i++) {
    AllocateInSpace(isolate, kSize, OLD_SPACE);
    size_t counter = heap->OldGenerationAllocationCounter();
    CHECK_LE(kSize, counter - start);
    start = counter;
  }
}

static void CheckLeak(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  Isolate* isolate = CcTest::i_isolate();
  Tagged<Object> message(
      *reinterpret_cast<Address*>(isolate->pending_message_address()));
  CHECK(IsTheHole(message));
}

TEST(MessageObjectLeak) {
  CcTest::InitializeVM();
  v8_flags.allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->Set(isolate, "check", v8::FunctionTemplate::New(isolate, CheckLeak));
  v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
  v8::Context::Scope cscope(context);

  const char* test =
      "function test() {"
      "  try {"
      "    throw 'message 1';"
      "  } catch (e) {"
      "  }"
      "  check();"
      "  L: try {"
      "    throw 'message 2';"
      "  } finally {"
      "    break L;"
      "  }"
      "  check();"
      "}"
      "%PrepareFunctionForOptimization(test);"
      "test()";
  CompileRun(test);

  const char* test2 =
      "%OptimizeFunctionOnNextCall(test);"
      "test()";
  CompileRun(test2);
}

static void CheckEqualSharedFunctionInfos(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  DirectHandle<Object> obj1 = v8::Utils::OpenDirectHandle(*info[0]);
  DirectHandle<Object> obj2 = v8::Utils::OpenDirectHandle(*info[1]);
  DirectHandle<JSFunction> fun1 = Cast<JSFunction>(obj1);
  DirectHandle<JSFunction> fun2 = Cast<JSFunction>(obj2);
  CHECK(fun1->shared() == fun2->shared());
}

static void RemoveCodeAndGC(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  Isolate* isolate = CcTest::i_isolate();
  DirectHandle<Object> obj = v8::Utils::OpenDirectHandle(*info[0]);
  DirectHandle<JSFunction> fun = Cast<JSFunction>(obj);
  // Bytecode is code too.
  SharedFunctionInfo::DiscardCompiled(isolate,
                                      direct_handle(fun->shared(), isolate));
  fun->UpdateCode(isolate, *BUILTIN_CODE(isolate, CompileLazy));
  heap::InvokeMemoryReducingMajorGCs(CcTest::heap());
}

TEST(CanonicalSharedFunctionInfo) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->Set(
      isolate, "check",
      v8::FunctionTemplate::New(isolate, CheckEqualSharedFunctionInfos));
  global->Set(isolate, "remove",
              v8::FunctionTemplate::New(isolate, RemoveCodeAndGC));
  v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
  v8::Context::Scope cscope(context);
  CompileRun(
      "function f() { return function g() {}; }"
      "var g1 = f();"
      "remove(f);"
      "var g2 = f();"
      "check(g1, g2);");

  CompileRun(
      "function f() { return (function() { return function g() {}; })(); }"
      "var g1 = f();"
      "remove(f);"
      "var g2 = f();"
      "check(g1, g2);");
}

TEST(ScriptIterator) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = CcTest::heap();
  LocalContext context;

  heap::InvokeMajorGC(heap);

  int script_count = 0;
  {
    HeapObjectIterator it(heap);
    for (Tagged<HeapObject> obj = it.Next(); !obj.is_null(); obj = it.Next()) {
      if (IsScript(obj)) script_count++;
    }
  }

  {
    Script::Iterator iterator(isolate);
    for (Tagged<Script> script = iterator.Next(); !script.is_null();
         script = iterator.Next()) {
      script_count--;
    }
  }

  CHECK_EQ(0, script_count);
}

// This is the same as Factory::NewByteArray, except it doesn't retry on
// allocation failure.
AllocationResult HeapTester::AllocateByteArrayForTest(
    Heap* heap, uint32_t length, AllocationType allocation_type) {
  DCHECK_LE(length, ByteArray::kMaxLength);
  int size = ByteArray::SizeFor(length);
  Tagged<HeapObject> result;
  {
    AllocationResult allocation = heap->AllocateRaw(size, allocation_type);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map_after_allocation(heap->isolate(),
                                   ReadOnlyRoots(heap).byte_array_map(),
                                   SKIP_WRITE_BARRIER);
  Cast<ByteArray>(result)->set_length(length);
  return AllocationResult::FromObject(result);
}

bool HeapTester::CodeEnsureLinearAllocationArea(Heap* heap, int size_in_bytes) {
  MainAllocator* allocator = heap->allocator()->code_space_allocator();
  return allocator->EnsureAllocationForTesting(
      size_in_bytes, AllocationAlignment::kTaggedAligned,
      AllocationOrigin::kRuntime);
}

HEAP_TEST(Regress587004) {
  if (v8_flags.single_generation) return;
  ManualGCScope manual_gc_scope;
#ifdef VERIFY_HEAP
  v8_flags.verify_heap = false;
#endif
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  const int N = (kMaxRegularHeapObjectSize - OFFSET_OF_DATA_START(FixedArray)) /
                kTaggedSize;
  DirectHandle<FixedArray> array =
      factory->NewFixedArray(N, AllocationType::kOld);
  CHECK(heap->old_space()->Contains(*array));
  DirectHandle<Object> number = factory->NewHeapNumber(1.0);
  CHECK(HeapLayout::InYoungGeneration(*number));
  for (int i = 0; i < N; i++) {
    array->set(i, *number);
  }
  heap::InvokeMajorGC(heap);
  heap::SimulateFullSpace(heap->old_space());
  heap->RightTrimArray(*array, 1, N);
  heap->EnsureSweepingCompleted(Heap::SweepingForcedFinalizationMode::kV8Only,
                                CompleteSweepingReason::kTesting);
  Tagged<ByteArray> byte_array;
  const uint32_t M = 256;
  // Don't allow old space expansion. The test works without this flag too,
  // but becomes very slow.
  heap->set_force_oom(true);
  while (
      AllocateByteArrayForTest(heap, M, AllocationType::kOld).To(&byte_array)) {
    for (uint32_t j = 0; j < M; j++) {
      byte_array->set(j, 0x31);
    }
  }
  // Re-enable old space expansion to avoid OOM crash.
  heap->set_force_oom(false);
  heap::InvokeMinorGC(heap);
}

HEAP_TEST(Regress589413) {
  if (!v8_flags.incremental_marking || v8_flags.stress_concurrent_allocation) {
    return;
  }
  v8_flags.stress_compaction = true;
  ManualGCScope manual_gc_scope;
  heap::ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);
  v8_flags.parallel_compaction = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  // Get the heap in clean state.
  heap::InvokeMajorGC(heap);
  heap::InvokeMajorGC(heap);
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  // Fill the new space with byte arrays with elements looking like pointers.
  const uint32_t M = 256;
  Tagged<ByteArray> byte_array;
  NormalPage* young_page = nullptr;
  while (AllocateByteArrayForTest(heap, M, AllocationType::kYoung)
             .To(&byte_array)) {
    // Only allocate objects on one young page as a rough estimate on
    // how much memory can be promoted into the old generation.
    // Otherwise we would crash when forcing promotion of all young
    // live objects.
    if (!young_page) young_page = NormalPage::FromHeapObject(byte_array);
    if (NormalPage::FromHeapObject(byte_array) != young_page) break;

    for (uint32_t j = 0; j < M; j++) {
      byte_array->set(j, 0x31);
    }
    // Add the array in root set.
    handle(byte_array, isolate);
  }
  auto reset_oom = [](void* heap, size_t limit, size_t) -> size_t {
    reinterpret_cast<Heap*>(heap)->set_force_oom(false);
    return limit;
  };
  heap->AddNearHeapLimitCallback(reset_oom, heap);

  {
    // Ensure that incremental marking is not started unexpectedly.
    AlwaysAllocateScopeForTesting always_allocate(isolate->heap());

    // Make sure the byte arrays will be promoted on the next GC.
    heap::InvokeMinorGC(heap);
    // This number is close to large free list category threshold.
    const uint32_t N = 0x3EEE;

    std::vector<Tagged<FixedArray>> arrays;
    std::set<NormalPage*> pages;
    Tagged<FixedArray> array;
    // Fill all pages with fixed arrays.
    heap->set_force_oom(true);
    while (
        AllocateFixedArrayForTest(heap, N, AllocationType::kOld).To(&array)) {
      arrays.push_back(array);
      pages.insert(NormalPage::FromHeapObject(array));
      // Add the array in root set.
      handle(array, isolate);
    }
    heap->set_force_oom(false);
    size_t initial_pages = pages.size();
    // Expand and fill two pages with fixed array to ensure enough space both
    // the young objects and the evacuation candidate pages.
    while (
        AllocateFixedArrayForTest(heap, N, AllocationType::kOld).To(&array)) {
      arrays.push_back(array);
      pages.insert(NormalPage::FromHeapObject(array));
      // Add the array in root set.
      handle(array, isolate);
      // Do not expand anymore.
      if (pages.size() - initial_pages == 2) {
        heap->set_force_oom(true);
      }
    }
    // Expand and mark the new page as evacuation candidate.
    heap->set_force_oom(false);
    {
      DirectHandle<HeapObject> ec_obj =
          factory->NewFixedArray(5000, AllocationType::kOld);
      NormalPage* ec_page = NormalPage::FromHeapObject(*ec_obj);
      heap::ForceEvacuationCandidate(ec_page);
      // Make all arrays point to evacuation candidate so that
      // slots are recorded for them.
      for (size_t j = 0; j < arrays.size(); j++) {
        array = arrays[j];
        for (uint32_t i = 0; i < N; i++) {
          array->set(i, *ec_obj);
        }
      }
    }
    CHECK(heap->incremental_marking()->IsStopped());
    heap::SimulateIncrementalMarking(heap);
    for (size_t j = 0; j < arrays.size(); j++) {
      heap->RightTrimArray(arrays[j], 1, N);
    }
  }

  // Force allocation from the free list.
  heap->set_force_oom(true);
  heap::InvokeMajorGC(heap);
  heap->RemoveNearHeapLimitCallback(reset_oom, 0);
}

TEST(Regress598319) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope;
  // This test ensures that no white objects can cross the progress bar of large
  // objects during incremental marking. It checks this by using Shift() during
  // incremental marking.
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();

  // The size of the array should be larger than kProgressBarScanningChunk.
  const uint32_t kNumberOfObjects =
      std::max(FixedArray::kMaxRegularLength + 1, 128 * KB);

  struct Arr {
    Arr(Isolate* isolate, uint32_t number_of_objects) {
      root = isolate->factory()->NewFixedArray(1, AllocationType::kOld);
      {
        // Temporary scope to avoid getting any other objects into the root set.
        v8::HandleScope new_scope(CcTest::isolate());
        DirectHandle<FixedArray> tmp = isolate->factory()->NewFixedArray(
            number_of_objects, AllocationType::kOld);
        root->set(0, *tmp);
        const uint32_t length = get()->length().value();
        for (uint32_t i = 0; i < length; i++) {
          tmp = isolate->factory()->NewFixedArray(100, AllocationType::kOld);
          get()->set(i, *tmp);
        }
      }
      global_root.Reset(CcTest::isolate(), Utils::ToLocal(Cast<Object>(root)));
    }

    Tagged<FixedArray> get() { return Cast<FixedArray>(root->get(0)); }

    Handle<FixedArray> root;

    // Store array in global as well to make it part of the root set when
    // starting incremental marking.
    v8::Global<Value> global_root;
  } arr(isolate, kNumberOfObjects);

  CHECK_EQ(arr.get()->length().value(), kNumberOfObjects);
  CHECK(heap->lo_space()->Contains(arr.get()));
  LargePage* page = LargePage::FromHeapObject(isolate, arr.get());
  CHECK_NOT_NULL(page);

  // GC to cleanup state
  heap::InvokeMajorGC(heap);
  if (heap->sweeping_in_progress()) {
    heap->EnsureSweepingCompleted(Heap::SweepingForcedFinalizationMode::kV8Only,
                                  CompleteSweepingReason::kTesting);
  }

  CHECK(heap->lo_space()->Contains(arr.get()));
  IncrementalMarking* marking = heap->incremental_marking();
  MarkingState* marking_state = heap->marking_state();
  CHECK(marking_state->IsUnmarked(arr.get()));

  for (uint32_t i = 0; i < arr.get()->length().value(); i++) {
    Tagged<HeapObject> arr_value = Cast<HeapObject>(arr.get()->get(i));
    CHECK(marking_state->IsUnmarked(arr_value));
  }

  // Start incremental marking.
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::GCFlag::kNoFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking());

  // Check that we have not marked the interesting array during root scanning.
  for (uint32_t i = 0; i < arr.get()->length().value(); i++) {
    Tagged<HeapObject> arr_value = Cast<HeapObject>(arr.get()->get(i));
    CHECK(marking_state->IsUnmarked(arr_value));
  }

  // Now we search for a state where we are in incremental marking and have
  // only partially marked the large object.
  static constexpr auto kSmallStepSize =
      v8::base::TimeDelta::FromMillisecondsD(0.1);
  static constexpr size_t kSmallMaxBytesToMark = 100;
  while (!marking->IsMajorMarkingComplete()) {
    marking->AdvanceForTesting(kSmallStepSize, kSmallMaxBytesToMark);
    MarkingProgressTracker& progress_tracker = page->marking_progress_tracker();
    if (progress_tracker.IsEnabled() &&
        progress_tracker.GetCurrentChunkForTesting() > 0) {
      CHECK_NE(progress_tracker.GetCurrentChunkForTesting(), arr.get()->Size());
      {
        // Shift by 1, effectively moving one white object across the progress
        // bar, meaning that we will miss marking it.
        v8::HandleScope new_scope(CcTest::isolate());
        DirectHandle<JSArray> js_array =
            isolate->factory()->NewJSArrayWithElements(
                DirectHandle<FixedArray>(arr.get(), isolate));
        js_array->GetElementsAccessor()->Shift(isolate, js_array);
      }
      break;
    }
  }

  SafepointScope safepoint_scope(heap->isolate(),
                                 kGlobalSafepointForSharedSpaceIsolate);
  MarkingBarrier::PublishAll(heap);

  // Finish marking with bigger steps to speed up test.
  static constexpr auto kLargeStepSize =
      v8::base::TimeDelta::FromMilliseconds(1000);
  while (!marking->IsMajorMarkingComplete()) {
    marking->AdvanceForTesting(kLargeStepSize);
  }
  CHECK(marking->IsMajorMarkingComplete());

  // All objects need to be black after marking. If a white object crossed the
  // progress bar, we would fail here.
  for (uint32_t i = 0; i < arr.get()->length().value(); i++) {
    Tagged<HeapObject> arr_value = Cast<HeapObject>(arr.get()->get(i));
    CHECK(HeapLayout::InReadOnlySpace(arr_value) ||
          marking_state->IsMarked(arr_value));
  }
}

DirectHandle<FixedArray> ShrinkArrayAndCheckSize(Heap* heap, int length) {
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
  // Make sure there is no garbage and the compilation cache is empty.
  for (int i = 0; i < 5; i++) {
    heap::InvokeMajorGC(heap);
  }
  heap->EnsureSweepingCompleted(Heap::SweepingForcedFinalizationMode::kV8Only,
                                CompleteSweepingReason::kTesting);
  // Disable LAB, such that calculations with SizeOfObjects() and object size
  // are correct.
  heap->DisableInlineAllocation();
  size_t size_before_allocation = heap->SizeOfObjects();
  IndirectHandle<FixedArray> array(
      *heap->isolate()->factory()->NewFixedArray(length, AllocationType::kOld),
      heap->isolate());
  size_t size_after_allocation = heap->SizeOfObjects();
  CHECK_EQ(size_after_allocation, size_before_allocation + array->Size());
  array->RightTrim(heap->isolate(), 1);
  size_t size_after_shrinking = heap->SizeOfObjects();
  // Shrinking does not change the space size immediately.
  CHECK_EQ(size_after_allocation, size_after_shrinking);
  // GC and sweeping updates the size to account for shrinking.
  heap::InvokeMajorGC(heap);
  heap->EnsureSweepingCompleted(Heap::SweepingForcedFinalizationMode::kV8Only,
                                CompleteSweepingReason::kTesting);
  intptr_t size_after_gc = heap->SizeOfObjects();
  CHECK_EQ(size_after_gc, size_before_allocation + array->Size());
  return array;
}

TEST(Regress609761) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  int length = kMaxRegularHeapObjectSize / kTaggedSize + 1;
  DirectHandle<FixedArray> array = ShrinkArrayAndCheckSize(heap, length);
  CHECK(heap->lo_space()->Contains(*array));
}

TEST(LiveBytes) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  DirectHandle<FixedArray> array = ShrinkArrayAndCheckSize(heap, 2000);
  CHECK(heap->old_space()->Contains(*array));
}

TEST(Regress615489) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();
  heap::InvokeMajorGC(heap);

  i::IncrementalMarking* marking = heap->incremental_marking();
  if (heap->sweeping_in_progress()) {
    heap->EnsureSweepingCompleted(Heap::SweepingForcedFinalizationMode::kV8Only,
                                  CompleteSweepingReason::kTesting);
  }
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::GCFlag::kNoFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking());
  CHECK(marking->black_allocation());
  {
    AlwaysAllocateScopeForTesting always_allocate(heap);
    v8::HandleScope inner(CcTest::isolate());
    isolate->factory()->NewFixedArray(500, AllocationType::kOld)->Size();
  }
  static constexpr auto kStepSize = v8::base::TimeDelta::FromMilliseconds(100);
  while (!marking->IsMajorMarkingComplete()) {
    marking->AdvanceForTesting(kStepSize);
  }
  CHECK(marking->IsMajorMarkingComplete());
  intptr_t size_before = heap->SizeOfObjects();
  heap::InvokeMajorGC(heap);
  intptr_t size_after = heap->SizeOfObjects();
  // Live size does not increase after garbage collection.
  CHECK_LE(size_after, size_before);
}

class StaticOneByteResource : public v8::String::ExternalOneByteStringResource {
 public:
  explicit StaticOneByteResource(const char* data) : data_(data) {}

  ~StaticOneByteResource() override = default;

  const char* data() const override { return data_; }

  size_t length() const override { return strlen(data_); }

 private:
  const char* data_;
};

TEST(Regress631969) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope;
  heap::ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);
  v8_flags.parallel_compaction = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  // Get the heap in clean state.
  heap::InvokeMajorGC(heap);
  heap::InvokeMajorGC(heap);
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  // Allocate two strings in a fresh page and mark the page as evacuation
  // candidate.
  heap::SimulateFullSpace(heap->old_space());
  Handle<String> s1 =
      factory->NewStringFromStaticChars("123456789", AllocationType::kOld);
  Handle<String> s2 =
      factory->NewStringFromStaticChars("01234", AllocationType::kOld);
  heap::ForceEvacuationCandidate(NormalPage::FromHeapObject(*s1));

  heap::SimulateIncrementalMarking(heap, false);

  // Allocate a cons string and promote it to a fresh page in the old space.
  DirectHandle<String> s3 = factory->NewConsString(s1, s2).ToHandleChecked();
  heap::EmptyNewSpaceUsingGC(heap);

  heap::SimulateIncrementalMarking(heap, false);

  // Finish incremental marking.
  static constexpr auto kStepSize = v8::base::TimeDelta::FromMilliseconds(100);
  IncrementalMarking* marking = heap->incremental_marking();
  while (!marking->IsMajorMarkingComplete()) {
    marking->AdvanceForTesting(kStepSize);
  }

  {
    StaticOneByteResource external_string("12345678901234");
    s3->MakeExternal(isolate, &external_string);
    heap::InvokeMajorGC(heap);
    // This avoids the GC from trying to free stack allocated resources.
    i::Cast<i::ExternalOneByteString>(s3)->SetResource(isolate, nullptr);
  }
}

TEST(ContinuousRightTrimFixedArrayInBlackArea) {
  if (v8_flags.black_allocated_pages) return;
  if (!v8_flags.incremental_marking) return;
  v8_flags.stress_concurrent_allocation = false;  // For SimulateFullSpace.
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = CcTest::i_isolate();
  heap::InvokeMajorGC(heap);

  i::IncrementalMarking* marking = heap->incremental_marking();
  if (heap->sweeping_in_progress()) {
    heap->EnsureSweepingCompleted(Heap::SweepingForcedFinalizationMode::kV8Only,
                                  CompleteSweepingReason::kTesting);
  }
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::GCFlag::kNoFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking());
  CHECK(marking->black_allocation());

  // Ensure that we allocate a new page, set up a bump pointer area, and
  // perform the allocation in a black area.
  heap::SimulateFullSpace(heap->old_space());
  isolate->factory()->NewFixedArray(10, AllocationType::kOld);

  // Allocate the fixed array that will be trimmed later.
  DirectHandle<FixedArray> array =
      isolate->factory()->NewFixedArray(100, AllocationType::kOld);
  Address start_address = array->address();
  Address end_address = start_address + array->Size();
  NormalPage* page = NormalPage::FromAddress(start_address);
  NonAtomicMarkingState* marking_state = heap->non_atomic_marking_state();
  CHECK(marking_state->IsMarked(*array));
  CHECK(page->marking_bitmap()->AllBitsSetInRange(
      MarkingBitmap::AddressToIndex(start_address),
      MarkingBitmap::LimitAddressToIndex(end_address)));
  CHECK(heap->old_space()->Contains(*array));

  // Trim it once by one word to check that the trimmed area gets unmarked.
  Address previous = end_address - kTaggedSize;
  isolate->heap()->RightTrimArray(*array, 99, 100);

  Tagged<HeapObject> filler = HeapObject::FromAddress(previous);
  CHECK(IsFreeSpaceOrFiller(filler));

  // Trim 10 times by one, two, and three word.
  for (int i = 1; i <= 3; i++) {
    for (int j = 0; j < 10; j++) {
      previous -= kTaggedSize * i;
      const uint32_t old_capacity = array->capacity().value();
      CHECK_GE(old_capacity, i);
      const uint32_t new_capacity = old_capacity - i;
      isolate->heap()->RightTrimArray(*array, new_capacity, old_capacity);
      filler = HeapObject::FromAddress(previous);
      CHECK(IsFreeSpaceOrFiller(filler));
      CHECK(marking_state->IsUnmarked(filler));
    }
  }

  heap::InvokeAtomicMajorGC(heap);
}

TEST(RightTrimFixedArrayWithBlackAllocatedPages) {
  if (!v8_flags.black_allocated_pages) return;
  if (!v8_flags.incremental_marking) return;
  v8_flags.stress_concurrent_allocation = false;  // For SimulateFullSpace.
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = CcTest::i_isolate();
  heap::InvokeMajorGC(heap);

  i::IncrementalMarking* marking = heap->incremental_marking();
  if (heap->sweeping_in_progress()) {
    heap->EnsureSweepingCompleted(Heap::SweepingForcedFinalizationMode::kV8Only,
                                  CompleteSweepingReason::kTesting);
  }
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::GCFlag::kNoFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking());
  CHECK(marking->black_allocation());

  // Ensure that we allocate a new page, set up a bump pointer area, and
  // perform the allocation in a black area.
  heap::SimulateFullSpace(heap->old_space());
  isolate->factory()->NewFixedArray(10, AllocationType::kOld);

  // Allocate the fixed array that will be trimmed later.
  DirectHandle<FixedArray> array =
      isolate->factory()->NewFixedArray(100, AllocationType::kOld);
  Address start_address = array->address();
  Address end_address = start_address + array->Size();
  NormalPage* page = NormalPage::FromHeapObject(*array);
  CHECK(page->is_black_allocated());
  CHECK(heap->old_space()->Contains(*array));

  // Trim it once by one word, which shouldn't affect the BLACK_ALLOCATED flag.
  Address previous = end_address - kTaggedSize;
  isolate->heap()->RightTrimArray(*array, 99, 100);

  Tagged<HeapObject> filler = HeapObject::FromAddress(previous);
  CHECK(IsFreeSpaceOrFiller(filler));
  CHECK(page->is_black_allocated());

  heap::InvokeAtomicMajorGC(heap);
  CHECK(!NormalPage::FromHeapObject(*array)->is_black_allocated());

  heap->StartIncrementalMarking(i::GCFlag::kNoFlags,
                                i::GarbageCollectionReason::kTesting);

  // Allocate the large fixed array that will be trimmed later.
  array = isolate->factory()->NewFixedArray(200000, AllocationType::kOld);
  CHECK(heap->lo_space()->Contains(*array));
  CHECK(!BasePage::FromHeapObject(*array)->is_black_allocated());

  heap::InvokeAtomicMajorGC(heap);
  CHECK(!BasePage::FromHeapObject(*array)->is_black_allocated());
}

TEST(Regress618958) {
  if (!v8_flags.incremental_marking) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  bool isolate_is_locked = true;
  v8::ExternalMemoryAccounter accounter;
  accounter.Increase(CcTest::isolate(), 100 * MB);
  int mark_sweep_count_before = heap->ms_count();
  heap->MemoryPressureNotification(MemoryPressureLevel::kCritical,
                                   isolate_is_locked);
  int mark_sweep_count_after = heap->ms_count();
  int mark_sweeps_performed = mark_sweep_count_after - mark_sweep_count_before;
  // The memory pressuer handler either performed two GCs or performed one and
  // started incremental marking.
  CHECK(mark_sweeps_performed == 2 ||
        (mark_sweeps_performed == 1 &&
         !heap->incremental_marking()->IsStopped()));
  accounter.Decrease(CcTest::isolate(), 100 * MB);
}

TEST(YoungGenerationLargeObjectAllocationScavenge) {
  if (v8_flags.minor_ms) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();
  if (!isolate->serializer_enabled()) return;

  DirectHandle<FixedArray> array_small =
      isolate->factory()->NewFixedArray(200000);
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(*array_small);
  BasePage* metadata = chunk->Metadata(isolate);
  CHECK_EQ(NEW_LO_SPACE, SbxCast<MutablePage>(metadata)->owner_identity());
  CHECK(metadata->is_large());
  CHECK(chunk->IsToPage());

  DirectHandle<Object> number = isolate->factory()->NewHeapNumber(123.456);
  array_small->set(0, *number);

  heap::InvokeMinorGC(heap);

  // After the first young generation GC array_small will be in the old
  // generation large object space.
  chunk = MemoryChunk::FromHeapObject(*array_small);
  CHECK_EQ(LO_SPACE, SbxCast<MutablePage>(chunk->Metadata())->owner_identity());
  CHECK(!chunk->InYoungGeneration());

  heap::InvokeMemoryReducingMajorGCs(heap);
}

TEST(YoungGenerationLargeObjectAllocationMarkCompact) {
  if (v8_flags.minor_ms) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();
  if (!isolate->serializer_enabled()) return;

  DirectHandle<FixedArray> array_small =
      isolate->factory()->NewFixedArray(200000);
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(*array_small);
  BasePage* metadata = chunk->Metadata(isolate);
  CHECK_EQ(NEW_LO_SPACE, SbxCast<MutablePage>(metadata)->owner_identity());
  CHECK(metadata->is_large());
  CHECK(chunk->IsToPage());

  DirectHandle<Object> number = isolate->factory()->NewHeapNumber(123.456);
  array_small->set(0, *number);

  heap::InvokeMajorGC(heap);

  // After the first full GC array_small will be in the old generation
  // large object space.
  chunk = MemoryChunk::FromHeapObject(*array_small);
  CHECK_EQ(LO_SPACE, SbxCast<MutablePage>(chunk->Metadata())->owner_identity());
  CHECK(!chunk->InYoungGeneration());

  heap::InvokeMemoryReducingMajorGCs(heap);
}

TEST(YoungGenerationLargeObjectAllocationReleaseScavenger) {
  if (v8_flags.minor_ms) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();
  if (!isolate->serializer_enabled()) return;

  {
    HandleScope new_scope(isolate);
    for (int i = 0; i < 10; i++) {
      DirectHandle<FixedArray> array_small =
          isolate->factory()->NewFixedArray(20000);
      MemoryChunk* chunk = MemoryChunk::FromHeapObject(*array_small);
      CHECK_EQ(NEW_LO_SPACE,
               SbxCast<MutablePage>(chunk->Metadata())->owner_identity());
      CHECK(chunk->IsToPage());
    }
  }

  heap::InvokeMinorGC(heap);
  CHECK(isolate->heap()->new_lo_space()->IsEmpty());
  CHECK_EQ(0, isolate->heap()->new_lo_space()->Size());
  CHECK_EQ(0, isolate->heap()->new_lo_space()->SizeOfObjects());
  CHECK(isolate->heap()->lo_space()->IsEmpty());
  CHECK_EQ(0, isolate->heap()->lo_space()->Size());
  CHECK_EQ(0, isolate->heap()->lo_space()->SizeOfObjects());
}

TEST(UncommitUnusedLargeObjectMemory) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Heap* heap = CcTest::heap();
  Isolate* isolate = heap->isolate();

  DirectHandle<FixedArray> array =
      isolate->factory()->NewFixedArray(200000, AllocationType::kOld);
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(*array);
  MutablePage* page = SbxCast<MutablePage>(chunk->Metadata());
  CHECK_EQ(page->owner_identity(), LO_SPACE);

  intptr_t size_before = array->Size();
  size_t committed_memory_before = page->CommittedPhysicalMemory();

  array->RightTrim(isolate, 1);
  CHECK(array->Size() < size_before);

  heap::InvokeMajorGC(heap);
  CHECK(page->CommittedPhysicalMemory() < committed_memory_before);
  size_t shrinked_size = RoundUp(
      (array->address() - chunk->address()) + array->Size(), CommitPageSize());
  CHECK_EQ(shrinked_size, page->CommittedPhysicalMemory());
}

const int kHeapLimit = 100 * MB;
Isolate* oom_isolate = nullptr;

void OOMCallback(const char* location, const OOMDetails&) {
  Heap* heap = oom_isolate->heap();
  size_t kSlack = heap->new_space() ? heap->MaxSemiSpaceSize() : 0;
  CHECK_LE(heap->OldGenerationCapacity(), kHeapLimit + kSlack);
  base::OS::ExitProcess(0);
}

UNINITIALIZED_TEST(OutOfMemory) {
  if (v8_flags.stress_incremental_marking) return;
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) return;
#endif
  v8_flags.max_old_space_size = kHeapLimit / MB;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  v8::Isolate::Scope isolate_scope(isolate);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  oom_isolate = i_isolate;
  isolate->SetOOMErrorHandler(OOMCallback);
  {
    PtrComprCageAccessScope ptr_compr_cage_access_scope(i_isolate);
    Factory* factory = i_isolate->factory();
    HandleScope handle_scope(i_isolate);
    while (true) {
      factory->NewFixedArray(100);
    }
  }
}

UNINITIALIZED_TEST(OutOfMemoryIneffectiveGC) {
  if (!v8_flags.detect_ineffective_gcs_near_heap_limit) return;
  if (v8_flags.stress_incremental_marking ||
      v8_flags.stress_concurrent_allocation) {
    return;
  }
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) return;
#endif

  v8_flags.max_old_space_size = kHeapLimit / MB;
  // The test timeouts on some platforms with the default 95% heap size
  // threshold.
  v8_flags.ineffective_gc_size_threshold = 0.8;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  oom_isolate = i_isolate;
  isolate->SetOOMErrorHandler(OOMCallback);
  Factory* factory = i_isolate->factory();
  Heap* heap = i_isolate->heap();
  {
    v8::Isolate::Scope isolate_scope(isolate);
    PtrComprCageAccessScope ptr_compr_cage_access_scope(i_isolate);
    std::vector<Handle<FixedArray>> arrays;
    heap::InvokeMajorGC(heap);

    HandleScope scope(i_isolate);
    const size_t heap_threshold = heap->MaxOldGenerationSize() * 0.8;
    while (heap->OldGenerationSizeOfObjects() < heap_threshold) {
      arrays.push_back(factory->NewFixedArray(1000, AllocationType::kOld));
      // This ensures OldGenerationSizeOfObjects() remains above the threshold
      // even after a last resort GC.
      if (heap->OldGenerationSizeOfObjects() >= heap_threshold) {
        heap->CollectAllAvailableGarbage(GarbageCollectionReason::kLastResort);
      }
    }
    {
      int initial_ms_count = heap->ms_count();
      int ineffective_ms_start = initial_ms_count;
      while (heap->ms_count() < initial_ms_count + 10) {
        HandleScope inner_scope(i_isolate);
        arrays.push_back(factory->NewFixedArray(30000, AllocationType::kOld));
        if (heap->tracer()->AverageMarkCompactMutatorUtilization() >= 0.3) {
          ineffective_ms_start = heap->ms_count() + 1;
        }
      }
      int consecutive_ineffective_ms = heap->ms_count() - ineffective_ms_start;
      if (heap->tracer()->AverageMarkCompactMutatorUtilization() < 0.3) {
        // The threshold for consecutive MC is 4, but CollectAllAvailableGarbage
        // can run 2 cycles which counts as 1 "ineffective MC".
        CHECK_LE(consecutive_ineffective_ms, 8);
      }
    }
  }
  isolate->Dispose();
}

UNINITIALIZED_TEST(OutOfMemoryIneffectiveGCRunningJS) {
  if (!v8_flags.detect_ineffective_gcs_near_heap_limit) return;
  if (v8_flags.stress_incremental_marking) return;

  v8_flags.max_old_space_size = 10;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  oom_isolate = i_isolate;

  isolate->SetOOMErrorHandler(OOMCallback);

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::New(isolate)->Enter();

  // Test that source positions are not collected as part of a failing GC, which
  // will fail as allocation is disallowed. If the test works, this should call
  // OOMCallback and terminate without crashing.
  CompileRun(R"javascript(
      var array = [];
      for(var i = 20000; i < 40000; ++i) {
        array.push(new Array(i));
      }
      )javascript");

  FATAL("Should not get here as OOMCallback should be called");
}

HEAP_TEST(Regress779503) {
  // The following regression test ensures that the Scavenger does not allocate
  // over invalid slots. More specific, the Scavenger should not sweep a page
  // that it currently processes because it might allocate over the currently
  // processed slot.
  if (v8_flags.single_generation) return;
  v8_flags.stress_concurrent_allocation = false;  // For SealCurrentObjects.
  const uint32_t kArraySize = 2048;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = CcTest::heap();
  heap::SealCurrentObjects(heap);
  {
    HandleScope handle_scope(isolate);
    // The byte array filled with kHeapObjectTag ensures that we cannot read
    // from the slot again and interpret it as heap value. Doing so will crash.
    DirectHandle<ByteArray> byte_array =
        isolate->factory()->NewByteArray(kArraySize);
    CHECK(HeapLayout::InYoungGeneration(*byte_array));
    for (uint32_t i = 0; i < kArraySize; i++) {
      byte_array->set(i, kHeapObjectTag);
    }

    {
      HandleScope new_scope(isolate);
      // The FixedArray in old space serves as space for slots.
      DirectHandle<FixedArray> fixed_array =
          isolate->factory()->NewFixedArray(kArraySize, AllocationType::kOld);
      CHECK(!HeapLayout::InYoungGeneration(*fixed_array));
      for (uint32_t i = 0; i < kArraySize; i++) {
        fixed_array->set(i, *byte_array);
      }
    }
    // Delay sweeper tasks to allow the scavenger to sweep the page it is
    // currently scavenging.
    heap->delay_sweeper_tasks_for_testing_ = true;
    heap::InvokeMajorGC(heap);
    CHECK(!HeapLayout::InYoungGeneration(*byte_array));
  }
  // Scavenging and sweeping the same page will crash as slots will be
  // overridden.
  heap::InvokeMinorGC(heap);
  heap->delay_sweeper_tasks_for_testing_ = false;
}

struct OutOfMemoryState {
  Heap* heap;
  bool oom_triggered;
  size_t old_generation_capacity_at_oom;
  size_t memory_allocator_size_at_oom;
  size_t new_space_capacity_at_oom;
  size_t new_lo_space_size_at_oom;
  size_t current_heap_limit;
  size_t initial_heap_limit;
};

size_t NearHeapLimitCallback(void* raw_state, size_t current_heap_limit,
                             size_t initial_heap_limit) {
  OutOfMemoryState* state = static_cast<OutOfMemoryState*>(raw_state);
  Heap* heap = state->heap;
  state->oom_triggered = true;
  state->old_generation_capacity_at_oom = heap->OldGenerationCapacity();
  state->memory_allocator_size_at_oom = heap->memory_allocator()->Size();
  state->new_space_capacity_at_oom =
      heap->new_space() ? heap->new_space()->Capacity() : 0;
  state->new_lo_space_size_at_oom =
      heap->new_lo_space() ? heap->new_lo_space()->Size() : 0;
  state->current_heap_limit = current_heap_limit;
  state->initial_heap_limit = initial_heap_limit;
  return initial_heap_limit + 100 * MB;
}

size_t MemoryAllocatorSizeFromHeapCapacity(size_t capacity) {
  // Size to capacity factor.
  double factor = NormalPage::kPageSize * 1.0 /
                  MemoryChunkLayout::AllocatableMemoryInDataPage();
  // Some tables (e.g. deoptimization table) are allocated directly with the
  // memory allocator. Allow some slack to account for them.
  size_t slack = 5 * MB;
  return static_cast<size_t>(capacity * factor) + slack;
}

UNINITIALIZED_TEST(OutOfMemorySmallObjects) {
  if (v8_flags.stress_incremental_marking) return;
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) return;
#endif
  const size_t kOldGenerationLimit = 50 * MB;
  v8_flags.max_old_space_size = kOldGenerationLimit / MB;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  Heap* heap = i_isolate->heap();
  Factory* factory = i_isolate->factory();
  OutOfMemoryState state;
  state.heap = heap;
  state.oom_triggered = false;
  heap->AddNearHeapLimitCallback(NearHeapLimitCallback, &state);
  {
    PtrComprCageAccessScope ptr_compr_cage_access_scope(i_isolate);

    v8::Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(i_isolate);
    while (!state.oom_triggered) {
      factory->NewFixedArray(100);
    }
  }
  CHECK_LE(state.old_generation_capacity_at_oom,
           kOldGenerationLimit + heap->MaxSemiSpaceSize());
  CHECK_LE(kOldGenerationLimit,
           state.old_generation_capacity_at_oom + heap->MaxSemiSpaceSize());
  CHECK_LE(
      state.memory_allocator_size_at_oom,
      MemoryAllocatorSizeFromHeapCapacity(state.old_generation_capacity_at_oom +
                                          2 * state.new_space_capacity_at_oom));
  isolate->Dispose();
}

UNINITIALIZED_TEST(OutOfMemoryLargeObjects) {
  if (v8_flags.stress_incremental_marking) return;
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) return;
#endif
  const size_t kOldGenerationLimit = 50 * MB;
  v8_flags.max_old_space_size = kOldGenerationLimit / MB;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  Heap* heap = i_isolate->heap();
  Factory* factory = i_isolate->factory();
  OutOfMemoryState state;
  state.heap = heap;
  state.oom_triggered = false;
  heap->AddNearHeapLimitCallback(NearHeapLimitCallback, &state);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    PtrComprCageAccessScope ptr_compr_cage_access_scope(i_isolate);
    const int kFixedArrayLength = 1000000;
    {
      HandleScope handle_scope(i_isolate);
      while (!state.oom_triggered) {
        factory->NewFixedArray(kFixedArrayLength);
      }
    }
    CHECK_LE(state.old_generation_capacity_at_oom,
             kOldGenerationLimit + state.new_space_capacity_at_oom +
                 state.new_lo_space_size_at_oom +
                 FixedArray::SizeFor(kFixedArrayLength));
    CHECK_LE(kOldGenerationLimit, state.old_generation_capacity_at_oom +
                                      state.new_space_capacity_at_oom +
                                      state.new_lo_space_size_at_oom +
                                      FixedArray::SizeFor(kFixedArrayLength));
    CHECK_LE(state.memory_allocator_size_at_oom,
             MemoryAllocatorSizeFromHeapCapacity(
                 state.old_generation_capacity_at_oom +
                 2 * state.new_space_capacity_at_oom +
                 state.new_lo_space_size_at_oom));
  }
  isolate->Dispose();
}

UNINITIALIZED_TEST(RestoreHeapLimit) {
  if (v8_flags.stress_incremental_marking) return;
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) return;
#endif
  ManualGCScope manual_gc_scope;
  const size_t kOldGenerationLimit = 50 * MB;
  v8_flags.max_old_space_size = kOldGenerationLimit / MB;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  Heap* heap = i_isolate->heap();
  Factory* factory = i_isolate->factory();

  {
    v8::Isolate::Scope isolate_scope(isolate);
    PtrComprCageAccessScope ptr_compr_cage_access_scope(i_isolate);

    // In this test, we need to invoke GC without stack, otherwise some objects
    // may not be reclaimed because of conservative stack scanning and the heap
    // limit may be reached.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);

    OutOfMemoryState state;
    state.heap = heap;
    state.oom_triggered = false;
    heap->AddNearHeapLimitCallback(NearHeapLimitCallback, &state);
    heap->AutomaticallyRestoreInitialHeapLimit(0.5);
    const int kFixedArrayLength = 1000000;
    {
      HandleScope handle_scope(i_isolate);
      while (!state.oom_triggered) {
        factory->NewFixedArray(kFixedArrayLength);
      }
    }
    heap->MemoryPressureNotification(MemoryPressureLevel::kCritical, true);
    state.oom_triggered = false;
    {
      HandleScope handle_scope(i_isolate);
      while (!state.oom_triggered) {
        factory->NewFixedArray(kFixedArrayLength);
      }
    }
    CHECK_EQ(state.current_heap_limit, state.initial_heap_limit);
  }

  isolate->Dispose();
}

void HeapTester::UncommitUnusedMemory(Heap* heap) {
  if (!v8_flags.minor_ms) heap->ReduceNewSpaceSizeForTesting();
  heap->memory_allocator()->ReleasePooledChunksImmediately();
}

class DeleteNative {
 public:
  static constexpr ManagedTypeId kTypeID = ManagedTypeId::kTestDeleteNative;
};

// The test only runs on 64-bit, because it simulates allocating ~9Gb
// external memory.
#if defined(V8_TARGET_ARCH_64_BIT)
TEST(Regress8014) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  Heap* heap = isolate->heap();
  {
    HandleScope scope(isolate);
    for (int i = 0; i < 10000; i++) {
      auto handle = CppGCManaged<DeleteNative>::Create(
          isolate, 1000000, std::make_shared<DeleteNative>());
      USE(handle);
    }
  }
  int ms_count = heap->ms_count();
  heap->MemoryPressureNotification(MemoryPressureLevel::kCritical, true);
  // Several GCs can be triggred by the above call.
  // The bad case triggers 10000 GCs.
  CHECK_LE(heap->ms_count(), ms_count + 10);
}
#endif

TEST(Regress8617) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope;
  heap::ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);
  LocalContext env;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  heap::SimulateFullSpace(heap->old_space());
  // Step 1. Create a function and ensure that it is in the old space.
  DirectHandle<Object> foo =
      v8::Utils::OpenDirectHandle(*CompileRun("function foo() { return 42; };"
                                              "foo;"));
  if (HeapLayout::InYoungGeneration(*foo)) {
    heap::EmptyNewSpaceUsingGC(heap);
  }
  // Step 2. Create an object with a reference to foo in the descriptor array.
  CompileRun(
      "var obj = {};"
      "obj.method = foo;"
      "obj;");
  // Step 3. Make sure that foo moves during Mark-Compact.
  NormalPage* ec_page = NormalPage::FromAddress((*foo).ptr());
  heap::ForceEvacuationCandidate(ec_page);
  // Step 4. Start incremental marking.
  heap::SimulateIncrementalMarking(heap, false);
  CHECK(ec_page->Chunk()->IsEvacuationCandidate());
  // Step 5. Install a new descriptor array on the map of the object.
  // This runs the marking barrier for the descriptor array.
  // In the bad case it sets the number of marked descriptors but does not
  // change the color of the descriptor array.
  CompileRun("obj.bar = 10;");
  // Step 6. Promote the descriptor array to old space. During promotion
  // the Scavenger will not record the slot of foo in the descriptor array.
  heap::EmptyNewSpaceUsingGC(heap);
  // Step 7. Complete the Mark-Compact.
  heap::InvokeMajorGC(heap);
  // Step 8. Use the descriptor for foo, which contains a stale pointer.
  CompileRun("obj.method()");
}

HEAP_TEST(MemoryReducerActivationForSmallHeaps) {
  if (v8_flags.single_generation || !v8_flags.memory_reducer) return;
  ManualGCScope manual_gc_scope;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  CHECK_EQ(heap->memory_reducer()->state_.id(), MemoryReducer::kUninit);
  LocalContext env;
  HandleScope scope(isolate);
  const size_t kActivationThreshold = 1 * MB;
  size_t initial_capacity = heap->OldGenerationCapacity();
  while (heap->OldGenerationCapacity() <
         initial_capacity + kActivationThreshold) {
    isolate->factory()->NewFixedArray(1 * KB, AllocationType::kOld);
  }
  CHECK_EQ(heap->memory_reducer()->state_.id(), MemoryReducer::kWait);
}

TEST(AllocateExternalBackingStore) {
  ManualGCScope manual_gc_scope;
  LocalContext env;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  int initial_ms_count = heap->ms_count();
  void* result =
      heap->AllocateExternalBackingStore([](size_t) { return nullptr; }, 10);
  CHECK_NULL(result);
  // At least two GCs should happen.
  CHECK_LE(2, heap->ms_count() - initial_ms_count);
}

TEST(CodeObjectRegistry) {
  // We turn off compaction to ensure that code is not moving.
  v8_flags.compact = false;

  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  DirectHandle<InstructionStream> code1;
  HandleScope outer_scope(heap->isolate());
  Address code2_address;
  {
    // Ensure that both code objects end up on the same page.
    CHECK(HeapTester::CodeEnsureLinearAllocationArea(
        heap, MemoryChunkLayout::MaxRegularCodeObjectSize()));
    code1 = DummyOptimizedCode(isolate);
    DirectHandle<InstructionStream> code2 = DummyOptimizedCode(isolate);
    code2_address = code2->address();

    CHECK_EQ(MutablePage::FromHeapObject(isolate, *code1),
             MutablePage::FromHeapObject(isolate, *code2));
    CHECK(MutablePage::FromHeapObject(isolate, *code1)
              ->Contains(code1->address()));
    CHECK(MutablePage::FromHeapObject(isolate, *code2)
              ->Contains(code2->address()));
  }
  heap::InvokeMemoryReducingMajorGCs(heap);
  CHECK(
      MutablePage::FromHeapObject(isolate, *code1)->Contains(code1->address()));
  CHECK(MutablePage::FromAddress(isolate, code2_address)
            ->Contains(code2_address));
}

TEST(Regress9701) {
  ManualGCScope manual_gc_scope;
  if (!v8_flags.incremental_marking) return;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  // Start with an empty new space.
  InvokeMinorGC(heap);

  int mark_sweep_count_before = heap->ms_count();
  // Allocate many short living array buffers.
  for (int i = 0; i < 1000; i++) {
    HandleScope scope(heap->isolate());
    CcTest::i_isolate()->factory()->NewJSArrayBufferAndBackingStore(
        64 * KB, InitializedFlag{true});
  }
  int mark_sweep_count_after = heap->ms_count();
  // We expect only scavenges, no full GCs.
  CHECK_EQ(mark_sweep_count_before, mark_sweep_count_after);
}

#if defined(V8_TARGET_ARCH_64_BIT) && !defined(V8_OS_ANDROID)
UNINITIALIZED_TEST(HugeHeapLimit) {
  uint64_t kMemoryGB = 16;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  create_params.constraints.ConfigureDefaults(kMemoryGB * GB, kMemoryGB * GB);
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
#ifdef V8_COMPRESS_POINTERS
  size_t kExpectedHeapLimit = Heap::kAllocatorLimitOnMaxOldGenerationSize;
#else
  size_t kExpectedHeapLimit = size_t{4} * GB;
#endif
  CHECK_EQ(kExpectedHeapLimit, i_isolate->heap()->MaxOldGenerationSize());
  CHECK_LT(size_t{3} * GB, i_isolate->heap()->MaxOldGenerationSize());
  isolate->Dispose();
}
#endif

UNINITIALIZED_TEST(HeapLimit) {
  uint64_t kMemoryGB = 16;
  v8_flags.high_end_android_physical_memory_threshold =
      static_cast<unsigned int>(kMemoryGB);
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  create_params.constraints.ConfigureDefaults(kMemoryGB * GB, kMemoryGB * GB);
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
#if defined(V8_TARGET_ARCH_64_BIT)
  // Because we explicitly set --high_end_android_physical_memory_threshold,
  // Android has the same expected heap limit.
  size_t kExpectedHeapLimit = size_t{4} * GB;
#else
  size_t kExpectedHeapLimit = size_t{1} * GB;
#endif
  CHECK_EQ(kExpectedHeapLimit, i_isolate->heap()->MaxOldGenerationSize());
  isolate->Dispose();
}

TEST(NoCodeRangeInJitlessMode) {
  if (!v8_flags.jitless) return;
  CcTest::InitializeVM();
  CHECK(CcTest::i_isolate()->heap()->code_region().is_empty());
}

TEST(GarbageCollectionWithLocalHeap) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();

  LocalHeap* local_heap = CcTest::i_isolate()->main_thread_local_heap();

  heap::InvokeMajorGC(CcTest::heap());
  local_heap->ExecuteWhileParked([]() { /* nothing */ });
  heap::InvokeMajorGC(CcTest::heap());
}

TEST(Regress10698) {
  if (!v8_flags.incremental_marking) return;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  HandleScope handle_scope(isolate);
  // This is modeled after the manual allocation folding of heap numbers in
  // JSON parser (See commit ba7b25e).
  // Step 1. Allocate a byte array in the old space.
  DirectHandle<ByteArray> array =
      factory->NewByteArray(kTaggedSize, AllocationType::kOld);
  // Step 2. Start incremental marking.
  SimulateIncrementalMarking(heap, false);
  // Step 3. Allocate another byte array. It will be black.
  factory->NewByteArray(kTaggedSize, AllocationType::kOld);
  Address address = reinterpret_cast<Address>(array->begin());
  Tagged<HeapObject> filler = HeapObject::FromAddress(address);
  // Step 4. Set the filler at the end of the first array.
  // It will have an impossible markbit pattern because the second markbit
  // will be taken from the second array.
  filler->set_map_after_allocation(isolate, *factory->one_pointer_filler_map());
}

class TestAllocationTracker : public HeapObjectAllocationTracker {
 public:
  explicit TestAllocationTracker(int expected_size)
      : expected_size_(expected_size) {}

  void AllocationEvent(Address addr, int size) {
    CHECK(expected_size_ == size);
    address_ = addr;
  }

  Address address() { return address_; }

 private:
  int expected_size_;
  Address address_;
};

HEAP_TEST(CodeLargeObjectSpace) {
  Heap* heap = CcTest::heap();
  int size_in_bytes =
      heap->MaxRegularHeapObjectSize(AllocationType::kCode) + kTaggedSize;
  TestAllocationTracker allocation_tracker{size_in_bytes};
  heap->AddHeapObjectAllocationTracker(&allocation_tracker);

  Tagged<HeapObject> obj;
  {
    AllocationResult allocation = heap->AllocateRaw(
        size_in_bytes, AllocationType::kCode, AllocationOrigin::kRuntime);
    CHECK(allocation.To(&obj));
    CHECK_EQ(allocation.ToAddress(), allocation_tracker.address());
    ThreadIsolation::RegisterInstructionStreamAllocation(obj.address(),
                                                         size_in_bytes);

    heap->CreateFillerObjectAt(obj.address(), size_in_bytes);
  }

  CHECK(HeapLayout::InAnyLargeSpace(obj));
  heap->RemoveHeapObjectAllocationTracker(&allocation_tracker);
}

UNINITIALIZED_HEAP_TEST(CodeLargeObjectSpace64k) {
  // Simulate having a system with 64k OS pages.
  i::v8_flags.v8_os_page_size = 64;

  // Initialize the isolate manually to make sure --v8-os-page-size is taken
  // into account.
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  Heap* heap = i_isolate->heap();

  // Allocate a regular code object.
  {
    v8::Isolate::Scope isolate_scope(isolate);
    int size_in_bytes =
        heap->MaxRegularHeapObjectSize(AllocationType::kCode) - kTaggedSize;
    TestAllocationTracker allocation_tracker{size_in_bytes};
    heap->AddHeapObjectAllocationTracker(&allocation_tracker);

    Tagged<HeapObject> obj;
    {
      AllocationResult allocation = heap->AllocateRaw(
          size_in_bytes, AllocationType::kCode, AllocationOrigin::kRuntime);
      CHECK(allocation.To(&obj));
      CHECK_EQ(allocation.ToAddress(), allocation_tracker.address());
      ThreadIsolation::RegisterInstructionStreamAllocation(obj.address(),
                                                           size_in_bytes);

      heap->CreateFillerObjectAt(obj.address(), size_in_bytes);
    }

    CHECK(!HeapLayout::InAnyLargeSpace(obj));
    heap->RemoveHeapObjectAllocationTracker(&allocation_tracker);
  }

  // Allocate a large code object.
  {
    v8::Isolate::Scope isolate_scope(isolate);
    int size_in_bytes =
        heap->MaxRegularHeapObjectSize(AllocationType::kCode) + kTaggedSize;
    TestAllocationTracker allocation_tracker{size_in_bytes};
    heap->AddHeapObjectAllocationTracker(&allocation_tracker);

    Tagged<HeapObject> obj;
    {
      AllocationResult allocation = heap->AllocateRaw(
          size_in_bytes, AllocationType::kCode, AllocationOrigin::kRuntime);
      CHECK(allocation.To(&obj));
      CHECK_EQ(allocation.ToAddress(), allocation_tracker.address());
      ThreadIsolation::RegisterInstructionStreamAllocation(obj.address(),
                                                           size_in_bytes);

      heap->CreateFillerObjectAt(obj.address(), size_in_bytes);
    }

    CHECK(HeapLayout::InAnyLargeSpace(obj));
    heap->RemoveHeapObjectAllocationTracker(&allocation_tracker);
  }

  isolate->Dispose();
}

TEST(IsPendingAllocationNewSpace) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  HandleScope handle_scope(isolate);
  DirectHandle<FixedArray> object =
      factory->NewFixedArray(5, AllocationType::kYoung);
  CHECK(heap->IsPendingAllocation(*object));
  heap->PublishMainThreadPendingAllocations();
  CHECK(!heap->IsPendingAllocation(*object));
}

TEST(IsPendingAllocationNewLOSpace) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  HandleScope handle_scope(isolate);
  DirectHandle<FixedArray> object = factory->NewFixedArray(
      FixedArray::kMaxRegularLength + 1, AllocationType::kYoung);
  CHECK(heap->IsPendingAllocation(*object));
  heap->PublishMainThreadPendingAllocations();
  CHECK(!heap->IsPendingAllocation(*object));
}

TEST(IsPendingAllocationOldSpace) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  HandleScope handle_scope(isolate);
  DirectHandle<FixedArray> object =
      factory->NewFixedArray(5, AllocationType::kOld);
  CHECK(heap->IsPendingAllocation(*object));
  heap->PublishMainThreadPendingAllocations();
  CHECK(!heap->IsPendingAllocation(*object));
}

TEST(IsPendingAllocationLOSpace) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  HandleScope handle_scope(isolate);
  DirectHandle<FixedArray> object = factory->NewFixedArray(
      FixedArray::kMaxRegularLength + 1, AllocationType::kOld);
  CHECK(heap->IsPendingAllocation(*object));
  heap->PublishMainThreadPendingAllocations();
  CHECK(!heap->IsPendingAllocation(*object));
}

TEST(Regress10900) {
  v8_flags.compact_on_every_full_gc = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope handle_scope(isolate);
  uint8_t buffer[i::Assembler::kDefaultBufferSize];
  MacroAssembler masm(isolate, v8::internal::CodeObjectRequired{true},
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));
#if V8_TARGET_ARCH_ARM64
  UseScratchRegisterScope temps(&masm);
  Register tmp = temps.AcquireX();
  masm.Mov(tmp, Operand(static_cast<int32_t>(
                    ReadOnlyRoots(heap).undefined_value().ptr())));
  masm.Push(tmp, tmp);
#else
  masm.Push(isolate->factory()->undefined_value());
#endif
  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  {
    DirectHandle<Code> code;
    for (int i = 0; i < 100; i++) {
      // Generate multiple code pages.
      code = Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
    }
  }
  // Force garbage collection that compacts code pages and triggers
  // an assertion in Isolate::AddCodeMemoryRange before the bug fix.
  heap::InvokeMemoryReducingMajorGCs(heap);
}

namespace {
v8::Local<v8::Value> GenerateGarbage() {
  const char* source =
      "let roots = [];"
      "for (let i = 0; i < 100; i++) roots.push(new Array(1000).fill(0));"
      "roots.push(new Array(1000000).fill(0));"
      "roots;";
  return CompileRun(source);
}

}  // anonymous namespace

TEST(Regress11181) {
  v8_flags.compact_on_every_full_gc = true;
  CcTest::InitializeVM();
  TracingFlags::runtime_stats.store(
      v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE,
      std::memory_order_relaxed);
  v8::HandleScope scope(CcTest::isolate());
  GenerateGarbage();
  heap::InvokeMemoryReducingMajorGCs(CcTest::heap());
}

TEST(LongTaskStatsFullAtomic) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(CcTest::isolate());
  GenerateGarbage();
  v8::metrics::LongTaskStats::Reset(isolate);
  CHECK_EQ(0u, v8::metrics::LongTaskStats::Get(isolate)
                   .gc_full_atomic_wall_clock_duration_us);
  for (int i = 0; i < 10; ++i) {
    heap::InvokeMemoryReducingMajorGCs(CcTest::heap());
  }
  CHECK_LT(0u, v8::metrics::LongTaskStats::Get(isolate)
                   .gc_full_atomic_wall_clock_duration_us);
  v8::metrics::LongTaskStats::Reset(isolate);
  CHECK_EQ(0u, v8::metrics::LongTaskStats::Get(isolate)
                   .gc_full_atomic_wall_clock_duration_us);
}

TEST(LongTaskStatsFullIncremental) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(CcTest::isolate());
  v8::Global<v8::Value> objects(isolate, GenerateGarbage());
  v8::metrics::LongTaskStats::Reset(isolate);
  CHECK_EQ(0u, v8::metrics::LongTaskStats::Get(isolate)
                   .gc_full_incremental_wall_clock_duration_us);
  for (int i = 0; i < 10; ++i) {
    heap::SimulateIncrementalMarking(CcTest::heap());
    heap::InvokeMemoryReducingMajorGCs(CcTest::heap());
  }
  CHECK_LT(0u, v8::metrics::LongTaskStats::Get(isolate)
                   .gc_full_incremental_wall_clock_duration_us);
  v8::metrics::LongTaskStats::Reset(isolate);
  CHECK_EQ(0u, v8::metrics::LongTaskStats::Get(isolate)
                   .gc_full_incremental_wall_clock_duration_us);
}

TEST(LongTaskStatsYoung) {
  if (v8_flags.single_generation) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(CcTest::isolate());
  GenerateGarbage();
  v8::metrics::LongTaskStats::Reset(isolate);
  CHECK_EQ(
      0u,
      v8::metrics::LongTaskStats::Get(isolate).gc_young_wall_clock_duration_us);
  for (int i = 0; i < 10; ++i) {
    heap::InvokeMinorGC(CcTest::heap());
  }
  CHECK_LT(
      0u,
      v8::metrics::LongTaskStats::Get(isolate).gc_young_wall_clock_duration_us);
  v8::metrics::LongTaskStats::Reset(isolate);
  CHECK_EQ(
      0u,
      v8::metrics::LongTaskStats::Get(isolate).gc_young_wall_clock_duration_us);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8

#undef __
