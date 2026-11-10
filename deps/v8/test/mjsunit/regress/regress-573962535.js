// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --no-compilation-cache --stress-flush-code
// Flags: --stress-lazy-source-positions

class B {
  #x = 'B';
  static makeHeritageEvalFunction() {
    let g;
    let h;
    let nested;
    let nestedEval;
    class C extends (
      g = eval('0, function(o) { return o.#x; }'),
      h = eval('(() => (o) => o.#x)()'),
      nested = eval('(() => { let ctx = 1; return (o) => { () => ctx; return o.#x; }; })()'),
      nestedEval = eval("eval('0, function(o) { return o.#x; }')"),
      Object
    ) {
      #x = 'C';
    }
    return {g, h, nested, nestedEval, C};
  }
}

const {g, h, nested, nestedEval, C} = B.makeHeritageEvalFunction();

// Drop the eval's top-level SharedFunctionInfo and flush bytecode before
// lazy compilation.
for (let i = 0; i < 4; i++) gc();

assertEquals('B', g(new B()));
assertThrows(() => g(new C()), TypeError);

assertEquals('B', h(new B()));
assertThrows(() => h(new C()), TypeError);

assertEquals('B', nested(new B()));
assertThrows(() => nested(new C()), TypeError);

assertEquals('B', nestedEval(new B()));
assertThrows(() => nestedEval(new C()), TypeError);
