// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var my_global = 0;

// The problem was that we allowed lazy functions inside evals, but did not
// force context allocation on the eval scope. Thus, foo was not context
// allocated since we didn't realize that a lazy function referred to it.
eval(`let foo = 1;
      let maybe_lazy = function() { foo = 2; }
      maybe_lazy();
      my_global = foo;`);
assertEquals(2, my_global);

(function TestVarInStrictEval() {
  "use strict";
  eval(`var foo = 3;
        let maybe_lazy = function() { foo = 4; }
        maybe_lazy();
        my_global = foo;`);
  assertEquals(4, my_global);
})();

eval("let foo = 1; function lazy() { foo = 2; } lazy(); my_global = foo;");
assertEquals(my_global, 2);

// Lexical variable inside a subscope in eval.
eval(`{ let foo = 5;
        function not_lazy() { foo = 6; }
        not_lazy();
        my_global = foo;
      }`);
assertEquals(my_global, 6);
