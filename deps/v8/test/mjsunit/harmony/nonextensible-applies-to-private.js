// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-nonextensible-applies-to-private --allow-natives-syntax

// 1. Private instance data field on non-extensible object
(function TestPrivateInstanceFieldNonExtensible() {
  class Base {
    constructor(seal) {
      if (seal) Object.preventExtensions(this);
    }
  }

  class Derived extends Base {
    #field = 42;
    constructor(seal) {
      super(seal);
    }
    getField() {
      return this.#field;
    }
  }

  const d1 = new Derived(false);
  assertEquals(42, d1.getField());

  assertThrows(() => new Derived(true), TypeError);
})();

// 2. Private instance method on non-extensible object
(function TestPrivateInstanceMethodNonExtensible() {
  class Base {
    constructor(seal) {
      if (seal) Object.preventExtensions(this);
    }
  }

  class Derived extends Base {
    #method() {
      return 42;
    }
    constructor(seal) {
      super(seal);
    }
    callMethod() {
      return this.#method();
    }
  }

  const d1 = new Derived(false);
  assertEquals(42, d1.callMethod());

  assertThrows(() => new Derived(true), TypeError);
})();

// 3. Private instance accessor on non-extensible object
(function TestPrivateInstanceAccessorNonExtensible() {
  class Base {
    constructor(seal) {
      if (seal) Object.preventExtensions(this);
    }
  }

  class Derived extends Base {
    get #accessor() {
      return 42;
    }
    constructor(seal) {
      super(seal);
    }
    getAccessor() {
      return this.#accessor;
    }
  }

  const d1 = new Derived(false);
  assertEquals(42, d1.getAccessor());

  assertThrows(() => new Derived(true), TypeError);
})();

// 4. Return override with non-extensible object
(function TestReturnOverrideNonExtensible() {
  class ReturnOverrideBase {
    constructor(obj) {
      return obj;
    }
  }

  class WithField extends ReturnOverrideBase {
    #field = 100;
    constructor(obj) {
      super(obj);
    }
    static getField(obj) {
      return obj.#field;
    }
  }

  const extObj = {};
  const inst1 = new WithField(extObj);
  assertEquals(100, WithField.getField(inst1));

  const nonExtObj = Object.preventExtensions({});
  assertThrows(() => new WithField(nonExtObj), TypeError);

  const sealedObj = Object.seal({});
  assertThrows(() => new WithField(sealedObj), TypeError);

  const frozenObj = Object.freeze({});
  assertThrows(() => new WithField(frozenObj), TypeError);
})();

// 5. Private static fields on non-extensible class
(function TestPrivateStaticFieldNonExtensible() {
  assertThrows(() => {
    class C {
      static #field = (Object.preventExtensions(C), 1);
    }
  }, TypeError);
})();

// 6. Proxy with non-extensible target
(function TestProxyNonExtensible() {
  class ReturnOverrideBase {
    constructor(obj) {
      return obj;
    }
  }

  class WithField extends ReturnOverrideBase {
    #field = 123;
    constructor(obj) {
      super(obj);
    }
    static getField(obj) {
      return obj.#field;
    }
  }

  const target = {};
  const proxy = new Proxy(target, {});
  const inst = new WithField(proxy);
  assertEquals(123, WithField.getField(inst));

  const frozenTarget = Object.freeze({});
  const frozenProxy = new Proxy(frozenTarget, {});
  assertThrows(() => new WithField(frozenProxy), TypeError);
})();

// 7. Modifying/reading private fields after freezing is still allowed
(function TestModifyPrivateFieldAfterFreeze() {
  class C {
    #count = 0;
    inc() {
      this.#count++;
    }
    get() {
      return this.#count;
    }
  }

  const c = new C();
  assertEquals(0, c.get());
  c.inc();
  assertEquals(1, c.get());

  Object.freeze(c);
  assertTrue(Object.isFrozen(c));

  c.inc();
  assertEquals(2, c.get());
})();

// 8. Proxy trap dynamic behaviors
(function TestProxyTrapBehaviors() {
  class ReturnOverrideBase {
    constructor(obj) {
      return obj;
    }
  }

  class WithField extends ReturnOverrideBase {
    #field = 'secret';
    constructor(obj) {
      super(obj);
    }
    static getField(obj) {
      return obj.#field;
    }
  }

  // 8a. Trap makes target non-extensible and returns false
  {
    let target = {};
    let proxy = new Proxy(target, {
      isExtensible(t) {
        Object.preventExtensions(t);
        return false;
      }
    });
    assertThrows(() => new WithField(proxy), TypeError);
  }

  // 8b. Trap makes target non-extensible but attempts to return true (invariant violation)
  {
    let target = {};
    let proxy = new Proxy(target, {
      isExtensible(t) {
        Object.preventExtensions(t);
        return true;
      }
    });
    assertThrows(() => new WithField(proxy), TypeError);
  }

  // 8c. Trap installs private field recursively
  {
    let target = {};
    let proxy;
    let called = false;
    proxy = new Proxy(target, {
      isExtensible(t) {
        if (!called) {
          called = true;
          new WithField(proxy);
        }
        return true;
      }
    });
    assertThrows(() => new WithField(proxy), TypeError);
  }

  // 8d. Already revoked proxy
  {
    let target = {};
    let { proxy, revoke } = Proxy.revocable(target, {});
    revoke();
    assertThrows(() => new WithField(proxy), TypeError);
  }

  // 8e. Trap throws custom error
  {
    let target = {};
    let proxy = new Proxy(target, {
      isExtensible(t) {
        throw new RangeError("custom trap error");
      }
    });
    assertThrows(() => new WithField(proxy), RangeError, "custom trap error");
  }
})();

// 9. LookupIterator restart verification
// Demonstrates that when the isExtensible trap executes arbitrary JS, restarting
// the LookupIterator ensures that state modifications (such as private fields or
// brands installed during the trap) are properly detected by PrivateElementFind.
(function TestLookupIteratorRestart() {
  // 9a. Private field added during isExtensible trap:
  // Before isExtensible, it.state() is NOT_FOUND.
  // Trap executes new FieldClass(proxy), which adds #field to proxy's property dictionary.
  // With it->Restart(), it.state() becomes DATA, and it throws duplicate field TypeError.
  // Without it->Restart(), it.state() would remain NOT_FOUND, erroneously bypassing the check.
  class FieldBase {
    constructor(o) { return o; }
  }
  class FieldClass extends FieldBase {
    #field = 'initial';
    constructor(o) { super(o); }
  }

  {
    let target = {};
    let called = false;
    let proxy = new Proxy(target, {
      isExtensible(t) {
        if (!called) {
          called = true;
          // Inner call stamps #field onto proxy while outer LookupIterator is in flight
          new FieldClass(proxy);
        }
        return true;
      }
    });

    assertThrows(
        () => new FieldClass(proxy),
        TypeError,
        /Cannot initialize #field twice on the same object/);
  }

  // 9b. Private brand installation on JSProxy invokes isExtensible trap.
  class BrandBase {
    constructor(o) { return o; }
  }
  class BrandClass extends BrandBase {
    #method() { return 1; }
    constructor(o) {
      // Calling super() from arrow function forces slow-path Runtime_AddPrivateBrand
      const init = () => super(o);
      init();
    }
  }

  {
    let target = {};
    let isExtensibleCalled = false;
    let proxy = new Proxy(target, {
      isExtensible(t) {
        isExtensibleCalled = true;
        return true;
      }
    });

    new BrandClass(proxy);
    assertTrue(isExtensibleCalled);
    isExtensibleCalled = false;
    assertThrows(
        () => new BrandClass(proxy),
        TypeError,
        /Cannot initialize private methods of class BrandClass twice on the same object/);
    assertTrue(isExtensibleCalled);
  }

  // Duplicate private field and brand on JSProxy:
  // IsExtensible trap runs before duplicate check (PR #7 and PR #12).
  {
    class MyError extends Error {}

    // 1. FieldClass: isExtensible returns true on second initialization -> duplicate TypeError
    {
      let target = {};
      let trapCalls = 0;
      let proxy = new Proxy(target, {
        isExtensible(t) {
          trapCalls++;
          return Reflect.isExtensible(t);
        }
      });
      new FieldClass(proxy);
      assertEquals(1, trapCalls);
      assertThrows(
          () => new FieldClass(proxy),
          TypeError,
          /Cannot initialize #field twice on the same object/);
      assertEquals(2, trapCalls);
    }

    // 2. FieldClass: isExtensible returns false on second initialization -> not extensible TypeError
    {
      let target = {};
      let trapCalls = 0;
      let proxy = new Proxy(target, {
        isExtensible(t) {
          trapCalls++;
          return Reflect.isExtensible(t);
        }
      });
      new FieldClass(proxy);
      assertEquals(1, trapCalls);
      Object.preventExtensions(target);
      assertThrows(
          () => new FieldClass(proxy),
          TypeError,
          /Cannot define property #field, object is not extensible/);
      assertEquals(2, trapCalls);
    }

    // 3. FieldClass: isExtensible throws custom error on second initialization -> custom error
    {
      let target = {};
      let shouldThrow = false;
      let proxy = new Proxy(target, {
        isExtensible(t) {
          if (shouldThrow) throw new MyError('field trap error');
          return Reflect.isExtensible(t);
        }
      });
      new FieldClass(proxy);
      shouldThrow = true;
      assertThrows(() => new FieldClass(proxy), MyError, 'field trap error');
    }

    // 4. BrandClass: isExtensible returns false on second initialization -> not extensible TypeError
    {
      let target = {};
      let trapCalls = 0;
      let proxy = new Proxy(target, {
        isExtensible(t) {
          trapCalls++;
          return Reflect.isExtensible(t);
        }
      });
      new BrandClass(proxy);
      assertEquals(1, trapCalls);
      Object.preventExtensions(target);
      assertThrows(
          () => new BrandClass(proxy),
          TypeError,
          /Cannot add private methods of class BrandClass, object is not extensible/);
      assertEquals(2, trapCalls);
    }

    // 5. BrandClass: isExtensible throws custom error on second initialization -> custom error
    {
      let target = {};
      let shouldThrow = false;
      let proxy = new Proxy(target, {
        isExtensible(t) {
          if (shouldThrow) throw new MyError('brand trap error');
          return Reflect.isExtensible(t);
        }
      });
      new BrandClass(proxy);
      shouldThrow = true;
      assertThrows(() => new BrandClass(proxy), MyError, 'brand trap error');
    }
  }

  // Brand added re-entrantly during isExtensible trap:
  {
    let target = {};
    let called = false;
    let proxy = new Proxy(target, {
      isExtensible(t) {
        if (!called) {
          called = true;
          // Inner call stamps brand onto proxy while outer LookupIterator is in flight
          new BrandClass(proxy);
        }
        return true;
      }
    });

    assertThrows(
        () => new BrandClass(proxy),
        TypeError,
        /Cannot initialize private methods of class BrandClass twice on the same object/);
  }

  {
    let obj = {};
    new FieldClass(obj);
    assertThrows(
        () => new FieldClass(obj),
        TypeError,
        /Cannot initialize #field twice on the same object/);
  }

  {
    let obj = {};
    new BrandClass(obj);
    assertThrows(
        () => new BrandClass(obj),
        TypeError,
        /Cannot initialize private methods of class BrandClass twice on the same object/);
  }

  {
    let nonExtTarget = Object.preventExtensions({});
    let nonExtProxy = new Proxy(nonExtTarget, {});
    assertThrows(
        () => new BrandClass(nonExtProxy),
        TypeError,
        /Cannot add private methods of class BrandClass, object is not extensible/);
  }

  {
    let nonExt = Object.preventExtensions({});
    assertThrows(
        () => new BrandClass(nonExt),
        TypeError,
        /Cannot add private methods of class BrandClass, object is not extensible/);
  }

  {
    let AnonBrandClass = class extends BrandBase {
      #method() { return 1; }
      constructor(o) {
        const init = () => super(o);
        init();
      }
    };
    let obj = {};
    new AnonBrandClass(obj);
    assertThrows(
        () => new AnonBrandClass(obj),
        TypeError,
        /Cannot initialize private methods of class anonymous twice on the same object/);

    let nonExt = Object.preventExtensions({});
    assertThrows(
        () => new AnonBrandClass(nonExt),
        TypeError,
        /Cannot add private methods of class anonymous, object is not extensible/);
  }
})();

// 10. Non-extensible promise awaited with internal symbols attached
(function TestNonExtensiblePromiseAwaited() {
  async function foo() {
    const p = Promise.resolve();
    Object.preventExtensions(p);
    await p;
  }
  foo();
})();
