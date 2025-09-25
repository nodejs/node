// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --no-lazy-feedback-allocation --minimum-invocations-before-optimization=1
// Flags: --invocation-count-for-turbofan=1

let x = 0;

function foo(a) {
  eval();
  x = a;
}

assertEquals(undefined, foo());
assertEquals(undefined, foo());
