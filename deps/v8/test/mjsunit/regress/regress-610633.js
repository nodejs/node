// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getLength(a) {
  return a.length;
}

function getByteLength(a) {
  return a.byteLength;
}

function getByteOffset(a) {
  return a.byteOffset;
}

var a = new Uint8Array([1, 2, 3]);
getLength(a);
getLength(a);

Object.defineProperty(a.__proto__, 'length', {value: 42});

assertEquals(42, getLength(a));
assertEquals(42, a.length);

getByteLength(a);
getByteLength(a);

Object.defineProperty(a.__proto__, 'byteLength', {value: 42});

assertEquals(42, getByteLength(a));
assertEquals(42, a.byteLength);

getByteOffset(a);
getByteOffset(a);

Object.defineProperty(a.__proto__, 'byteOffset', {value: 42});

assertEquals(42, getByteOffset(a));
assertEquals(42, a.byteOffset);
