// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestBigInt() {
  function createObjects() {
    const b = 100n;
    const c = 2n ** 222n;
    globalThis.foo = { bar: b, bar1: c };
  }
  const {foo} = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(100n, foo.bar);
  assertEquals(2n ** 222n , foo.bar1)
})();

(function TestBigIntInArray() {
  function createObjects() {
    const b = 100n;
    const c = 2n ** 222n;
    globalThis.foo = [b, c];
  }
  const {foo} = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([100n, 2n ** 222n], foo)
})();

(function TestBigIntInFunctionContext() {
  function createObjects() {
    globalThis.foo = {
      key: (function () {
        const b = 100n;
        const c = 2n ** 222n;
        function inner() {
          return [b, c];
        }
        return inner;
      })()
    };
  }
  const {foo} = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([100n, 2n**222n], foo.key());
})();

(function TestBigIntInFunctionContextWithParentContext() {
  function createObjects() {
    globalThis.foo = {
      key: (function () {
        const b = 100n;
        function inner() {
          const c = 2n ** 222n;
          function innerinner() {
            return [b, c]
          }
          return innerinner
        }
        return inner();
      })()
    };
  }
  const {foo} = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([100n, 2n**222n], foo.key());
})();

(function TestBigIntInTopLevelFunctionWithContext() {
  function createObjects() {
    globalThis.foo = (function () {
      const b = 100n;
      const c = 2n ** 222n;
      function inner() { return [b, c]; }
      return inner;
    })();
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([100n, 2n**222n], foo());
})();


(function TestBigIntInClassStaticProperty() {
  function createObjects() {
    globalThis.foo = class Foo {
      static b = 100n;
      static c = 2n ** 222n;
    };
  }
  const { foo: Foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([100n, 2n**222n], [Foo.b, Foo.c]);
})();

(function TestBigIntInClassWithConstructor() {
  function createObjects() {
    globalThis.foo = class Foo {
      constructor() {
        this.b = 100n;
        this.c = 2n ** 222n;
      }
    };
  }
  const { foo: Foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  const foo = new Foo()
  assertEquals([100n, 2n**222n], [foo.b, foo.c]);
})();

(async function TestBigIntInClassWithMethods() {
  function createObjects() {
    globalThis.foo = class Foo {
      b() {
        return 100n;
      }
      async c() {
        return 2n ** 222n;
      }
    };
  }
  const { foo: Foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  const foo = new Foo()
  assertEquals([100n, 2n**222n], [foo.b(), await foo.c()]);
})();
