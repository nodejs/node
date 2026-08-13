// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --min-maglev-inlining-frequency=0.0

let glob_str = ("a" + "b").repeat(50);

function bar(x, cond) {
  if (cond) {
    return x === "hello";
  }
}

function foo(x, run_store, run_cmp) {
  if (run_store) {
    glob_str = x;
    return bar(x, run_cmp);
  }
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);

// Train bar with internalized strings
for (let i = 0; i < 20; i++) {
  bar("hello", true);
  bar("world", true);
}

// Train foo
for (let i = 0; i < 20; i++) {
  foo(glob_str, true, false);
}

// Optimize foo with Maglev
%OptimizeMaglevOnNextCall(foo);

// Trigger the bug
foo(glob_str, true, true);
