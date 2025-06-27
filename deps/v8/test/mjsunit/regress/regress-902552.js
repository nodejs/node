// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  var C = class {};
  for (var i = 0; i < 4; ++i) {
    if (i == 2) %OptimizeOsr();
    C.prototype.foo = 42;
  }
}
%PrepareFunctionForOptimization(f);
f();
