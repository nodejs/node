// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --turbo --opt

// Make sure literals are strongly rooted and safe from weak-code deopts.

(function() {
  function foo() {
    var a = {y: 0};
    a.y = 1;
    return a;
  }

  foo();
  foo();
  % OptimizeFunctionOnNextCall(foo);
  foo();
  gc();
  assertOptimized(foo);
})();

(function() {
  function hot(o) {
    return o.x + o.y;
  }
  function mapPlus(a, y) {
    return a.map(x => hot({x, y}));
  }

  var a = [1, 2, 3];
  print(mapPlus(a, 1));
  print(mapPlus(a, 2));
  % OptimizeFunctionOnNextCall(hot);
  print(mapPlus(a, 3));
  gc();  // BOOOM!
  assertOptimized(hot);
  print(mapPlus(a, 4));
})();

// Verify that we can handle the creation of a new script, where the
// code is cached and the feedback vector has to be re-created.
(function() {
  var sopen = 'function wrapper() {  ';
  var s1 = 'function foo() { return bar(5); } ';
  var s2 = 'foo(); foo(); %OptimizeFunctionOnNextCall(foo); foo(); ';
  var sclose = '}  wrapper(); ';
  var s = sopen + s1 + s2 + sclose;
  function bar(i){return i + 3};

  for (var i = 0; i < 4; i++) {
    eval(s);
  }
})();
