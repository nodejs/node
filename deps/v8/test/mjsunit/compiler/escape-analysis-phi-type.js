// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape --no-turbo-loop-peeling

function f(x) {
  var o = {a : 0};
  var l = [1,2,3,4];
  var res;
  for (var i = 0; i < 3; ++i) {
    if (x%2 == 0) { o.a = 1; b = false}
    res = l[o.a];
    o.a = x;
  }
  return res;
}

%PrepareFunctionForOptimization(f);
f(0);
f(1);
f(0);
f(1);
%OptimizeFunctionOnNextCall(f);
assertEquals(undefined, f(101));
