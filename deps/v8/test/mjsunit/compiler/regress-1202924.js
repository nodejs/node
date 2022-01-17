// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation --interrupt-budget=1000
// Flags: --no-analyze-environment-liveness

function foo() {
  for (var i = 1; i < 10; i++) {
    var n = 1;
    for (var j = 1; j < 10; j++) {
      if (n == j) j = 0;
      foo = j % - n;
      n++;
    }
  }
}
foo();
