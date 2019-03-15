// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs

(function TestConstructFinalizationGroup() {
  let fg = new FinalizationGroup(() => {});
  assertEquals(fg.toString(), "[object FinalizationGroup]");
  assertNotSame(fg.__proto__, Object.prototype);
  assertSame(fg.__proto__.__proto__, Object.prototype);
})();

(function TestFinalizationGroupConstructorCallAsFunction() {
  let caught = false;
  let message = "";
  try {
    let f = FinalizationGroup(() => {});
  } catch (e) {
    message = e.message;
    caught = true;
  } finally {
    assertTrue(caught);
    assertEquals(message, "Constructor FinalizationGroup requires 'new'");
  }
})();

(function TestConstructFinalizationGroupCleanupNotCallable() {
  let message = "FinalizationGroup: cleanup must be callable";
  assertThrows(() => { let fg = new FinalizationGroup(); }, TypeError, message);
  assertThrows(() => { let fg = new FinalizationGroup(1); }, TypeError, message);
  assertThrows(() => { let fg = new FinalizationGroup(null); }, TypeError, message);
})();

(function TestConstructFinalizationGroupWithCallableProxyAsCleanup() {
  let handler = {};
  let obj = () => {};
  let proxy = new Proxy(obj, handler);
  let fg = new FinalizationGroup(proxy);
})();

(function TestConstructFinalizationGroupWithNonCallableProxyAsCleanup() {
  let message = "FinalizationGroup: cleanup must be callable";
  let handler = {};
  let obj = {};
  let proxy = new Proxy(obj, handler);
  assertThrows(() => { let fg = new FinalizationGroup(proxy); }, TypeError, message);
})();

(function TestRegisterWithNonObjectTarget() {
  let fg = new FinalizationGroup(() => {});
  let message = "FinalizationGroup.prototype.register: target must be an object";
  assertThrows(() => fg.register(1, "holdings"), TypeError, message);
  assertThrows(() => fg.register(false, "holdings"), TypeError, message);
  assertThrows(() => fg.register("foo", "holdings"), TypeError, message);
  assertThrows(() => fg.register(Symbol(), "holdings"), TypeError, message);
  assertThrows(() => fg.register(null, "holdings"), TypeError, message);
  assertThrows(() => fg.register(undefined, "holdings"), TypeError, message);
})();

(function TestRegisterWithProxy() {
  let handler = {};
  let obj = {};
  let proxy = new Proxy(obj, handler);
  let fg = new FinalizationGroup(() => {});
  fg.register(proxy);
})();

(function TestRegisterTargetAndHoldingsSameValue() {
  let fg = new FinalizationGroup(() => {});
  let obj = {a: 1};
  // SameValue(target, holdings) not ok
  assertThrows(() => fg.register(obj, obj), TypeError,
               "FinalizationGroup.prototype.register: target and holdings must not be same");
  let holdings = {a: 1};
  fg.register(obj, holdings);
})();

(function TestRegisterWithoutFinalizationGroup() {
  assertThrows(() => FinalizationGroup.prototype.register.call({}, {}, "holdings"), TypeError);
  // Does not throw:
  let fg = new FinalizationGroup(() => {});
  FinalizationGroup.prototype.register.call(fg, {}, "holdings");
})();

(function TestUnregisterWithNonExistentKey() {
  let fg = new FinalizationGroup(() => {});
  fg.unregister({"k": "whatever"});
})();

(function TestWeakRefConstructor() {
  let wr = new WeakRef({});
  assertEquals(wr.toString(), "[object WeakRef]");
  assertNotSame(wr.__proto__, Object.prototype);

  let deref_desc = Object.getOwnPropertyDescriptor(wr.__proto__, "deref");
  assertEquals(true, deref_desc.configurable);
  assertEquals(false, deref_desc.enumerable);
  assertEquals("function", typeof deref_desc.value);
})();

(function TestWeakRefConstructorWithNonObject() {
  let message = "WeakRef: target must be an object";
  assertThrows(() => new WeakRef(), TypeError, message);
  assertThrows(() => new WeakRef(1), TypeError, message);
  assertThrows(() => new WeakRef(false), TypeError, message);
  assertThrows(() => new WeakRef("foo"), TypeError, message);
  assertThrows(() => new WeakRef(Symbol()), TypeError, message);
  assertThrows(() => new WeakRef(null), TypeError, message);
  assertThrows(() => new WeakRef(undefined), TypeError, message);
})();

(function TestWeakRefConstructorCallAsFunction() {
  let caught = false;
  let message = "";
  try {
    let f = WeakRef({});
  } catch (e) {
    message = e.message;
    caught = true;
  } finally {
    assertTrue(caught);
    assertEquals(message, "Constructor WeakRef requires 'new'");
  }
})();

(function TestWeakRefWithProxy() {
  let handler = {};
  let obj = {};
  let proxy = new Proxy(obj, handler);
  let wr = new WeakRef(proxy);
})();

(function TestCleanupSomeWithoutFinalizationGroup() {
  assertThrows(() => FinalizationGroup.prototype.cleanupSome.call({}), TypeError);
  // Does not throw:
  let fg = new FinalizationGroup(() => {});
  let rv = FinalizationGroup.prototype.cleanupSome.call(fg);
  assertEquals(undefined, rv);
})();
