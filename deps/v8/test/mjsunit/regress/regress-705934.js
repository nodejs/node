// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

function call_replace_close_to_stack_overflow() {
  try {
    call_replace_close_to_stack_overflow();
  } catch {
    "b".replace(/(b)/g);
  }
}

call_replace_close_to_stack_overflow();
