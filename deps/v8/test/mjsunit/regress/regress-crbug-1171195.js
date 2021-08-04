// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function OriginalRegressionTest() {
  function lazy() {
  class X {
    static x = function() {
      function f() { eval(); }
     };
   }
  }
  lazy();
})();

(function TestEvalInsideFunctionInsideInitializer() {
  function lazy() {
    class A {}
    class B extends A {
      x = function() {
        eval('super.y');
      };
    }
    return B;
  }
  let c = lazy();
  let o = new c();
  assertThrows(() => {o.x()});
})();

(function TestEvalInsideArrowFunctionInsideInitializer() {
  let result;
  function lazy() {
    class A {}
    A.prototype.y = 42;
    class B extends A {
      x = () => {
        eval('result = super.y');
      };
    }
    return B;
  }
  let c = lazy();
  let o = new c();
  o.x();
  assertEquals(42, result);
})();

(function TestEvalInsideFunctionInsideMethod() {
  class A {}
  A.prototype.x = 42;
  class B extends A {
    m() {
      function f() {
        eval("super.x;");
      }
      return f;
    }
  }
  let f = (new B()).m();
  assertThrows(() => { f(); });
})();

// Same as the previous test, except for object literals.
(function TestEvalInsideFunctionInsideObjectLiteralMethod() {
  let o = {
    m() {
      function f() {
        eval("super.x;");
      }
      return f;
    }
  };
  let f = o.m();
  assertThrows(() => { f(); });
})();

(function TestEvalInsideArrowFunctionInsideMethod() {
  let result;
  class A {}
  A.prototype.x = 42;
  class B extends A {
    m() {
      let f = () => {
        eval("result = super.x;");
      }
      return f;
    }
  }
  let o = new B();
  o.m()();
  assertEquals(42, result);
})();

(function TestEvalInsideArrowFunctionInsideObjectLiteralMethod() {
  let result;
  let o = {
    __proto__: {'x': 42},
    m() {
      let f = () => {
        eval("result = super.x;");
      }
      return f;
    }
  };
  o.m()();
  assertEquals(42, result);
})();

(function TestSkippingMethodWithEvalInsideInnerFunc() {
  function lazy() {
    class MyClass {
      test_method() {
        var var1;
        function f1() { eval(''); }
        function skippable() { }
      }
    }
    var o = new MyClass(); return o.test_method;
  }
  lazy();
})();

(function TestSkippingMethod() {
  function lazy() {
    class A {}
    class B extends A {
      skip_me() { return super.bar; }
    }
  }
  lazy();
})();

(function TestSkippingObjectLiteralMethod() {
  function lazy() {
    let o = {
      skip_me() { return super.bar; }
    };
  }
  lazy();
})();

(function TestSkippingMethodWithEval() {
  function lazy() {
    class A {}
    class B extends A {
      skip_me() { eval(''); }
    }
  }
  lazy();
})();

(function TestSkippingObjectLiteralMethodWithEval() {
  function lazy() {
    let o = {
      skip_me() { eval(''); }
    };
  }
  lazy();
})();
