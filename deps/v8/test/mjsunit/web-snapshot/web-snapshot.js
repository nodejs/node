// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax


function use(exports) {
  const result = Object.create(null);
  exports.forEach(x => result[x] = globalThis[x]);
  return result;
}

function takeAndUseWebSnapshot(createObjects, exports) {
  // Take a snapshot in Realm r1.
  const r1 = Realm.create();
  Realm.eval(r1, createObjects, { type: 'function' });
  const snapshot = Realm.takeWebSnapshot(r1, exports);
  // Use the snapshot in Realm r2.
  const r2 = Realm.create();
  const success = Realm.useWebSnapshot(r2, snapshot);
  assertTrue(success);
  return Realm.eval(r2, use, { type: 'function', arguments: [exports] });
}

(function TestMinimal() {
  function createObjects() {
    globalThis.foo = {
      str: 'hello',
      n: 42,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('hello', foo.str);
  assertEquals(42, foo.n);
})();

(function TestEmptyObject() {
  function createObjects() {
    globalThis.foo = {};
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([], Object.keys(foo));
})();

(function TestNumbers() {
  function createObjects() {
    globalThis.foo = {
      a: 6,
      b: -7,
      c: 7.3,
      d: NaN,
      e: Number.POSITIVE_INFINITY,
      f: Number.NEGATIVE_INFINITY,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(6, foo.a);
  assertEquals(-7, foo.b);
  assertEquals(7.3, foo.c);
  assertEquals(NaN, foo.d);
  assertEquals(Number.POSITIVE_INFINITY, foo.e);
  assertEquals(Number.NEGATIVE_INFINITY, foo.f);
})();

(function TestTopLevelNumbers() {
  function createObjects() {
    globalThis.a = 6;
    globalThis.b = -7;
  }
  const { a, b } = takeAndUseWebSnapshot(createObjects, ['a', 'b']);
  assertEquals(6, a);
  assertEquals(-7, b);
})();

(function TestOddballs() {
  function createObjects() {
    globalThis.foo = {
      a: true,
      b: false,
      c: null,
      d: undefined,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertTrue(foo.a);
  assertFalse(foo.b);
  assertEquals(null, foo.c);
  assertEquals(undefined, foo.d);
})();

(function TestTopLevelOddballs() {
  function createObjects() {
    globalThis.a = true;
    globalThis.b = false;
  }
  const { a, b } = takeAndUseWebSnapshot(createObjects, ['a', 'b']);
  assertTrue(a);
  assertFalse(b);
})();

(function TestStringWithNull() {
  function createObjects() {
    globalThis.s = 'l\0l';
  }
  const { s } = takeAndUseWebSnapshot(createObjects, ['s']);
  assertEquals(108, s.charCodeAt(0));
  assertEquals(0, s.charCodeAt(1));
  assertEquals(108, s.charCodeAt(2));
})();

(function TestTwoByteString() {
  function createObjects() {
    globalThis.s = '\u{1F600}';
  }
  const { s } = takeAndUseWebSnapshot(createObjects, ['s']);
  assertEquals('\u{1F600}', s);
})();

(function TestTwoByteStringWithNull() {
  function createObjects() {
    globalThis.s = 'l\0l\u{1F600}';
  }
  const { s } = takeAndUseWebSnapshot(createObjects, ['s']);
  assertEquals(108, s.charCodeAt(0));
  assertEquals(0, s.charCodeAt(1));
  assertEquals(108, s.charCodeAt(2));
})();

(function TestFunction() {
  function createObjects() {
    globalThis.foo = {
      key: function () { return 'bar'; },
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('bar', foo.key());
})();

(function TestFunctionWithContext() {
  function createObjects() {
    globalThis.foo = {
      key: (function () {
        let result = 'bar';
        function inner() { return result; }
        return inner;
      })(),
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('bar', foo.key());
})();

(function TestInnerFunctionWithContextAndParentContext() {
  function createObjects() {
    globalThis.foo = {
      key: (function () {
        let part1 = 'snap';
        function inner() {
          let part2 = 'shot';
          function innerinner() {
            return part1 + part2;
          }
          return innerinner;
        }
        return inner();
      })()
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('snapshot', foo.key());
})();

(function TestTopLevelFunctionWithContext() {
  function createObjects() {
    globalThis.foo = (function () {
      let result = 'bar';
      function inner() { return result; }
      return inner;
    })();
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('bar', foo());
})();

(function TestRegExp() {
  function createObjects() {
    globalThis.foo = {
      re: /ab+c/gi,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('/ab+c/gi', foo.re.toString());
  assertTrue(foo.re.test('aBc'));
  assertFalse(foo.re.test('ac'));
})();

(function TestRegExpNoFlags() {
  function createObjects() {
    globalThis.foo = {
      re: /ab+c/,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('/ab+c/', foo.re.toString());
  assertTrue(foo.re.test('abc'));
  assertFalse(foo.re.test('ac'));
})();

(function TestTopLevelRegExp() {
  function createObjects() {
    globalThis.re = /ab+c/gi;
  }
  const { re } = takeAndUseWebSnapshot(createObjects, ['re']);
  assertEquals('/ab+c/gi', re.toString());
  assertTrue(re.test('aBc'));
  assertFalse(re.test('ac'));
})();

(function TestObjectReferencingObject() {
  function createObjects() {
    globalThis.foo = {
      bar: { baz: 11525 }
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(11525, foo.bar.baz);
})();

(function TestContextReferencingObject() {
  function createObjects() {
    function outer() {
      let o = { value: 11525 };
      function inner() { return o; }
      return inner;
    }
    globalThis.foo = {
      func: outer()
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(11525, foo.func().value);
})();

(function TestCircularObjectReference() {
  function createObjects() {
    globalThis.foo = {
      bar: {}
    };
    globalThis.foo.bar.circular = globalThis.foo;
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertSame(foo, foo.bar.circular);
})();

(function TestArray() {
  function createObjects() {
    globalThis.foo = {
      array: [5, 6, 7]
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([5, 6, 7], foo.array);
})();

(function TestEmptyArray() {
  function createObjects() {
    globalThis.foo = {
      array: []
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(0, foo.array.length);
  assertEquals([], foo.array);
})();

(function TestArrayContainingArray() {
  function createObjects() {
    globalThis.foo = {
      array: [[2, 3], [4, 5]]
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([[2, 3], [4, 5]], foo.array);
})();

(function TestArrayContainingObject() {
  function createObjects() {
    globalThis.foo = {
      array: [{ a: 1 }, { b: 2 }]
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(1, foo.array[0].a);
  assertEquals(2, foo.array[1].b);
})();

(function TestArrayContainingFunction() {
  function createObjects() {
    globalThis.foo = {
      array: [function () { return 5; }]
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(5, foo.array[0]());
})();

(function TestContextReferencingArray() {
  function createObjects() {
    function outer() {
      let o = [11525];
      function inner() { return o; }
      return inner;
    }
    globalThis.foo = {
      func: outer()
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(11525, foo.func()[0]);
})();

(function TestEmptyClass() {
  function createObjects() {
    globalThis.Foo = class Foo { };
  }
  const { Foo } = takeAndUseWebSnapshot(createObjects, ['Foo']);
  const x = new Foo();
})();

(function TestClassWithConstructor() {
  function createObjects() {
    globalThis.Foo = class {
      constructor() {
        this.n = 42;
      }
    };
  }
  const { Foo } = takeAndUseWebSnapshot(createObjects, ['Foo']);
  const x = new Foo(2);
  assertEquals(42, x.n);
})();

(function TestClassWithMethods() {
  function createObjects() {
    globalThis.Foo = class {
      f() { return 7; };
    };
  }
  const { Foo } = takeAndUseWebSnapshot(createObjects, ['Foo']);
  const x = new Foo();
  assertEquals(7, x.f());
})();

(async function TestClassWithAsyncMethods() {
  function createObjects() {
    globalThis.Foo = class {
      async g() { return 6; };
    };
  }
  const { Foo } = takeAndUseWebSnapshot(createObjects, ['Foo']);
  const x = new Foo();
  assertEquals(6, await x.g());
})();

(function TwoExportedObjects() {
  function createObjects() {
    globalThis.one = {x: 1};
    globalThis.two = {x: 2};
  }
  const { one, two } = takeAndUseWebSnapshot(createObjects, ['one', 'two']);
  assertEquals(1, one.x);
  assertEquals(2, two.x);
})();

(function TestOptimizingFunctionFromSnapshot() {
  function createObjects() {
    globalThis.f = function(a, b) { return a + b; }
  }
  const { f } = takeAndUseWebSnapshot(createObjects, ['f']);
  %PrepareFunctionForOptimization(f);
  assertEquals(3, f(1, 2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(4, f(1, 3));
})();

(function TestOptimizingConstructorFromSnapshot() {
  function createObjects() {
    globalThis.C = class {
      constructor(a, b) {
        this.x = a + b;
      }
    }
  }
  const { C } = takeAndUseWebSnapshot(createObjects, ['C']);
  %PrepareFunctionForOptimization(C);
  assertEquals(3, new C(1, 2).x);
  %OptimizeFunctionOnNextCall(C);
  assertEquals(4, new C(1, 3).x);
})();

(function TestFunctionPrototype() {
  function createObjects() {
    globalThis.F = function(p1, p2) {
      this.x = p1 + p2;
    }
    globalThis.F.prototype.m = function(p1, p2) {
      return this.x + p1 + p2;
    }
  }
  const { F } = takeAndUseWebSnapshot(createObjects, ['F']);
  const o = new F(1, 2);
  assertEquals(3, o.x);
  assertEquals(10, o.m(3, 4));
})();
