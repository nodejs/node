// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(foo) {
  var g;
  true ? (g = foo + 0) : g = null;
  if (null != g) {}
};

%PrepareFunctionForOptimization(f);
f(1.4);
f(1.4);
%OptimizeFunctionOnNextCall(f);
f(1.4);
