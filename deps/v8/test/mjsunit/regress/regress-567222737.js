// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-code

// CoverInitializedName inside a call expression on the LHS of for-in/of must
// be rejected as a SyntaxError even though legacy web compatibility defers the
// invalid LHS ReferenceError for call expressions to runtime.
assertThrows("for (g({x = 1}) in {}) {}", SyntaxError);
assertThrows("for (g({x = 1}) of []) {}", SyntaxError);
assertThrows("async function f() { for await (g({x = 1}) of []) {} }", SyntaxError);

// Valid call expressions on the LHS of for-in/of should still parse and throw
// ReferenceError at runtime.
let count = 0;
function g() {
  count++;
}
assertThrows(() => {
  for (g({x: 1}) in {a: 1}) {}
}, ReferenceError);
assertEquals(1, count);

assertThrows(() => {
  for (g({x: 1}) of [1]) {}
}, ReferenceError);
assertEquals(2, count);

// Ensure lazy vs. eager parsing of an inner function containing a call target
// with CoverInitializedName in a for-in header cannot cause a ContextCell
// mutability mismatch.
assertThrows(() => {
  Function(`
    function maker() {
      return (() => {
        let x = 1, z = 2;
        function getter() { return x; }
        function inner() {
          for (g({x = z}) in {}) {}
        }
        return getter;
      });
    }
    const outer = maker();
    const get1 = outer();
    get1();
    gc();
    const get2 = outer();
    get2();
  `)();
}, SyntaxError);
