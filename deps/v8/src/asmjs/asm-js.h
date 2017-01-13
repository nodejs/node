// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ASMJS_ASM_JS_H_
#define V8_ASMJS_ASM_JS_H_

#ifndef V8_SHARED
#include "src/allocation.h"
#include "src/base/hashmap.h"
#else
#include "include/v8.h"
#include "src/base/compiler-specific.h"
#endif  // !V8_SHARED
#include "src/parsing/parser.h"

namespace v8 {
namespace internal {
// Interface to compile and instantiate for asmjs.
class AsmJs {
 public:
  static MaybeHandle<FixedArray> ConvertAsmToWasm(i::ParseInfo* info);
  static bool IsStdlibValid(i::Isolate* isolate, Handle<FixedArray> wasm_data,
                            Handle<JSReceiver> stdlib);
  static MaybeHandle<Object> InstantiateAsmWasm(i::Isolate* isolate,
                                                Handle<FixedArray> wasm_data,
                                                Handle<JSArrayBuffer> memory,
                                                Handle<JSReceiver> foreign);
};

}  // namespace internal
}  // namespace v8
#endif
