// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DIAGNOSTICS_ETW_JIT_WIN_H_
#define V8_DIAGNOSTICS_ETW_JIT_WIN_H_

#include <atomic>

#include "include/v8config.h"

namespace v8 {

class Isolate;
struct JitCodeEvent;

namespace internal {
namespace ETWJITInterface {
extern std::atomic<bool> is_etw_enabled;

void Register();
void Unregister();
void AddIsolate(Isolate* isolate);
void RemoveIsolate(Isolate* isolate);
void EventHandler(const v8::JitCodeEvent* event);
void MaybeSetHandlerNow(Isolate* isolate);
}  // namespace ETWJITInterface
}  // namespace internal
}  // namespace v8

#endif  // V8_DIAGNOSTICS_ETW_JIT_WIN_H_
