// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ISOLATE_INL_H_
#define V8_ISOLATE_INL_H_

#include "debug.h"
#include "isolate.h"
#include "utils/random-number-generator.h"

namespace v8 {
namespace internal {


SaveContext::SaveContext(Isolate* isolate)
  : isolate_(isolate),
    prev_(isolate->save_context()) {
  if (isolate->context() != NULL) {
    context_ = Handle<Context>(isolate->context());
  }
  isolate->set_save_context(this);

  c_entry_fp_ = isolate->c_entry_fp(isolate->thread_local_top());
}


bool Isolate::IsCodePreAgingActive() {
  return FLAG_optimize_for_size && FLAG_age_code && !IsDebuggerActive();
}


bool Isolate::IsDebuggerActive() {
  return debugger()->IsDebuggerActive();
}


bool Isolate::DebuggerHasBreakPoints() {
  return debug()->has_break_points();
}


RandomNumberGenerator* Isolate::random_number_generator() {
  if (random_number_generator_ == NULL) {
    random_number_generator_ = new RandomNumberGenerator;
  }
  return random_number_generator_;
}

} }  // namespace v8::internal

#endif  // V8_ISOLATE_INL_H_
