// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --optimize-to-maglev-on-next-call
// Flags: --cache-property-key-string-adds

function runtest(max) {
  var that = new Object();
  that["function" + (max - 1)]();
}
function f(max) {
  try {
    runtest(30);
  } catch (e) {}
}
%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
