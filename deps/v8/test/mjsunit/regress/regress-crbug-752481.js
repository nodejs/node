// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const A = class A {}

function test(foo) {
  assertThrows(foo);
  assertThrows(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo);
}

// Test combinations of rest parameters and primitive new.targets
test((...args) => Reflect.construct(A, args, 0));
test((...args) => Reflect.construct(A, args, true));
test((...args) => Reflect.construct(A, args, false));
test((...args) => Reflect.construct(A, args, ""));
test((...args) => Reflect.construct(A, args, null));
test((...args) => Reflect.construct(A, args, undefined));
test((...args) => Reflect.construct(A, args, Symbol.species));

// Test combinations of arguments object and primitive new.targets.
test(function() { Reflect.construct(A, arguments, 0); });
test(function() { Reflect.construct(A, arguments, true); });
test(function() { Reflect.construct(A, arguments, false); });
test(function() { Reflect.construct(A, arguments, ""); });
test(function() { Reflect.construct(A, arguments, null); });
test(function() { Reflect.construct(A, arguments, undefined); });
test(function() { Reflect.construct(A, arguments, Symbol.species); });
