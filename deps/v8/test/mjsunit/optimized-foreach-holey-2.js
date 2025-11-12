// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --turbo-inline-array-builtins

(function() {
  var result = 0;
  var proto_set_func = function(p, s) {
    %NeverOptimizeFunction(proto_set_func);
    if (s) {
      p[0] = 1;
    }
  }
  var f = function(s) {
    var b = [,,];
    proto_set_func(b.__proto__, s);
    b[1] = 0;
    b[2] = 2;
    var sum = function(v,i,o) {
      result += v;
    };
    b.forEach(sum);
  };
  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
  f(true);
  f();
  assertEquals(12, result);
})();
