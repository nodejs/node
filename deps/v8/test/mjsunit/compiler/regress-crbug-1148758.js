// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --interrupt-budget=100

var caught = 0;

function runManyTimes(f) {
  for (let i = 0; i < 1000; ++i) {
    try {
      // Seems to be important that this is inside a try catch.
      f();
    } catch (e) {
      assertUnreachable();
    }
  }
}

let A = {
  get foo() {
    return 0;
  }
};

let B = {
  __proto__: A,

  aa() {
    "use strict";
    super.foo;
  }
};

var superAccessingFunc = B.aa;

runManyTimes(function () {
  try {
    superAccessingFunc();
  } catch (e) {
    caught++;
  }
});

assertEquals(0, caught);
