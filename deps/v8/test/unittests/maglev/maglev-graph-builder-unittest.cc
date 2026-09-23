// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_ENABLE_MAGLEV

#include "src/maglev/maglev-graph-builder.h"

#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compiler.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "test/unittests/maglev/maglev-test.h"

namespace v8 {
namespace internal {
namespace maglev {

class MaglevGraphBuilderTest : public TestWithNativeContextAndZone {};

template <typename T>
static T* getUniqueNode(Graph* graph) {
  T* target = nullptr;
  for (auto* block : graph->blocks()) {
    for (auto* node : block->nodes()) {
      if (node->Is<T>()) {
        CHECK_NULL(target);
        target = node->Cast<T>();
      }
    }
  }
  CHECK_NOT_NULL(target);
  return target;
}

TEST_F(MaglevGraphBuilderTest, TrailingArgumentsRemoval) {
  i::v8_flags.allow_natives_syntax = true;
  for (bool strict : {true, false}) {
    HandleScope scope(isolate());
    std::string script;
    if (strict) {
      script += "'use strict'\n";
    }
    script += R"(
      function f(a, b) { return a + b; }
      function g(a, b, c, d) { return f(a, b, c, d); }
      %PrepareFunctionForOptimization(g);
      g(1, 2, 3, 4);
      (g)
    )";
    Handle<JSFunction> function = RunJS<JSFunction>(script.c_str());
    CHECK_EQ(strict, function->shared()->CanOnlyAccessFixedFormalParameters());
    auto info =
        MaglevCompilationInfo::New(isolate(), function, BytecodeOffset::None());
    Graph* graph = Graph::New(info.get());
    compiler::CurrentHeapBrokerScope current_broker(info->broker());
    MaglevGraphBuilder graph_builder(isolate()->AsLocalIsolate(),
                                     info->toplevel_compilation_unit(), graph);
    PersistentHandlesScope persistent_scope(isolate());
    CHECK(graph_builder.Build());
    CallKnownJSFunction* func = getUniqueNode<CallKnownJSFunction>(graph);
    CHECK_EQ(strict ? 2 : 4, func->num_args());
    auto callee = func->shared_function_info().object()->Name()->ToCString();
    CHECK_EQ(0, strcmp("f", callee.get()));
    persistent_scope.Detach();
  }
}

TEST_F(MaglevGraphBuilderTest, UnusedArgumentsRemoval) {
  i::v8_flags.allow_natives_syntax = true;
  HandleScope scope(isolate());
  std::string script = R"(
      'use strict'
      function f(a, b) { return b; }
      function g(a, b) { return f(a, b); }
      %PrepareFunctionForOptimization(g);
      g(1, 2);
      (g)
    )";
  Handle<JSFunction> function = RunJS<JSFunction>(script.c_str());
  auto info =
      MaglevCompilationInfo::New(isolate(), function, BytecodeOffset::None());
  Graph* graph = Graph::New(info.get());
  compiler::CurrentHeapBrokerScope current_broker(info->broker());
  MaglevGraphBuilder graph_builder(isolate()->AsLocalIsolate(),
                                   info->toplevel_compilation_unit(), graph);
  PersistentHandlesScope persistent_scope(isolate());
  CHECK(graph_builder.Build());
  CallKnownJSFunction* func = getUniqueNode<CallKnownJSFunction>(graph);
  CHECK_EQ(2, func->num_args());
  auto callee = func->shared_function_info().object()->Name()->ToCString();
  CHECK_EQ(0, strcmp("f", callee.get()));
  // First arg has been eliminated.
  CHECK_EQ(func->arg(0).node()->Cast<RootConstant>()->index(),
           RootIndex::kOptimizedOut);
  persistent_scope.Detach();
}

// Regression test for the unused-parameter replacement loop bounding on
// `sizeof(unused_parameter_bits)` (== 4) instead of the highest set bit. An
// unused parameter at an index >= 4 must still be replaced by OptimizedOut.
TEST_F(MaglevGraphBuilderTest, UnusedArgumentsRemovalHighIndex) {
  i::v8_flags.allow_natives_syntax = true;
  HandleScope scope(isolate());
  // The 6th parameter (index 5) is unused; all others are used.
  std::string script = R"(
      'use strict'
      function f(a, b, c, d, e, unused) { return a + b + c + d + e; }
      function g(a, b, c, d, e, h) { return f(a, b, c, d, e, h); }
      %PrepareFunctionForOptimization(g);
      g(1, 2, 3, 4, 5, 6);
      (g)
    )";
  Handle<JSFunction> function = RunJS<JSFunction>(script.c_str());
  auto info =
      MaglevCompilationInfo::New(isolate(), function, BytecodeOffset::None());
  Graph* graph = Graph::New(info.get());
  compiler::CurrentHeapBrokerScope current_broker(info->broker());
  MaglevGraphBuilder graph_builder(isolate()->AsLocalIsolate(),
                                   info->toplevel_compilation_unit(), graph);
  PersistentHandlesScope persistent_scope(isolate());
  CHECK(graph_builder.Build());
  CallKnownJSFunction* func = getUniqueNode<CallKnownJSFunction>(graph);
  // No trailing arguments to remove: all 6 are formal parameters.
  CHECK_EQ(6, func->num_args());
  auto callee = func->shared_function_info().object()->Name()->ToCString();
  CHECK_EQ(0, strcmp("f", callee.get()));
  // The unused parameter at index 5 must have been replaced.
  CHECK_EQ(func->arg(5).node()->Cast<RootConstant>()->index(),
           RootIndex::kOptimizedOut);
  persistent_scope.Detach();
}

// Regression test for the truncation path: when the callee is invoked with more
// arguments than it has formal parameters, the trailing arguments are dropped
// and unused parameters (including ones at high indices) must still be replaced
// by OptimizedOut, without indexing past the truncated argument list.
TEST_F(MaglevGraphBuilderTest, UnusedArgumentsRemovalWithTruncation) {
  i::v8_flags.allow_natives_syntax = true;
  HandleScope scope(isolate());
  // `f` has 5 formal parameters; the 5th (index 4) is unused. It is called with
  // 6 arguments, so the call is truncated down to 5 arguments.
  std::string script = R"(
      'use strict'
      function f(a, b, c, d, unused) { return a + b + c + d; }
      function g(a, b, c, d, e, h) { return f(a, b, c, d, e, h); }
      %PrepareFunctionForOptimization(g);
      g(1, 2, 3, 4, 5, 6);
      (g)
    )";
  Handle<JSFunction> function = RunJS<JSFunction>(script.c_str());
  auto info =
      MaglevCompilationInfo::New(isolate(), function, BytecodeOffset::None());
  Graph* graph = Graph::New(info.get());
  compiler::CurrentHeapBrokerScope current_broker(info->broker());
  MaglevGraphBuilder graph_builder(isolate()->AsLocalIsolate(),
                                   info->toplevel_compilation_unit(), graph);
  PersistentHandlesScope persistent_scope(isolate());
  CHECK(graph_builder.Build());
  CallKnownJSFunction* func = getUniqueNode<CallKnownJSFunction>(graph);
  // The trailing 6th argument has been dropped, leaving the 5 formal params.
  CHECK_EQ(5, func->num_args());
  auto callee = func->shared_function_info().object()->Name()->ToCString();
  CHECK_EQ(0, strcmp("f", callee.get()));
  // The unused parameter at index 4 must have been replaced.
  CHECK_EQ(func->arg(4).node()->Cast<RootConstant>()->index(),
           RootIndex::kOptimizedOut);
  persistent_scope.Detach();
}

// Regression test for `unused_parameter_bits` being stored as a 31-bit pattern
// in a Smi: reading it back used to sign-extend bit 30 into the upper bits.
// With exactly 31 used-or-tracked parameters where the 31st parameter (index
// 30) is unused, the sign extension would incorrectly mark the 32nd parameter
// (index 31) as unused and replace it with OptimizedOut.
TEST_F(MaglevGraphBuilderTest, UnusedArgumentsRemovalSignExtension) {
  i::v8_flags.allow_natives_syntax = true;
  HandleScope scope(isolate());
  // `f` has 32 parameters. Only the last one (index 31) is used; all of the
  // first 31 parameters (indices 0..30) are unused, yielding unused-parameter
  // bits of 0x7FFFFFFF. The used parameter at index 31 must NOT be replaced.
  std::string script = R"(
      'use strict'
      function f(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13,
                 p14, p15, p16, p17, p18, p19, p20, p21, p22, p23, p24, p25,
                 p26, p27, p28, p29, p30, p31) {
        return p31;
      }
      function g() {
        return f(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
                 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);
      }
      %PrepareFunctionForOptimization(g);
      g();
      (g)
    )";
  Handle<JSFunction> function = RunJS<JSFunction>(script.c_str());
  auto info =
      MaglevCompilationInfo::New(isolate(), function, BytecodeOffset::None());
  Graph* graph = Graph::New(info.get());
  compiler::CurrentHeapBrokerScope current_broker(info->broker());
  MaglevGraphBuilder graph_builder(isolate()->AsLocalIsolate(),
                                   info->toplevel_compilation_unit(), graph);
  PersistentHandlesScope persistent_scope(isolate());
  CHECK(graph_builder.Build());
  CallKnownJSFunction* func = getUniqueNode<CallKnownJSFunction>(graph);
  CHECK_EQ(32, func->num_args());
  auto callee = func->shared_function_info().object()->Name()->ToCString();
  CHECK_EQ(0, strcmp("f", callee.get()));
  // The used parameter at index 31 must NOT have been replaced.
  RootConstant* arg31 = func->arg(31).node()->TryCast<RootConstant>();
  CHECK(arg31 == nullptr || arg31->index() != RootIndex::kOptimizedOut);
  // The unused parameter at index 30 must have been replaced.
  CHECK_EQ(func->arg(30).node()->Cast<RootConstant>()->index(),
           RootIndex::kOptimizedOut);
  persistent_scope.Detach();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_MAGLEV
