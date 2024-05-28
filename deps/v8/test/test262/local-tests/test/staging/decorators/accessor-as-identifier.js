// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*---
description: Test `accessor` can be used as an identifier.
features: [decorators]
---*/

(function TestAccessorIdentifierDeclaration() {
  var accessor;
  var foo, accessor;
  try {
  } catch (accessor) {
  }
  function accessor() {}
  (function accessor() {})
  function foo(accessor) {}
  function foo(bar, accessor) {}
  accessor = 1;
  accessor += 1;
  var foo = accessor = 1;
  ++accessor;
  accessor++;
})();

(function TestAccessorIdentifierLetDeclaration() {
  let accessor;
  for (let accessor; false;) {
  }
  for (let accessor in {}) {
  }
  for (let accessor of []) {
  }
})();

(function TestAccessorIdentifierConstDeclaration() {
  const accessor = null;
  for (const accessor = null; false;) {
  }
  for (const accessor in {}) {
  }
  for (const accessor of []) {
  }
})();

(function TestAccessorAsClassElementName() {
  let C = class {
    accessor;
  }
  let c = new C();
  assert.sameValue(c.accessor, undefined);

  C = class {
    accessor = 42;
  }
  c = new C();
  assert.sameValue(c.accessor, 42);

  C = class {
    // Intentional new line after "accessor".
    accessor
    a = 42;
  }
  c = new C();
  assert.sameValue(c.a, 42);
  assert.sameValue(c.accessor, undefined);

  C = class {
    accessor() {
      return 42;
    }
  }
  c = new C();
  assert.sameValue(c.accessor(), 42);

  C = class {
    #value = 42;
    get accessor() {
      return this.#value;
    };
    set accessor(value) {
      this.#value = value
    }
  }
  c = new C();
  assert.sameValue(c.accessor, 42);
  c.accessor = 43;
  assert.sameValue(c.accessor, 43);
})();

(function TestAccessorAsStaticClassElementName() {
  class C1 {
    static accessor;
  }
  assert.sameValue(C1.accessor, undefined);

  class C2 {
    static accessor = 42;
  }
  assert.sameValue(C2.accessor, 42);

  class C3 {
    // Intentional new line after "accessor".
    static accessor
    static a = 42;
  }
  assert.sameValue(C3.a, 42);
  assert.sameValue(C3.accessor, undefined);

  class C4{
    static accessor() {
      return 42;
    }
  }
  assert.sameValue(C4.accessor(), 42);

  class C5 {
    static #value = 42;
    static get accessor() {
      return C5.#value;
    };
    static set accessor(value) {
      this.#value = value
    }
  }
  assert.sameValue(C5.accessor, 42);
  C5.accessor = 43;
  assert.sameValue(C5.accessor, 43);
})();

(function TestAccessorAsPrivateClassElementName() {
  let C = class {
    #accessor = 42;
    test() {
      assert.sameValue(this.#accessor, 42);
    }
  }
  let c = new C();
  c.test();

  C = class {
    #accessor() {
      return 42;
    }
    test() {
      assert.sameValue(this.#accessor(), 42);
    }
  }
  c = new C();
  c.test();

  C = class {
    #value = 42;
    get #accessor() {
      return this.#value
    };
    set #accessor(value) {
      this.#value = value
    }
    test() {
      assert.sameValue(this.#accessor, 42);
      this.#accessor = 43;
      assert.sameValue(this.#accessor, 43);
    }
  }
  c = new C();
  c.test();
})();

(function TestAccessorAsStaticPrivateClassElementName() {
  class C1 {
    static #accessor = 42;
    static test() {
      assert.sameValue(C1.#accessor, 42);
    }
  }
  C1.test();

  class C2 {
    static #accessor() {
      return 42;
    }
    static test() {
      assert.sameValue(C2.#accessor(), 42);
    }
  }
  C2.test();

  class C3 {
    static #value = 42;
    static get #accessor() {
      return C3.#value
    };
    static set #accessor(value) {
      C3.#value = value
    }
    static test() {
      assert.sameValue(C3.#accessor, 42);
      C3.#accessor = 43;
      assert.sameValue(C3.#accessor, 43);
    }
  }
  C3.test();
})();
