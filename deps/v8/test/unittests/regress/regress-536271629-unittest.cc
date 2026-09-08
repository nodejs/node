// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate-inl.h"
#include "src/execution/isolate.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/scope-info.h"
#include "src/objects/shared-function-info-inl.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

using SloppyArgumentsScopeInfoTest = TestWithContext;

namespace {

DirectHandle<JSFunction> GetFunction(v8::Local<v8::Context> context,
                                     v8::Local<v8::Object> global,
                                     v8::Local<v8::String> name) {
  return Cast<JSFunction>(v8::Utils::OpenDirectHandle(
      *global->Get(context, name).ToLocalChecked()));
}

}  // namespace

// A SharedFunctionInfo whose ScopeInfo says that the parameters are not simple
// must never end up on the mapped arguments path: the arguments object would be
// allocated with the strict arguments map and then be given the larger fast
// aliased arguments map.
TEST_F(SloppyArgumentsScopeInfoTest, MappedArgumentsWithNonSimpleScopeInfo) {
  v8::HandleScope scope(isolate());

  RunJS(
      "function nonSimple(a, b = 0) { var g = () => a; return arguments[0]; }"
      "function duplicate(a, a) { var g = () => a; return arguments[0]; }"
      "nonSimple(1, 2);"
      "duplicate(1, 2);");

  v8::Local<v8::Object> global = context()->Global();
  DirectHandle<JSFunction> non_simple =
      GetFunction(context(), global, NewString("nonSimple"));
  DirectHandle<JSFunction> duplicate =
      GetFunction(context(), global, NewString("duplicate"));

  DirectHandle<ScopeInfo> non_simple_scope_info(
      non_simple->shared()->scope_info(), i_isolate());
  CHECK(!non_simple_scope_info->HasSimpleParameters());
  CHECK(duplicate->shared()->scope_info()->HasSimpleParameters());
  CHECK(duplicate->shared()->has_duplicate_parameters());

  // Emulate the incompatible ScopeInfo that a mismatched code cache can install
  // on an otherwise simple-parameter function.
  duplicate->shared()->SetScopeInfo(*non_simple_scope_info);

  EXPECT_DEATH_IF_SUPPORTED(RunJS("duplicate(1, 2)"), "");
}

// The mapped arguments path allocates with the sloppy arguments map and then
// installs the fast aliased arguments map, so the two must have the same
// instance size.
TEST_F(SloppyArgumentsScopeInfoTest, ArgumentsMapInstanceSizes) {
  v8::HandleScope scope(isolate());
  Tagged<NativeContext> context = i_isolate()->raw_native_context();

  const int sloppy_size = context->sloppy_arguments_map()->instance_size();
  CHECK_EQ(sloppy_size, context->fast_aliased_arguments_map()->instance_size());
  CHECK_EQ(sloppy_size, context->slow_aliased_arguments_map()->instance_size());
  CHECK_LT(context->strict_arguments_map()->instance_size(), sloppy_size);
}

}  // namespace internal
}  // namespace v8
