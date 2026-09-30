// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-maglev --allow-natives-syntax

function Point(x, y) {
  this.x = x;
  this.y = y;
}

function leaf(obj) {
  return obj.foo.bar;
}

function test() {
  let elided = new Point(42, 43);
  let res = leaf({ foo: { bar: 1 } });
  return elided.x + res;
}

%PrepareFunctionForOptimization(Point);
%PrepareFunctionForOptimization(leaf);
%PrepareFunctionForOptimization(test);
for (let i = 0; i < 2000; i++) {
  test();
}
%OptimizeMaglevOnNextCall(test);
test();
