// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var typedArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array
];

var descriptor = { get: function() { throw new Error("accessed length"); } };

for (var constructor of typedArrayConstructors) {
  var differentConstructor =
    constructor === Uint8Array ? Int8Array : Uint8Array;
  var target = new constructor(16);
  Object.defineProperty(target, "length", descriptor);

  var sameBuffer = new differentConstructor(target.buffer, 0, 2);
  Object.defineProperty(sameBuffer, "length", descriptor);
  target.set(sameBuffer);

  var differentBuffer = new differentConstructor(16);
  Object.defineProperty(differentBuffer, "length", descriptor);
  target.set(differentBuffer);

  var array = [0, 1, 2];
  target.set(array);
}
