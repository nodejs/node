// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASELINE_BASELINE_H_
#define V8_BASELINE_BASELINE_H_

#include "src/handles/handles.h"

namespace v8 {
namespace internal {

class Code;
class SharedFunctionInfo;
class MacroAssembler;

bool CanCompileWithBaseline(Isolate* isolate, SharedFunctionInfo shared);

MaybeHandle<Code> GenerateBaselineCode(Isolate* isolate,
                                       Handle<SharedFunctionInfo> shared);

void EmitReturnBaseline(MacroAssembler* masm);

}  // namespace internal
}  // namespace v8

#endif  // V8_BASELINE_BASELINE_H_
