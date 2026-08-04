// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
var buffer = new ArrayBuffer(0x100);
var array = new Uint8Array(buffer).fill(55);
var tmp = {};
tmp[Symbol.toPrimitive] = function () {
  %ArrayBufferDetach(array.buffer)
  return 0;
};


assertEquals(-1, Array.prototype.indexOf.call(array, 0x00, tmp));

buffer = new ArrayBuffer(0x100);
array = new Uint8Array(buffer).fill(55);
tmp = {};
tmp[Symbol.toPrimitive] = function () {
  %ArrayBufferDetach(array.buffer)
  return 0;
};


assertEquals(false, Array.prototype.includes.call(array, 0x00, tmp));

buffer = new ArrayBuffer(0x100);
array = new Uint8Array(buffer).fill(55);
tmp = {};
tmp[Symbol.toPrimitive] = function () {
  %ArrayBufferDetach(array.buffer)
  return 0;
};
assertEquals(true, Array.prototype.includes.call(array, undefined, tmp));
