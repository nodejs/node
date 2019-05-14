// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/thread-local-top.h"
#include "src/isolate.h"
#include "src/simulator.h"
#include "src/trap-handler/trap-handler.h"

namespace v8 {
namespace internal {

void ThreadLocalTop::Initialize(Isolate* isolate) {
  *this = ThreadLocalTop();
  isolate_ = isolate;
#ifdef USE_SIMULATOR
  simulator_ = Simulator::current(isolate);
#endif
  thread_id_ = ThreadId::Current();
  thread_in_wasm_flag_address_ = reinterpret_cast<Address>(
      trap_handler::GetThreadInWasmThreadLocalAddress());
}

void ThreadLocalTop::Free() {
  // Match unmatched PopPromise calls.
  while (promise_on_stack_) isolate_->PopPromise();
}

}  // namespace internal
}  // namespace v8
