// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

var x = {};

function f(a) {
  a[200000000] = x;
};
%PrepareFunctionForOptimization(f);
f(new Array(100000));
f([]);
%OptimizeFunctionOnNextCall(f);
f([]);
