// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function foo(a, b) {
  // Write to a.x, which will also cache the value in a.x
  a.x = 7;
  // This write should be cached here.
  const x = a.x;
  // a might alias b, so we should clear the cached value here...
  b.x = 4;
  // ... and return the old and uncached new value here (which is 4 if a aliases
  // b, 7 otherwise).
  const y = a.x;
  return [x, y];
}

const a = {x: 1};
const b = {x: 2};

%PrepareFunctionForOptimization(foo);
assertEquals([7,4], foo(a, a));
assertEquals([7,7], foo(a, b));
%OptimizeMaglevOnNextCall(foo);
assertEquals([7,4], foo(a, a));
assertEquals([7,7], foo(a, b));
