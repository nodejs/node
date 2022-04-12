// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

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

(function TestInPlaceStringsInArray() {
  function createObjects() {
    globalThis.foo = {
      array: ['foo', 'bar', 'baz']
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  // We cannot test that the strings are really in-place; that's covered by
  // cctests.
  assertEquals('foobarbaz', foo.array.join(''));
})();

(function TestRepeatedInPlaceStringsInArray() {
  function createObjects() {
    globalThis.foo = {
      array: ['foo', 'bar', 'foo']
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  // We cannot test that the strings are really in-place; that's covered by
  // cctests.
  assertEquals('foobarfoo', foo.array.join(''));
})();

(function TestInPlaceStringsInObject() {
  function createObjects() {
    globalThis.foo = {a: 'foo', b: 'bar', c: 'baz'};
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  // We cannot test that the strings are really in-place; that's covered by
  // cctests.
  assertEquals('foobarbaz', foo.a + foo.b + foo.c);
})();

(function TestRepeatedInPlaceStringsInObject() {
  function createObjects() {
    globalThis.foo = {a: 'foo', b: 'bar', c: 'foo'};
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  // We cannot test that the strings are really in-place; that's covered by
  // cctests.
  assertEquals('foobarfoo', foo.a + foo.b + foo.c);
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

(function TestFunctionPrototypeBecomesProto() {
  function createObjects() {
    globalThis.F = function() {}
    globalThis.F.prototype.x = 100;
  }
  const { F } = takeAndUseWebSnapshot(createObjects, ['F']);
  const o = new F();
  assertEquals(100, Object.getPrototypeOf(o).x);
})();

(function TestFunctionCtorCallsFunctionInPrototype() {
  function createObjects() {
    globalThis.F = function() {
      this.fooCalled = false;
      this.foo();
    }
    globalThis.F.prototype.foo = function() { this.fooCalled = true; };
  }
  const { F } = takeAndUseWebSnapshot(createObjects, ['F']);
  const o = new F();
  assertTrue(o.fooCalled);
})();

(function TestFunctionPrototypeConnectedToObjectPrototype() {
  function createObjects() {
    globalThis.F = function() {}
  }
  const { F } = takeAndUseWebSnapshot(createObjects, ['F']);
  const o = new F();
  assertEquals(Object.prototype,
               Object.getPrototypeOf(Object.getPrototypeOf(o)));
})();

(function TestFunctionInheritance() {
  function createObjects() {
    globalThis.Super = function() {}
    globalThis.Super.prototype.superfunc = function() { return 'superfunc'; };
    globalThis.Sub = function() {}
    globalThis.Sub.prototype = Object.create(Super.prototype);
    globalThis.Sub.prototype.subfunc = function() { return 'subfunc'; };
  }
  const { Sub } = takeAndUseWebSnapshot(createObjects, ['Sub']);
  const o = new Sub();
  assertEquals('superfunc', o.superfunc());
  assertEquals('subfunc', o.subfunc());
})();
