// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

function js_func(a, b, c, d, e, f, g, h, i, j, k, l) {
  Object.defineProperty(arguments, "length", { get: function() { throw "error" }});
  return arguments;
}

%PrepareFunctionForOptimization(js_func);
js_func(1, 2);
%OptimizeFunctionOnNextCall(js_func);
const args = js_func(1, 2);
