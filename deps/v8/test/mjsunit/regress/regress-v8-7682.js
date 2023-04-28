// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const impl = Symbol();
class MyArrayLike {
  constructor() {
    this[impl] = [2, 1];
    Object.freeze(this);
  }
  get 0() { return this[impl][0]; }
  set 0(value) { this[impl][0] = value; }
  get 1() { return this[impl][1]; }
  set 1(value) { this[impl][1] = value; }
  get length() { return 2; }
}

const xs = new MyArrayLike();
Array.prototype.sort.call(xs);

// Sort-order is implementation-defined as we actually hit two conditions from
// the spec:
//   - "xs" is sparse and IsExtensible(xs) is false (its frozen).
//   - "xs" is sparse and the prototype has properties in the sort range.
assertEquals(1, xs[0]);
assertEquals(2, xs[1]);
