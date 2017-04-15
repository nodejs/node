// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ASMJS_ASM_JS_H_
#define V8_ASMJS_ASM_JS_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

class CompilationInfo;
class JSArrayBuffer;

// Interface to compile and instantiate for asmjs.
class AsmJs {
 public:
  static MaybeHandle<FixedArray> CompileAsmViaWasm(CompilationInfo* info);
  static bool IsStdlibValid(Isolate* isolate, Handle<FixedArray> wasm_data,
                            Handle<JSReceiver> stdlib);
  static MaybeHandle<Object> InstantiateAsmWasm(Isolate* isolate,
                                                Handle<FixedArray> wasm_data,
                                                Handle<JSArrayBuffer> memory,
                                                Handle<JSReceiver> foreign);
};

}  // namespace internal
}  // namespace v8
#endif
