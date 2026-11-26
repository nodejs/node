// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Resolve many distinct names across nested scopes of varying size, from a
// lazily (pre)parsed inner function.
function MakeSource(n) {
  let decls = [];
  let uses = [];
  for (let i = 0; i < n; i++) {
    decls.push(`let v${i} = ${i};`);
    uses.push(`v${i}`);
  }
  return `
    ${decls.join('\n')}
    return function inner() {
      let shadow = -1;
      {
        let shadow = 1;
        return [${uses.join(',')}].reduce((a, b) => a + b, 0) + shadow;
      }
    };
  `;
}

for (let n of [0, 1, 7, 64, 200, 1000]) {
  let expected = (n * (n - 1)) / 2 + 1;
  let f = new Function(MakeSource(n));
  assertEquals(expected, f()());
}

// Named function expression variables are added to the scope after the fact
// when the function is compiled lazily.
var named = function namedFn(k) {
  return k === 0 ? namedFn : namedFn(k - 1);
};
assertSame(named, named(3));

// Class name variables, likewise.
var C = class InnerC {
  m() {
    return InnerC;
  }
};
assertSame(C, new C().m());

// Dynamic lookups through with and sloppy eval.
function WithLookup(o) {
  with (o) {
    return function() {
      return x + y;
    };
  }
}
assertEquals(3, WithLookup({x: 1, y: 2})());

function EvalLookup() {
  eval('var fromEval = 42');
  return function() {
    return fromEval;
  };
}
assertEquals(42, EvalLookup()());
