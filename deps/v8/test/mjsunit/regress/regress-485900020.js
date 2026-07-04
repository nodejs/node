// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const buf = new ArrayBuffer(8, { maxByteLength: 8 });
const ta = new Uint8Array(buf);
let speciesCalled = false;
ta.constructor = {
  [Symbol.species]: function(...args) {
    speciesCalled = true;
    assertEquals(["object", "number"], args.map(x => typeof x));
    return ta;
  }
};
ta.subarray(0);
assertTrue(speciesCalled);
