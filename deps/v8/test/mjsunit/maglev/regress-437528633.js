// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax  --maglev-non-eager-inlining

var a = new Array();
function foo(x) {
  try {
    return x < 0 ? + error : x;
  } catch (e) {}
}

function bar(i) {
  return foo(foo(a[i] ^ a[i - 1] >>> 1) << 0);
}

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(bar);
bar(1);
bar(2);
%OptimizeMaglevOnNextCall(bar);
bar(3);
