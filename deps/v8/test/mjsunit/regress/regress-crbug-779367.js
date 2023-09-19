// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function g(o) {
  return o.x;
};
%PrepareFunctionForOptimization(g);
Object.defineProperty(g, 'x', {set(v) {}});

g.prototype = 1;
g(g);
g(g);
%OptimizeFunctionOnNextCall(g);
g(g);
