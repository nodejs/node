// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestReflectConstructBogusNewTarget1() {
  class C {}
  function g() {
    Reflect.construct(C, arguments, 23);
  }
  function f() {
    return new g();
  }
  new C();  // Warm-up!
  assertThrows(f, TypeError);
  assertThrows(f, TypeError);
  %OptimizeFunctionOnNextCall(f);
  assertThrows(f, TypeError);
})();

(function TestReflectConstructBogusNewTarget2() {
  class C {}
  // Note that {unescape} is an example of a non-constructable function. If that
  // ever changes and this test needs to be adapted, make sure to choose another
  // non-constructable {JSFunction} object instead.
  function g() {
    Reflect.construct(C, arguments, unescape);
  }
  function f() {
    return new g();
  }
  new C();  // Warm-up!
  assertThrows(f, TypeError);
  assertThrows(f, TypeError);
  %OptimizeFunctionOnNextCall(f);
  assertThrows(f, TypeError);
})();

(function TestReflectConstructBogusTarget() {
  function g() {
    Reflect.construct(23, arguments);
  }
  function f() {
    return new g();
  }
  assertThrows(f, TypeError);
  assertThrows(f, TypeError);
  %OptimizeFunctionOnNextCall(f);
  assertThrows(f, TypeError);
})();

(function TestReflectApplyBogusTarget() {
  function g() {
    Reflect.apply(23, this, arguments);
  }
  function f() {
    return g();
  }
  assertThrows(f, TypeError);
  assertThrows(f, TypeError);
  %OptimizeFunctionOnNextCall(f);
  assertThrows(f, TypeError);
})();
