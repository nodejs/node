// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy

// Reparsing class member initializers when collecting source positions or
// rendering call sites with --no-lazy.
class C1 {
  m() {}
  x = new Error().stack;
}
assertEquals('string', typeof new C1().x);

class C2 {
  static m() {}
  static x = new Error().stack;
}
assertEquals('string', typeof C2.x);

assertThrows(() => {
  class C3 {
    m() {}
    x = null.prop;
  }
  new C3();
}, TypeError);

// Private brand check (#x in o) should not push #x onto FuncNameInferrer.
class C4 {
  #x;
  static testLogical(o) {
    #x in o || (o.prop = function() { return new Error().stack; });
    return o.prop();
  }
  static testComma(o) {
    #x in (o.prop = function() { return new Error().stack; }, o);
    return o.prop();
  }
}

const stack1 = C4.testLogical({});
assertFalse(stack1.includes('#x'));

const stack2 = C4.testComma({});
assertFalse(stack2.includes('#x'));

// Private brand check (#y in o) with an undeclared private name should report
// the SyntaxError at the position of #y, not the preceding token.
if (typeof Worker !== 'undefined') {
  const w1 = new Worker(function() {
    globalThis.onerror = function(msg, source, lineno, colno) {
      postMessage([lineno, colno]);
      return true;
    };
    eval('class C { m(o) {\n  return\n  #y in o;\n} }');
  }, {type: 'function'});
  assertEquals([3, 3], w1.getMessage());
  w1.terminate();

  const w2 = new Worker(function() {
    globalThis.onerror = function(msg, source, lineno, colno) {
      postMessage([lineno, colno]);
      return true;
    };
    eval('1;\n  #y in o;');
  }, {type: 'function'});
  assertEquals([2, 3], w2.getMessage());
  w2.terminate();
}
