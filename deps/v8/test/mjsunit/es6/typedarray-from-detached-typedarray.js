// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

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

for (constructor of typedArrayConstructors) {
  var ta = new constructor(10);
  %ArrayBufferDetach(ta.buffer);
  assertThrows(() => constructor.from(ta), TypeError);
}
