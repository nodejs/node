// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-turbo-loop-peeling --turbo-escape

function foo(){
  var o = {a : 5}
  for (var i = 0; i < 100; ++i) {
    o.a = 5;
    o.a = 7;
  }
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo)
foo();
