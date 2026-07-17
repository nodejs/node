// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function wrap(f) {
    return f();
}

let glob = [1.5, undefined, 0.0, 0.0];

function foo(x, y) {
  let a = wrap(() => y[x]);
  wrap(() => y ? a : 4);
}

%PrepareFunctionForOptimization(foo);
foo(0, glob);
foo(2, glob);
foo(2, glob);
foo(2, glob);
foo(3, glob);
foo(0, glob);
foo(1, glob);
foo(8, glob);
foo(3, glob);
foo(3, glob);

%OptimizeFunctionOnNextCall(foo);
foo(3, glob);
