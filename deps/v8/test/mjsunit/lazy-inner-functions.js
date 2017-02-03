// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --min-preparse-length 1

(function TestLazyInnerFunctionCallsEval() {
  var i = (function eager_outer() {
    var a = 41; // Should be context-allocated
    function lazy_inner() {
      return eval("a");
    }
    return lazy_inner;
  })();
  assertEquals(41, i());
})();
