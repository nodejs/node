// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

var glob_str = "a" + "b".repeat(50); // non-internalized SeqOneByteString constant

function bar(x, y, cond) {
  if (cond) {
    glob_str = x;
    y === "world";
    glob_str = x;
  }
}

function foo(x, run_store) {
  let x_orig = x;
  return bar(x_orig, x_orig, run_store);
}

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(bar);

for (let i = 0; i < 40; i++) {
  foo();
}
bar(glob_str, "world", true);

%OptimizeMaglevOnNextCall(foo);
foo();
