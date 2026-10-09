// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-code

// CoverInitializedName in a call argument must be rejected even when followed
// by a destructuring assignment in a subsequent argument.
assertThrows("f({x = 1}, [a] = [])", SyntaxError);
assertThrows("new C({x = 1}, [a] = [])", SyntaxError);
assertThrows("obj.m({x = 1}, {a} = {})", SyntaxError);
assertThrows("f(...{x = 1}, [a] = [])", SyntaxError);

// Object literals inside call arguments of a valid property destructuring
// target must still be allowed.
let target = {};
[(() => target)({x: 1}).prop] = [42];
assertEquals(42, target.prop);

// Ensure lazy vs. eager parsing of an inner function containing a call with
// CoverInitializedName followed by destructuring cannot cause a ContextCell
// mutability mismatch.
assertThrows(() => {
  Function(`
    function maker() {
      return (() => {
        let x = 1, z = 2;
        function getter() { return x; }
        function inner() {
          f({x = z}, [a] = []);
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
