// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Detached source
var ab = new ArrayBuffer(10);
ab.constructor = { get [Symbol.species]() { %ArrayBufferDetach(ab); return ArrayBuffer; } };
assertThrows(() => ab.slice(0), TypeError);

// Detached target
class DetachedArrayBuffer extends ArrayBuffer {
  constructor(...args) {
    super(...args);
    %ArrayBufferDetach(this);
  }
}

var ab2 = new ArrayBuffer(10);
ab2.constructor = DetachedArrayBuffer;
assertThrows(() => ab2.slice(0), TypeError);
