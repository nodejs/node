// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/compiler.h"
#include "src/compiler/pipeline.h"
#include "src/handles.h"
#include "src/parser.h"
#include "src/rewriter.h"
#include "src/scopes.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

TEST(PipelineAdd) {
  InitializedHandleScope handles;
  const char* source = "(function(a,b) { return a + b; })";
  Handle<JSFunction> function = v8::Utils::OpenHandle(
      *v8::Handle<v8::Function>::Cast(CompileRun(source)));
  CompilationInfoWithZone info(function);

  CHECK(Parser::Parse(&info));
  StrictMode strict_mode = info.function()->strict_mode();
  info.SetStrictMode(strict_mode);
  CHECK(Rewriter::Rewrite(&info));
  CHECK(Scope::Analyze(&info));
  CHECK_NE(NULL, info.scope());

  Pipeline pipeline(&info);
#if V8_TURBOFAN_TARGET
  Handle<Code> code = pipeline.GenerateCode();
  CHECK(Pipeline::SupportedTarget());
  CHECK(!code.is_null());
#else
  USE(pipeline);
#endif
}
