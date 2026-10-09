// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-code

// CoverInitializedName in or before an import() expression must be rejected
// even when followed by a destructuring assignment argument.
assertThrows("import({x = 1})", SyntaxError);
assertThrows("import('foo', {x = 1})", SyntaxError);
assertThrows("import({x = 1}, [a] = [])", SyntaxError);
assertThrows("1 + {x = 1} + import([a] = [])", SyntaxError);
assertThrows("import('foo', {x = 1} + ([a] = []))", SyntaxError);

// Ensure lazy vs. eager parsing of an inner function containing an import()
// expression with CoverInitializedName followed by destructuring cannot cause
// a ContextCell mutability mismatch.
assertThrows(() => {
  Function(`
    function maker() {
      return (() => {
        let x = 1, z = 2;
        function getter() { return x; }
        function inner() {
          import({x = z}, [a] = []);
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
