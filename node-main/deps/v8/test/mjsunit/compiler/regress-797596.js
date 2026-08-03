// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --allow-natives-syntax
var notCallable;
function inferReceiverMapsInDeadCode() {
  var obj = { func() {} };
  gc();
  function wrappedCode() { try { code(); } catch (e) {} }
  %PrepareFunctionForOptimization(wrappedCode);
  function code() {
    obj.a;
    try {
      Object.defineProperty(obj, "func", { get() {} });
    } catch (neverCaught) {}
    for (var i = 0; i < 1; i++) {
      try {
        notCallable(arguments[i]);
      } catch (alwaysCaught) {}
    }
  }
  wrappedCode();
  try {
    %OptimizeFunctionOnNextCall(wrappedCode);
    wrappedCode();
  } catch (e) {}
}
inferReceiverMapsInDeadCode();
inferReceiverMapsInDeadCode();
inferReceiverMapsInDeadCode();
