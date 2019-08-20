// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

var a = new (function() { this[0] = 1 });
function f() {
  for (var i = 0; i < 4; ++i) {
    var x = a[0];
    (function() { return x });
    if (i == 1) %OptimizeOsr();
    gc();
  }
}
%PrepareFunctionForOptimization(f);
f();
