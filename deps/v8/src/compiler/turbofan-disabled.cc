// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file stubs out the Turbofan API when TF is disabled.
// See also v8_enable_turbofan in BUILD.gn.

#include "src/codegen/compiler.h"
#include "src/compiler/turbofan.h"

namespace v8 {
namespace internal {
namespace compiler {

std::unique_ptr<TurbofanCompilationJob> NewCompilationJob(
    Isolate* isolate, Handle<JSFunction> function, IsScriptAvailable has_script,
    BytecodeOffset osr_offset) {
  FATAL(
      "compiler::NewCompilationJob must not be called when Turbofan is "
      "disabled (`v8_enable_turbofan = false`)");
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
