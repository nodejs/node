// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/codegen/compiler.h"
#include "src/execution/isolate-inl.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/parsing/parse-info.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using Regress568831099Test = TestWithContext;

TEST_F(Regress568831099Test, ReparseFunctionConstructorToplevel) {
  v8::HandleScope scope(isolate());

  RunJS(
      "var anonymous = 40;\n"
      "var fn = new Function('a', 'if (a === 0) { anonymous; a.x } a.s = 1; "
      "return a.s + anonymous;');\n"
      "for (var i = 0; i < 20; i++) fn({s: 0});\n");

  v8::Local<v8::Object> global = context()->Global();
  DirectHandle<JSFunction> fn = Cast<JSFunction>(v8::Utils::OpenDirectHandle(
      *global->Get(context(), NewString("fn")).ToLocalChecked()));

  Handle<SharedFunctionInfo> inner_sfi(fn->shared(), i_isolate());
  DirectHandle<Script> script(Cast<Script>(inner_sfi->script()), i_isolate());
  EXPECT_EQ(script->compilation_kind(),
            Script::CompilationKind::kFunctionConstructor);
  EXPECT_EQ(inner_sfi->syntax_kind(), FunctionSyntaxKind::kAnonymousExpression);

  const int expected_slot_count = inner_sfi->feedback_metadata()->slot_count();
  ASSERT_TRUE(fn->has_feedback_vector());
  EXPECT_EQ(static_cast<int>(fn->feedback_vector()->length().value()),
            expected_slot_count);

  Tagged<MaybeObject> maybe_toplevel =
      script->infos()->get(kFunctionLiteralIdTopLevel);
  Handle<SharedFunctionInfo> toplevel_sfi(
      Cast<SharedFunctionInfo>(maybe_toplevel.GetHeapObjectAssumeWeak()),
      i_isolate());
  ASSERT_TRUE(toplevel_sfi->is_toplevel());

  // Top-level compiles for kFunctionConstructor scripts must restore the
  // ONLY_SINGLE_FUNCTION_LITERAL parse restriction, while inner function
  // compiles must not.
  UnoptimizedCompileFlags toplevel_flags =
      UnoptimizedCompileFlags::ForFunctionCompile(i_isolate(), *toplevel_sfi);
  EXPECT_EQ(toplevel_flags.parse_restriction(), ONLY_SINGLE_FUNCTION_LITERAL);

  UnoptimizedCompileFlags inner_flags =
      UnoptimizedCompileFlags::ForFunctionCompile(i_isolate(), *inner_sfi);
  EXPECT_EQ(inner_flags.parse_restriction(), NO_PARSE_RESTRICTION);

  // Exercise source position collection on the top-level SFI while compiled.
  SharedFunctionInfo::EnsureSourcePositionsAvailable(i_isolate(), toplevel_sfi);

  // Flush bytecode from both the top-level SFI and the inner function SFI,
  // while `fn` keeps its FeedbackVector.
  if (toplevel_sfi->CanDiscardCompiled()) {
    SharedFunctionInfo::DiscardCompiled(i_isolate(), toplevel_sfi);
  }
  ASSERT_TRUE(inner_sfi->CanDiscardCompiled());
  SharedFunctionInfo::DiscardCompiled(i_isolate(), inner_sfi);
  ASSERT_FALSE(toplevel_sfi->is_compiled());
  ASSERT_FALSE(inner_sfi->is_compiled());

  // Recompiling the top-level SFI also eagerly recompiles the parenthesized
  // `(function anonymous(...) { ... })` inner SFI. It must still be parsed as
  // an anonymous dynamic function declaration rather than a named function
  // expression.
  IsCompiledScope is_compiled_scope;
  SharedFunctionInfo::EnsureBytecodeArrayAvailable(i_isolate(), toplevel_sfi,
                                                   &is_compiled_scope,
                                                   CreateSourcePositions{true});
  EXPECT_TRUE(toplevel_sfi->is_compiled());
  EXPECT_TRUE(inner_sfi->is_compiled());
  EXPECT_EQ(inner_sfi->syntax_kind(), FunctionSyntaxKind::kAnonymousExpression);
  EXPECT_EQ(inner_sfi->feedback_metadata()->slot_count(), expected_slot_count);

  v8::Local<v8::Value> result = RunJS("fn({s: 0})");
  EXPECT_EQ(result->Int32Value(context()).FromJust(), 41);
}

}  // namespace internal
}  // namespace v8
