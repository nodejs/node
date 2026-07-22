// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var o = {a:1.5, b:{}};

function f(o) {
  var result = [];
  for (var k in o) {
    result[result.length] = o[k];
  }
  return result;
}

%PrepareFunctionForOptimization(f);
f(o);
f(o);
%OptimizeFunctionOnNextCall(f);
var array = f(o);
o.a = 1.7;
assertEquals(1.5, array[0]);
