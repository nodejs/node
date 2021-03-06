// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


// Without instance creation:

(function() {
  function* Goo() {};
  const goo = {};

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertFalse(IsGoo(goo));
  assertFalse(IsGoo(goo));
  %OptimizeFunctionOnNextCall(IsGoo);
  assertFalse(IsGoo(goo));
})();

(function() {
  function* Goo() {};
  const goo = {};
  Goo.prototype = Object.prototype;

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertTrue(IsGoo(goo));
  assertTrue(IsGoo(goo));
  %OptimizeFunctionOnNextCall(IsGoo);
  assertTrue(IsGoo(goo));
})();

(function() {
  function* Goo() {};
  const goo = {};
  Goo.prototype = 42

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertThrows(_ => IsGoo(goo), TypeError);
  assertThrows(_ => IsGoo(goo), TypeError);
  %OptimizeFunctionOnNextCall(IsGoo);
  assertThrows(_ => IsGoo(goo), TypeError);
})();

(function() {
  function* Goo() {};
  const goo = {};

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertFalse(IsGoo(goo));
  assertFalse(IsGoo(goo));
  %OptimizeFunctionOnNextCall(IsGoo);
  assertFalse(IsGoo(goo));
  Goo.prototype = Object.prototype;
  assertTrue(IsGoo(goo));
})();

(function() {
  function* Goo() {};
  const goo = {};

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertFalse(IsGoo(goo));
  assertFalse(IsGoo(goo));
  %OptimizeFunctionOnNextCall(IsGoo);
  assertFalse(IsGoo(goo));
  Goo.prototype = 42;
  assertThrows(_ => IsGoo(goo), TypeError);
})();


// With instance creation:

(function() {
  function* Goo() {};
  const goo = Goo();

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertTrue(IsGoo(goo));
  assertTrue(IsGoo(goo));
  %OptimizeFunctionOnNextCall(IsGoo);
  assertTrue(IsGoo(goo));
})();

(function() {
  function* Goo() {};
  const goo = Goo();
  Goo.prototype = {};

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertFalse(IsGoo(goo));
  assertFalse(IsGoo(goo));
  %OptimizeFunctionOnNextCall(IsGoo);
  assertFalse(IsGoo(goo));
})();

(function() {
  function* Goo() {};
  const goo = Goo();
  Goo.prototype = 42;

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertThrows(_ => IsGoo(goo), TypeError);
  assertThrows(_ => IsGoo(goo), TypeError);
  %OptimizeFunctionOnNextCall(IsGoo);
  assertThrows(_ => IsGoo(goo), TypeError);
})();

(function() {
  function* Goo() {};
  const goo = Goo();

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertTrue(IsGoo(goo));
  assertTrue(IsGoo(goo));
  %OptimizeFunctionOnNextCall(IsGoo);
  assertTrue(IsGoo(goo));
  Goo.prototype = {};
  assertFalse(IsGoo(goo));
})();

(function() {
  function* Goo() {};
  const goo = Goo();

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertTrue(IsGoo(goo));
  assertTrue(IsGoo(goo));
  %OptimizeFunctionOnNextCall(IsGoo);
  assertTrue(IsGoo(goo));
  Goo.prototype = 42
  assertThrows(_ => IsGoo(goo), TypeError);
})();

(function() {
  function* Goo() {};
  Goo.prototype = 42;
  const goo = Goo();

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertThrows(_ => IsGoo(goo), TypeError);
  assertThrows(_ => IsGoo(goo), TypeError);
  %OptimizeFunctionOnNextCall(IsGoo);
  assertThrows(_ => IsGoo(goo), TypeError);
  Goo.prototype = {};
  assertFalse(IsGoo(goo));
})();

(function() {
  function* Goo() {};
  Goo.prototype = 42;
  const goo = Goo();
  Goo.prototype = {};

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertFalse(IsGoo(goo));
  assertFalse(IsGoo(goo));
  %OptimizeFunctionOnNextCall(IsGoo);
  assertFalse(IsGoo(goo));
  Goo.prototype = Object.prototype;
  assertTrue(IsGoo(goo));
})();

(function() {
  function* Goo() {};
  Goo.prototype = {};
  const goo = Goo();
  Goo.prototype = 42;

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertThrows(_ => IsGoo(goo), TypeError);
  assertThrows(_ => IsGoo(goo), TypeError);
  %OptimizeFunctionOnNextCall(IsGoo);
  assertThrows(_ => IsGoo(goo), TypeError);
  Goo.prototype = Object.prototype;
  assertTrue(IsGoo(goo));
})();

(function() {
  function* Goo() {};
  Goo.prototype = {};
  const goo = Goo();
  Goo.prototype = {};

  function IsGoo(x) {
    return x instanceof Goo;
  }

  %PrepareFunctionForOptimization(IsGoo);
  assertFalse(IsGoo(goo));
  assertFalse(IsGoo(goo));
  %OptimizeFunctionOnNextCall(IsGoo);
  Goo.prototype = Object.prototype;
  assertTrue(IsGoo(goo));
})();
