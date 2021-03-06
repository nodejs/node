// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function inlined(abort, n, a, b) {
  if (abort) return;
  var x = a ? true : "" + a;
  if (!a) {
    var y = n + y + 10;
    if(!b) {
      x = y;
    }
    if (x) {
      x = false;
    }
  }
  return x + 1;
}
inlined();
function optimized(abort, a, b) {
  return inlined(abort, "abc", a, b);
}
%PrepareFunctionForOptimization(optimized);
optimized(true);
%OptimizeFunctionOnNextCall(optimized);
optimized();
