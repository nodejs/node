// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ASMJS_ASM_WASM_BUILDER_H_
#define V8_ASMJS_ASM_WASM_BUILDER_H_

#include "src/allocation.h"
#include "src/asmjs/asm-typer.h"
#include "src/objects.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class CompilationInfo;

namespace wasm {

class AsmWasmBuilder {
 public:
  struct Result {
    ZoneBuffer* module_bytes;
    ZoneBuffer* asm_offset_table;
    bool success;
  };

  explicit AsmWasmBuilder(CompilationInfo* info);
  Result Run(Handle<FixedArray>* foreign_args);

  static const char* foreign_init_name;
  static const char* single_function_name;

  const AsmTyper* typer() { return &typer_; }

 private:
  CompilationInfo* info_;
  AsmTyper typer_;
};
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_ASM_WASM_BUILDER_H_
