// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100 --no-turbofan

function f(
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
  x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
) { }

function runNearStackLimit(f) {
  let recursing_towards_stack_limit = true;
  let f_succeeded = false;

  function t() {
    try {
      t();
      if (f_succeeded) return;
      // Keep calling f until it stops throwing stack overflow exceptions.
      f();
      // f didn't throw, so we are done.
      f_succeeded = true;
    } catch(e) {
      if (recursing_towards_stack_limit) {
        recursing_towards_stack_limit = false;
        // We reached the near stack limit state, call f first time.
        f();
        // f didn't throw, so we are done.
        f_succeeded = true;
      }
    }
  };

  try {
    t();
  } catch(e) {}
}

runNearStackLimit(f);
