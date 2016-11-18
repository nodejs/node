// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestDeoptFromCopmputedNameInObjectLiteral() {
  function f() {
    var o = {
      toString: function() {
        %DeoptimizeFunction(f);
        return "x";
      }
    };
    return { [o]() { return 23 } };
  }
  assertEquals(23, f().x());
  assertEquals(23, f().x());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(23, f().x());
})();

(function TestDeoptFromCopmputedNameInClassLiteral() {
  function g() {
    var o = {
      toString: function() {
        %DeoptimizeFunction(g);
        return "y";
      }
    };
    class C {
      [o]() { return 42 };
    }
    return new C();
  }
  assertEquals(42, g().y());
  assertEquals(42, g().y());
  %OptimizeFunctionOnNextCall(g);
  assertEquals(42, g().y());
})();
