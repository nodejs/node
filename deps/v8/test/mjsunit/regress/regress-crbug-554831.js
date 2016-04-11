// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  var key = "s";
  function f(object) { return object[key]; };
  f("");
  f("");
  %OptimizeFunctionOnNextCall(f);
  f("");
  assertOptimized(f);
})();
