// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler.h"
#include "src/compiler/pipeline.h"
#include "src/handles.h"
#include "src/parsing/parser.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

static void RunPipeline(Zone* zone, const char* source) {
  Handle<JSFunction> function = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(CompileRun(source))));
  ParseInfo parse_info(zone, function);
  CHECK(Compiler::ParseAndAnalyze(&parse_info));
  CompilationInfo info(&parse_info);
  info.SetOptimizing();

  Pipeline pipeline(&info);
  Handle<Code> code = pipeline.GenerateCode();
  CHECK(!code.is_null());
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8
