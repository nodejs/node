// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --verify-heap

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestFunction() {
  function createObjects() {
    globalThis.foo = {
      key: function () { return 'bar'; },
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('bar', foo.key());
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
  const realm = Realm.create();
  const { F } = takeAndUseWebSnapshot(createObjects, ['F'], realm);
  const o = new F();
  assertSame(Realm.eval(realm, 'Object.prototype'),
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
  const realm = Realm.create();
  const { Sub, Super } =
      takeAndUseWebSnapshot(createObjects, ['Sub', 'Super'], realm);
  const o = new Sub();
  assertEquals('superfunc', o.superfunc());
  assertEquals('subfunc', o.subfunc());
  assertSame(Super.prototype, Sub.prototype.__proto__);
  const realmFunctionPrototype = Realm.eval(realm, 'Function.prototype');
  assertSame(realmFunctionPrototype, Super.__proto__);
  assertSame(realmFunctionPrototype, Sub.__proto__);
})();

(function TestFunctionKinds() {
  function createObjects() {
    globalThis.normalFunction = function() {}
    globalThis.asyncFunction = async function() {}
    globalThis.generatorFunction = function*() {}
    globalThis.asyncGeneratorFunction = async function*() {}
  }
  const realm = Realm.create();
  const {normalFunction, asyncFunction, generatorFunction,
         asyncGeneratorFunction} =
      takeAndUseWebSnapshot(createObjects, ['normalFunction', 'asyncFunction',
          'generatorFunction', 'asyncGeneratorFunction'], realm);
  const newNormalFunction = Realm.eval(realm, 'f1 = function() {}');
  const newAsyncFunction = Realm.eval(realm, 'f2 = async function() {}');
  const newGeneratorFunction = Realm.eval(realm, 'f3 = function*() {}');
  const newAsyncGeneratorFunction =
      Realm.eval(realm, 'f4 = async function*() {}');

  assertSame(newNormalFunction.__proto__, normalFunction.__proto__);
  assertSame(newNormalFunction.prototype.__proto__,
             normalFunction.prototype.__proto__);

  assertSame(newAsyncFunction.__proto__, asyncFunction.__proto__);
  assertEquals(undefined, asyncFunction.prototype);
  assertEquals(undefined, newAsyncFunction.prototype);

  assertSame(newGeneratorFunction.__proto__, generatorFunction.__proto__);
  assertSame(newGeneratorFunction.prototype.__proto__,
             generatorFunction.prototype.__proto__);

  assertSame(newAsyncGeneratorFunction.__proto__,
             asyncGeneratorFunction.__proto__);
  assertSame(newAsyncGeneratorFunction.prototype.__proto__,
             asyncGeneratorFunction.prototype.__proto__);
})();

(function TestFunctionWithProperties() {
  function createObjects() {
    function bar() { return 'bar'; };
    bar.key1 = 'value1';
    bar.key2 = 1;
    bar.key3 = 2.2;
    bar.key4 = function key4() {
      return 'key4';
    }
    bar.key5 = [1, 2];
    bar.key6 = {'key':'value'}
    globalThis.foo = {
      bar: bar,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('bar', foo.bar());
  assertEquals('value1', foo.bar.key1);
  assertEquals(1, foo.bar.key2);
  assertEquals(2.2, foo.bar.key3);
  assertEquals('key4', foo.bar.key4());
  assertEquals([1, 2], foo.bar.key5);
  assertEquals({ 'key': 'value' }, foo.bar.key6 );
})();

(function TestAsyncFunctionWithProperties() {
  function createObjects() {
    async function bar() { return 'bar'; };
    bar.key1 = 'value1';
    bar.key2 = 1;
    bar.key3 = 2.2;
    bar.key4 = function key4() {
      return 'key4';
    }
    bar.key5 = [1, 2];
    bar.key6 = {'key':'value'}
    globalThis.foo = {
      bar: bar,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('value1', foo.bar.key1);
  assertEquals(1, foo.bar.key2);
  assertEquals(2.2, foo.bar.key3);
  assertEquals('key4', foo.bar.key4());
  assertEquals([1, 2], foo.bar.key5);
  assertEquals({'key': 'value'}, foo.bar.key6 );
})();

(function TestGeneratorFunctionWithProperties() {
  function createObjects() {
    function *bar() { return 'bar'; };
    bar.key1 = 'value1';
    bar.key2 = 1;
    bar.key3 = 2.2;
    bar.key4 = function key4() {
      return 'key4';
    };
    bar.key5 = [1, 2];
    bar.key6 = {'key':'value'};
    globalThis.foo = {
      bar: bar,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('value1', foo.bar.key1);
  assertEquals(1, foo.bar.key2);
  assertEquals(2.2, foo.bar.key3);
  assertEquals('key4', foo.bar.key4());
  assertEquals([1, 2], foo.bar.key5);
  assertEquals({'key': 'value'}, foo.bar.key6 );
})();

(function TestAsyncGeneratorFunctionWithProperties() {
  function createObjects() {
    async function *bar() { return 'bar'; };
    bar.key1 = 'value1';
    bar.key2 = 1;
    bar.key3 = 2.2;
    bar.key4 = function key4() {
      return 'key4';
    }
    bar.key5 = [1, 2];
    bar.key6 = {'key':'value'}
    globalThis.foo = {
      bar: bar,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('value1', foo.bar.key1);
  assertEquals(1, foo.bar.key2);
  assertEquals(2.2, foo.bar.key3);
  assertEquals('key4', foo.bar.key4());
  assertEquals([1, 2], foo.bar.key5);
  assertEquals({'key': 'value'}, foo.bar.key6);
})();

(function TestFunctionsWithSameMap() {
  function createObjects() {
    function bar1() { return 'bar1'; };
    bar1.key = 'value';

    function bar2() {
      return 'bar2';
    }
    bar2.key = 'value';

    globalThis.foo = {
      bar1: bar1,
      bar2: bar2
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('bar1', foo.bar1());
  assertEquals('value', foo.bar1.key);
  assertEquals('bar2', foo.bar2());
  assertEquals('value', foo.bar2.key);
  assertTrue(%HaveSameMap(foo.bar1, foo.bar2))
})();
