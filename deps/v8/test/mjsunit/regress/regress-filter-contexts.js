// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  return f.x;
};
%PrepareFunctionForOptimization(f);
f.__proto__ = null;
f.prototype = "";

f();
f();
%OptimizeFunctionOnNextCall(f);
f();
