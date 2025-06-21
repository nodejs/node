// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Object.defineProperty(Number.prototype, "v", { get: constructor });
function get_v(num) {
  return num.v;
}

let n = new Number(42);
%PrepareFunctionForOptimization(get_v);
get_v(n);
get_v(n);
%OptimizeFunctionOnNextCall(get_v);
get_v(n);
