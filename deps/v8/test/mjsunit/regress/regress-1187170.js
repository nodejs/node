// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

var bar = 0;
function foo(outer_arg) {
  var arr = [1];
  var func = function (arg) {
      bar += arg;
      if (outer_arg) {}
  };
  try {
    arr.filter(func);
  } catch (e) {}
};

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
bar = {};
foo();
