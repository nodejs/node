// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(obj) {
  var key;
  for (key in obj) { }
  return key === undefined;
}

%PrepareFunctionForOptimization(f);
%OptimizeFunctionOnNextCall(f);
assertFalse(f({ foo:0 }));
