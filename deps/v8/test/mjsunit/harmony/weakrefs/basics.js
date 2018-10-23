// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs

(function TestConstructWeakFactory() {
  let wf = new WeakFactory();
  assertEquals(wf.toString(), "[object WeakFactory]");
  assertNotSame(wf.__proto__, Object.prototype);
  assertSame(wf.__proto__.__proto__, Object.prototype);
})();

(function TestWeakFactoryConstructorCallAsFunction() {
  let caught = false;
  let message = "";
  try {
    let f = WeakFactory();
  } catch (e) {
    message = e.message;
    caught = true;
  } finally {
    assertTrue(caught);
    assertEquals(message, "Constructor WeakFactory requires 'new'");
  }
})();

(function TestMakeCell() {
  let wf = new WeakFactory();
  let wc = wf.makeCell({});
  assertEquals(wc.toString(), "[object WeakCell]");
  assertNotSame(wc.__proto__, Object.prototype);
  assertSame(wc.__proto__.__proto__, Object.prototype);
  assertEquals(wc.holdings, undefined);
  let desc = Object.getOwnPropertyDescriptor(wc.__proto__, "holdings");
  assertEquals(true, desc.configurable);
  assertEquals(false, desc.enumerable);
  assertEquals("function", typeof desc.get);
  assertEquals(undefined, desc.set);
})();

(function TestMakeCellWithHoldings() {
  let wf = new WeakFactory();
  let obj = {a: 1};
  let holdings = {b: 2};
  let wc = wf.makeCell(obj, holdings);
  assertSame(wc.holdings, holdings);
})();

(function TestMakeCellWithHoldingsSetHoldings() {
  let wf = new WeakFactory();
  let obj = {a: 1};
  let holdings = {b: 2};
  let wc = wf.makeCell(obj, holdings);
  assertSame(wc.holdings, holdings);
  wc.holdings = 5;
  assertSame(wc.holdings, holdings);
})();

(function TestMakeCellWithHoldingsSetHoldingsStrict() {
  "use strict";
  let wf = new WeakFactory();
  let obj = {a: 1};
  let holdings = {b: 2};
  let wc = wf.makeCell(obj, holdings);
  assertSame(wc.holdings, holdings);
  assertThrows(() => { wc.holdings = 5; }, TypeError);
  assertSame(wc.holdings, holdings);
})();

(function TestMakeCellWithNonObject() {
  let wf = new WeakFactory();
  let message = "WeakFactory.makeCell: target must be an object";
  assertThrows(() => wf.makeCell(), TypeError, message);
  assertThrows(() => wf.makeCell(1), TypeError, message);
  assertThrows(() => wf.makeCell(false), TypeError, message);
  assertThrows(() => wf.makeCell("foo"), TypeError, message);
  assertThrows(() => wf.makeCell(Symbol()), TypeError, message);
})();

(function TestMakeCellWithProxy() {
  let handler = {};
  let obj = {};
  let proxy = new Proxy(obj, handler);
  let wf = new WeakFactory();
  let wc = wf.makeCell(proxy);
})();

(function TestMakeCellTargetAndHoldingsSameValue() {
  let wf = new WeakFactory();
  let obj = {a: 1};
  // SameValue(target, holdings) not ok
  assertThrows(() => wf.makeCell(obj, obj), TypeError,
               "WeakFactory.makeCell: target and holdings must not be same");
  // target == holdings ok
  let holdings = {a: 1};
  let wc = wf.makeCell(obj, holdings);
})();

(function TestMakeCellWithoutWeakFactory() {
  assertThrows(() => WeakFactory.prototype.makeCell.call({}, {}), TypeError);
  // Does not throw:
  let wf = new WeakFactory();
  WeakFactory.prototype.makeCell.call(wf, {});
})();

(function TestHoldingsWithoutWeakCell() {
  let wf = new WeakFactory();
  let wc = wf.makeCell({});
  let holdings_getter = Object.getOwnPropertyDescriptor(wc.__proto__, "holdings").get;
  assertThrows(() => holdings_getter.call({}), TypeError);
  // Does not throw:
  holdings_getter.call(wc);
})();
