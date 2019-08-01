// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function MaterializeStrictArguments() {
  "use strict"

  function f(x, y) {
    return x + y;
  }

  function test1() {
    %DeoptimizeNow();
    return f.apply(null, arguments);
  }

  %PrepareFunctionForOptimization(test1);
  assertEquals(test1(1, 2), 3);
  assertEquals(test1(1, 2, 3), 3);

  %OptimizeFunctionOnNextCall(test1);
  assertEquals(test1(1, 2), 3);
  %PrepareFunctionForOptimization(test1);
  %OptimizeFunctionOnNextCall(test1);
  assertEquals(test1(1, 2, 3), 3);
})();

(function MaterializeSloppyArguments() {
  function f(x, y) {
    return x + y;
  }

  function test2() {
    %DeoptimizeNow();
    return f.apply(null, arguments);
  }

  %PrepareFunctionForOptimization(test2);
  assertEquals(test2(1, 2), 3);
  assertEquals(test2(1, 2, 3), 3);

  %OptimizeFunctionOnNextCall(test2);
  assertEquals(test2(1, 2), 3);
  %PrepareFunctionForOptimization(test2);
  %OptimizeFunctionOnNextCall(test2);
  assertEquals(test2(1, 2, 3), 3);
})();

(function MaterializeStrictOverwrittenArguments() {
  "use strict"

  function f(x, y) {
    return x + y;
  }

  function test3(a, b) {
    a = 4;
    %DeoptimizeNow();
    return f.apply(null, arguments);
  }

  %PrepareFunctionForOptimization(test3);
  assertEquals(test3(1, 2), 3);
  assertEquals(test3(1, 2, 3), 3);

  %OptimizeFunctionOnNextCall(test3);
  assertEquals(test3(11, 12), 23);
  %PrepareFunctionForOptimization(test3);
  %OptimizeFunctionOnNextCall(test3);
  assertEquals(test3(11, 12, 13), 23);
})();

(function MaterializeSloppyOverwrittenArguments() {
  function f(x, y) {
    return x + y;
  }

  function test4(a, b) {
    a = 4;
    %DeoptimizeNow();
    return f.apply(null, arguments);
  }

  test4(1, 2);
  test4(3, 4, 5);

  %PrepareFunctionForOptimization(test4);
  assertEquals(test4(1, 2), 6);
  assertEquals(test4(1, 2, 3), 6);

  %OptimizeFunctionOnNextCall(test4);
  assertEquals(test4(1, 2), 6);
  %PrepareFunctionForOptimization(test4);
  %OptimizeFunctionOnNextCall(test4);
  assertEquals(test4(1, 2, 3), 6);
})();

(function ArgumentsAccessStrict () {
  "use strict"
  function sum1(a,b,c) {
    var sum = 0;
    var rest = arguments;
    for (var i = 0; i < rest.length; ++i) {
      var j = i;
      if (rest.length % 15 == 0 && i == 10) j += 10000;
      sum += rest[j] || i+1;
    }
    return sum;
  };

  %PrepareFunctionForOptimization(sum1);
  var args = []
  for (var i = 1; i < 30; ++i) {
    args.push(i);
    if (i%10 == 0) %OptimizeFunctionOnNextCall(sum1);
    assertEquals(i*(i+1)/2, sum1(...args));
  }
})();

(function ArgumentsAccessSloppy () {
  function sum2(a,b,c) {
    var sum = 0;
    for (var i = 0; i < arguments.length; ++i) {
      var j = i;
      if (arguments.length % 15 == 0 && i == 10) j += 10000;
      sum += arguments[j] || i+1;
    }
    return sum;
  };

  %PrepareFunctionForOptimization(sum2);
  var args = []
  for (var i = 1; i < 30; ++i) {
    args.push(i);
    if (i%10 == 0) %OptimizeFunctionOnNextCall(sum2);
    assertEquals(i*(i+1)/2, sum2(...args));
  }
})();

(function RestAccess0 () {
  function sum3(...rest) {
    var sum = 0;
    for (var i = 0; i < rest.length; ++i) {
      var j = i;
      if (rest.length % 15 == 0 && i == 10) j += 10000;
      sum += rest[j] || i+1;
    }
    return sum;
  };

  %PrepareFunctionForOptimization(sum3);
  var args = []
  for (var i = 1; i < 30; ++i) {
    args.push(i);
    if (i%10 == 0) %OptimizeFunctionOnNextCall(sum3);
    assertEquals(i*(i+1)/2, sum3(...args));
  }
})();

(function RestAccess1 () {
  function sum4(a,...rest) {
    var sum = 0;
    for (var i = 0; i < rest.length; ++i) {
      var j = i;
      if (rest.length % 15 == 0 && i == 10) j += 10000;
      sum += rest[j] || i+2;
    }
    return sum;
  };

  %PrepareFunctionForOptimization(sum4);
  var args = []
  for (var i = 1; i < 30; ++i) {
    args.push(i);
    if (i%10 == 0) %OptimizeFunctionOnNextCall(sum4);
    assertEquals(i*(i+1)/2-1, sum4(...args));
  }
})();


(function ReadArguments () {
  function read() {
    if (arguments.length % 10 == 5) %DeoptimizeNow();
    return arguments[arguments.length-1];
  };

  %PrepareFunctionForOptimization(read);
  var args = []
  for (var i = 1; i < 30; ++i) {
    args.push(i);
    if (i%10 == 0) %OptimizeFunctionOnNextCall(read);
    assertEquals(i, read(...args));
  }
})();
