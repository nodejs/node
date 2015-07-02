// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/compiler.h"
#include "src/compiler/pipeline.h"
#include "src/handles.h"
#include "src/parser.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

static void RunPipeline(Zone* zone, const char* source) {
  Handle<JSFunction> function = v8::Utils::OpenHandle(
      *v8::Handle<v8::Function>::Cast(CompileRun(source)));
  ParseInfo parse_info(zone, function);
  CHECK(Compiler::ParseAndAnalyze(&parse_info));
  CompilationInfo info(&parse_info);

  Pipeline pipeline(&info);
#if V8_TURBOFAN_TARGET
  Handle<Code> code = pipeline.GenerateCode();
  CHECK(Pipeline::SupportedTarget());
  CHECK(!code.is_null());
#else
  USE(pipeline);
#endif
}


TEST(PipelineTyped) {
  HandleAndZoneScope handles;
  FLAG_turbo_types = true;
  RunPipeline(handles.main_zone(), "(function(a,b) { return a + b; })");
}


TEST(PipelineGeneric) {
  HandleAndZoneScope handles;
  FLAG_turbo_types = false;
  RunPipeline(handles.main_zone(), "(function(a,b) { return a + b; })");
}
