// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

"use strict";

function foo() {
  let count = 0;
  try {
    for (p of v) {
      count += 1;
    }
  } catch (e) { }
  assertEquals(count, 0);
}

var v = [ "0", {}];

foo();
Reflect.deleteProperty(v, '0');

let count_loop = 0;
try {
 for (p of v) { count_loop += 1; }
} catch (e) {}
assertEquals(count_loop, 0);

foo();
