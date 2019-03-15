// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape --no-turbo-load-elimination

function f(i) {
  var o1 = {a: 1, b: 2};
  var o2 = {a: 1, b: 3};
  var o3 = {a: o2.b};
  o3.a = "";
  o2.a = o1;
  if (i == 4) return o3;
}
%PrepareFunctionForOptimization(f);
for (var i = 0; i < 100; ++i){
  f(i%5)
}
%OptimizeFunctionOnNextCall(f);
f(false);
