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
      return 1;
    })();
    nonExtensible = 4;
    constructor(arg) {
      super(arg);
    }
  }

  // Test dictionary objects.
  {
    let dict = Object.create(null);

    new ClassWithNormalField(dict);
    assertEquals(1, dict.field);
    assertEquals("written", dict.normalField);

    new ClassWithSetterField(dict);
    assertFalse(setterCalled);

    new ClassWithReadOnlyField(dict);
    assertEquals("written", dict.readOnlyField);

    assertThrows(() => {
      new ClassWithNonConfigurableField(dict);
    }, TypeError, /Cannot redefine property: nonConfigurableField/);
    assertEquals("initial", dict.nonConfigurableField);

    assertThrows(() => {
      new ClassNonExtensible(dict);
    }, TypeError, /Cannot define property nonExtensible, object is not extensible/);
    assertEquals(undefined, dict.nonExtensible);
  }

  // Test proxies.
  {
    let trapCalls = [];
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

    new ClassWithNormalField(proxy);
    assertEquals(1, proxy.field);
    assertEquals("written", proxy.normalField);
    assertEquals(["normalField", "field", "normalField"], trapCalls);

    trapCalls = [];
    new ClassWithSetterField(proxy);
    assertFalse(setterCalled);
    assertEquals("written", proxy.setterField);
    assertEquals(["setterField", "field", "setterField"], trapCalls);

    trapCalls = [];
    new ClassWithReadOnlyField(proxy);
    assertEquals("written", proxy.readOnlyField);
    assertEquals(["readOnlyField", "field", "readOnlyField"], trapCalls);

    trapCalls = [];
    assertThrows(() => {
      new ClassWithNonConfigurableField(proxy);
    }, TypeError, /Cannot redefine property: nonConfigurableField/);
    assertEquals("initial", proxy.nonConfigurableField);
    assertEquals(["nonConfigurableField", "field", "nonConfigurableField"], trapCalls);

    trapCalls = [];
    assertThrows(() => {
      new ClassNonExtensible(proxy);
    }, TypeError, /Cannot define property nonExtensible, object is not extensible/);
    assertEquals(undefined, proxy.nonExtensible);
    assertEquals(["field", "nonExtensible"], trapCalls);
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
    }, TypeError, /Cannot add property nonExtensible, object is not extensible/);
    assertEquals("undefined", typeof nonExtensible);
  }
}
