// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

function* g(h) {
  return yield* h;
}

var f = Object.getPrototypeOf(function*(){}).prototype;
var t = f.throw;
const h = (function*(){})();
h.next = function () { return { }; };
const x = g(h);
x.next();
delete f.throw;

try {
  t.bind(x)();
} catch (e) {}

%PrepareFunctionForOptimization(g);
g();
%OptimizeFunctionOnNextCall(g);
g();
