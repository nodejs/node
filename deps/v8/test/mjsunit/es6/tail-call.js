// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-tailcalls
// Flags: --harmony-do-expressions

"use strict";

Error.prepareStackTrace = (error,stack) => {
  error.strace = stack;
  return error.message + "\n    at " + stack.join("\n    at ");
}


function CheckStackTrace(expected) {
  var e = new Error();
  e.stack;  // prepare stack trace
  var stack = e.strace;
  assertEquals("CheckStackTrace", stack[0].getFunctionName());
  for (var i = 0; i < expected.length; i++) {
    assertEquals(expected[i].name, stack[i + 1].getFunctionName());
  }
}
%NeverOptimizeFunction(CheckStackTrace);


function f(expected_call_stack, a, b) {
  CheckStackTrace(expected_call_stack);
  return a;
}

function f_153(expected_call_stack, a) {
  CheckStackTrace(expected_call_stack);
  return 153;
}


// Tail call when caller does not have an arguments adaptor frame.
(function() {
  // Caller and callee have same number of arguments.
  function f1(a) {
    CheckStackTrace([f1, test]);
    return 10 + a;
  }
  function g1(a) { return f1(2); }

  // Caller has more arguments than callee.
  function f2(a) {
    CheckStackTrace([f2, test]);
    return 10 + a;
  }
  function g2(a, b, c) { return f2(2); }

  // Caller has less arguments than callee.
  function f3(a, b, c) {
    CheckStackTrace([f3, test]);
    return 10 + a + b + c;
  }
  function g3(a) { return f3(2, 3, 4); }

  // Callee has arguments adaptor frame.
  function f4(a, b, c) {
    CheckStackTrace([f4, test]);
    return 10 + a;
  }
  function g4(a) { return f4(2); }

  function test() {
    assertEquals(12, g1(1));
    assertEquals(12, g2(1, 2, 3));
    assertEquals(19, g3(1));
    assertEquals(12, g4(1));
  }
  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();


// Tail call when caller has an arguments adaptor frame.
(function() {
  // Caller and callee have same number of arguments.
  function f1(a) {
    CheckStackTrace([f1, test]);
    return 10 + a;
  }
  function g1(a) { return f1(2); }

  // Caller has more arguments than callee.
  function f2(a) {
    CheckStackTrace([f2, test]);
    return 10 + a;
  }
  function g2(a, b, c) { return f2(2); }

  // Caller has less arguments than callee.
  function f3(a, b, c) {
    CheckStackTrace([f3, test]);
    return 10 + a + b + c;
  }
  function g3(a) { return f3(2, 3, 4); }

  // Callee has arguments adaptor frame.
  function f4(a, b, c) {
    CheckStackTrace([f4, test]);
    return 10 + a;
  }
  function g4(a) { return f4(2); }

  function test() {
    assertEquals(12, g1());
    assertEquals(12, g2());
    assertEquals(19, g3());
    assertEquals(12, g4());
  }
  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();


// Tail call bound function when caller does not have an arguments
// adaptor frame.
(function() {
  // Caller and callee have same number of arguments.
  function f1(a) {
    assertEquals(153, this.a);
    CheckStackTrace([f1, test]);
    return 10 + a;
  }
  var b1 = f1.bind({a: 153});
  function g1(a) { return b1(2); }

  // Caller has more arguments than callee.
  function f2(a) {
    assertEquals(153, this.a);
    CheckStackTrace([f2, test]);
    return 10 + a;
  }
  var b2 = f2.bind({a: 153});
  function g2(a, b, c) { return b2(2); }

  // Caller has less arguments than callee.
  function f3(a, b, c) {
    assertEquals(153, this.a);
    CheckStackTrace([f3, test]);
    return 10 + a + b + c;
  }
  var b3 = f3.bind({a: 153});
  function g3(a) { return b3(2, 3, 4); }

  // Callee has arguments adaptor frame.
  function f4(a, b, c) {
    assertEquals(153, this.a);
    CheckStackTrace([f4, test]);
    return 10 + a;
  }
  var b4 = f4.bind({a: 153});
  function g4(a) { return b4(2); }

  function test() {
    assertEquals(12, g1(1));
    assertEquals(12, g2(1, 2, 3));
    assertEquals(19, g3(1));
    assertEquals(12, g4(1));
  }
  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();


// Tail call bound function when caller has an arguments adaptor frame.
(function() {
  // Caller and callee have same number of arguments.
  function f1(a) {
    assertEquals(153, this.a);
    CheckStackTrace([f1, test]);
    return 10 + a;
  }
  var b1 = f1.bind({a: 153});
  function g1(a) { return b1(2); }

  // Caller has more arguments than callee.
  function f2(a) {
    assertEquals(153, this.a);
    CheckStackTrace([f2, test]);
    return 10 + a;
  }
  var b2 = f2.bind({a: 153});
  function g2(a, b, c) { return b2(2); }

  // Caller has less arguments than callee.
  function f3(a, b, c) {
    assertEquals(153, this.a);
    CheckStackTrace([f3, test]);
    return 10 + a + b + c;
  }
  var b3 = f3.bind({a: 153});
  function g3(a) { return b3(2, 3, 4); }

  // Callee has arguments adaptor frame.
  function f4(a, b, c) {
    assertEquals(153, this.a);
    CheckStackTrace([f4, test]);
    return 10 + a;
  }
  var b4 = f4.bind({a: 153});
  function g4(a) { return b4(2); }

  function test() {
    assertEquals(12, g1());
    assertEquals(12, g2());
    assertEquals(19, g3());
    assertEquals(12, g4());
  }
  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();


// Tail calling from getter.
(function() {
  function g(v) {
    CheckStackTrace([g, test]);
    %DeoptimizeFunction(test);
    return 153;
  }
  %NeverOptimizeFunction(g);

  function f(v) {
    return g();
  }
  %SetForceInlineFlag(f);

  function test() {
    var o = {};
    o.__defineGetter__('p', f);
    assertEquals(153, o.p);
  }

  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();


// Tail calling from setter.
(function() {
  function g() {
    CheckStackTrace([g, test]);
    %DeoptimizeFunction(test);
    return 153;
  }
  %NeverOptimizeFunction(g);

  function f(v) {
    return g();
  }
  %SetForceInlineFlag(f);

  function test() {
    var o = {};
    o.__defineSetter__('q', f);
    assertEquals(1, o.q = 1);
  }

  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();


// Tail calling from constructor.
(function() {
  function g(context) {
    CheckStackTrace([g, test]);
    %DeoptimizeFunction(test);
    return {x: 153};
  }
  %NeverOptimizeFunction(g);

  function A() {
    this.x = 42;
    return g();
  }

  function test() {
    var o = new A();
    //%DebugPrint(o);
    assertEquals(153, o.x);
  }

  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();


// Tail calling via various expressions.
(function() {
  function g1(a) {
    return f([f, g1, test], false) || f([f, test], true);
  }

  function g2(a) {
    return f([f, g2, test], true) && f([f, test], true);
  }

  function g3(a) {
    return f([f, g3, test], 13), f([f, test], 153);
  }

  function g4(a) {
    return f([f, g4, test], false) ||
        (f([f, g4, test], true) && f([f, test], true));
  }

  function g5(a) {
    return f([f, g5, test], true) &&
        (f([f, g5, test], false) || f([f, test], true));
  }

  function g6(a) {
    return f([f, g6, test], 13), f([f, g6, test], 42),
        f([f, test], 153);
  }

  function g7(a) {
    return f([f, g7, test], false) ||
        (f([f, g7, test], false) ? f([f, test], true)
             : f([f, test], true));
  }

  function g8(a) {
    return f([f, g8, test], false) || f([f, g8, test], true) &&
        f([f, test], true);
  }

  function g9(a) {
    return f([f, g9, test], true) && f([f, g9, test], false) ||
        f([f, test], true);
  }

  function g10(a) {
    return f([f, g10, test], true) && f([f, g10, test], false) ||
        f([f, g10, test], true) ?
            f([f, g10, test], true) && f([f, g10, test], false) ||
                f([f, test], true) :
            f([f, g10, test], true) && f([f, g10, test], false) ||
                f([f, test], true);
  }

  function test() {
    assertEquals(true, g1());
    assertEquals(true, g2());
    assertEquals(153, g3());
    assertEquals(true, g4());
    assertEquals(true, g5());
    assertEquals(153, g6());
    assertEquals(true, g7());
    assertEquals(true, g8());
    assertEquals(true, g9());
    assertEquals(true, g10());
  }
  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();


// Tail calling from various statements.
(function() {
  function g1() {
    for (var v in {a:0}) {
      return f_153([f_153, g1, test]);
    }
  }

  function g1let() {
    for (let v in {a:0}) {
      return f_153([f_153, g1let, test]);
    }
  }

  function g1nodecl() {
    var v;
    for (v in {a:0}) {
      return f_153([f_153, g1nodecl, test]);
    }
  }

  function g2() {
    for (var v of [1, 2, 3]) {
      return f_153([f_153, g2, test]);
    }
  }

  function g2let() {
    for (let v of [1, 2, 3]) {
      return f_153([f_153, g2let, test]);
    }
  }

  function g2nodecl() {
    var v;
    for (v of [1, 2, 3]) {
      return f_153([f_153, g2nodecl, test]);
    }
  }

  function g3() {
    for (var i = 0; i < 10; i++) {
      return f_153([f_153, test]);
    }
  }

  function g3let() {
    for (let i = 0; i < 10; i++) {
      return f_153([f_153, test]);
    }
  }

  function g3nodecl() {
    var i;
    for (i = 0; i < 10; i++) {
      return f_153([f_153, test]);
    }
  }

  function g4() {
    while (true) {
      return f_153([f_153, test]);
    }
  }

  function g5() {
    do {
      return f_153([f_153, test]);
    } while (true);
  }

  function test() {
    assertEquals(153, g1());
    assertEquals(153, g1let());
    assertEquals(153, g1nodecl());
    assertEquals(153, g2());
    assertEquals(153, g2let());
    assertEquals(153, g2nodecl());
    assertEquals(153, g3());
    assertEquals(153, g3let());
    assertEquals(153, g3nodecl());
    assertEquals(153, g4());
    assertEquals(153, g5());
  }
  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();


// Test tail calls from try-catch constructs.
(function() {
  function tc1(a) {
    try {
      f_153([f_153, tc1, test]);
      return f_153([f_153, tc1, test]);
    } catch(e) {
      f_153([f_153, tc1, test]);
    }
  }

  function tc2(a) {
    try {
      f_153([f_153, tc2, test]);
      throw new Error("boom");
    } catch(e) {
      f_153([f_153, tc2, test]);
      return f_153([f_153, test]);
    }
  }

  function tc3(a) {
    try {
      f_153([f_153, tc3, test]);
      throw new Error("boom");
    } catch(e) {
      f_153([f_153, tc3, test]);
    }
    f_153([f_153, tc3, test]);
    return f_153([f_153, test]);
  }

  function test() {
    assertEquals(153, tc1());
    assertEquals(153, tc2());
    assertEquals(153, tc3());
  }
  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();


// Test tail calls from try-finally constructs.
(function() {
  function tf1(a) {
    try {
      f_153([f_153, tf1, test]);
      return f_153([f_153, tf1, test]);
    } finally {
      f_153([f_153, tf1, test]);
    }
  }

  function tf2(a) {
    try {
      f_153([f_153, tf2, test]);
      throw new Error("boom");
    } finally {
      f_153([f_153, tf2, test]);
      return f_153([f_153, test]);
    }
  }

  function tf3(a) {
    try {
      f_153([f_153, tf3, test]);
    } finally {
      f_153([f_153, tf3, test]);
    }
    return f_153([f_153, test]);
  }

  function test() {
    assertEquals(153, tf1());
    assertEquals(153, tf2());
    assertEquals(153, tf3());
  }
  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();


// Test tail calls from try-catch-finally constructs.
(function() {
  function tcf1(a) {
    try {
      f_153([f_153, tcf1, test]);
      return f_153([f_153, tcf1, test]);
    } catch(e) {
    } finally {
      f_153([f_153, tcf1, test]);
    }
  }

  function tcf2(a) {
    try {
      f_153([f_153, tcf2, test]);
      throw new Error("boom");
    } catch(e) {
      f_153([f_153, tcf2, test]);
      return f_153([f_153, tcf2, test]);
    } finally {
      f_153([f_153, tcf2, test]);
    }
  }

  function tcf3(a) {
    try {
      f_153([f_153, tcf3, test]);
      throw new Error("boom");
    } catch(e) {
      f_153([f_153, tcf3, test]);
    } finally {
      f_153([f_153, tcf3, test]);
      return f_153([f_153, test]);
    }
  }

  function tcf4(a) {
    try {
      f_153([f_153, tcf4, test]);
      throw new Error("boom");
    } catch(e) {
      f_153([f_153, tcf4, test]);
    } finally {
      f_153([f_153, tcf4, test]);
    }
    return f_153([f_153, test]);
  }

  function test() {
    assertEquals(153, tcf1());
    assertEquals(153, tcf2());
    assertEquals(153, tcf3());
    assertEquals(153, tcf4());
  }
  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();


// Test tail calls from arrow functions.
(function () {
  function g1(a) {
    return (() => { return f_153([f_153, test]); })();
  }

  function g2(a) {
    return (() => f_153([f_153, test]))();
  }

  function g3(a) {
    var closure = () => f([f, closure, test], true)
                              ? f_153([f_153, test])
                              : f_153([f_153, test]);
    return closure();
  }

  function test() {
    assertEquals(153, g1());
    assertEquals(153, g2());
    assertEquals(153, g3());
  }
  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();


// Test tail calls from do expressions.
(function () {
  function g1(a) {
    var a = do { return f_153([f_153, test]); 42; };
    return a;
  }

  function test() {
    assertEquals(153, g1());
  }
  test();
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
})();
