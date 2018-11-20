// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ISOLATE_INL_H_
#define V8_ISOLATE_INL_H_

#include "src/base/utils/random-number-generator.h"
#include "src/debug.h"
#include "src/isolate.h"

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


base::RandomNumberGenerator* Isolate::random_number_generator() {
  if (random_number_generator_ == NULL) {
    if (FLAG_random_seed != 0) {
      random_number_generator_ =
          new base::RandomNumberGenerator(FLAG_random_seed);
    } else {
      random_number_generator_ = new base::RandomNumberGenerator();
    }
  }
  return random_number_generator_;
}

} }  // namespace v8::internal

#endif  // V8_ISOLATE_INL_H_
