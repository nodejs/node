// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  class X {
    static name = "name";
    static length = 15;
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
    field = Object.preventExtensions(this);
  }

  assertThrows(() => {
    new X();
  }, TypeError, /Cannot define property field, object is not extensible/);
}

{
  class X {
    field = Object.defineProperty(
      this,
      "field2",
      { writable: false, configurable: true, value: 1}
    );
    field2 = 2;
  }

  let x = new X();
  assertEquals(2, x.field2);
}

{
  class X {
    field = Object.defineProperty(
      this,
      "field2",
      { writable: false, configurable: false, value: 1}
    );
    field2 = true;
  }

  assertThrows(() => {
    new X();
  }, TypeError, /Cannot redefine property: field2/);
}

{
  class X {
    field = Object.defineProperty(
      this,
      "field2",
      { writable: true, configurable: false, value: 1}
    );
    field2 = true;
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
      "field2",
      {
        configurable: true,
        set(val) {
          setterCalled = true;
        }
      }
    );
    field2 = 2;
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
      "field2",
      {
        configurable: false,
        set(val) {
          setterCalled = true;
        }
      }
    );
    field2 = 2;
  }

  assertThrows(() => {
    new X();
  }, TypeError, /Cannot redefine property: field2/);
  assertFalse(setterCalled);
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
        { writable: true, configurable: true, value: "initial"}
      );
      return 1;
    })();
    normalField = "written";
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
    setterField = "written";
    constructor(arg) {
      super(arg);
    }
  }

  class ClassWithReadOnlyField extends Base {
    field = (() => {
      Object.defineProperty(
        this,
        "readOnlyField",
        { writable: false, configurable: true, value: "initial"}
      );
      return 1;
    })();
    readOnlyField = "written";
    constructor(arg) {
      super(arg);
    }
  }

  class ClassWithNonConfigurableField extends Base {
    field = (() => {
      Object.defineProperty(
        this,
        "nonConfigurableField",
        { writable: false, configurable: false, value: "initial"}
      );
      return 1;
    })();
    nonConfigurableField = "configured";
    constructor(arg) {
      super(arg);
    }
  }

  class ClassNonExtensible extends Base {
    field = (() => {
      Object.preventExtensions(this);
      return 'defined';
    })();
    nonExtensible = 4;
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
    new ClassWithSetterField(obj);
    assertEquals(1, obj.field);
    assertFalse(setterCalled);

    obj = getObject();
    new ClassWithReadOnlyField(obj);
    assertEquals(1, obj.field);
    assertEquals("written", obj.readOnlyField);

    obj = getObject();
    assertThrows(() => {
      new ClassWithNonConfigurableField(obj);
    }, TypeError, /Cannot redefine property: nonConfigurableField/);
    assertEquals("initial", obj.nonConfigurableField);
    assertEquals(1, obj.field);

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

    return obj;
  }
  testObject(() => Object.create(null));
  testObject( () => { return {field: 1000 } });

  // Test proxies.
  {
    let trapCalls = [];
    function getProxy() {
      trapCalls = [];
      let target = {};
      return new Proxy(target, {
        get(oTarget, sKey) {
          return oTarget[sKey];
        },
        defineProperty(oTarget, sKey, oDesc) {
          trapCalls.push(sKey);
          Object.defineProperty(oTarget, sKey, oDesc);
          return oTarget;
        }
      });
    }

    let proxy = getProxy();
    new ClassWithNormalField(proxy);
    assertEquals(1, proxy.field);
    assertEquals("written", proxy.normalField);
    assertEquals(["normalField", "field", "normalField"], trapCalls);

    proxy = getProxy();
    new ClassWithSetterField(proxy);
    assertFalse(setterCalled);
    assertEquals("written", proxy.setterField);
    assertEquals(["setterField", "field", "setterField"], trapCalls);

    proxy = getProxy();
    new ClassWithReadOnlyField(proxy);
    assertEquals("written", proxy.readOnlyField);
    assertEquals(["readOnlyField", "field", "readOnlyField"], trapCalls);

    proxy = getProxy();
    assertThrows(() => {
      new ClassWithNonConfigurableField(proxy);
    }, TypeError, /Cannot redefine property: nonConfigurableField/);
    assertEquals("initial", proxy.nonConfigurableField);
    assertEquals(["nonConfigurableField", "field", "nonConfigurableField"], trapCalls);

    proxy = getProxy();
    assertThrows(() => {
      new ClassNonExtensible(proxy);
    }, TypeError, /Cannot define property field, object is not extensible/);
    assertFalse(Object.hasOwn(proxy, 'field'));
    assertFalse(Object.hasOwn(proxy, 'nonExtensible'));
    assertEquals(["field"], trapCalls);
  }

  // Test globalThis.
  {
    new ClassWithNormalField(globalThis);
    assertEquals(1, field);
    assertEquals("written", normalField);

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
