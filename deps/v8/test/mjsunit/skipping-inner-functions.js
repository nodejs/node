// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-preparser-scope-analysis

(function TestBasicSkipping() {
  var result = 0;

  function lazy(ctxt_alloc_param) {
    var ctxt_alloc_var = 10;
    function skip_me() {
      result = ctxt_alloc_param + ctxt_alloc_var;
    }
    return skip_me;
  }
  // Test that parameters and variables of the outer function get context
  // allocated even if we skip the inner function.
  lazy(9)();
  assertEquals(19, result);
})();

(function TestSkippingFunctionWithEval() {
  var result = 0;

  function lazy(ctxt_alloc_param) {
    var ctxt_alloc_var = 10;
    function skip_me() {
      eval('result = ctxt_alloc_param + ctxt_alloc_var');
    }
    return skip_me;
  }
  // Test that parameters and variables of the outer function get context
  // allocated even if we skip the inner function.
  lazy(9)();
  assertEquals(19, result);
})();
