// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(o) {
  return o.hello, Object.getPrototypeOf(o);
};
%PrepareFunctionForOptimization(bar);
var y = { __proto__: {}, hello: 44 };
var z = { hello: 45 };

bar(y);
bar(z);
bar(y);
%OptimizeFunctionOnNextCall(bar);
bar(y);
