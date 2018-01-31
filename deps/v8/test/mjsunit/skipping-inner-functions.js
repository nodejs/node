// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --preparser-scope-analysis --enable-slow-asserts

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

(function TestCtxtAllocatingNonSimpleParams1() {
  var result = 0;

  function lazy([other_param1, ctxt_alloc_param, other_param2]) {
    function skip_me() {
      result = ctxt_alloc_param;
    }
    return skip_me;
  }
  // Test that parameters and variables of the outer function get context
  // allocated even if we skip the inner function.
  lazy([30, 29, 28])();
  assertEquals(29, result);
})();

(function TestCtxtAllocatingNonSimpleParams2() {
  var result = 0;

  function lazy({a: other_param1, b: ctxt_alloc_param, c: other_param2}) {
    function skip_me() {
      result = ctxt_alloc_param;
    }
    return skip_me;
  }
  // Test that parameters and variables of the outer function get context
  // allocated even if we skip the inner function.
  lazy({a: 31, b: 32, c: 33})();
  assertEquals(32, result);
})();

(function TestCtxtAllocatingNonSimpleParams3() {
  var result = 0;

  function lazy(...ctxt_alloc_param) {
    function skip_me() {
      result = ctxt_alloc_param;
    }
    return skip_me;
  }
  // Test that parameters and variables of the outer function get context
  // allocated even if we skip the inner function.
  lazy(34, 35)();
  assertEquals([34, 35], result);
})();

// Skippable top level functions.
var result = 0;
function lazy_top_level(ctxt_alloc_param) {
  let ctxt_alloc_var = 24;
  function skip_me() {
    result = ctxt_alloc_param + ctxt_alloc_var;
  }
  skip_me();
}

lazy_top_level(10);
assertEquals(34, result);

// Tests for using a function name in an inner function.
var TestUsingNamedExpressionName1 = function this_is_the_name() {
  function inner() {
    this_is_the_name;
  }
  inner();
}
TestUsingNamedExpressionName1();

function TestUsingNamedExpressionName2() {
  let f = function this_is_the_name() {
    function inner() {
      this_is_the_name;
    }
    inner();
  }
  f();
}
TestUsingNamedExpressionName2();

function TestSkippedFunctionInsideLoopInitializer() {
  let saved_func;
  for (let i = 0, f = function() { return i }; i < 1; ++i) {
    saved_func = f;
  }
  assertEquals(0, saved_func());
}
TestSkippedFunctionInsideLoopInitializer();

(function TestSkippedFunctionWithParameters() {
  var result = 0;

  function lazy(ctxt_alloc_param) {
    var ctxt_alloc_var = 10;
    function skip_me(param1, param2) {
      result = ctxt_alloc_param + ctxt_alloc_var + param1 + param2;
    }
    return skip_me;
  }
  lazy(9)(8, 7);
  assertEquals(34, result);
})();

function TestSkippingDeeperLazyFunctions() {
  let result = 0;
  function inner_lazy(ctxt_alloc_param) {
    let ctxt_alloc_var = 13;
    function skip_me() {
      result = ctxt_alloc_param + ctxt_alloc_var;
    }
    return skip_me;
  }
  let f = inner_lazy(12);
  f();
  assertEquals(25, result);
}

TestSkippingDeeperLazyFunctions();

function TestEagerFunctionsBetweenLazyFunctions() {
  let result = 0;
  // We produce one data set for TestEagerFunctionsBetweenLazyFunctions and
  // another one for inner. The variable data for eager belongs to the former
  // data set.
  let ctxt_allocated1 = 3;
  (function eager() {
    let ctxt_allocated2 = 4;
    function inner() {
      result = ctxt_allocated1 + ctxt_allocated2;
    }
    return inner;
  })()();
  assertEquals(7, result);
}

TestEagerFunctionsBetweenLazyFunctions();

function TestEagerNotIifeFunctionsBetweenLazyFunctions() {
  let result = 0;
  // We produce one data set for TestEagerFunctionsBetweenLazyFunctions and
  // another one for inner. The variable data for eager belongs to the former
  // data set.
  let ctxt_allocated1 = 3;
  (function eager_not_iife() {
    let ctxt_allocated2 = 4;
    function inner() {
      result = ctxt_allocated1 + ctxt_allocated2;
    }
    return inner;
  }); // Function not called; not an iife.
  // This is just a regression test. We cannot test that the context allocation
  // was done correctly (since there's no way to call eager_not_iife), but code
  // like this used to trigger some DCHECKs.
}

TestEagerNotIifeFunctionsBetweenLazyFunctions();

// Regression test for functions inside a lazy arrow function. (Only top-level
// arrow functions are lazy, so this cannot be wrapped in a function.)
result = 0;
let f1 = (ctxt_alloc_param) => {
  let ctxt_alloc_var = 10;
  function inner() {
    result = ctxt_alloc_param + ctxt_alloc_var;
  }
  return inner;
}
f1(9)();
assertEquals(19, result);

function TestStrictEvalInParams() {
  "use strict";
  var result = 0;

  function lazy(a = function() { return 2; }, b = eval('3')) {
    function skip_me() {
      result = a() + b;
    }
    return skip_me;
  }
  lazy()();
  assertEquals(5, result);

  function not_skippable_either() {}
}

TestStrictEvalInParams();

function TestSloppyEvalInFunctionWithComplexParams() {
  var result = 0;

  function lazy1(ctxt_alloc_param = 2) {
    var ctxt_alloc_var = 3;
    function skip_me() {
      result = ctxt_alloc_param + ctxt_alloc_var;
    }
    eval('');
    return skip_me;
  }
  lazy1()();
  assertEquals(5, result);

  function lazy2(ctxt_alloc_param = 4) {
    var ctxt_alloc_var = 5;
    function skip_me() {
      eval('result = ctxt_alloc_param + ctxt_alloc_var;');
    }
    return skip_me;
  }
  lazy2()();
  assertEquals(9, result);
}

TestSloppyEvalInFunctionWithComplexParams();

function TestSkippableFunctionInForOfHeader() {
  var c;
  function inner() {
    for (let [a, b = c = function() { return a; }] of [[10]]) {
    }
  }
  inner();
  var result = c();
  assertEquals(10, result);
}

TestSkippableFunctionInForOfHeader();

function TestSkippableFunctionInForOfBody() {
  var c;
  function inner() {
    for (let [a, b] of [[10, 11]]) {
      c = function f() {
        return a + b;
      }
    }
  }
  inner();
  var result = c();
  assertEquals(21, result);
}

TestSkippableFunctionInForOfBody();


function TestSkippableFunctionInForOfHeaderAndBody() {
  var c1;
  var c2;
  function inner() {
    for (let [a, b = c1 = function() { return a; }] of [[10]]) {
      c2 = function f() {
        return a + 1;
      }
    }
  }
  inner();
  var result = c1() + c2();
  assertEquals(21, result);
}

TestSkippableFunctionInForOfHeaderAndBody();

(function TestSkippableGeneratorInSloppyBlock() {
  var result = 0;

  function lazy(ctxt_alloc_param) {
    var ctxt_alloc_var = 10;
    {
      function *skip_me() {
        result = ctxt_alloc_param + ctxt_alloc_var;
        yield 3;
      }
      return skip_me;
    }
  }
  // Test that parameters and variables of the outer function get context
  // allocated even if we skip the inner function.
  assertEquals(3, lazy(9)().next().value);
  assertEquals(19, result);
})();

(function TestRestoringDataToAsyncArrowFunctionWithNonSimpleParams_1() {
  // Regression test for
  // https://bugs.chromium.org/p/chromium/issues/detail?id=765532
  function lazy() {
    // The arrow function is not skippable, but we need to traverse its scopes
    // and restore data to them.
    async(a=0) => { const d = 0; }
    function skippable() {}
  }
  lazy();
})();

(function TestRestoringDataToAsyncArrowFunctionWithNonSimpleParams_2() {
  // Regression test for
  // https://bugs.chromium.org/p/chromium/issues/detail?id=765532
  function lazy() {
    // The arrow function is not skippable, but we need to traverse its scopes
    // and restore data to them.
    async(...a) => { const d = 0; }
    function skippable() {}
  }
  lazy();
})();

(function TestSloppyBlockFunctionShadowingCatchVariable() {
  // Regression test for
  // https://bugs.chromium.org/p/chromium/issues/detail?id=771474
  function lazy() {
    try {
    } catch (my_var) {
      if (false) {
        function my_var() { }
      }
    }
  }
  lazy();
})();


(function TestLazinessDecisionWithDefaultConstructors() {
  // Regression test for
  // https://bugs.chromium.org/p/chromium/issues/detail?id=773576

  // The problem was that Parser and PreParser treated default constructors
  // differently, and that threw off the "next / previous function is likely
  // called" logic.

  function lazy(p = (function() {}, class {}, function() {}, class { method1() { } })) { }
  lazy();
})();
