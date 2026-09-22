// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function bar() {}

function foo(x) {
  let phi = 42;
  let c = x + 1;
  let b = false;
  while (true) {
    if (b) { phi++ }
    if (c == 2) break;
    phi = c;
  }
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo(1);

%OptimizeMaglevOnNextCall(foo);
foo(1);
