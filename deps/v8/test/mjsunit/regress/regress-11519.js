// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --gc-interval=500 --stress-compaction

function bar(a) {
  return Object.defineProperty(a, 'x', {get() { return 1; }});
}

function foo() {
  return {};
}

%NeverOptimizeFunction(bar);
%PrepareFunctionForOptimization(foo);
const o = foo();  // Keep a reference so the GC doesn't kill the map.
%SimulateNewspaceFull();
bar(o);
const a = bar(foo());
%SimulateNewspaceFull();
%OptimizeFunctionOnNextCall(foo);
const b = bar(foo());

assertTrue(%HaveSameMap(a, b));
