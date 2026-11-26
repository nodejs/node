// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test lazy preparsing, PreparseData generation, and lazy inner compilation
// for top-level and nested arrow functions.

// 0. Script-top-level simple-parameter block-body arrow with inner closures.
const topLevelArrow = (a, b) => {
  function innerSum() {
    return a + b;
  }
  const nestedArrow = (c) => {
    return innerSum() + c;
  };
  return nestedArrow;
};
assertEquals(15, topLevelArrow(4, 5)(6));

// 1. Webpack-style IIFE module map with simple-parameter arrow modules, inner
// classes/methods, and nested arrow closures.
(function testWebpackStyleArrowModules() {
  let result = (() => {
    var shared = 10;
    var modules = {
      100: (module, exports, require) => {
        "use strict";
        class Widget {
          #secret = 5;
          constructor(v) {
            this.v = v;
          }
          compute(delta) {
            const add = (x) => {
              return x + this.v + this.#secret + shared;
            };
            return add(delta);
          }
          unusedMethod() {
            function deeplyNestedUnused() {
              return 999;
            }
            return deeplyNestedUnused();
          }
        }
        exports.Widget = Widget;
      },
      200: (module, exports, require) => {
        "use strict";
        // Never called; should stay preparsed with PreparseData.
        class UnusedModuleClass {
          methodA() { return 1; }
          methodB() { return () => 2; }
        }
        exports.Unused = UnusedModuleClass;
      }
    };
    var exp = {};
    modules[100]({}, exp, null);
    shared = 20;
    return new exp.Widget(7).compute(3);
  })();
  assertEquals(35, result);
})();

// 2. Context allocation and mutation of outer bindings from inner lazy arrows.
function testOuterVariableCaptureAndAssignment() {
  let counter = 0;
  const inc = (step) => {
    const inner = () => {
      counter += step;
      return counter;
    };
    return inner();
  };
  assertEquals(4, inc(4));
  assertEquals(10, inc(6));
}
testOuterVariableCaptureAndAssignment();

// 3. Nested lazy arrows capturing `this`, `arguments`, and `new.target`.
function testThisArgumentsNewTargetInLazyArrows() {
  function Ctor(a, b) {
    const arrow1 = () => {
      const arrow2 = () => {
        return {
          self: this,
          arg0: arguments[0],
          arg1: arguments[1],
          target: new.target,
        };
      };
      return arrow2;
    };
    this.getter = arrow1;
  }

  const inst = new Ctor("hello", 42);
  const info = inst.getter()();
  assertSame(inst, info.self);
  assertEquals("hello", info.arg0);
  assertEquals(42, info.arg1);
  assertSame(Ctor, info.target);

  // Also test `this` inside arrow head default parameter.
  function WithDefaultThis() {
    const arrow = (x = this) => {
      const nested = () => x;
      return nested();
    };
    return arrow;
  }
  const obj = { f: WithDefaultThis };
  assertSame(obj, obj.f()());
}
testThisArgumentsNewTargetInLazyArrows();

// 4. `super.prop`, `super()`, object-literal `super`, class fields/static
// blocks, and `eval("super...")` inside nested lazy arrow functions.
function testSuperAndClassFormsInLazyArrows() {
  class Base {
    constructor(x) {
      this.baseVal = x;
    }
    greet() {
      return "base:" + this.baseVal;
    }
    static staticGreet() {
      return "static-base";
    }
  }

  let staticBlockFn;
  class Derived extends Base {
    fieldArrow = (suffix) => {
      const nested = () => super.greet() + ":" + suffix;
      return nested();
    };
    static {
      staticBlockFn = () => {
        const inner = () => super.staticGreet() + ":block";
        return inner();
      };
    }
    constructor(x) {
      const callSuper = () => {
        super(x * 2);
        const readSuper = () => {
          return super.greet();
        };
        this.msg = readSuper();
      };
      callSuper();
    }
    greet() {
      const inner = () => {
        const deeper = () => {
          return super.greet() + ":derived";
        };
        return deeper();
      };
      return inner();
    }
    evalSuper() {
      const inner = () => {
        return eval("super.greet() + ':eval'");
      };
      return inner();
    }
    static staticGreet() {
      const inner = () => {
        return super.staticGreet() + ":derived";
      };
      return inner();
    }
  }

  const d = new Derived(21);
  assertEquals(42, d.baseVal);
  assertEquals("base:42", d.msg);
  assertEquals("base:42:derived", d.greet());
  assertEquals("base:42:field", d.fieldArrow("field"));
  assertEquals("base:42:eval", d.evalSuper());
  assertEquals("static-base:derived", Derived.staticGreet());
  assertEquals("static-base:block", staticBlockFn());

  // Object-literal method and accessor `super` in nested lazy arrows.
  const proto = { tag: "proto" };
  const obj = {
    __proto__: proto,
    readMethod() {
      const a = () => {
        const b = () => super.tag + ":method";
        return b();
      };
      return a();
    },
    get readGetter() {
      const a = () => {
        return super.tag + ":getter";
      };
      return a();
    }
  };
  assertEquals("proto:method", obj.readMethod());
  assertEquals("proto:getter", obj.readGetter);
}
testSuperAndClassFormsInLazyArrows();

// 5. Non-simple parameters, async arrows (including IIFEs), concise arrows,
// arrowheads, and unparenthesized single-identifier arrows inside a lazy outer
// function.
async function testComplexArrowForms() {
  const nonSimple = (a = 10, { b = 20 } = {}, ...rest) => {
    class Local {
      sum() {
        return a + b + rest.length;
      }
    }
    return new Local().sum();
  };
  assertEquals(32, nonSimple(undefined, undefined, 1, 2));

  const singleExpr = (x) => ({
    run(y) {
      return x + y;
    }
  });
  assertEquals(15, singleExpr(7).run(8));

  let defaultHolder;
  const parenNonArrow = (defaultHolder = () => {
    function helper() { return 77; }
    return helper();
  });
  assertEquals(77, parenNonArrow());
  assertEquals(77, defaultHolder());

  const arrowInArrowHead = (fn = () => {
    function helper() { return 88; }
    return helper();
  }) => {
    return fn();
  };
  assertEquals(88, arrowInArrowHead());

  const asyncArrow = async (x, y) => {
    const v = await Promise.resolve(x + y);
    function scale(k) { return v * k; }
    return scale(3);
  };
  assertEquals(27, await asyncArrow(4, 5));

  const asyncIife = await (async () => {
    const v = await Promise.resolve(14);
    return v + 1;
  })();
  assertEquals(15, asyncIife);

  const asyncIdentIife = await (async x => {
    const v = await Promise.resolve(x);
    return v * 2;
  })(11);
  assertEquals(22, asyncIdentIife);

  const singleParamArrow = x => {
    function inner() { return x * 2; }
    return inner();
  };
  assertEquals(18, singleParamArrow(9));

  const asyncSingleParamArrow = async x => {
    const v = await Promise.resolve(x + 1);
    function scale(k) { return v * k; }
    return scale(3);
  };
  assertEquals(30, await asyncSingleParamArrow(9));
}
testComplexArrowForms();

// 6. Sloppy and strict eval interaction and syntax errors in inner arrows.
function testEvalAndSyntaxErrors() {
  function withSloppyEval() {
    let captured = 123;
    const f = () => {
      return eval("var dyn = 10; captured + dyn");
    };
    return f();
  }
  assertEquals(133, withSloppyEval());

  function withStrictEval() {
    "use strict";
    let captured = 200;
    const f = () => {
      return eval("let local = 22; captured + local");
    };
    return f();
  }
  assertEquals(222, withStrictEval());

  assertThrows(
      "(() => { const f = (a, a) => { 'use strict'; }; })()", SyntaxError);
  assertThrows(
      "(() => { const f = (eval) => { 'use strict'; }; })()", SyntaxError);
  assertThrows(
      "(() => { const f = (a) => { let x; let x; }; })()", SyntaxError);
  assertThrows(
      "(() => { const f = (eval) => { 'use strict'; let x; let x; }; })()",
      SyntaxError);
  // Class field initializer exercises failed arrow preparse directly while a
  // private-name scope is live on the outer Parser.
  assertThrows(
      "(() => { class C { #p = 1; f = (a) => { this.#p; let x; let x; }; } })()",
      SyntaxError);
  assertThrows(
      "(() => { const f = () => { 'use strict'; return 010; }; })()",
      SyntaxError);
  assertThrows(
      "(() => { const f = () => { return super.x; }; })()", SyntaxError);
}
testEvalAndSyntaxErrors();
