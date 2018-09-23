// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  for (var a = 0; a < 2; a++) {
    if (a === 1) %OptimizeOsr();
    while (0 && 1) {
      for (var j = 1; j < 2; j++) { }
    }
  }
}

foo();
