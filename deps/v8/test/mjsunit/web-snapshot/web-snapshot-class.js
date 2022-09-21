// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --verify-heap

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

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

(function TestDerivedClass() {
  function createObjects() {
    globalThis.Base = class { f() { return 8; }};
    globalThis.Foo = class extends Base { };
  }
  const realm = Realm.create();
  const { Foo, Base } = takeAndUseWebSnapshot(createObjects, ['Foo', 'Base'], realm);
  assertEquals(Base.prototype, Foo.prototype.__proto__);
  assertEquals(Base, Foo.__proto__);
  const x = new Foo();
  assertEquals(8, x.f());
})();

(function TestDerivedClassWithConstructor() {
  function createObjects() {
    globalThis.Base = class { constructor() {this.m = 43;}};
    globalThis.Foo = class extends Base{
      constructor() {
        super();
        this.n = 42;
      }
    };
  }
  const { Foo } = takeAndUseWebSnapshot(createObjects, ['Foo']);
  const x = new Foo();
  assertEquals(42, x.n);
  assertEquals(43, x.m);
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

(function TestClassWithProperties() {
  function createObjects() {
    globalThis.Foo = class Foo { };
    Foo.key1 = 'value1';
    Foo.key2 = 1;
    Foo.key3 = 2.2;
    Foo.key4 = function key4() {
      return 'key4';
    }
    Foo.key5 = [1, 2];
    Foo.key6 = {'key':'value'}
  }
  const { Foo } = takeAndUseWebSnapshot(createObjects, ['Foo']);
  assertEquals('value1', Foo.key1);
  assertEquals(1, Foo.key2);
  assertEquals(2.2, Foo.key3);
  assertEquals('key4', Foo.key4());
  assertEquals([1, 2], Foo.key5);
  assertEquals({ 'key': 'value' }, Foo.key6 );
})();

(function TestClassWithStaticProperties() {
  function createObjects() {
    globalThis.Foo = class Foo {
      static key1 = 'value1';
      static key2 = 1;
      static key3 = 2.2;
      static key4 = [1, 2];
      static key5 = {'key':'value'}
    };
  }
  const { Foo } = takeAndUseWebSnapshot(createObjects, ['Foo']);
  assertEquals('value1', Foo.key1);
  assertEquals(1, Foo.key2);
  assertEquals(2.2, Foo.key3);
  assertEquals([1, 2], Foo.key4);
  assertEquals({'key': 'value'}, Foo.key5);
})();

(function TestClassWithStaticMethods() {
  function createObjects() {
    globalThis.Foo = class Foo {
      static foo() {
        return 'foo'
      }
    };
  }
  const { Foo } = takeAndUseWebSnapshot(createObjects, ['Foo']);
  assertEquals('foo', Foo.foo());
})();

(async function TestClassWithStaticAsyncMethods() {
  function createObjects() {
    globalThis.Foo = class Foo {
      static async foo() {
        await Promise.resolve(1);
        return 'foo'
      }
    };
  }
  const { Foo } = takeAndUseWebSnapshot(createObjects, ['Foo']);
  assertEquals('foo', await Foo.foo());
})();

(function TestClassWithStaticGeneratorMethods() {
  function createObjects() {
    globalThis.Foo = class Foo {
      static *foo() {
        yield 'foo1'
        return 'foo2'
      }
    };
  }
  const { Foo } = takeAndUseWebSnapshot(createObjects, ['Foo']);
  const foo = Foo.foo()
  assertEquals('foo1', foo.next().value);
  assertEquals('foo2', foo.next().value);
  assertEquals(true, foo.next().done);
})();

(async function TestClassWithStaticAsyncGeneratorMethods() {
  function createObjects() {
    globalThis.Foo = class Foo {
      static async *foo() {
        yield 'foo1'
        return 'foo2'
      }
    };
  }
  const { Foo } = takeAndUseWebSnapshot(createObjects, ['Foo']);
  const foo = Foo.foo()
  assertEquals('foo1', (await foo.next()).value);
  assertEquals('foo2', (await foo.next()).value);
  assertEquals(true, (await foo.next()).done);
})();
