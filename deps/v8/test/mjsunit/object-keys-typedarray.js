// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array,
 Uint32Array, Float32Array, Float64Array]
    .forEach(constructor => {
      const huge = new constructor(128);
      assertEquals(Array.from({length: 128}).map((_, i) => String(i)),
                   Object.keys(huge));

      const tiny = new constructor(2);
      assertEquals(["0", "1"], Object.keys(tiny));

      const empty = new constructor(0);
      assertEquals([], Object.keys(empty));
});
