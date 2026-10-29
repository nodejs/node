// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-truncation

function bar(a, b) {
  try {
    var x = a >>> 16 & 0xffff;
    var y = a & 0xffff;
  } catch (e) {}
    var z = b & 0xffff;
    x * z + y * z << 16 >>> 0 | 0;
}

function foo(a, b, c) {
  bar(b, c);
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo();
foo(4, -2, -2);
%OptimizeFunctionOnNextCall(foo);
foo();
