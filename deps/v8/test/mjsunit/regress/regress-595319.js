// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// https://bugs.chromium.org/p/chromium/issues/detail?id=595319
// Ensure exceptions are checked for by Array.prototype.concat from adding
// an element, and that elements are added to array subclasses appropriately

// If adding a property does throw, the exception is propagated
class MyException extends Error { }
class NoDefinePropertyArray extends Array {
  constructor(...args) {
    super(...args);
    return new Proxy(this, {
      defineProperty() { throw new MyException(); }
    });
  }
}
assertThrows(() => new NoDefinePropertyArray().concat([1]), MyException);

// Ensure elements are added to the instance, rather than calling [[Set]].
class ZeroGetterArray extends Array { get 0() {} };
assertArrayEquals([1], new ZeroGetterArray().concat(1));

// Frozen arrays lead to throwing

class FrozenArray extends Array {
  constructor(...args) { super(...args); Object.freeze(this); }
}
assertThrows(() => new FrozenArray().concat([1]), TypeError);

// Non-configurable non-writable zero leads to throwing
class ZeroFrozenArray extends Array {
  constructor(...args) {
    super(...args);
    Object.defineProperty(this, 0, {value: 1});
  }
}
assertThrows(() => new ZeroFrozenArray().concat([1]), TypeError);
