// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestGeneratorOSRSimple() {
  function* gen1() {
    for (var i = 0; i < 3; ++i) {
      if (i == 1) %OptimizeOsr();
    }
    return 23;
  }
  %PrepareFunctionForOptimization(gen1);
  var g = gen1();
  assertEquals({ value:23, done:true }, g.next());
})();

(function TestGeneratorOSRYieldAfterArming() {
  function* gen2() {
    for (var i = 0; i < 3; ++i) {
      if (i == 1) %OptimizeOsr();
      yield i;
    }
    return 23;
  }
  %PrepareFunctionForOptimization(gen2);
  var g = gen2();
  assertEquals({ value:0, done:false }, g.next());
  assertEquals({ value:1, done:false }, g.next());
  assertEquals({ value:2, done:false }, g.next());
  assertEquals({ value:23, done:true }, g.next());
})();

(function TestGeneratorOSRYieldBeforeArming() {
  function* gen3() {
    for (var i = 0; i < 3; ++i) {
      yield i;
      if (i == 1) %OptimizeOsr();
    }
    return 23;
  }
  %PrepareFunctionForOptimization(gen3);
  var g = gen3();
  assertEquals({ value:0, done:false }, g.next());
  assertEquals({ value:1, done:false }, g.next());
  assertEquals({ value:2, done:false }, g.next());
  assertEquals({ value:23, done:true }, g.next());
})();

(function TestGeneratorOSRNested() {
  function* gen4() {
    for (var i = 0; i < 3; ++i) {
      for (var j = 0; j < 3; ++j) {
        for (var k = 0; k < 10; ++k) {
          if (k == 5) %OptimizeOsr();
        }
        %PrepareFunctionForOptimization(gen4);
      }
      yield i;
    }
    return 23;
  }
  %PrepareFunctionForOptimization(gen4);
  var g = gen4();
  assertEquals({ value:0, done:false }, g.next());
  assertEquals({ value:1, done:false }, g.next());
  assertEquals({ value:2, done:false }, g.next());
  assertEquals({ value:23, done:true }, g.next());
})();
