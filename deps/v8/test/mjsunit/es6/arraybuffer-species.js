// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ArrayBuffer.prototype.slice makes subclass and checks length

class MyArrayBuffer extends ArrayBuffer { }
assertEquals(MyArrayBuffer, new MyArrayBuffer(0).slice().constructor);

class MyShortArrayBuffer extends ArrayBuffer {
  constructor(length) { super(length - 1); }
}
assertThrows(() => new MyShortArrayBuffer(5).slice(0, 4), TypeError);

class SingletonArrayBuffer extends ArrayBuffer {
  constructor(...args) {
    if (SingletonArrayBuffer.cached) return SingletonArrayBuffer.cached;
    super(...args);
    SingletonArrayBuffer.cached = this;
  }
}
assertThrows(() => new SingletonArrayBuffer(5).slice(0, 4), TypeError);

class NonArrayBuffer extends ArrayBuffer {
  constructor() {
    return {};
  }
}
assertThrows(() => new NonArrayBuffer(5).slice(0, 4), TypeError);

// Species fallback is ArrayBuffer
class UndefinedArrayBuffer extends ArrayBuffer { }
UndefinedArrayBuffer.prototype.constructor = undefined;
assertEquals(ArrayBuffer, new UndefinedArrayBuffer(0).slice().constructor);
