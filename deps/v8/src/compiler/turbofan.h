// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOFAN_H_
#define V8_COMPILER_TURBOFAN_H_

#include <memory>

// Clients of this interface shouldn't depend on compiler internals.
// Do not include anything from src/compiler here, and keep includes minimal.

#include "src/base/macros.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

class Isolate;
class JSFunction;
class TurbofanCompilationJob;

namespace compiler {

// Whether the given JSFunction has an associated Script.
enum class IsScriptAvailable {
  kNo,
  kYes,
};

V8_EXPORT_PRIVATE std::unique_ptr<TurbofanCompilationJob> NewCompilationJob(
    Isolate* isolate, Handle<JSFunction> function, IsScriptAvailable has_script,
    BytecodeOffset osr_offset = BytecodeOffset::None());

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_TURBOFAN_H_
