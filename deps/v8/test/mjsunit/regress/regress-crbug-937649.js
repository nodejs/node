// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --allow-natives-syntax

(function() {
function foo(x) {
  const i = x > 0;
  const dv = new DataView(ab);
  return dv.getUint16(i);
};
%PrepareFunctionForOptimization(foo);
const ab = new ArrayBuffer(2);
foo(0);
foo(0);
%OptimizeFunctionOnNextCall(foo);
foo(0);
})();
