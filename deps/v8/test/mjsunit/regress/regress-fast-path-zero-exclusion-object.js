// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function f(obj) {
  let { ...rest } = obj;
  return rest;
}

assertEquals({}, f({}));
assertEquals({a: 1}, f({a: 1}));

// Test that null and undefined throw TypeError.
assertThrows(() => f(null), TypeError);
assertThrows(() => f(undefined), TypeError);

// Test with prototype properties.
let proto = { b: 2 };
let obj = Object.create(proto);
obj.a = 1;
assertEquals({a: 1}, f(obj));

// Ensure it compiles and optimizes.
%PrepareFunctionForOptimization(f);
f({});
f({});
%OptimizeFunctionOnNextCall(f);
assertEquals({a: 1}, f({a: 1}));
