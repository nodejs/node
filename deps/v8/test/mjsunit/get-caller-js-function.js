// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-filter=* --nostress-opt

// Test that for fully optimized but non inlined code, GetCallerJSFunction walks
// up a single stack frame to get the calling function. Full optimization elides
// the check in the runtime version of the intrinsic that would throw since the
// caller isn't a stub. It's a bit of a hack, but allows minimal testing of the
// intrinsic without writing a full-blown cctest.
(function() {
  var a = function() {
    return %_GetCallerJSFunction();
  };
  var b = function() {
    return a();
  };
  %OptimizeFunctionOnNextCall(a);
  assertEquals(b, b());
}());
