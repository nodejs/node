// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

%RuntimeEvaluateREPL('let v1 = 0;');
%RuntimeEvaluateREPL('let v1 = 0; (() => { eval(""); v1--; })(); assertEquals(-1, v1);');

%RuntimeEvaluateREPL('let v2 = 10;');
%RuntimeEvaluateREPL(`
  let v2 = 10;
  function outer(code) {
    eval(code);
    function inner() {
      v2--;
      return v2;
    }
    return inner;
  }
  assertEquals(9, outer("")());
  assertEquals(9, v2);
  assertEquals(99, outer("var v2 = 100")());
  assertEquals(9, v2);
`);
