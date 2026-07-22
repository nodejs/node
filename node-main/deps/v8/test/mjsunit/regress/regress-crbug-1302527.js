// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  class X {
    static ['name'] = "name";
    static ['length'] = 15;
  }

  assertEquals({
    "value": "name",
    "writable": true,
    "enumerable": true,
    "configurable": true
  }, Object.getOwnPropertyDescriptor(X, "name"));

  assertEquals({
    "value": 15,
    "writable": true,
    "enumerable": true,
    "configurable": true
  }, Object.getOwnPropertyDescriptor(X, "length"));
}

{
  class X {
    ['field'] = Object.preventExtensions(this);
  }

  assertThrows(() => {
    new X();
  }, TypeError, /Cannot define property field, object is not extensible/);
}

{
  class X {
    [0] = Object.preventExtensions(this);
  }

  assertThrows(() => {
    new X();
  }, TypeError, /Cannot define property 0, object is not extensible/);
}

{
  class X {
    field = Object.defineProperty(
      this,
      "field2",
      { writable: false, configurable: true, value: 1 }
    );
    ['field2'] = 2;
  }

  let x = new X();
  assertEquals({
    "value": 2,
    "writable": true,
    "enumerable": true,
    "configurable": true
  }, Object.getOwnPropertyDescriptor(x, "field2"));
}

{
  class X {
    field = Object.defineProperty(
      this,
      0,
      { writable: false, configurable: true, value: 1 }
    );
    [0] = 2;
  }

  let x = new X();
  assertEquals({
    "value": 2,
    "writable": true,
    "enumerable": true,
    "configurable": true
  }, Object.getOwnPropertyDescriptor(x, 0));
}

{
  class X {
    field = Object.defineProperty(
      this,
      "field2",
      { writable: false, configurable: false, value: 1 }
    );
    ['field2'] = true;
  }

  assertThrows(() => {
    new X();
  }, TypeError, /Cannot redefine property: field2/);
}

{
  class X {
    field = Object.defineProperty(
      this,
      0,
      { writable: false, configurable: false, value: 1 }
    );
    [0] = true;
  }

  assertThrows(() => {
    new X();
  }, TypeError, /Cannot redefine property: 0/);
}

{
  class X {
    field = Object.defineProperty(
      this,
      "field2",
      { writable: true, configurable: false, value: 1 }
    );
    ['field2'] = true;
  }

  assertThrows(() => {
    new X();
  }, TypeError, /Cannot redefine property: field2/);
}

{
  class X {
    field = Object.defineProperty(
      this,
      0,
      { writable: true, configurable: false, value: 1 }
    );
    [0] = true;
  }

  assertThrows(() => {
    new X();
  }, TypeError, /Cannot redefine property: 0/);
}

{
  let setterCalled = false;
  class X {
    field = Object.defineProperty(
      this,
      "field2",
      {
        configurable: true,
        set(val) {
          setterCalled = true;
        }
      }
    );
    ['field2'] = 2;
  }

  let x = new X();
  assertFalse(setterCalled);
  assertEquals({
    "value": 2,
    "writable": true,
    "enumerable": true,
    "configurable": true
  }, Object.getOwnPropertyDescriptor(x, 'field2'));
}

{
  let setterCalled = false;
  class X {
    field = Object.defineProperty(
      this,
      0,
      {
        configurable: true,
        set(val) {
          setterCalled = true;
        }
      }
    );
    [0] = 2;
  }

  let x = new X();
  assertFalse(setterCalled);
  assertEquals({
    "value": 2,
    "writable": true,
    "enumerable": true,
    "configurable": true
  }, Object.getOwnPropertyDescriptor(x, 0));
}

{
  let setterCalled = false;
  class X {
    field = Object.defineProperty(
      this,
      "field2",
      {
        configurable: false,
        set(val) {
          setterCalled = true;
        }
      }
    );
    ['field2'] = 2;
  }

  assertThrows(() => {
    new X();
  }, TypeError, /Cannot redefine property: field2/);
}

{
  let setterCalled = false;
  class X {
    field = Object.defineProperty(
      this,
      0,
      {
        configurable: false,
        set(val) {
          setterCalled = true;
        }
      }
    );
    [0] = 2;
  }

  assertThrows(() => {
    new X();
  }, TypeError, /Cannot redefine property: 0/);
}

{
  class Base {
    constructor(arg) {
      return arg;
    }
  }

  class ClassWithNormalField extends Base {
    field = (() => {
      Object.defineProperty(
        this,
        "normalField",
        { writable: true, configurable: true, value: "initial" }
      );
      return 1;
    })();
    ['normalField'] = "written";
    constructor(arg) {
      super(arg);
    }
  }

  class ClassWithNormalIndexField extends Base {
    field = (() => {
      Object.defineProperty(
        this,
        0,
        { writable: true, configurable: true, value: "initial" }
      );
      return 1;
    })();
    [0] = "written";
    constructor(arg) {
      super(arg);
    }
  }

  let setterCalled = false;
  class ClassWithSetterField extends Base {
    field = (() => {
      Object.defineProperty(
        this,
        "setterField",
        { configurable: true, set(val) { setterCalled = true; } }
      );
      return 1;
    })();
    ['setterField'] = "written";
    constructor(arg) {
      super(arg);
    }
  }

  class ClassWithSetterIndexField extends Base {
    field = (() => {
      Object.defineProperty(
        this,
        0,
        { configurable: true, set(val) { setterCalled = true; } }
      );
      return 1;
    })();
    [0] = "written";
    constructor(arg) {
      super(arg);
    }
  }

  class ClassWithReadOnlyField extends Base {
    field = (() => {
      Object.defineProperty(
        this,
        "readOnlyField",
        { writable: false, configurable: true, value: "initial" }
      );
      return 1;
    })();
    ['readOnlyField'] = "written";
    constructor(arg) {
      super(arg);
    }
  }

  class ClassWithReadOnlyIndexField extends Base {
    field = (() => {
      Object.defineProperty(
        this,
        0,
        { writable: false, configurable: true, value: "initial" }
      );
      return 1;
    })();
    [0] = "written";
    constructor(arg) {
      super(arg);
    }
  }

  class ClassWithNonConfigurableField extends Base {
    field = (() => {
      Object.defineProperty(
        this,
        "nonConfigurableField",
        { writable: false, configurable: false, value: "initial" }
      );
      return 1;
    })();
    ['nonConfigurableField'] = "configured";
    constructor(arg) {
      super(arg);
    }
  }

  class ClassWithNonConfigurableIndexField extends Base {
    field = (() => {
      Object.defineProperty(
        this,
        0,
        { writable: false, configurable: false, value: "initial" }
      );
      return 1;
    })();
    [0] = "configured";
    constructor(arg) {
      super(arg);
    }
  }

  class ClassNonExtensible extends Base {
    ['field'] = (() => {
      Object.preventExtensions(this);
      return 'defined';
    })();
    ['nonExtensible'] = 4;
    constructor(arg) {
      super(arg);
    }
  }
  class ClassNonExtensibleWithIndexField extends Base {
    [0] = (() => {
      Object.preventExtensions(this);
      return 'defined';
    })();
    ['nonExtensible'] = 4;
    constructor(arg) {
      super(arg);
    }
  }

  class ClassNonExtensibleWithPrivateField extends Base {
    #privateField = (() => {
      Object.preventExtensions(this);
      return "defined";
    })();
    // In case the object has a null prototype, we'll use a static
    // method to access the field.
    static getPrivateField(obj) { return obj.#privateField; }
    constructor(arg) {
      super(arg);
    }
  }

  // Test dictionary objects.
  function testObject(getObject) {
    let obj = getObject();
    new ClassWithNormalField(obj);
    assertEquals(1, obj.field);
    assertEquals("written", obj.normalField);

    obj = getObject();
    new ClassWithNormalIndexField(obj);
    assertEquals(1, obj.field);
    assertEquals("written", obj[0]);

    obj = getObject();
    new ClassWithSetterField(obj);
    assertFalse(setterCalled);

    obj = getObject();
    new ClassWithSetterIndexField(obj);
    assertFalse(setterCalled);

    obj = getObject();
    new ClassWithReadOnlyField(obj);
    assertEquals("written", obj.readOnlyField);

    obj = getObject();
    new ClassWithReadOnlyIndexField(obj);
    assertEquals("written", obj[0]);

    obj = getObject();
    assertThrows(() => {
      new ClassWithNonConfigurableField(obj);
    }, TypeError, /Cannot redefine property: nonConfigurableField/);
    assertEquals("initial", obj.nonConfigurableField);

    obj = getObject();
    assertThrows(() => {
      new ClassWithNonConfigurableIndexField(obj);
    }, TypeError, /Cannot redefine property: 0/);
    assertEquals("initial", obj[0]);

    obj = getObject();
    if (Object.hasOwn(obj, 'field')) {
      assertThrows(() => {
        new ClassNonExtensible(obj);
      }, TypeError, /Cannot define property nonExtensible, object is not extensible/);
      assertEquals({
        "value": 'defined',
        "writable": true,
        "enumerable": true,
        "configurable": true
      }, Object.getOwnPropertyDescriptor(obj, 'field'));
    } else {
      assertThrows(() => {
        new ClassNonExtensible(obj);
      }, TypeError, /Cannot define property field, object is not extensible/);
      assertFalse(Object.hasOwn(obj, 'field'));
    }
    assertFalse(Object.hasOwn(obj, 'nonExtensible'));

    obj = getObject();
    if (Object.hasOwn(obj, 0)) {
      assertThrows(() => {
        new ClassNonExtensibleWithIndexField(obj);
      }, TypeError, /Cannot define property nonExtensible, object is not extensible/);
      assertEquals({
        "value": 'defined',
        "writable": true,
        "enumerable": true,
        "configurable": true
      }, Object.getOwnPropertyDescriptor(obj, 0));
    } else {
      assertThrows(() => {
        new ClassNonExtensibleWithIndexField(obj);
      }, TypeError, /Cannot define property 0, object is not extensible/);
      assertFalse(Object.hasOwn(obj, 0));
    }
    assertFalse(Object.hasOwn(obj, 'nonExtensible'));

    obj = getObject();
    new ClassNonExtensibleWithPrivateField(obj);
    assertEquals("defined", ClassNonExtensibleWithPrivateField.getPrivateField(obj));

    return obj;
  }

  let obj = testObject(() => Object.create(null));
  assertEquals(undefined, obj.field);

  let fieldValue = 100;
  let indexValue = 100;
  obj = testObject(() => { return { field: fieldValue }; });
  obj = testObject(() => { return { field: fieldValue, [0]: indexValue }; });

  // Test proxies.
  {
    let trapCalls = [];
    function getProxy() {
      trapCalls = [];
      let target = {};
      let proxy = new Proxy(target, {
        get(oTarget, sKey) {
          return oTarget[sKey];
        },
        defineProperty(oTarget, sKey, oDesc) {
          trapCalls.push(sKey);
          Object.defineProperty(oTarget, sKey, oDesc);
          return oTarget;
        }
      });
      return proxy;
    }

    let proxy = getProxy();
    new ClassWithNormalField(proxy);
    assertEquals(1, proxy.field);
    assertEquals("written", proxy.normalField);
    assertEquals(["normalField", "field", "normalField"], trapCalls);

    proxy = getProxy();
    new ClassWithNormalIndexField(proxy);
    assertEquals(1, proxy.field);
    assertEquals("written", proxy[0]);
    assertEquals(["0", "field", "0"], trapCalls);

    proxy = getProxy();
    new ClassWithSetterField(proxy);
    assertFalse(setterCalled);
    assertEquals("written", proxy.setterField);
    assertEquals(["setterField", "field", "setterField"], trapCalls);

    proxy = getProxy();
    new ClassWithSetterIndexField(proxy);
    assertFalse(setterCalled);
    assertEquals("written", proxy[0]);
    assertEquals(["0", "field", "0"], trapCalls);

    proxy = getProxy();
    new ClassWithReadOnlyField(proxy);
    assertEquals("written", proxy.readOnlyField);
    assertEquals(["readOnlyField", "field", "readOnlyField"], trapCalls);

    proxy = getProxy();
    new ClassWithReadOnlyIndexField(proxy);
    assertEquals("written", proxy[0]);
    assertEquals(["0", "field", "0"], trapCalls);

    proxy = getProxy();
    assertThrows(() => {
      new ClassWithNonConfigurableField(proxy);
    }, TypeError, /Cannot redefine property: nonConfigurableField/);
    assertEquals("initial", proxy.nonConfigurableField);
    assertEquals(["nonConfigurableField", "field", "nonConfigurableField"], trapCalls);

    proxy = getProxy();
    assertThrows(() => {
      new ClassWithNonConfigurableIndexField(proxy);
    }, TypeError, /Cannot redefine property: 0/);
    assertEquals("initial", proxy[0]);
    assertEquals(["0", "field", "0"], trapCalls);

    proxy = getProxy();
    assertThrows(() => {
      new ClassNonExtensible(proxy);
    }, TypeError, /Cannot define property field, object is not extensible/);
    assertFalse(Object.hasOwn(proxy, 'field'));
    assertFalse(Object.hasOwn(proxy, 'nonExtensible'));
    assertEquals(["field"], trapCalls);

    proxy = getProxy();
    assertThrows(() => {
      new ClassNonExtensibleWithIndexField(proxy);
    }, TypeError, /Cannot define property 0, object is not extensible/);
    assertFalse(Object.hasOwn(proxy, 0));
    assertFalse(Object.hasOwn(proxy, 'nonExtensible'));
    assertEquals(["0"], trapCalls);

    proxy = getProxy();
    new ClassNonExtensibleWithPrivateField(proxy);
    assertEquals("defined", ClassNonExtensibleWithPrivateField.getPrivateField(proxy));
  }

  // Test globalThis.
  {
    new ClassWithNormalField(globalThis);
    assertEquals(1, field);
    assertEquals("written", normalField);

    new ClassWithNormalIndexField(globalThis);
    assertEquals(1, field);
    assertEquals("written", globalThis[0]);

    new ClassWithSetterField(globalThis);
    assertFalse(setterCalled);
    assertEquals("written", setterField);

    new ClassWithSetterField(globalThis);
    assertFalse(setterCalled);
    assertEquals("written", setterField);

    new ClassWithReadOnlyField(globalThis);
    assertEquals("written", readOnlyField);

    assertThrows(() => {
      new ClassWithNonConfigurableField(globalThis);
    }, TypeError, /Cannot redefine property: nonConfigurableField/);
    assertEquals("initial", nonConfigurableField);

    assertThrows(() => {
      new ClassNonExtensible(globalThis);
    }, TypeError, /Cannot define property nonExtensible, object is not extensible/);
    assertEquals("undefined", typeof nonExtensible);
  }
}
