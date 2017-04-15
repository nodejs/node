// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Neutered source
var ab = new ArrayBuffer(10);
ab.constructor = { get [Symbol.species]() { %ArrayBufferNeuter(ab); return ArrayBuffer; } };
assertThrows(() => ab.slice(0), TypeError);

// Neutered target
class NeuteredArrayBuffer extends ArrayBuffer {
  constructor(...args) {
    super(...args);
    %ArrayBufferNeuter(this);
  }
}

var ab2 = new ArrayBuffer(10);
ab2.constructor = NeuteredArrayBuffer;
assertThrows(() => ab2.slice(0), TypeError);
