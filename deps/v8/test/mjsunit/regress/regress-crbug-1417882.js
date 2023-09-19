// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(className) {
  var obj = {x: 12, y: 13};
  delete obj.x;
  obj[Symbol.toStringTag] = className;
  return obj.toString();
}

%PrepareFunctionForOptimization(foo);
assertEquals('[object A]', foo('A'));
assertEquals('[object B]', foo('B'));
%OptimizeFunctionOnNextCall(foo);
assertEquals('[object C]', foo('C'));
