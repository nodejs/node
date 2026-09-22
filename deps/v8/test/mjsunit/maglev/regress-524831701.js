// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var glob_str = ("a" + "b").repeat(50); // non-internalized SeqOneByteString constant

function bar(x, cond) {
  if (cond) {
    glob_str = x;
  }
}

function foo(x, run_store) {
  let x_copy = x;
  let is_eq = (x === "hello");
  return bar(x_copy, run_store);
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);

// 1. Train foo with internalized strings (do not write to glob)
for (let i = 0; i < 40; i++) {
  foo("hello", false);
  foo("world", false);
}

// 2. Train bar with glob directly (do not run foo with glob)
for (let i = 0; i < 40; i++) {
  bar(glob_str, true);
}

// Optimize foo with Maglev
%OptimizeMaglevOnNextCall(foo);
foo("hello", false);
